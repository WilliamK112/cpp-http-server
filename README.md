# C++ Multithreaded HTTP Server (C++17 + CMake)

A clean, interview-friendly HTTP server built from scratch in modern C++.

This project starts from correctness and incrementally adds production-minded features: routing, request parsing, a thread pool, static file serving, logging, and benchmarking scripts.

---

## Project Overview

- Language: **C++17**
- Build system: **CMake**
- Networking: POSIX sockets (`socket`, `bind`, `listen`, `accept`, `recv`, `send`)
- Concurrency: fixed-size **thread pool** (`std::thread`, `std::mutex`, `std::condition_variable`)
- Goal: be practical, understandable, and easy to explain in interviews

---

## Feature List

- HTTP/1.1 response formatting with status line, headers, and content length
- Simple HTTP request parsing (request line + headers)
- Routes:
  - `GET /` → `HTTP server is running`
  - `GET /health` → `{"status":"ok"}`
  - `GET /hello` → `Hello from C++ HTTP server`
- 404 for unknown dynamic routes
- 400 for malformed requests / unsupported methods in this minimal server
- 500 for unhandled internal exceptions
- Fixed-size thread pool with a blocking task queue for concurrent client handling
- Static file serving from `public/` using `/static/...`
- Basic MIME support: `html`, `css`, `js`, `json`, `txt`
- Path traversal protection (`../` blocked via normalized/canonical paths)
- Request logging (client, method, path, status)
- Configurable host/port/thread count/static dir via CLI flags
- Benchmark + smoke test scripts

---

## Architecture

### Core modules

- `Server`: socket lifecycle, accept loop, dispatches client tasks to thread pool
- `ThreadPool`: worker threads consume queued connection-handling tasks
- `HttpRequest` parser: parses raw bytes into method/path/version/headers/body
- `Router`: path → handler map for dynamic routes
- `StaticFileHandler`: serves files from static directory with safety checks + in-memory hot-file cache
- `HttpResponse`: serializes to wire-format HTTP/1.1 response
- `Logger`: structured console logging
- `Config`: command-line config parsing

### Request lifecycle

1. Main thread accepts a TCP client.
2. Accepted socket gets enqueued as a task in `ThreadPool`.
3. Worker reads request bytes and parses headers.
4. Server routes:
   - `/`, `/hello`, `/health` via `Router`
   - `/static/...` via `StaticFileHandler`
5. Worker serializes and sends HTTP response, logs request, closes client socket.

---

## Folder Structure

```text
cpp-http-server/
├── CMakeLists.txt
├── .gitignore
├── README.md
├── include/http/
│   ├── config.hpp
│   ├── logger.hpp
│   ├── request.hpp
│   ├── response.hpp
│   ├── router.hpp
│   ├── server.hpp
│   ├── static_file_handler.hpp
│   └── thread_pool.hpp
├── src/
│   ├── config.cpp
│   ├── logger.cpp
│   ├── main.cpp
│   ├── request.cpp
│   ├── response.cpp
│   ├── router.cpp
│   ├── server.cpp
│   ├── static_file_handler.cpp
│   └── thread_pool.cpp
├── public/
│   ├── index.html
│   ├── style.css
│   ├── app.js
│   ├── data.json
│   └── hello.txt
├── scripts/
│   └── run.sh
└── benchmarks/
    ├── README.md
    ├── smoke_test.sh
    ├── run_bench.sh
    └── results/
```

---

## Build Instructions

```bash
cd cpp-http-server
cmake -S . -B build
cmake --build build
```

## Run Instructions

```bash
# defaults: host 127.0.0.1, port 8080, threads 4, static_dir public
./build/http_server

# custom options
./build/http_server --host=127.0.0.1 --port=9090 --threads=8 --static-dir=public

# one-command helper
./scripts/run.sh --port=8080 --threads=4
```

---

## Example curl Usage

```bash
curl -i http://127.0.0.1:8080/
curl -i http://127.0.0.1:8080/health
curl -i http://127.0.0.1:8080/hello

# static files
curl -i http://127.0.0.1:8080/static/index.html
curl -i http://127.0.0.1:8080/static/style.css
curl -i http://127.0.0.1:8080/static/data.json

# error paths
curl -i http://127.0.0.1:8080/does-not-exist
curl -i -X POST http://127.0.0.1:8080/hello
```

