#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
FILE="${1:-$ROOT/test_for_http.txt}"

# You can override HOST/PORT/BASE via env
export HOST="${HOST:-127.0.0.1}"
export PORT="${PORT:-8090}"
export BASE="${BASE:-http://$HOST:$PORT}"

python3 "$ROOT/scripts/run_test_for_http.py" "$FILE"
