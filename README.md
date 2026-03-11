# C++ Multithreaded HTTP Server (C++17 + CMake)

A modular, interview-friendly HTTP server built with POSIX sockets, a fixed-size thread pool, and practical HTTP/1.1 behavior.

## What’s New in this iteration

- **HTTP keep-alive** (HTTP/1.1 default, HTTP/1.0 opt-in via `Connection: keep-alive`)
- **Per-connection lifecycle loop** (multiple requests per socket, bounded by `max_requests_per_connection`)
- **Request/connection timeouts** for robustness against slow or idle clients
- **Improved parser**:
  - query string parsing (`request.query_params`)
  - request body parsing using `Content-Length`
  - minimal `Content-Type` extraction
  - lower-cased header keys for predictable lookup
- **More methods**: `GET`, `POST`, `PUT`, `DELETE`
- **405 Method Not Allowed** with `Allow` header
- Updated smoke/benchmark scripts to validate and compare keep-alive behavior

---

## Core Features

- C++17 + CMake
- POSIX sockets (`socket`, `bind`, `listen`, `accept`, `recv`, `send`)
- Fixed-size thread pool + blocking task queue
- Dynamic routing for API-style handlers
- Static file serving from `/static/...` with path traversal protection
- HTTP response serialization with status line + headers + `Content-Length`
- Structured request logging
- CLI configuration

---

## Architecture

### Modules

- `Server`: socket lifecycle, accept loop, client request loop, timeout policy
- `ThreadPool`: worker ownership and task queue
- `HttpRequest` parser: request line/headers/body/query parsing
- `Router`: method+path dispatch, 404/405 behavior
- `StaticFileHandler`: secure static file mapping + cache
- `HttpResponse`: wire serialization
- `Config`: CLI parsing

### Request lifecycle (thread-pool model)

1. Main thread accepts TCP connections.
2. Connection fd is queued onto the worker pool.
3. Worker handles **one connection in a loop**:
   - reads one request (with timeout + size bounds)
   - parses and routes
   - sends response
   - keeps socket open when allowed
4. Connection closes on timeout, protocol signal (`Connection: close`), parse error, or request cap.

This keeps the design simple while avoiding one-request-per-connection overhead.

---

## HTTP behavior (minimal but production-minded)

### Keep-alive policy

- HTTP/1.1: keep-alive by default, unless `Connection: close`
- HTTP/1.0: close by default, unless `Connection: keep-alive`
- Response includes `Connection: keep-alive|close`
- Connection count per socket is bounded (`--max-requests-per-connection`)

### Timeouts & limits

- `--request-timeout-ms` for initial request read
- `--keep-alive-timeout-ms` between requests on persistent sockets
- `--max-request-bytes` cap for total request size
- `--max-body-bytes` cap for body size

These controls reduce risk from slow clients and oversized payloads.

### Parser details

- Parses request line: `METHOD target HTTP/x.y`
- Splits target into `path` + `query_params`
- Parses headers into lower-case map
- Reads body using `Content-Length`
- Exposes `content_type` convenience field

> Limitations: chunked request bodies are not implemented yet (see design note below).

---

## Routes

- `GET /` -> health text
- `GET /health` -> JSON status
- `GET /hello?name=...` -> query parameter demo
- `POST /echo` -> echoes request body (uses incoming `Content-Type` when present)
- `PUT /resource` -> returns bytes-updated summary JSON
- `DELETE /resource` -> `204 No Content`
- `GET /static/...` -> static files

---

## Build & Run

```bash
cd cpp-http-server
cmake -S . -B build
cmake --build build
./build/http_server
```

### Useful runtime flags

```bash
./build/http_server \
  --host=127.0.0.1 \
  --port=8080 \
  --threads=4 \
  --static-dir=public \
  --request-timeout-ms=5000 \
  --keep-alive-timeout-ms=15000 \
  --max-request-bytes=65536 \
  --max-body-bytes=1048576 \
  --max-requests-per-connection=100
```

---

## Smoke test & benchmark

```bash
./benchmarks/smoke_test.sh
./benchmarks/run_bench.sh
```

- Smoke test now includes POST/PUT/DELETE + query parameter and a basic keep-alive check.
- Benchmark script includes keep-alive comparison mode when using `ab` (`ab -k`).

---

## Design note: event-driven future (epoll/kqueue/select)

Current model: **thread-pool + blocking sockets per active worker**.

- Pros: straightforward control flow, easy debugging, clean interview explanation.
- Cons: per-connection worker occupancy during keep-alive idle periods; higher context-switch/memory overhead at very high concurrency.

Future event-driven slot (without rewriting modules):

- Keep `Router`, parser, `HttpResponse`, and static handler as-is.
- Replace connection execution engine (`Server::Run` + `HandleClient`) with a reactor/event loop.
- Use per-connection state objects to accumulate bytes and parse incrementally.

This lets you compare models clearly:

- Thread-per-connection (simple, poor scalability)
- Thread-pool blocking I/O (current: balanced simplicity/perf)
- Event-driven non-blocking I/O (better high-connection scalability, higher complexity)

### Chunked transfer (future)

Not implemented for request bodies yet. To add it, parser would need a chunk-state machine (`size line -> chunk bytes -> CRLF -> repeat`) and stronger DoS guards for chunk count/aggregate size.

---

## Interview talking points

- Added keep-alive correctly with explicit lifecycle and protocol-aware close semantics.
- Improved resilience with practical timeout and request-size controls.
- Extended parser and method routing without a large architecture rewrite.
- Kept module boundaries intact so future event-driven migration is incremental, not a restart.
