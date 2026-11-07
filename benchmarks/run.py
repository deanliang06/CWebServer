#!/usr/bin/env python3
"""Linux-only, dependency-free, closed-loop static HTTP comparison."""
import argparse
import asyncio
import collections
import datetime
import hashlib
import json
import math
import os
from pathlib import Path
import platform
import shutil
import signal
import socket
import statistics
import subprocess
import tempfile
import time

REPO = Path(__file__).resolve().parents[1]


def percentile(values, p):
    if not values:
        return None
    ordered = sorted(values)
    return ordered[max(0, math.ceil(p * len(ordered)) - 1)]


def resources(pgid):
    """All members of the launched process group; CPU includes reaped children."""
    ticks = rss = 0
    count = 0
    for entry in Path('/proc').iterdir():
        if not entry.name.isdigit():
            continue
        try:
            fields = (entry / 'stat').read_text().rsplit(')', 1)[1].split()
            if int(fields[2]) != pgid:
                continue
            ticks += sum(int(fields[i]) for i in (11, 12, 13, 14))
            rss += int(fields[21]) * os.sysconf('SC_PAGE_SIZE')
            count += 1
        except (OSError, ValueError, IndexError):
            continue
    return ticks / os.sysconf('SC_CLK_TCK'), rss, count


async def fetch(port, path, expected, timeout):
    writer = None
    start = time.perf_counter()
    try:
        async with asyncio.timeout(timeout):
            reader, writer = await asyncio.open_connection('127.0.0.1', port)
            writer.write(f'GET /{path} HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n'.encode())
            await writer.drain()
            header = await reader.readuntil(b'\r\n\r\n')
            lines = header[:-4].split(b'\r\n')
            if lines[0].split()[1] != b'200':
                raise ValueError('non-200 status')
            headers = dict(line.split(b':', 1) for line in lines[1:])
            headers = {k.lower(): v.strip() for k, v in headers.items()}
            if int(headers[b'content-length']) != len(expected):
                raise ValueError('wrong content length')
            body = await reader.readexactly(len(expected))
            if body != expected:
                raise ValueError('body mismatch')
            # Enforce connection-close framing and reject extra body bytes.
            if await reader.read(1):
                raise ValueError('extra response bytes')
        return (time.perf_counter() - start) * 1000
    finally:
        if writer:
            writer.close()
            try:
                await writer.wait_closed()
            except OSError:
                pass


async def exercise(port, path, expected, concurrency, duration, timeout, pgid):
    latencies = []
    errors = collections.Counter()
    attempts = 0
    peak_rss = resources(pgid)[1]
    before_cpu = resources(pgid)[0]
    client_cpu = time.process_time()
    start = time.perf_counter()
    deadline = start + duration

    async def client():
        nonlocal attempts
        while time.perf_counter() < deadline:
            attempts += 1
            try:
                latencies.append(await fetch(port, path, expected, timeout))
            except (OSError, ValueError, KeyError, asyncio.IncompleteReadError,
                    asyncio.LimitOverrunError, TimeoutError) as exc:
                errors[type(exc).__name__ + ': ' + str(exc)] += 1

    async def sample():
        nonlocal peak_rss
        while time.perf_counter() < deadline:
            peak_rss = max(peak_rss, resources(pgid)[1])
            await asyncio.sleep(0.1)

    await asyncio.gather(sample(), *(client() for _ in range(concurrency)))
    elapsed = time.perf_counter() - start
    after_cpu, end_rss, processes = resources(pgid)
    cpu = max(0, after_cpu - before_cpu)
    return dict(concurrency=concurrency, file=path, file_bytes=len(expected),
                elapsed_s=elapsed, attempts=attempts, successful=len(latencies),
                errors=dict(errors), error_pct=100 * sum(errors.values()) / max(1, attempts),
                requests_s=len(latencies) / elapsed,
                p50_ms=percentile(latencies, .50), p95_ms=percentile(latencies, .95),
                p99_ms=percentile(latencies, .99), server_cpu_s=cpu,
                cpu_pct_one_core=100 * cpu / elapsed,
                requests_per_cpu_s=len(latencies) / cpu if cpu else None,
                peak_rss_mib=peak_rss / 2**20, end_rss_mib=end_rss / 2**20,
                processes=processes, client_cpu_s=time.process_time() - client_cpu)


