#!/usr/bin/env bash
set -euo pipefail

BASE_URL="${1:-http://127.0.0.1:8080}"
OUT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/results"
mkdir -p "$OUT_DIR"
TS="$(date +%Y%m%d-%H%M%S)"
OUT_FILE="$OUT_DIR/bench-$TS.txt"

{
  echo "# Benchmark run: $(date)"
  echo "# Base URL: $BASE_URL"
  echo

  if command -v wrk >/dev/null 2>&1; then
    echo "## wrk /"
    wrk -t4 -c64 -d10s "$BASE_URL/"
    echo

    echo "## wrk /hello"
    wrk -t4 -c64 -d10s "$BASE_URL/hello"
    echo

    echo "## wrk /health"
    wrk -t4 -c64 -d10s "$BASE_URL/health"
    echo

    echo "## wrk /static/index.html"
    wrk -t4 -c64 -d10s "$BASE_URL/static/index.html"
    echo
  elif command -v ab >/dev/null 2>&1; then
    echo "## ab /"
    ab -n 5000 -c 100 "$BASE_URL/"
    echo

    echo "## ab /hello"
    ab -n 5000 -c 100 "$BASE_URL/hello"
    echo

    echo "## ab /health"
    ab -n 5000 -c 100 "$BASE_URL/health"
    echo

    echo "## ab /static/index.html"
    ab -n 5000 -c 100 "$BASE_URL/static/index.html"
    echo
  else
    echo "## curl fallback"
    echo "wrk/ab not found; using curl timing loops."
    for route in / /hello /health /static/index.html; do
      echo "### route: $route"
      for i in $(seq 1 200); do
        curl -sS -o /dev/null "$BASE_URL$route"
      done
      curl -sS -o /dev/null -w "time_total=%{time_total}\n" "$BASE_URL$route"
      echo
    done
  fi
} | tee "$OUT_FILE"

echo "Saved benchmark output to: $OUT_FILE"
