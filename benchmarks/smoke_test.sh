#!/usr/bin/env bash
set -euo pipefail

BASE_URL="${1:-http://127.0.0.1:8080}"

echo "Testing $BASE_URL"

curl -i "$BASE_URL/"
curl -i "$BASE_URL/health"
curl -i "$BASE_URL/hello"
curl -i "$BASE_URL/static/index.html"
curl -i "$BASE_URL/static/hello.txt"
curl -i "$BASE_URL/does-not-exist"
curl -i -X POST "$BASE_URL/hello"

echo "Done."
