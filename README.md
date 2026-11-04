# CWebServer

An experimental HTTP/1.1 server and small routing framework written in C. A supervisor creates a pool of worker processes sharing a TCP listener. Workers use `poll()` for connection readiness and a per-worker pipe for signal wakeups.

The example application serves an HTML welcome page at `/`, a health response at `/health`, and a plain-text 404 for unmatched targets. This is a learning project; HTTP parsing, shutdown, and resource management are still incomplete.

## Requirements

- Linux with GCC and Make, including POSIX process, signal, socket, and polling APIs.
- On Windows, use WSL or the included Docker development environment.

Despite its name, `server.exe` is a Linux executable when built in Linux. It is not a native Windows program.

## Build and run

From the repository root in a Linux shell:

```sh
make
./server.exe
```

The server listens on **all IPv4 interfaces, port 8000**, with a listen backlog of 10. The current `main.c` calls the blocking `start_server(0)` function, so the application stays in the supervisor loop until shutdown.

Try these manual checks from another terminal:

```sh
curl --http1.1 -i http://localhost:8000/
curl --http1.1 -i http://localhost:8000/health
curl --http1.1 -i http://localhost:8000/missing
```

The root route returns HTML containing “Hello from C!”. The health route returns `{"health": "ok"}`. Both use `format_get()`, which currently labels every successful response `Content-Type: text/html`, including the health response.

Use Ctrl+C (`SIGINT`) to request shutdown. SIGTERM does not currently have a coordinated shutdown handler. These commands are manual checks, not an automated test suite or a guarantee of reliable shutdown.

The Makefile recompiles object files when their source or any project header changes. It does not generate `.d` dependency files.

## Framework interface

Register routes before starting the server:

```c
#include "server.h"
#include "http_helper.h"

char *hello(Headers *headers, char *body) {
    (void)headers;
    (void)body;
    return format_get("<h1>Hello from C!</h1>");
}

int main(void) {
    register_route("/", hello);
    return start_server(0);
}
```

Callbacks have the signature `char *handler(Headers *headers, char *body)`. They receive parsed request metadata and an extracted body, or `NULL` when no body was collected. The request method, target, and protocol are available through `headers->req_line`.

A callback returns a complete, null-terminated HTTP response. `format_get()` allocates a 200 OK response with a content length and HTML content type. The worker sends the response and closes the client connection. Response ownership is not fully implemented: the worker currently does not free responses allocated by `format_get()`.

Routing compares the complete request target with `strcmp()`. It does not distinguish methods, extract query parameters, or support path parameters. For example, `/health?check=1` does not match `/health`. Routes are prepended to a list, so the most recently registered duplicate target wins. Registering routes after workers have forked does not update their inherited copies.

### Configuration

`start_server(count, ...)` accepts zero to three positional integer settings. `count` specifies how many follow it:

```c
start_server(0);              /* Defaults. */
start_server(1, 2);           /* Two workers. */
start_server(3, 4, 30, 120);   /* Workers, poll timeout, worker lifetime. */
```

| Setting | Default | Accepted range |
| --- | --- | --- |
| Worker count | 4 | 1–50 |
| Worker polling timeout | 30 seconds | 5–960 seconds |
| Maximum worker lifetime | 120 seconds | 30–3600 seconds |

The polling timeout is not a client request deadline. The supervisor checks worker lifetimes and its tracked worker count approximately once per second. The example does not parse command-line arguments; the old `-n`, `-t`, and `-l` usage text in `server.c` is stale.

### Background startup

`server.c` also implements `start_server_no_head(count, ...)`, which forks a supervisor and returns to the caller. `kill_server()` sends SIGINT to that supervisor and waits for it.

The example uses blocking startup and does not exercise this interface. `kill_server()` is intended for the background mode; blocking startup leaves `child_pid` at `-1`, so calling it in that mode can signal unrelated permitted processes.

## Request processing

