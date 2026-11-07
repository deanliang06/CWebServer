# Local comparison

Environment: Linux-6.18.33.2-microsoft-standard-WSL2-x86_64-with-glibc2.43; 32 logical CPUs. Compiler: gcc (Ubuntu 15.2.0-16ubuntu1) 15.2.0.

Measured settings: 3.0 seconds per scenario, 0.5 seconds warmup, 3 repetitions. See JSON for executable versions and source diff.

Each value below is the median across repetitions for that scenario. p95 is the median of per-run p95 values, not a pooled percentile.

| Server | Bytes | Concurrency | Runs | Good req/s | p95 ms | Req/CPU-s | Peak RSS MiB |
|---|---:|---:|---:|---:|---:|---:|---:|
| cweb | 1024 | 1 | 3 | 5806.75 | 0.21 | 14167.74 | 4.62 |
| cweb | 1024 | 4 | 3 | 8221.18 | 0.55 | 24656.57 | 4.62 |
| cweb | 1024 | 16 | 3 | 9327.04 | 1.30 | 38360.44 | 4.62 |
| cweb | 102400 | 1 | 3 | 2864.18 | 0.42 | 4225.37 | 5.62 |
| cweb | 102400 | 4 | 3 | 6464.37 | 0.68 | 5512.78 | 5.87 |
| cweb | 102400 | 16 | 3 | 8460.31 | 1.84 | 6028.77 | 6.05 |
| lighttpd | 1024 | 1 | 3 | 5554.03 | 0.21 | 21160.00 | 6.94 |
| lighttpd | 1024 | 4 | 3 | 7190.74 | 0.58 | 25021.84 | 6.94 |
| lighttpd | 1024 | 16 | 3 | 8400.49 | 1.87 | 26038.78 | 6.94 |
| lighttpd | 102400 | 1 | 3 | 4433.72 | 0.27 | 14105.26 | 6.94 |
| lighttpd | 102400 | 4 | 3 | 5940.81 | 0.68 | 16957.55 | 6.94 |
| lighttpd | 102400 | 16 | 3 | 6356.60 | 2.42 | 18242.45 | 6.94 |
| nginx | 1024 | 1 | 3 | 6357.96 | 0.21 | 32975.86 | 20.48 |
| nginx | 1024 | 4 | 3 | 9297.60 | 0.49 | 49331.58 | 20.48 |
| nginx | 1024 | 16 | 3 | 10763.13 | 1.58 | 49471.21 | 20.48 |
| nginx | 102400 | 1 | 3 | 4562.49 | 0.31 | 17684.62 | 20.73 |
| nginx | 102400 | 4 | 3 | 7831.91 | 0.56 | 28537.35 | 20.73 |
| nginx | 102400 | 16 | 3 | 8392.55 | 1.91 | 32464.56 | 20.73 |

Load generator exceeded 80% of one core in 50 of 54 measured runs. Throughput may be client-limited; do not interpret similar rates as equal maximum capacity.

All servers used four workers, warm files, connection-close HTTP/1.1, disabled compression/access logs, userspace file sending, and the same host. Production packages retain their distro build flags; CWebServer uses -O2. RSS includes shared-page double counting. These short WSL/local tests do not establish internet readiness or production capacity.

See [raw results](results.json) and [all repetitions](RESULTS.md).
