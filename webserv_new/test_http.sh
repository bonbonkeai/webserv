#!/bin/bash
set -euo pipefail

CXX=${CXX:-c++}
CXXFLAGS="-std=c++98 -Wall -Wextra -Werror"

WEBSERV=${WEBSERV:-./webserv}
CONFIG=${CONFIG:-./conf/default.conf}

HOST=${HOST:-127.0.0.1}
PORT=${PORT:-8080}

ROOT_DIR=${ROOT_DIR:-/tmp/ws_root}
UPLOAD_DIR=${UPLOAD_DIR:-/tmp/ws_upload}

BIN=./test_http
LOG=/tmp/webserv_test.log

pick() {
  # prefer repo path, else fallback to /mnt/data
  if [ -f "$1" ]; then echo "$1"; return 0; fi
  if [ -f "/mnt/data/$1" ]; then echo "/mnt/data/$1"; return 0; fi
  echo "MISSING:$1"
  return 0
}

echo "[1] build test_http"

FILES=(
  "HTTP/cpp/HTTPRequest.cpp"
  "HTTP/cpp/HTTPRequestParser.cpp"
  "HTTP/cpp/HTTPUtils.cpp"
  "HTTP/cpp/HTTPResponse.cpp"
  "HTTP/cpp/ErrorResponse.cpp"
  "HTTP/cpp/ResponseBuilder.cpp"
  "HTTP/cpp/RequestFactory.cpp"
  "Method_Handle/cpp/GetRequest.cpp"
  "Method_Handle/cpp/PostRequest.cpp"
  "Method_Handle/cpp/DeleteRequest.cpp"
  "Method_Handle/cpp/ErrorRequest.cpp"
  "Method_Handle/cpp/StaticHandle.cpp"
  "Method_Handle/cpp/DirectoryHandle.cpp"
  "Method_Handle/cpp/UploadHandle.cpp"
  "Method_Handle/cpp/RedirectHandle.cpp"
  "Method_Handle/cpp/FileUtils.cpp"
)

SRC=()
for f in "${FILES[@]}"; do
  p=$(pick "$f")
  if [[ "$p" == MISSING:* ]]; then
    echo "[ERROR] cannot find source file: ${p#MISSING:}"
    exit 1
  fi
  SRC+=("$p")
done

# include path: repo root or /mnt/data
INC="-I."
if [ -d "./HTTP/hpp" ] || [ -f "./HTTP/hpp/HTTPRequestParser.hpp" ]; then
  INC="-I."
elif [ -d "/mnt/data/HTTP/hpp" ] || [ -f "/mnt/data/HTTP/hpp/HTTPRequestParser.hpp" ]; then
  INC="-I/mnt/data"
fi

$CXX $CXXFLAGS test_http.cpp "${SRC[@]}" $INC -o "$BIN"

echo "[2] prepare filesystem"
rm -rf "$ROOT_DIR" "$UPLOAD_DIR"
mkdir -p "$ROOT_DIR/dir" "$ROOT_DIR/emptydir" "$UPLOAD_DIR"

echo "HELLO" > "$ROOT_DIR/hello.txt"
echo "<h1>INDEX</h1>" > "$ROOT_DIR/dir/index.html"

# permission-denied GET
echo "NOACCESS" > "$ROOT_DIR/noperm.txt"
chmod 000 "$ROOT_DIR/noperm.txt"

# permission-denied DELETE (file in root)
echo "PROTECTED" > "$ROOT_DIR/protected.txt"
chmod 000 "$ROOT_DIR/protected.txt"

restore_perms() {
  chmod 644 "$ROOT_DIR/noperm.txt" 2>/dev/null || true
  chmod 644 "$ROOT_DIR/protected.txt" 2>/dev/null || true
  chmod 755 "$UPLOAD_DIR" 2>/dev/null || true
}
restore_perms

echo "[3] run UNIT tests"
"$BIN" --unit

echo "[4] start webserv"
"$WEBSERV" "$CONFIG" > "$LOG" 2>&1 &
WS_PID=$!

cleanup() {
  echo "[cleanup] restore perms + stop webserv pid=$WS_PID"
  restore_perms
  kill "$WS_PID" >/dev/null 2>&1 || true
  wait "$WS_PID" >/dev/null 2>&1 || true
}
trap cleanup EXIT

echo -n "[wait] port $PORT "
for i in $(seq 1 80); do
  if (echo > /dev/tcp/"$HOST"/"$PORT") >/dev/null 2>&1; then
    echo "ready"
    break
  fi
  echo -n "."
  sleep 0.1
done

# Make upload dir temporarily non-writable for one integration test
# We do this BEFORE integration so the test case "POST forbidden.txt -> 403" is deterministic.
chmod 000 "$UPLOAD_DIR"

echo "[5] run INTEGRATION tests"
"$BIN" --integration "$HOST" "$PORT" "$ROOT_DIR" "$UPLOAD_DIR" || true

# restore upload permissions after tests (also in trap)
chmod 755 "$UPLOAD_DIR"

echo "[DONE] server log: $LOG"
