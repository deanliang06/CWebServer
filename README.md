# CWebServer

A work-in-progress web server written in C, exploring POSIX sockets and a pool of worker processes. The parent process opens a shared TCP listener, forks workers, and creates a pipe for each worker. Workers use `poll()` to wait for incoming connections and pipe wakeups.

This is an experimental prototype. It does not yet implement a complete HTTP server or serve files.

## Requirements

- Linux or a compatible POSIX environment with `fork()`, Unix sockets, and `poll()`.
- GCC with support for GNU C23 (`-std=gnu2x`).
- Docker for the included Ubuntu development environment (optional).

On Windows, use the Docker environment below or a Linux environment such as WSL. The `.exe` filename is just the project's executable name; the source uses POSIX APIs.

## Build and run

The current source has a build blocker in `workers.c`: `sizeof(sigaction)` refers to a function rather than the signal-action structure. This must be corrected (for example, to `sizeof(new)`) before building. The commands below describe the intended build and launch workflow, not a verified working release.

From the repository root in a Linux shell, after resolving that blocker:

```sh
gcc -std=gnu2x -Wall -Wextra main.c helper.c socket.c workers.c request.c worker_queue.c -o server.exe
./server.exe
```

The server binds to **all IPv4 interfaces on port 8000**, with a listen backlog of 10. Defaults are **4 workers**, a **30-second polling timeout**, and a **120-second maximum worker lifetime**. The polling timeout controls how long a worker waits in `poll()`; it is not a request deadline. The parent checks worker ages and replenishes the worker list once per second.

Use a local development environment. Request handling, error handling, and process cleanup are incomplete.

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

- Before each fork, the parent creates a health file and a pipe. The parent keeps the pipe's write end; the worker keeps its read end alongside the inherited listening socket.
- The parent records worker start times. When a worker exceeds the maximum lifetime, the parent sends `SIGTERM`, writes a wakeup byte to its pipe, removes its tracking entry, and creates a replacement during the worker-count check.
- The worker's signal handler adds signals to a linked queue in `worker_queue.c`. A pipe wakeup is intended to trigger queue processing in the worker loop.
- Workers attempt to update health-file timestamps with `futimens()` after handling ready descriptors. Poll timeouts skip this update, so these timestamps are not periodic idle heartbeats.

This lifecycle is still under development; the limitations below affect shutdown and worker replacement.

## Current behavior and limitations

- Workers look for `\r\n\r\n` as the end of request headers. HTTP methods, routes, headers, and bodies are not parsed.
- The response is a short raw acknowledgment without an HTTP status line or headers. Browsers and HTTP clients may reject it.
- Request buffering has unfinished bounds, initialization, and read/error handling. Connections may hang or behave unpredictably.
- Worker health files (`0health`, `1health`, and so on) are created in the working directory. File creation errors, timestamp updates, and file cleanup are not fully handled.
- Signal queue handling is unfinished: the handler allocates memory, `empty()` returns the opposite of what the processing loop expects, and popped queue nodes are not freed. Termination is not reliable.
- Worker removal does not reap child processes with `waitpid()`. Unexpected exits are not detected by the worker-count check, and the worker-side cleanup path needs correction.
- The worker-count reduction path does not advance its list iterator and can hang. Graceful shutdown and process cleanup remain incomplete.

Run in the foreground and use Ctrl+C to interrupt. Check for remaining worker processes and generated health files after a run; automatic cleanup is not implemented.

## Source layout

| File | Role |
| --- | --- |
| `main.c` | Configuration, parent loop, worker creation, pipe setup, and lifetime-based removal |
| `socket.c`, `socket.h` | TCP listener setup and connection acceptance |
| `workers.c`, `workers.h` | Worker polling loop, signal handling, health updates, and request buffering |
| `worker_queue.c`, `worker_queue.h` | Linked queue for pending worker signals |
| `request.c`, `request.h` | End-of-headers detection |
| `helper.c`, `helper.h` | Integer argument parsing and health-file naming |
| `types.h` | Configuration and worker tracking structures |
| `Makefile` | Existing build rules; the explicit GCC command above avoids relying on these unfinished rules |
| `Dockerfile`, `run.sh` | Container development environment |
| `plan.md` | Early design notes, including planned framework integration |
