# CWebServer

A work-in-progress web server written in C, exploring POSIX sockets and a pool of worker processes. The parent process opens a shared TCP listener, forks workers, and creates a pipe for each worker. Workers use `poll()` to wait for incoming connections and pipe wakeups.

This is an experimental prototype with basic HTTP/1.1 request parsing and a fixed `Hello` response. It does not yet implement a complete HTTP server, routing, framework integration, or file serving. Connections use plain HTTP; TLS/HTTPS is not implemented.

## Requirements

- Linux or a compatible POSIX environment with `fork()`, Unix sockets, and `poll()`.
- GCC with support for GNU C23 (`-std=gnu2x`).
- Docker for the included Ubuntu development environment (optional).

On Windows, use the Docker environment below or a Linux environment such as WSL. The `.exe` filename is just the project's executable name; the source uses POSIX APIs.

## Build and run

The signal setup in both `main.c` and `workers.c` uses `sizeof(sigaction)`, which refers to a function rather than the signal-action structure. Replace it with the intended structure size (for example, `sizeof(new)`) before building. The commands below describe the intended build and launch workflow, not a verified working release.

From the repository root in a Linux shell, after resolving that blocker:

```sh
gcc -std=gnu2x -Wall -Wextra main.c helper.c socket.c workers.c request.c worker_queue.c -o server.exe
./server.exe
```

The server binds to **all IPv4 interfaces on port 8000**, with a listen backlog of 10. Defaults are **4 workers**, a **30-second polling timeout**, and a **120-second maximum worker lifetime**. The polling timeout controls how long a worker waits in `poll()`; it is not a request deadline. The parent checks worker ages and replenishes the worker list once per second.

Use a local development environment. Request handling, error handling, and process cleanup are incomplete.

## Request handling

- The shared listening socket is nonblocking. If another worker accepts a pending connection first, `accept_conn()` handles `EAGAIN`/`EWOULDBLOCK` by returning without a connection. Accepted client sockets still use blocking reads.
- Workers buffer incoming data, starting with 1024 bytes and doubling the allocation when full. They look for `\r\n\r\n` to locate the end of the headers.
- The parser extracts the request-line method, target, and protocol and checks for `HTTP/1.1`. These request-line fields are not yet attached to the returned `Headers` structure.
- Recognized header names are `Host`, `Content-Type`, `Content-Length`, `Transfer-Encoding`, `Connection`, `Accept`, and `Authorization`. Matching is currently case-sensitive and assumes a space after the colon.
- A nonzero `Content-Length` triggers body collection and extraction. Recording `Transfer-Encoding` does not implement chunked-body decoding.
- The response builder currently ignores the request and returns this fixed response, then the worker closes the connection:

```http
HTTP/1.1 200 OK
Content-Type: text/plain
Content-Length: 5
Connection: close

Hello
```

After resolving the build issues and starting the server, a basic manual check is `curl --http1.1 -i http://localhost:8000/`. This is an intended smoke check, not a verified test result.

## Docker development environment

The Dockerfile provides Ubuntu 24.04 with Bash, GCC, Make, and curl. It opens a shell; it does not build or start the server automatically.

In Bash, run the included helper:

```sh
bash run.sh
```

Or run its commands directly (also suitable for PowerShell):

```sh
docker build -t ubuntu .
docker run -it --rm -v "${PWD}:/app" -w /app -p 8000:8000 ubuntu
```

Inside the container, run the build and launch commands above. The repository is mounted at `/app`, and port 8000 is published to the host.

## Command-line options

| Option | Purpose | Default |
| --- | --- | --- |
| `-n NUMBER` | Number of worker processes; use 1–50 | `4` |
| `-t SECONDS` | Worker polling timeout; accepted range 5–960 | `30` |
| `-l SECONDS` | Maximum worker lifetime; accepted range 30–3600 | `120` |
| `-h` | Print usage information and exit | — |

The current argument parser skips the first argument after the executable. Supply a placeholder when using options:

```sh
./server.exe unused -n 2
./server.exe unused -t 30
./server.exe unused -h
```

The placeholder is not loaded or executed. Although the built-in help describes a “framework entry file,” framework integration is not implemented. The argument-count check currently prevents supplying multiple value-taking options together in the format above. Worker counts greater than 50 print an error but are not correctly rejected; stay within the documented range.

## Worker lifecycle

- Before each fork, the parent creates an anonymous temporary health file with `tmpfile()` and a pipe. The parent keeps the pipe's write end; the worker keeps its read end alongside the inherited listening socket.
- The parent records worker start times. When a worker exceeds the maximum lifetime, the parent sends `SIGTERM`, writes a wakeup byte to its pipe, removes its tracking entry, and creates a replacement during the worker-count check.
- The worker's signal handler adds signals to a linked queue in `worker_queue.c`. A pipe wakeup is intended to trigger queue processing in the worker loop.
- Workers attempt to update health-file timestamps with `futimens()` after handling ready descriptors. Poll timeouts skip this update, so these timestamps are not periodic idle heartbeats.
- The parent now registers a `SIGTERM` handler that attempts to terminate all tracked workers and exit. Reliable graceful shutdown is still unfinished.

This lifecycle is still under development; the limitations below affect shutdown and worker replacement.

## Current behavior and limitations

- HTTP parsing is partial: routing, method-specific behavior, chunked bodies, keep-alive, and structured HTTP error responses are not implemented.
- Header and body completion checks currently receive the latest read size instead of the total buffered size. Requests split across reads may hang. EOF, read errors, request deadlines, and maximum request sizes are not handled.
- Header-value termination writes past its allocation, request logging can read buffers without a terminating null byte, and parsed headers/bodies are not freed. Parser validation and allocation handling remain unfinished; unsupported protocols can lead to a null-pointer dereference in the worker.
- Response sending uses a single nonblocking `send()` without handling partial writes or errors.
- Health files now use temporary storage rather than named files in the working directory. Temporary-file creation errors, timestamp updates, and stream/descriptor cleanup are not fully handled.
- Signal queue handling is unfinished: the handler allocates memory, `empty()` returns the opposite of what the processing loop expects, and popped queue nodes are not freed. Termination is not reliable.
- Worker removal does not reap child processes with `waitpid()`. Unexpected exits are not detected by the worker-count check, and the worker-side cleanup path needs correction.
- The worker-count reduction path does not advance its list iterator and can hang. Graceful shutdown and process cleanup remain incomplete.

Run in the foreground and use Ctrl+C to interrupt. Check for remaining worker processes after a run; complete automatic process cleanup is not implemented.

## Source layout

| File | Role |
| --- | --- |
| `main.c` | Configuration, parent loop, worker creation, pipe setup, and lifetime-based removal |
| `socket.c`, `socket.h` | TCP listener setup and connection acceptance |
| `workers.c`, `workers.h` | Worker polling loop, signal handling, health updates, and request buffering |
| `worker_queue.c`, `worker_queue.h` | Linked queue for pending worker signals |
| `request.c`, `request.h` | Basic HTTP/1.1 request-line/header parsing, body completion and extraction, and a fixed response |
| `helper.c`, `helper.h` | Integer argument parsing and health-file naming |
| `types.h` | Configuration and worker tracking structures |
| `Makefile` | Existing build rules; the explicit GCC command above avoids relying on these unfinished rules |
| `Dockerfile`, `run.sh` | Container development environment |
| `plan.md` | Early design notes, including planned framework integration |
