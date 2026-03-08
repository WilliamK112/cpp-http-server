# Benchmarks

This folder contains lightweight scripts for functional smoke tests and basic load testing.

## Files

- `smoke_test.sh`: quick endpoint verification
- `run_bench.sh`: throughput/latency benchmark (`wrk` preferred, `ab` fallback, then `curl`)
- `results/`: benchmark output files

## Usage

```bash
cd cpp-http-server
./benchmarks/smoke_test.sh
./benchmarks/run_bench.sh
# custom base URL
./benchmarks/run_bench.sh http://127.0.0.1:9090
```

## What gets tested

- `/`
- `/hello`
- `/health`
- `/static/index.html`

## Notes

- If `wrk` and `ab` are unavailable, the script still runs a curl-based fallback.
- Results vary by machine/OS; compare changes on the same host when possible.
- Use output under `benchmarks/results/` as your reproducible benchmark artifact.