- The shared listening socket is nonblocking; accepted client sockets use blocking reads.
- Each worker handles one accepted connection at a time. Request buffers start at 1024 bytes and grow as needed.
- Parsing expects HTTP/1.1 and recognizes `Host`, `Content-Type`, `Content-Length`, `Transfer-Encoding`, `Connection`, `Accept`, and `Authorization`. Header names are compared without case sensitivity, and parsing assumes a space after the colon.
- A nonzero `Content-Length` triggers body collection. Chunked decoding, persistent connections, TLS, and static-file serving are not implemented.
- Each connection receives one response and is closed. Sending currently uses one nonblocking `send()` without retrying partial writes.

## Worker lifecycle and shutdown

Before forking each worker, the supervisor creates an anonymous temporary health file and a pipe. The worker inherits the listener, keeps its pipe read end, and updates the health file timestamps after `poll()` returns, including on timeouts.

Lifetime expiration sends SIGINT and a pipe wakeup, removes the worker from the tracking list, and allows a replacement to be created. For full shutdown, `kill_all()` signals and wakes all tracked workers first, then waits for each PID with `waitpid()`, retrying interrupted waits. Worker-side cleanup no longer calls the supervisor's worker-killing function.

Graceful shutdown remains unfinished:

- Shutdown cleanup still runs inside the supervisor's signal handler and uses operations such as allocation and freeing. Workers also allocate queue nodes inside their signal handler.
- An idle or incomplete client connection can trap a worker in its blocking read loops. EOF and read errors are not handled, so the supervisor can wait indefinitely for that worker.
- The worker checks pipe hangup/error flags after clearing `revents`, so that check cannot independently trigger an exit. Queue processing still determines whether it stops.
- The worker loop contains a five-second sleep after processing events, which delays further work and can delay shutdown.
- Lifetime-based removals do not reap their children. Unexpected child exits are not detected by the tracked worker-count check.

## Other current limitations

- Header completion uses the latest read length instead of the total buffered length. Requests split across reads may stall. There are no request size limits or read deadlines.
- Header-value termination writes past its allocation. Missing header fields are not consistently initialized, and parser failures can reach unchecked null pointers in the worker.
- Parsed strings, response buffers, and popped signal queue nodes are not fully freed. Allocation, fork, pipe, and temporary-file failures need stronger handling.
- Worker-count reduction does not advance its health-file scan iterator and can hang; selection can also leave no worker to remove.
- Signal setup still uses `sizeof(sigaction)` instead of the intended structure size, such as `sizeof(new)`. GCC accepts this as an extension; it is not a correct portable structure initialization expression.

## Docker development environment

The Dockerfile installs Bash, GCC, Make, and curl on Ubuntu 24.04. It starts a shell; it does not build or launch the application automatically.

From Bash:

```sh
bash run.sh
```

Or run the commands directly:

```sh
docker build -t ubuntu .
docker run -it --rm -v "${PWD}:/app" -w /app -p 8000:8000 ubuntu
```

Inside the container:

```sh
make
./server.exe
```

The repository is mounted at `/app`, and port 8000 is published to the host.

## Source layout

| Files | Role |
| --- | --- |
| `main.c` | Example HTML and health routes; blocking server startup |
| `server.c`, `server.h` | Route registration, configuration, supervisor loop, worker creation, and shutdown |
| `socket.c`, `socket.h` | TCP listener setup and connection acceptance |
| `workers.c`, `workers.h` | Worker polling, request buffering, signal processing, and health updates |
| `request.c`, `request.h` | HTTP request parsing, route dispatch, and fallback 404 response |
| `http_helper.c`, `http_helper.h` | Successful HTML response formatting and a 404 helper |
| `worker_queue.c`, `worker_queue.h` | Linked queue of pending worker signals |
| `types.h` | Configuration, route, and worker tracking structures |
| `helper.c`, `helper.h` | Integer conversion and health-file naming helper |
| `Makefile` | Compilation and linking |
| `Dockerfile`, `run.sh` | Ubuntu development container |
| `plan.md` | Early design notes |
