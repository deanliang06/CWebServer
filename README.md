# CWebServer

A work-in-progress web server written in C, exploring POSIX sockets and a pool of worker processes. The parent process opens a shared TCP listener, forks workers, and lets each worker wait for connections with `poll()`.

This is an experimental prototype. It does not yet implement a complete HTTP server or serve files.

## Requirements

- Linux or a compatible POSIX environment with `fork()`, Unix sockets, and `poll()`.
- GCC with support for GNU C23 (`-std=gnu2x`).
- Docker for the included Ubuntu development environment (optional).

On Windows, use the Docker environment below or a Linux environment such as WSL. The `.exe` filename is just the project's executable name; the source uses POSIX APIs.

## Build and run

From the repository root in a Linux shell:

```sh
gcc -std=gnu2x -Wall -Wextra main.c helper.c socket.c workers.c request.c -o server.exe
./server.exe
```

The server binds to **all IPv4 interfaces on port 8000**, with a listen backlog of 10. By default, it starts **4 workers** with a **60-second polling timeout**. The timeout controls how long a worker waits in `poll()`; it is not a request deadline or a worker lifetime.

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
| `-t SECONDS` | Worker polling timeout; accepted range 5–960 | `60` |
| `-h` | Print usage information and exit | — |

The current argument parser skips the first argument after the executable. Supply a placeholder when using options:

```sh
./server.exe unused -n 2
./server.exe unused -t 30
./server.exe unused -h
```

The placeholder is not loaded or executed. Although the built-in help describes a “framework entry file,” framework integration is not implemented. The argument-count check currently prevents supplying both `-n` and `-t` together in the format above. Worker counts greater than 50 print an error but are not correctly rejected; stay within the documented range.

## Current behavior and limitations

- Workers look for `\r\n\r\n` as the end of request headers. HTTP methods, routes, headers, and bodies are not parsed.
- The response is a short raw acknowledgment without an HTTP status line or headers. Browsers and HTTP clients may reject it.
- Request buffering has unfinished bounds, initialization, and read/error handling. Connections may hang or behave unpredictably.
- Worker health files (`health`, `1health`, and so on) are created in the working directory. Health monitoring, signaling, worker termination, and graceful shutdown are unfinished.

Run in the foreground and use Ctrl+C to interrupt. Check for remaining worker processes and generated health files after a run; automatic cleanup is not implemented.

## Source layout

| File | Role |
| --- | --- |
| `main.c` | Configuration, parent loop, and worker creation |
| `socket.c`, `socket.h` | TCP listener setup and connection acceptance |
| `workers.c`, `workers.h` | Worker polling loop and request buffering |
| `request.c`, `request.h` | End-of-headers detection |
| `helper.c`, `helper.h` | Integer argument parsing |
| `types.h` | Configuration and worker tracking structures |
| `Makefile` | Existing build rules; the explicit GCC command above avoids relying on these unfinished rules |
| `Dockerfile`, `run.sh` | Container development environment |
| `plan.md` | Early design notes, including planned framework integration |