def configurations(work, port):
    root = work / 'public'
    nginx = work / 'nginx.conf'
    nginx.write_text(f'''worker_processes 4;
pid "{work}/nginx.pid";
error_log "{work}/nginx-error.log" warn;
events {{ worker_connections 1024; }}
http {{
 client_body_temp_path "{work}/body";
 proxy_temp_path "{work}/proxy";
 fastcgi_temp_path "{work}/fastcgi";
 uwsgi_temp_path "{work}/uwsgi";
 scgi_temp_path "{work}/scgi";
 access_log off; gzip off; sendfile off; keepalive_timeout 0;
 server {{ listen 127.0.0.1:{port} backlog=10; root "{root}";
 default_type application/octet-stream; }}
}}
''')
    lighttpd = work / 'lighttpd.conf'
    lighttpd.write_text(f'''server.document-root = "{root}"
server.bind = "127.0.0.1"
server.port = {port}
server.max-worker = 4
server.listen-backlog = 10
server.max-keep-alive-requests = 0
server.errorlog = "{work}/lighttpd-error.log"
server.pid-file = "{work}/lighttpd.pid"
server.network-backend = "writev"
mimetype.assign = ( ".html" => "text/html", ".png" => "image/png" )
''')
    return nginx, lighttpd


def stop(proc):
    # Only the process group created by this runner is signalled.
    for sig in (signal.SIGINT, signal.SIGTERM, signal.SIGKILL):
        try:
            os.killpg(proc.pid, sig)
        except ProcessLookupError:
            break
        try:
            proc.wait(timeout=2)
        except subprocess.TimeoutExpired:
            continue
        # The supervisor can exit before its children.
        if not resources(proc.pid)[2]:
            break
    proc.wait()


