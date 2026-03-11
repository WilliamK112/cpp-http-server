#!/usr/bin/env bash
set -euo pipefail

BASE_URL="${1:-http://127.0.0.1:8080}"

echo "Testing $BASE_URL"

curl -i "$BASE_URL/"
curl -i "$BASE_URL/health"
curl -i "$BASE_URL/hello?name=William"
curl -i "$BASE_URL/static/index.html"
curl -i "$BASE_URL/static/hello.txt"
curl -i "$BASE_URL/does-not-exist"
curl -i -X DELETE "$BASE_URL/resource"
curl -i -X PUT "$BASE_URL/resource" -H 'Content-Type: application/json' -d '{"x":1}'
curl -i -X POST "$BASE_URL/echo" -H 'Content-Type: text/plain' -d 'echo-body'

# minimal keep-alive check: two sequential requests on one curl invocation
curl -i --http1.1 "$BASE_URL/hello?name=one" --next "$BASE_URL/hello?name=two"

echo "Done."
