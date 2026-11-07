# CWebServer

An HTTP/1.1 server and routing framework written in C for Linux. A supervisor forks four workers by default, sharing one TCP listener. Each worker handles one connection at a time and closes it after its response.

## Build and run

Requires Linux, GCC and Make (Windows users can use WSL). Run from the repository root:

```sh
make
./server.exe
curl --http1.1 -i http://localhost:8000/
curl --http1.1 -i http://localhost:8000/health
```

The executable listens on all IPv4 interfaces on **port 8000**, backlog 10. Despite its extension, `server.exe` is a Linux executable. The example has no command-line option parser. Ctrl+C requests shutdown; coordinated SIGTERM shutdown is not implemented.

## Current functionality

- Static GET requests resolve against the hardcoded `public` directory in the working directory. `/` resolves to `public/index.html`, `/style.css` to `public/style.css`, and `/images/profile.png` to `public/images/profile.png`.
- Static resolution runs before application routes. If it produces no response, exact target matching checks the route list. The latest duplicate registration wins. Query strings are not stripped or parsed; URL percent decoding is not implemented.
- The example registers `/health`, returning `{"health": "ok"}` as `text/plain` using `format_get_text()`.
- Responses use `Response { char *buf; int size; int capacity; }`. Workers send the explicit byte count and advance by successful partial writes. Static bodies use `Buffer.size`, preserving embedded zero bytes.
- MIME mappings cover HTML, CSS, JavaScript, PNG and SVG. Extension matching still needs correction; unsupported extensions produce no static response rather than an octet-stream fallback.
- Requests accumulate across reads in growable buffers. Parsing expects HTTP/1.1 and recognizes Host, Content-Type, Content-Length, Transfer-Encoding, Connection, Accept and Authorization. Transfer-Encoding is rejected; chunked decoding is unavailable.
- Header collection rejects more than 16 KiB of accumulated bytes, which may include an initial body portion. Additional body collection rejects more than 100,000 total buffered bytes. Incomplete requests at EOF are treated as errors.
- Reads/writes poll for readiness and check elapsed monotonic time. Timeouts are approximate: polls use five-second intervals, accepted sockets remain blocking, and sending starts a separate timeout budget.
- Responses include 200, 404, 405 and 500 statuses. Most request errors currently use 500 rather than specific client-error statuses.
- Path resolution rejects literal `..` components and uses `lstat()` to reject existing symlink directories/files. 

## Framework API

```c
#include "server.h"
#include "routing.h"
#include "http_helper.h"

Response *hello(Headers *headers, char *body) {
    (void)headers;
    (void)body;
    return format_get_text("Hello from C!");
}

int main(void) {
    register_route("/hello", hello, 0);
    return start_server(0);
}
```

Callbacks receive parsed metadata and an allocated body, or NULL when no body was collected. Return a heap-allocated response with a heap-allocated buffer; the worker frees both. Callback request arguments belong to the worker. `format_get_file()` consumes its heap-allocated Buffer and filename.

Register routes before startup, since workers inherit their route list when forked. Configuration accepts up to three positional integers:

```c
start_server(0);              /* defaults */
start_server(1, 2);           /* two workers */
start_server(3, 4, 30, 120);   /* workers, timeout seconds, lifetime seconds */
```

| Setting | Default | Accepted range |
|---|---:|---:|
| Workers | 4 | 1-50 |
| Timeout | 30 s | 5-960 s |
| Worker lifetime | 120 s | 30-3600 s |

`start_server_no_head()` forks a supervisor and returns. `kill_server()` is intended only for that background mode

## Benchmarks

Compared with NGINX 1.28.3 and lighttpd 1.4.82 on Linux under WSL2. All servers used four workers, identical static files, warm filesystem caches, and one request per connection, with compression and access logging disabled. CWebServer was compiled with GCC 15.2.0 and `-O2`.

The tables show medians across three 3-second runs after a 0.5-second warmup, at **four concurrent clients**. p95 is the median of each run's p95 latency.

| File size | Server | Successful requests/s | p95 latency | Requests/CPU-second | Peak RSS |
|---|---|---:|---:|---:|---:|
| 1 KiB | CWebServer | 8,221 | 0.55 ms | 24,657 | 4.62 MiB |
| 1 KiB | NGINX | 9,298 | 0.49 ms | 49,332 | 20.48 MiB |
| 1 KiB | lighttpd | 7,191 | 0.58 ms | 25,022 | 6.94 MiB |
| 100 KiB | CWebServer | 6,464 | 0.68 ms | 5,513 | 5.87 MiB |
| 100 KiB | NGINX | 7,832 | 0.56 ms | 28,537 | 20.73 MiB |
| 100 KiB | lighttpd | 5,941 | 0.68 ms | 16,958 | 6.94 MiB |

Across the full matrix (1 KiB and 100 KiB files; concurrency 1, 4 and 16), **54 measured runs completed with zero request errors**. Every response was checked for status, content length and exact body bytes. A separate binary fixture checked embedded zero-byte transfer integrity.

CWebServer used less aggregate resident memory in these runs, but more CPU per request than the production servers, especially for larger files. Higher requests/CPU-second means better CPU efficiency. RSS sums all server processes and counts shared pages more than once.

These are short, same-host measurements: the Python load generator exceeded 80% of one CPU core in 50 of 54 runs, so throughput may be client-limited.  See the [full comparison](benchmarks/results/COMPARISON.md) and [individual measurements](benchmarks/results/RESULTS.md).

## Known limitations

This remains a learning server, not a hardened internet service. TLS, persistent connections, HTTP/2, HEAD, range requests, compression, caching validators/304, and structured access logging are absent. 

Worker-reduction health scan can hang. Signal setup uses `sizeof(sigaction)` instead of the structure size. Performance tests do not certify security or shutdown correctness.

## Docker development environment

The Dockerfile provides Ubuntu 24.04, GCC, Make, Bash and curl. It starts a shell rather than building or launching the application:

```sh
docker build -t cwebserver-dev .
docker run -it --rm -v "${PWD}:/app" -w /app -p 8000:8000 cwebserver-dev
# Inside the container:
make
./server.exe
```

Benchmarking additionally needs Python 3.11+ and optional comparison-server binaries; these are not included in the development image.

## Source layout

| Files | Role |
|---|---|
| main.c | Example health route and startup |
| server.c / server.h | Supervisor, configuration and worker lifecycle |
| socket.c / socket.h | Listener and acceptance |
| workers.c / workers.h | Connection reads, polling, writes and cleanup |
| request.c / request.h | HTTP parsing, dispatch and request cleanup |
| routing.c / routing.h | Static serving and route registration |
| http_helper.c / http_helper.h | Response formatting and MIME selection |
| helper.c / helper.h | File/path operations and integer conversion |
| dyna_buffer.c / dyna_buffer.h | Growable byte buffers |
| worker_queue.c / worker_queue.h | Pending signal queue |
| types.h | Configuration, worker, response and route types |
| benchmarks/ | Performance suite, harness tests and comparison results |