async def benchmark(args):
    if platform.system() != 'Linux':
        raise SystemExit('Run under Linux or WSL; resource measurements use /proc.')
    with socket.socket() as check:
        check.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        try:
            check.bind(('127.0.0.1', args.port))
        except OSError:
            raise SystemExit(f'Port {args.port} is occupied; choose --port with a free port.')
    output = Path(args.output).resolve()
    output.mkdir(parents=True, exist_ok=True)
    sources = [p for p in REPO.glob('*.c')]
    flags = ['-O2', '-Wall', '-Wextra']
    # Use an isolated root and binary; never replace the user's public files.
    with tempfile.TemporaryDirectory(prefix='cweb-bench-') as temporary:
        work = Path(temporary)
        (work / 'public').mkdir()
        fixtures = {}
        for size in args.sizes:
            name = f'file-{size}.html'
            fixtures[name] = (b'<p>benchmark</p>\n' * (size // 17 + 2))[:size]
        fixtures['binary.png'] = bytes(range(256)) * 16
        for name, data in fixtures.items():
            (work / 'public' / name).write_bytes(data)
        socket_source = (REPO / 'socket.c').read_text()
        if socket_source.count('htons(8000)') != 1:
            raise RuntimeError('Update the benchmark port substitution for the current socket.c')
        (work / 'socket.c').write_text(socket_source.replace('htons(8000)', f'htons({args.port})'))
        sources = [p for p in sources if p.name != 'socket.c'] + [work / 'socket.c']
        subprocess.run(['gcc', *flags, '-I', str(REPO), *(str(p) for p in sources), '-o', str(work / 'server')], check=True)
        nginx, lighttpd = configurations(work, args.port)
        commands = {'cweb': [str(work / 'server')]}
        if args.nginx:
            commands['nginx'] = [str(Path(args.nginx).resolve()), '-p', str(work) + '/', '-c', str(nginx), '-g', 'daemon off;']
        if args.lighttpd:
            commands['lighttpd'] = [str(Path(args.lighttpd).resolve()), '-D', '-f', str(lighttpd)]
        report = dict(environment=dict(platform=platform.platform(), cpu_count=os.cpu_count(),
                      started_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
                      cpu_model=next((line.split(':', 1)[1].strip() for line in Path('/proc/cpuinfo').read_text().splitlines() if line.startswith('model name')), 'unknown'),
                      source_sha256={p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(list(REPO.glob('*.c')) + list(REPO.glob('*.h')))},
                      compiler=subprocess.check_output(['gcc', '--version'], text=True).splitlines()[0],
                      git_commit=subprocess.check_output(['git', '-C', str(REPO), 'rev-parse', 'HEAD'], text=True).strip(),
                      git_diff=subprocess.check_output(['git', '-C', str(REPO), 'diff'], text=True),
                      compile_flags=flags, port=args.port, port_substitution_only=True, duration_s=args.duration, repeats=args.repeats,
                      warmup_s=args.warmup, connection_reuse=False, workers=4,
                      cache='warm OS cache; no cache eviction', load_model='closed-loop asyncio on same host'),
                      results=[], failures={})
        for server, command in commands.items():
            version_command = [command[0], '-v'] if server == 'nginx' else [command[0], '-v']
            if server != 'cweb':
                version = subprocess.run(version_command, capture_output=True, text=True)
                report.setdefault('versions', {})[server] = version.stdout + version.stderr
            for repeat in range(args.repeats):
                log = open(output / f'{server}-{repeat}.log', 'w')
                proc = subprocess.Popen(command, cwd=work, stdout=log, stderr=log, start_new_session=True)
                try:
                    ready = False
                    for _ in range(100):
                        if proc.poll() is not None:
                            break
                        try:
                            name = next(iter(fixtures))
                            await fetch(args.port, name, fixtures[name], .5)
                            ready = True
                            break
                        except Exception:
                            await asyncio.sleep(.05)
                    if not ready:
                        raise RuntimeError('Server did not return a valid fixture; inspect its log')
                    # Binary integrity gate, including embedded NUL bytes.
                    await fetch(args.port, 'binary.png', fixtures['binary.png'], args.timeout)
                    for name, expected in fixtures.items():
                        if name == 'binary.png':
                            continue
                        for concurrency in args.concurrency:
                            await exercise(args.port, name, expected, concurrency, args.warmup, args.timeout, proc.pid)
                            row = await exercise(args.port, name, expected, concurrency, args.duration, args.timeout, proc.pid)
                            row.update(server=server, repeat=repeat + 1)
                            report['results'].append(row)
                            print(f"{server} repeat={repeat+1} {name} c={concurrency}: {row['requests_s']:.1f} req/s, p95={row['p95_ms']} ms, errors={row['error_pct']:.2f}%", flush=True)
                except Exception as exc:
                    report['failures'][f'{server}-{repeat+1}'] = str(exc)
                finally:
                    stop(proc)
                    log.close()
                (output / 'results.json').write_text(json.dumps(report, indent=2))
        lines = ['# Benchmark results', '', 'Measured locally; see environment and raw repetitions in `results.json`.', '',
                 '| Server | Bytes | Concurrency | Repeat | Good req/s | p95 ms | p99 ms | Error % | CPU % (1 core) | Req/CPU-s | Peak RSS MiB |',
                 '|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|']
        def fmt(v):
            return 'n/a' if v is None else f'{v:.2f}'
        for r in report['results']:
            lines.append('| ' + ' | '.join(str(r[k]) if k in ('server', 'file_bytes', 'concurrency', 'repeat') else fmt(r[k]) for k in
                         ('server', 'file_bytes', 'concurrency', 'repeat', 'requests_s', 'p95_ms', 'p99_ms', 'error_pct', 'cpu_pct_one_core', 'requests_per_cpu_s', 'peak_rss_mib')) + ' |')
        if report['failures']:
            lines += ['', 'Failures: ' + json.dumps(report['failures'])]
        lines += ['', 'RSS sums all server processes and double-counts shared pages; it is not PSS. CPU is server-only user+system time, including reaped children, sampled from /proc. Short runs can fall below CPU tick resolution. Latency covers successful requests from connection establishment through validated body and EOF. Failed attempts are reported separately. These are closed-loop, same-host results, not a production capacity rating.']
        (output / 'RESULTS.md').write_text('\n'.join(lines) + '\n')
        write_comparison(report, output)
        if report['failures'] or any(r['error_pct'] for r in report['results']):
            raise SystemExit(1)


def write_comparison(report, output):
    groups = collections.defaultdict(list)
    for row in report['results']:
        groups[row['server'], row['file_bytes'], row['concurrency']].append(row)
    lines = ['# Local comparison', '',
             f"Environment: {report['environment']['platform']}; {report['environment']['cpu_count']} logical CPUs. Compiler: {report['environment']['compiler']}.", '',
             f"Measured settings: {report['environment']['duration_s']} seconds per scenario, {report['environment']['warmup_s']} seconds warmup, {report['environment']['repeats']} repetitions. See JSON for executable versions and source diff.", '',
             'Each value below is the median across repetitions for that scenario. p95 is the median of per-run p95 values, not a pooled percentile.', '',
             '| Server | Bytes | Concurrency | Runs | Good req/s | p95 ms | Req/CPU-s | Peak RSS MiB |',
             '|---|---:|---:|---:|---:|---:|---:|---:|']
    for (server, size, concurrency), rows in sorted(groups.items()):
        values = []
        for key in ('requests_s', 'p95_ms', 'requests_per_cpu_s', 'peak_rss_mib'):
            samples = [r[key] for r in rows if r[key] is not None]
            values.append(f'{statistics.median(samples):.2f}' if samples else 'n/a')
        lines.append(f'| {server} | {size} | {concurrency} | {len(rows)} | ' + ' | '.join(values) + ' |')
    saturated = [r for r in report['results'] if r['client_cpu_s'] / r['elapsed_s'] > .8]
    lines += ['', f"Load generator exceeded 80% of one core in {len(saturated)} of {len(report['results'])} measured runs. Throughput may be client-limited; do not interpret similar rates as equal maximum capacity.", '',
              'All servers used four workers, warm files, connection-close HTTP/1.1, disabled compression/access logs, userspace file sending, and the same host. Production packages retain their distro build flags; CWebServer uses -O2. RSS includes shared-page double counting. These short WSL/local tests do not establish internet readiness or production capacity.', '',
              'See [raw results](results.json) and [all repetitions](RESULTS.md).']
    (output / 'COMPARISON.md').write_text('\n'.join(lines) + '\n')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--nginx', default=shutil.which('nginx'))
    parser.add_argument('--lighttpd', default=shutil.which('lighttpd'))
    parser.add_argument('--duration', type=float, default=10)
    parser.add_argument('--warmup', type=float, default=1)
    parser.add_argument('--repeats', type=int, default=3)
    parser.add_argument('--concurrency', type=int, nargs='+', default=[1, 4, 16, 64])
    parser.add_argument('--sizes', type=int, nargs='+', default=[1024, 102400, 1048576])
    parser.add_argument('--timeout', type=float, default=5)
    parser.add_argument('--port', type=int, default=18080)
    parser.add_argument('--output', default=str(REPO / 'benchmarks' / 'results'))
    parser.add_argument('--summarize', action='store_true', help='Regenerate COMPARISON.md from existing results.json without running servers')
    args = parser.parse_args()
    if min(args.duration, args.warmup, args.repeats, args.timeout, *args.concurrency, *args.sizes) <= 0:
        parser.error('durations, counts, concurrency and sizes must be positive')
    if args.summarize:
        output = Path(args.output).resolve()
        write_comparison(json.loads((output / 'results.json').read_text()), output)
    else:
        asyncio.run(benchmark(args))