---

## Benchmark Section

### Run

```bash
./benchmarks/smoke_test.sh
./benchmarks/run_bench.sh
# custom URL
./benchmarks/run_bench.sh http://127.0.0.1:9090
```

### Tool selection

`run_bench.sh` auto-selects:
1. `wrk` (preferred)
2. `ab`
3. `curl` fallback loops

### Latest sample run (this repo, local machine)

Using `ab -n 5000 -c 100`:

- `/`: Requests/sec = `32857.99`, mean latency = `3.043 ms`
- `/hello`: Requests/sec = `37246.72`, mean latency = `2.685 ms`
- `/health`: Requests/sec = `41801.83`, mean latency = `2.392 ms`
- `/static/index.html`: Requests/sec = `37320.95`, mean latency = `2.679 ms`

Saved output:
`benchmarks/results/bench-20260308-043642.txt`

> Numbers are hardware/OS dependent; rerun locally for your final metrics.

### Likely bottlenecks

- One request per TCP connection (`Connection: close`): extra connection setup/teardown overhead.
- Parsing and response generation done per request with minimal reuse.
- Thread contention under very high concurrency (socket accept + queue + logging lock).

### Improvements made based on measurement

- Kept accept path light and pushed request work to worker threads via task queue.
- Added a small in-memory static-file cache to reduce repeated disk I/O for hot assets.
- Added mutex-protected logger output to avoid interleaved lines under concurrent load.

---

## Future Improvements

- Keep-alive + multiple requests per connection (**intentionally deferred** for design simplicity)
- Better HTTP compliance (more methods, robust header parsing, chunked transfer)
- Connection/read timeouts and max request body handling
- Config file + env var support in addition to CLI flags
- Metrics endpoint and structured JSON logs
- TLS (via reverse proxy or direct integration)

---

## Lessons Learned

- Concurrency stays approachable when responsibilities are explicit: accept thread + worker pool.
- Correctness first (parsing, status codes, safety checks) makes later optimization straightforward.
- Benchmark scripts are essential to keep performance claims grounded.

---

## Resume Bullet Points (3)

- Built a modular **multithreaded HTTP server in C++17** using POSIX sockets and CMake, implementing request parsing, routing, and HTTP/1.1 response serialization.
- Designed and integrated a **fixed-size thread pool + blocking task queue** to process concurrent client connections safely with `std::mutex` and `std::condition_variable`.
- Implemented **secure static file serving** (MIME mapping + traversal protection), structured request logging, and reproducible load-testing workflows (`wrk`/`ab`/`curl`).

---

## Interview Summary

### 30-second version

“I built a C++17 HTTP server from scratch with clean modules for socket lifecycle, request parsing, routing, and response formatting. After validating correctness, I added a fixed-size thread pool and task queue so the accept loop can dispatch sockets while workers process requests concurrently. I also implemented secure static file serving, request logging, and benchmark scripts, so it’s practical and measurable—not just a toy server.”

### 90-second version

“This project is a from-scratch HTTP server in modern C++ with a focus on clear architecture and practical tradeoffs. The `Server` handles socket setup (`socket/bind/listen/accept`) and pushes accepted connections into a fixed-size `ThreadPool`. Worker threads parse requests, route dynamic endpoints (`/`, `/health`, `/hello`), or serve static content from `public/` through `/static/...`.

I separated concerns into small modules: `HttpRequest`, `HttpResponse`, `Router`, `ThreadPool`, `StaticFileHandler`, `Logger`, and `Config`. That keeps the implementation easy to reason about and easy to explain in interviews. On correctness and safety, the server returns 400 for malformed requests, 404 for missing routes/files, and blocks path traversal attempts for static files.

For performance, I added benchmark automation with `wrk`/`ab`/`curl` fallback and used those runs to guide improvements. One concrete change was adding a small static-file cache to reduce repeated disk reads under load. Keep-alive and deeper HTTP compliance are intentionally deferred so this version stays clean and interview-friendly while still demonstrating concurrency, security basics, and measurement-driven optimization.”
