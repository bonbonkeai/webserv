#!/usr/bin/env bash

set -u

HOST="${HOST:-127.0.0.1}"
PORTS=(${PORTS:-8080})
SERVER_BIN="${SERVER_BIN:-./webserv}"
SERVER_ARGS="${SERVER_ARGS:-}"
HOST_HEADER="${HOST_HEADER:-example.local}"
START_SERVER="${START_SERVER:-1}"
UPLOAD_DIR="${UPLOAD_DIR:-./www/upload}"

PASS=0
FAIL=0
SKIP=0
SERVER_PID=""

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m'

need_cmd() {
    command -v "$1" >/dev/null 2>&1 || {
        echo -e "${RED}missing command:${NC} $1"
        exit 1
    }
}

print_ok() {
    local name="$1"
    echo -e "${GREEN}ok${NC}  $name"
    PASS=$((PASS + 1))
}

print_fail() {
    local name="$1"
    local msg="${2:-}"
    echo -e "${RED}fail${NC} $name"
    [ -n "$msg" ] && echo "      $msg"
    FAIL=$((FAIL + 1))
}

print_skip() {
    local name="$1"
    local msg="${2:-}"
    echo -e "${YELLOW}skip${NC} $name"
    [ -n "$msg" ] && echo "      $msg"
    SKIP=$((SKIP + 1))
}

section() {
    echo
    echo -e "${CYAN}${BOLD}==== $1 ====${NC}"
}

restore_fixtures() {
    mkdir -p www
    printf "HELLO\n" > www/hello.txt
}

cleanup() {
    restore_fixtures
    if [ -n "${SERVER_PID:-}" ]; then
        kill "$SERVER_PID" >/dev/null 2>&1 || true
        wait "$SERVER_PID" 2>/dev/null || true
    fi
}
trap cleanup EXIT

check_tools() {
    need_cmd curl
    need_cmd nc
    need_cmd lsof
    need_cmd timeout
    need_cmd grep
    need_cmd awk
    need_cmd sed
    need_cmd mktemp
    need_cmd python3
    need_cmd rm
    need_cmd test
}

start_server() {
    local port="$1"

    if [ "$START_SERVER" != "1" ]; then
        print_skip "start server on $port" "START_SERVER=$START_SERVER, assume server already running"
        return 0
    fi

    if [ ! -x "$SERVER_BIN" ]; then
        print_fail "start server on $port" "server binary not executable: $SERVER_BIN"
        exit 1
    fi

    section "启动 server on port $port"
    echo "cmd: $SERVER_BIN $port $SERVER_ARGS"

    "$SERVER_BIN" "$port" $SERVER_ARGS >/tmp/test_http_${port}.log 2>&1 &
    SERVER_PID=$!

    local i
    for i in $(seq 1 30); do
        if lsof -i :"$port" 2>/dev/null | grep -q LISTEN; then
            print_ok "server started on $port"
            return 0
        fi
        sleep 0.2
    done

    print_fail "server started on $port" "port did not enter LISTEN state"
    echo "---- server log ----"
    cat "/tmp/test_http_${port}.log" 2>/dev/null || true
    exit 1
}

test_listen() {
    local port="$1"
    section "用系统命令确认端口监听"

    local out
    out="$(lsof -i :"$port" 2>/dev/null || true)"
    echo "cmd: lsof -i :$port"
    echo "$out"

    if echo "$out" | grep -q LISTEN; then
        print_ok "lsof shows LISTEN on $port"
    else
        print_fail "lsof shows LISTEN on $port" "no LISTEN socket found"
    fi
}

nc_raw() {
    local port="$1"
    local payload="$2"
    printf "%b" "$payload" | timeout 3 nc -v "$HOST" "$port" 2>&1
}

expect_status_from_curl() {
    local name="$1"
    local expected="$2"
    local url="$3"
    shift 3

    echo "cmd: curl -v \"$url\" $*"
    local status
    status="$(curl -sS -o /tmp/phase1_body.$$ -D /tmp/phase1_headers.$$ -w '%{http_code}' "$@" "$url" 2>/tmp/phase1_err.$$ || true)"

    if [ "$status" = "$expected" ]; then
        print_ok "$name"
    else
        print_fail "$name" "expected $expected got $status"
        echo "---- headers ----"
        cat /tmp/phase1_headers.$$ 2>/dev/null || true
        echo "---- body ----"
        cat /tmp/phase1_body.$$ 2>/dev/null || true
        echo "---- curl stderr ----"
        cat /tmp/phase1_err.$$ 2>/dev/null || true
    fi
}

expect_status_from_nc() {
    local name="$1"
    local expected="$2"
    local port="$3"
    local payload="$4"

    echo "cmd:"
    printf '%b\n' "$payload"

    local out
    out="$(nc_raw "$port" "$payload")"

    if echo "$out" | grep -q "^HTTP/1\.1 $expected "; then
        print_ok "$name"
    else
        print_fail "$name" "expected HTTP/1.1 $expected"
        echo "---- nc output ----"
        echo "$out"
    fi
}

expect_not_status_from_nc() {
    local name="$1"
    local forbidden="$2"
    local port="$3"
    local payload="$4"

    echo "cmd:"
    printf '%b\n' "$payload"

    local out
    out="$(nc_raw "$port" "$payload")"

    if echo "$out" | grep -q "^HTTP/1\.1 "; then
        if echo "$out" | grep -q "^HTTP/1\.1 $forbidden "; then
            print_fail "$name" "got forbidden status $forbidden"
            echo "---- nc output ----"
            echo "$out"
        else
            print_ok "$name"
        fi
    else
        print_fail "$name" "no HTTP response received"
        echo "---- nc output ----"
        echo "$out"
    fi
}

expect_any_http_from_nc() {
    local name="$1"
    local port="$2"
    local payload="$3"

    echo "cmd:"
    printf '%b\n' "$payload"

    local out
    out="$(nc_raw "$port" "$payload")"

    if echo "$out" | grep -q "^HTTP/1\."; then
        print_ok "$name"
    else
        print_fail "$name" "no HTTP response received"
        echo "---- nc output ----"
        echo "$out"
    fi
}

expect_status_from_python_socket() {
    local name="$1"
    local expected="$2"
    local port="$3"
    local pycode="$4"

    echo "cmd: python3 socket request on port $port"
    local out
    out="$(HOST="$HOST" PORT="$port" python3 - <<PY
$pycode
PY
)"

    if echo "$out" | grep -q "^HTTP/1\.1 $expected "; then
        print_ok "$name"
    else
        print_fail "$name" "expected HTTP/1.1 $expected"
        echo "---- python output ----"
        echo "$out"
    fi
}

count_http_responses() {
    local text="$1"
    printf '%s' "$text" | grep -o 'HTTP/1\.1 [0-9][0-9][0-9]' | wc -l | tr -d '[:space:]'
}

clean_upload_file() {
    local name="$1"
    rm -f "$UPLOAD_DIR/$name" >/dev/null 2>&1 || true
}

check_upload_content_exact() {
    local name="$1"
    local expected="$2"
    local path="$UPLOAD_DIR/$name"

    if [ ! -f "$path" ]; then
        print_fail "file $name exists after upload" "missing file at $path"
        return 1
    fi

    local got
    got="$(python3 - <<PY
p = r'''$path'''
with open(p, 'rb') as f:
    data = f.read()
print(data.decode('latin1'))
PY
)"
    if [ "$got" = "$expected" ]; then
        print_ok "uploaded file $name content matches"
        return 0
    else
        print_fail "uploaded file $name content matches" "expected [$expected] got [$got]"
        return 1
    fi
}

expect_no_early_response_from_python() {
    local name="$1"
    local port="$2"
    local pycode="$3"

    echo "cmd: python3 incremental socket test on port $port"
    local out
    out="$(HOST="$HOST" PORT="$port" python3 - <<PY
$pycode
PY
)"

    if echo "$out" | grep -q '\[OK\]'; then
        print_ok "$name"
    else
        print_fail "$name" "server responded too early or unexpectedly"
        echo "---- python output ----"
        echo "$out"
    fi
}

check_upload_content_exact_bytes() {
    local name="$1"
    local expected_py_bytes_expr="$2"
    local path="$UPLOAD_DIR/$name"

    if [ ! -f "$path" ]; then
        print_fail "file $name exists after upload" "missing file at $path"
        return 1
    fi

    local out
    out="$(python3 - <<PY
p = r'''$path'''
expected = $expected_py_bytes_expr
with open(p, "rb") as f:
    data = f.read()
print(repr(data))
print("[OK]" if data == expected else "[FAIL]")
PY
)"
    if echo "$out" | grep -q '\[OK\]'; then
        print_ok "uploaded file $name bytes match"
    else
        print_fail "uploaded file $name bytes match" "unexpected file content"
        echo "---- python output ----"
        echo "$out"
    fi
}

header_value_from_file() {
    local file="$1"
    local key="$2"
    grep -i "^${key}:" "$file" | tail -n 1 | sed -E "s/^[^:]+:[[:space:]]*//I" | tr -d '\r'
}

body_size_of_file() {
    local file="$1"
    wc -c < "$file" | tr -d '[:space:]'
}

expect_header_present_in_curl_response() {
    local name="$1"
    local header_file="$2"
    local key="$3"

    if grep -iq "^${key}:" "$header_file"; then
        print_ok "$name"
    else
        print_fail "$name" "missing header: $key"
        echo "---- headers ----"
        cat "$header_file" 2>/dev/null || true
    fi
}

expect_content_length_matches_body() {
    local name="$1"
    local header_file="$2"
    local body_file="$3"

    local cl
    local sz
    cl="$(header_value_from_file "$header_file" "content-length")"
    sz="$(body_size_of_file "$body_file")"

    if [ -z "$cl" ]; then
        print_fail "$name" "missing Content-Length"
        echo "---- headers ----"
        cat "$header_file" 2>/dev/null || true
        return
    fi

    if [ "$cl" = "$sz" ]; then
        print_ok "$name"
    else
        print_fail "$name" "Content-Length=$cl but body bytes=$sz"
        echo "---- headers ----"
        cat "$header_file" 2>/dev/null || true
    fi
}

prepare_http_method_get_fixtures() {
    section "4. HTTP_Method 测试准备"

    echo "cmd:"
    echo 'mkdir -p www www/upload www/dir www/emptydir www/cgi-bin www/html_error'
    echo 'rm -f www/hello.txt'
    echo 'rm -f www/dir/*'
    echo 'rm -f www/emptydir/*'
    echo 'printf "HELLO\n" > www/hello.txt'
    echo 'touch www/dir/a.txt'

    mkdir -p www www/upload www/dir www/emptydir www/cgi-bin www/html_error

    rm -f www/hello.txt
    rm -f www/dir/*
    rm -f www/emptydir/*

    printf "HELLO\n" > www/hello.txt
    touch www/dir/a.txt

    if [ -f "www/hello.txt" ] && [ -d "www/dir" ] && [ -d "www/emptydir" ]; then
        print_ok "HTTP GET fixtures prepared"
    else
        print_fail "HTTP GET fixtures prepared" "failed to create test fixtures under ./www"
    fi
}

prepare_http_method_post_delete_fixtures() {
    section "4.2 / 4.3 POST & DELETE 测试准备"

    echo "cmd:"
    echo 'mkdir -p www www/upload www/dir www/cgi-bin www/html_error'
    echo 'rm -f www/hello.txt'
    echo 'rm -f www/dir/*'
    echo 'rm -f www/upload/*'
    echo 'printf "HELLO\n" > www/hello.txt'
    echo 'touch www/dir/a.txt'

    mkdir -p www www/upload www/dir www/cgi-bin www/html_error

    rm -f www/hello.txt
    rm -f www/dir/*
    rm -f www/upload/*

    printf "HELLO\n" > www/hello.txt
    touch www/dir/a.txt

    if [ -f "www/hello.txt" ] && [ -d "www/upload" ] && [ -d "www/dir" ]; then
        print_ok "POST/DELETE fixtures prepared"
    else
        print_fail "POST/DELETE fixtures prepared" "failed to prepare fixture tree"
    fi
}

prepare_cgi_env_fixtures() {
    section "CGI env 测试准备"

    mkdir -p www/cgi-bin

    cat > www/cgi-bin/test_env.sh <<'SH'
#!/bin/sh
echo "Content-type: text/plain"
echo ""
echo "REQUEST_METHOD=$REQUEST_METHOD"
echo "QUERY_STRING=$QUERY_STRING"
echo "CONTENT_TYPE=$CONTENT_TYPE"
echo "CONTENT_LENGTH=$CONTENT_LENGTH"
echo "SCRIPT_NAME=$SCRIPT_NAME"
echo "PATH_INFO=$PATH_INFO"
echo "REQUEST_URI=$REQUEST_URI"
echo "SERVER_PORT=$SERVER_PORT"
echo "REMOTE_ADDR=$REMOTE_ADDR"
SH

    cat > www/cgi-bin/echo_body.sh <<'SH'
#!/bin/sh
echo "Content-type: text/plain"
echo ""
if [ -n "$CONTENT_LENGTH" ]; then
    dd bs=1 count="$CONTENT_LENGTH" 2>/dev/null
fi
SH

    chmod +x www/cgi-bin/test_env.sh www/cgi-bin/echo_body.sh

    if [ -x "www/cgi-bin/test_env.sh" ] && [ -x "www/cgi-bin/echo_body.sh" ]; then
        print_ok "CGI env fixtures prepared"
    else
        print_fail "CGI env fixtures prepared" "failed to create executable CGI scripts"
    fi
}

prepare_global_fixtures() {
    section "全局基础测试准备"

    mkdir -p www
    mkdir -p www/upload
    mkdir -p www/dir
    mkdir -p www/emptydir
    mkdir -p www/cgi-bin
    mkdir -p www/html_error

    printf "HELLO\n" > www/hello.txt
    printf "HELLO\n" > www/upload/hello.txt
    printf "hello world" > www/hello_len.txt
    touch www/dir/a.txt

    print_ok "global fixtures prepared"
}

expect_header_value_equals() {
    local name="$1"
    local header_file="$2"
    local key="$3"
    local expected="$4"

    local got
    got="$(header_value_from_file "$header_file" "$key")"

    if [ "$got" = "$expected" ]; then
        print_ok "$name"
    else
        print_fail "$name" "expected [$expected] got [$got]"
        echo "---- headers ----"
        cat "$header_file" 2>/dev/null || true
    fi
}
prepare_redirect_fixtures() {
    section "Redirect 测试准备"

    mkdir -p www/newplace
    printf "<h1>NEW PLACE</h1>\n" > www/newplace/index.html

    if [ -f "www/newplace/index.html" ]; then
        print_ok "redirect fixtures prepared"
    else
        print_fail "redirect fixtures prepared" "failed to create ./www/newplace/index.html"
    fi
}
# ---------------- 3.1 ----------------

test_valid_get() {
    local port="$1"
    local base="http://$HOST:$port"

    section "3.1.1 合法 GET 请求"
    echo "cmd: curl -v \"$base/\" -H \"Host: $HOST_HEADER\" --http1.1"

    local status
    status="$(curl -sS -o /tmp/phase1_body.$$ -D /tmp/phase1_headers.$$ \
        -w '%{http_code}' \
        -H "Host: $HOST_HEADER" \
        --http1.1 \
        "$base/" 2>/tmp/phase1_err.$$ || true)"

    local has_len="0"
    local has_te="0"
    local has_ka="0"

    grep -qi '^content-length:' /tmp/phase1_headers.$$ && has_len="1"
    grep -qi '^transfer-encoding:[[:space:]]*chunked' /tmp/phase1_headers.$$ && has_te="1"
    grep -qi '^connection:[[:space:]]*keep-alive' /tmp/phase1_headers.$$ && has_ka="1"

    if [ "$status" = "200" ] || [ "$status" = "301" ] || [ "$status" = "302" ] || [ "$status" = "303" ] || [ "$status" = "307" ] || [ "$status" = "308" ]; then
        if [ "$has_len" = "1" ] || [ "$has_te" = "1" ]; then
            print_ok "valid GET status/headers on $port"
        else
            print_fail "valid GET status/headers on $port" "missing both content-length and transfer-encoding: chunked"
            echo "---- headers ----"
            cat /tmp/phase1_headers.$$ 2>/dev/null || true
        fi
    else
        print_fail "valid GET status/headers on $port" "unexpected status $status"
        echo "---- headers ----"
        cat /tmp/phase1_headers.$$ 2>/dev/null || true
        echo "---- body ----"
        cat /tmp/phase1_body.$$ 2>/dev/null || true
    fi

    if [ "$has_ka" = "1" ]; then
        print_ok "valid GET keep-alive header visible on $port"
    else
        echo "note: no explicit Connection: keep-alive header, this can still be acceptable if connection is reused"
    fi
}

test_keep_alive_two_requests() {
    local port="$1"

    section "验证 keep-alive 同一连接连续两次请求"
    local payload='GET / HTTP/1.1\r\nHost: '"$HOST_HEADER"'\r\nConnection: keep-alive\r\n\r\nGET / HTTP/1.1\r\nHost: '"$HOST_HEADER"'\r\nConnection: close\r\n\r\n'
    echo "cmd:"
    printf '%b\n' "$payload"
    echo "| nc -v $HOST $port"

    local out
    out="$(nc_raw "$port" "$payload")"

    local count
    count="$(count_http_responses "$out")"

    if [ "$count" -ge 2 ]; then
        print_ok "keep-alive two responses on same connection ($port)"
    else
        print_fail "keep-alive two responses on same connection ($port)" "expected at least 2 HTTP responses, got $count"
        echo "---- nc output ----"
        echo "$out"
    fi
}

test_http_10_505() {
    local port="$1"
    section "HTTP 版本不是 1.1 -> 505"
    expect_status_from_nc \
        "HTTP/1.0 rejected with 505 on $port" \
        "505" \
        "$port" \
        'GET / HTTP/1.0\r\nHost: '"$HOST_HEADER"'\r\n\r\n'
}

test_method_405() {
    local port="$1"
    local base="http://$HOST:$port"
    section "Method 不允许 -> 405"
    expect_status_from_curl \
        "PUT rejected with 405 on $port" \
        "405" \
        "$base/" \
        -X PUT -H "Host: $HOST_HEADER" --http1.1
}

test_request_line_extra_token() {
    local port="$1"
    section "Request line 多余 token -> 400"
    expect_status_from_nc \
        "request line extra token rejected on $port" \
        "400" \
        "$port" \
        'GET / HTTP/1.1 EXTRA\r\nHost: '"$HOST_HEADER"'\r\n\r\n'
}

test_request_line_too_few_tokens() {
    local port="$1"
    section "请求行 token 不足 -> 400"
    expect_status_from_nc \
        "request line too few tokens rejected on $port" \
        "400" \
        "$port" \
        'GET /\r\nHost: '"$HOST_HEADER"'\r\n\r\n'
}

test_empty_method_path_version() {
    local port="$1"
    section "空 method / 空 path / 空 version"

    expect_status_from_nc \
        "empty method rejected on $port" \
        "400" \
        "$port" \
        ' / HTTP/1.1\r\nHost: '"$HOST_HEADER"'\r\n\r\n'

    expect_status_from_nc \
        "empty path rejected on $port" \
        "400" \
        "$port" \
        'GET  HTTP/1.1\r\nHost: '"$HOST_HEADER"'\r\n\r\n'

    expect_status_from_nc \
        "empty version rejected on $port" \
        "400" \
        "$port" \
        'GET / \r\nHost: '"$HOST_HEADER"'\r\n\r\n'
}

test_illegal_method_chars() {
    local port="$1"
    section "非法方法字符 -> 400"
    expect_status_from_nc \
        "illegal method characters rejected on $port" \
        "400" \
        "$port" \
        'GE T / HTTP/1.1\r\nHost: '"$HOST_HEADER"'\r\n\r\n'
}

test_absolute_uri_no_crash() {
    local port="$1"
    section "绝对 URI 形式，不崩溃"

    echo "cmd:"
    printf 'GET http://example.com/ HTTP/1.1\r\nHost: %s\r\n\r\n\n' "$HOST_HEADER"
    echo "| nc -v $HOST $port"

    local out
    out="$(nc_raw "$port" 'GET http://example.com/ HTTP/1.1\r\nHost: '"$HOST_HEADER"'\r\n\r\n')"

    if echo "$out" | grep -q '^HTTP/1\.1 '; then
        print_ok "absolute-form URI returns an HTTP response on $port"
    else
        print_fail "absolute-form URI returns an HTTP response on $port" "no HTTP response, possible parse crash/reset"
        echo "---- nc output ----"
        echo "$out"
    fi
}

# ---------------- 3.2 ----------------

test_missing_host() {
    local port="$1"
    section "3.2.1 缺 Host -> 400"

    expect_status_from_nc \
        "missing Host rejected on $port" \
        "400" \
        "$port" \
        'GET /hello.txt HTTP/1.1\r\n\r\n'

    expect_status_from_nc \
        "missing Host with Connection: close rejected on $port" \
        "400" \
        "$port" \
        'GET /hello.txt HTTP/1.1\r\nConnection: close\r\n\r\n'
}

test_host_with_port() {
    local port="$1"
    section "3.2.2 Host 带端口（合法，不应 400）"

    expect_not_status_from_nc \
        "Host localhost:$port accepted on $port" \
        "400" \
        "$port" \
        'GET /upload/hello.txt HTTP/1.1\r\nHost: localhost:'"$port"'\r\nConnection: close\r\n\r\n'
}

test_duplicate_host_case_insensitive() {
    local port="$1"
    section "3.2.3 Host 重复（大小写不同）-> 400"

    expect_status_from_nc \
        "duplicate Host with different case rejected on $port" \
        "400" \
        "$port" \
        'GET /hello.txt HTTP/1.1\r\nHost: a\r\nhOsT: b\r\n\r\n'
}

test_empty_host_value() {
    local port="$1"
    section "3.2.4 Host 为空值 -> 400"

    expect_status_from_nc \
        "empty Host value rejected on $port" \
        "400" \
        "$port" \
        'GET /hello.txt HTTP/1.1\r\nHost:\r\n\r\n'
}

test_blank_host_value() {
    local port="$1"
    section "3.2.5 Host 只有空白 -> 400"

    expect_status_from_nc \
        "blank Host value rejected on $port" \
        "400" \
        "$port" \
        'GET /hello.txt HTTP/1.1\r\nHost:   \r\n\r\n'
}

test_duplicate_host_same_value() {
    local port="$1"
    section "3.2.6 Host 重复但值相同 -> 400"

    expect_status_from_nc \
        "duplicate Host same value rejected on $port" \
        "400" \
        "$port" \
        'GET /hello.txt HTTP/1.1\r\nHost: a\r\nHost: a\r\n\r\n'
}

test_comma_separated_host() {
    local port="$1"
    section "3.2.7 单行多个 Host（逗号分隔）-> 400"

    expect_status_from_nc \
        "comma-separated Host rejected on $port" \
        "400" \
        "$port" \
        'GET /upload/hello.txt HTTP/1.1\r\nHost: a, b\r\n\r\n'
}

test_ipv6_host_with_port() {
    local port="$1"
    section "3.2.8 IPv6 Host 带端口，不支持也应保证不崩"

    expect_any_http_from_nc \
        "IPv6 Host returns an HTTP response on $port" \
        "$port" \
        'GET /upload/hello.txt HTTP/1.1\r\nHost: [::1]:'"$port"'\r\n\r\n'
}

# ---------------- 3.3 ----------------

test_absolute_form_host_match() {
    local port="$1"
    section "3.3.1 absolute-form + Host 一致（含端口，不应 400）"

    expect_not_status_from_nc \
        "absolute-form host/authority match accepted on $port" \
        "400" \
        "$port" \
        'GET http://example.com:'"$port"'/upload/hello.txt HTTP/1.1\r\nHost: example.com:'"$port"'\r\n\r\n'
}

test_absolute_form_host_mismatch() {
    local port="$1"
    section "3.3.2 absolute-form 与 Host 不一致 -> 400"

    expect_status_from_nc \
        "absolute-form mismatch host missing port rejected on $port" \
        "400" \
        "$port" \
        'GET http://example.com:'"$port"'/path HTTP/1.1\r\nHost: example.com\r\n\r\n'

    expect_status_from_nc \
        "absolute-form mismatch different authority rejected on $port" \
        "400" \
        "$port" \
        'GET http://evil.com:'"$port"'/path HTTP/1.1\r\nHost: example.com:'"$port"'\r\n\r\n'
}

test_uri_too_long() {
    local port="$1"
    section "3.3.3 URI 过长 -> 414"

    expect_status_from_python_socket \
        "URI too long rejected with 414 on $port" \
        "414" \
        "$port" \
'import os, socket
host=os.environ.get("HOST","127.0.0.1")
port=int(os.environ.get("PORT","8080"))
path="/" + "a"*9000
req=f"GET {path} HTTP/1.1\r\nHost: example.local\r\nConnection: close\r\n\r\n"
s=socket.create_connection((host,port))
s.sendall(req.encode())
data=s.recv(4096)
print(data.decode(errors="replace"))'
}

test_invalid_uri_chars() {
    local port="$1"
    section "3.3.4 非法 URI 字符 -> 400"

    expect_status_from_nc \
        "invalid URI chars <> rejected on $port" \
        "400" \
        "$port" \
        'GET /a<> HTTP/1.1\r\nHost: example.local\r\n\r\n'

    expect_status_from_nc \
        "control char in URI rejected on $port" \
        "400" \
        "$port" \
        'GET /a\001b HTTP/1.1\r\nHost: example.local\r\n\r\n'
}

test_path_dotdot_forbidden() {
    local port="$1"
    section "3.3.5 path 包含 .. -> 403"

    expect_status_from_nc \
        "path /a/../b rejected with 403 on $port" \
        "403" \
        "$port" \
        'GET /a/../b HTTP/1.1\r\nHost: example.local\r\n\r\n'

    expect_status_from_nc \
        "path /../secret rejected with 403 on $port" \
        "403" \
        "$port" \
        'GET /../secret HTTP/1.1\r\nHost: example.local\r\n\r\n'
}

test_percent_encoded_dotdot_no_crash() {
    local port="$1"
    section "3.3.5 URL 编码的 .. 至少不应崩"

    expect_any_http_from_nc \
        "percent-encoded .. returns an HTTP response on $port" \
        "$port" \
        'GET /a/%2e%2e/b HTTP/1.1\r\nHost: example.local\r\n\r\n'
}

test_percent20_behavior() {
    local port="$1"
    local base="http://$HOST:$port"
    section "3.3.5 正常编码字符 %20"

    echo "cmd: curl -isS --http1.1 \"$base/hello%20space.txt\" -H \"Host: localhost\""
    local status
    status="$(curl -sS -o /tmp/phase1_body.$$ -D /tmp/phase1_headers.$$ \
        -w '%{http_code}' \
        --http1.1 \
        -H "Host: localhost" \
        "$base/hello%20space.txt" 2>/tmp/phase1_err.$$ || true)"

    if [ "$status" = "404" ]; then
        print_ok "percent-encoded space returns 404 on $port"
    else
        print_fail "percent-encoded space returns 404 on $port" "expected 404 got $status"
        echo "---- headers ----"
        cat /tmp/phase1_headers.$$ 2>/dev/null || true
        echo "---- body ----"
        cat /tmp/phase1_body.$$ 2>/dev/null || true
    fi
}

test_query_path_split() {
    local port="$1"
    local base="http://$HOST:$port"
    section "3.3.6 Query string 与路径分离"

    echo "cmd: curl -isS --http1.1 \"$base/upload/hello.txt?x=1&y=2\" -H \"Host: localhost\""
    local status
    status="$(curl -sS -o /tmp/phase1_body.$$ -D /tmp/phase1_headers.$$ \
        -w '%{http_code}' \
        --http1.1 \
        -H "Host: localhost" \
        "$base/upload/hello.txt?x=1&y=2" 2>/tmp/phase1_err.$$ || true)"

    if [ "$status" = "200" ] || [ "$status" = "404" ]; then
        if [ "$status" = "200" ]; then
            print_ok "query string separated from path on $port"
        else
            print_fail "query string separated from path on $port" "got 404, expected existing file path to remain routable"
            echo "note: this test assumes /upload/hello.txt exists on your server"
            echo "---- headers ----"
            cat /tmp/phase1_headers.$$ 2>/dev/null || true
            echo "---- body ----"
            cat /tmp/phase1_body.$$ 2>/dev/null || true
        fi
    else
        print_fail "query string separated from path on $port" "unexpected status $status"
        echo "---- headers ----"
        cat /tmp/phase1_headers.$$ 2>/dev/null || true
        echo "---- body ----"
        cat /tmp/phase1_body.$$ 2>/dev/null || true
    fi
}

# ---------------- 3.4 ----------------

test_connection_close_keepalive_combo() {
    local port="$1"
    section "3.4.1 Connection: close, keep-alive"

    local payload='GET /upload/hello.txt HTTP/1.1\r\nHost: example.local\r\nConnection: close, keep-alive\r\n\r\nGET /hello.txt HTTP/1.1\r\nHost: example.local\r\nConnection: close\r\n\r\n'
    echo "cmd:"
    printf '%b\n' "$payload"
    echo "| nc -v $HOST $port"

    local out
    out="$(nc_raw "$port" "$payload")"
    local count
    count="$(count_http_responses "$out")"

    if [ "$count" -ge 2 ]; then
        print_ok "Connection: close, keep-alive allows second request on same connection ($port)"
    else
        print_fail "Connection: close, keep-alive allows second request on same connection ($port)" "expected 2 HTTP responses, got $count"
        echo "---- nc output ----"
        echo "$out"
    fi
}

test_connection_close_case_space() {
    local port="$1"
    section "3.4.2 Connection:  Close"

    local payload='GET /upload//hello.txt HTTP/1.1\r\nHost: example.local\r\nConnection:  Close \r\n\r\nGET /hello.txt HTTP/1.1\r\nHost: example.local\r\n\r\n'
    echo "cmd:"
    printf '%b\n' "$payload"
    echo "| nc -v $HOST $port"

    local out
    out="$(nc_raw "$port" "$payload")"
    local count
    count="$(count_http_responses "$out")"

    if [ "$count" -eq 1 ]; then
        print_ok "Connection: Close closes after first response ($port)"
    else
        print_fail "Connection: Close closes after first response ($port)" "expected exactly 1 HTTP response, got $count"
        echo "---- nc output ----"
        echo "$out"
    fi
}

test_connection_only_close() {
    local port="$1"
    section "3.4.3 只有 close"

    local payload='GET /upload/hello.txt HTTP/1.1\r\nHost: example.local\r\nConnection: close\r\n\r\nGET /hello.txt HTTP/1.1\r\nHost: example.local\r\n\r\n'
    echo "cmd:"
    printf '%b\n' "$payload"
    echo "| nc -v $HOST $port"

    local out
    out="$(nc_raw "$port" "$payload")"
    local count
    count="$(count_http_responses "$out")"

    if [ "$count" -eq 1 ]; then
        print_ok "Connection: close closes connection ($port)"
    else
        print_fail "Connection: close closes connection ($port)" "expected exactly 1 HTTP response, got $count"
        echo "---- nc output ----"
        echo "$out"
    fi
}

test_connection_unknown_tokens() {
    local port="$1"
    section "3.4.4 带未知 token"

    local payload='GET /upload/hello.txt HTTP/1.1\r\nHost: example.local\r\nConnection: close, upgrade, foo\r\n\r\nGET /hello.txt HTTP/1.1\r\nHost: example.local\r\n\r\n'
    echo "cmd:"
    printf '%b\n' "$payload"
    echo "| nc -v $HOST $port"

    local out
    out="$(nc_raw "$port" "$payload")"
    local count
    count="$(count_http_responses "$out")"

    if [ "$count" -ge 1 ]; then
        print_ok "Connection with unknown tokens still yields a valid response ($port)"
    else
        print_fail "Connection with unknown tokens still yields a valid response ($port)" "no HTTP response"
        echo "---- nc output ----"
        echo "$out"
    fi
}

test_connection_spaces_commas() {
    local port="$1"
    section "3.4.5 token 前后空白与多个逗号风格"

    local payload='GET /upload//hello.txt HTTP/1.1\r\nHost: example.local\r\nConnection:   keep-alive  ,   close   \r\n\r\nGET /hello.txt HTTP/1.1\r\nHost: example.local\r\n\r\n'
    echo "cmd:"
    printf '%b\n' "$payload"
    echo "| nc -v $HOST $port"

    local out
    out="$(nc_raw "$port" "$payload")"
    local count
    count="$(count_http_responses "$out")"

    if [ "$count" -eq 1 ]; then
        print_ok "Connection tokens with spaces/commas parsed consistently ($port)"
    else
        print_fail "Connection tokens with spaces/commas parsed consistently ($port)" "expected exactly 1 HTTP response, got $count"
        echo "---- nc output ----"
        echo "$out"
    fi
}

# ---------------- 3.5 ----------------

test_header_too_large_431() {
    local port="$1"
    section "3.5.1 Header 总长度过大 -> 431"

    echo "cmd: python3 auto-grow big header until 431 on port $port"
    local out
    out="$(HOST="$HOST" PORT="$port" python3 - <<'PY'
import os, socket

host=os.environ.get("HOST","127.0.0.1")
port=int(os.environ.get("PORT","8080"))

def once(n):
    big = "A"*n
    req = (
        "GET / HTTP/1.1\r\n"
        "Host: example.local\r\n"
        f"X-Big: {big}\r\n"
        "Connection: close\r\n"
        "\r\n"
    )
    s=socket.create_connection((host,port))
    s.sendall(req.encode())
    data=s.recv(4096).decode(errors="replace")
    s.close()
    line=data.split("\r\n",1)[0]
    return line, data

n=1024
seen_431 = False
while n <= 1024*1024:
    line, _ = once(n)
    print(f"{n} -> {line}")
    if " 431 " in line:
        seen_431 = True
        break
    n *= 2

print("SEEN_431=" + ("1" if seen_431 else "0"))
PY
)"

    if echo "$out" | grep -q 'SEEN_431=1'; then
        print_ok "header too large eventually returns 431 on $port"
    else
        print_fail "header too large eventually returns 431 on $port" "did not observe 431"
        echo "---- python output ----"
        echo "$out"
    fi
}

test_header_no_colon() {
    local port="$1"
    section "3.5.2 Header 行无冒号 -> 400"

    expect_status_from_nc \
        "header line without colon rejected on $port" \
        "400" \
        "$port" \
        'GET / HTTP/1.1\r\nHost: example.local\r\nBadHeaderLine\r\n\r\n'
}

test_header_empty_value_allowed() {
    local port="$1"
    section "3.5.3 只有 key 没 value 但有冒号"

    expect_not_status_from_nc \
        "header X-Empty: accepted on $port" \
        "400" \
        "$port" \
        'GET / HTTP/1.1\r\nHost: example.local\r\nX-Empty:\r\nConnection: close\r\n\r\n'
}

test_header_invalid_name() {
    local port="$1"
    section "3.5.4 Header 名非法 -> 400"

    expect_status_from_nc \
        "header name with space rejected on $port" \
        "400" \
        "$port" \
        'GET / HTTP/1.1\r\nHost: example.local\r\nBad Name: x\r\n\r\n'

    expect_status_from_nc \
        "header name with @ rejected on $port" \
        "400" \
        "$port" \
        'GET / HTTP/1.1\r\nHost: example.local\r\nBad@Name: x\r\n\r\n'

    expect_status_from_nc \
        "header name with [] rejected on $port" \
        "400" \
        "$port" \
        'GET / HTTP/1.1\r\nHost: example.local\r\nBad[Name]: x\r\n\r\n'
}

test_header_empty_key() {
    local port="$1"
    section "3.5.5 header key 为空 -> 400"

    expect_status_from_nc \
        "header with empty key rejected on $port" \
        "400" \
        "$port" \
        'GET / HTTP/1.1\r\nHost: example.local\r\n: value\r\n\r\n'
}

test_bare_lf_only() {
    local port="$1"
    section "3.5.6 行尾没有 CRLF / 出现裸 LF -> 400"

    expect_status_from_nc \
        "bare LF request rejected on $port" \
        "400" \
        "$port" \
        'GET / HTTP/1.1\nHost: example.local\n\n'
}

# ---------------- 3.6 ----------------

test_post_missing_length_411() {
    local port="$1"
    section "3.6.1 POST 无长度信息 -> 411"

    expect_status_from_nc \
        "POST missing length info rejected with 411 on $port" \
        "411" \
        "$port" \
        'POST /upload/misslen.txt HTTP/1.1\r\nHost: localhost\r\n\r\nhello'
}

test_duplicate_content_length_400() {
    local port="$1"
    section "3.6.2 重复 Content-Length -> 400"

    expect_status_from_nc \
        "duplicate identical Content-Length rejected on $port" \
        "400" \
        "$port" \
        'POST /upload/dupcl.txt HTTP/1.1\r\nHost: localhost\r\nContent-Length: 5\r\nContent-Length: 5\r\n\r\nhello'

    expect_status_from_nc \
        "duplicate conflicting Content-Length rejected on $port" \
        "400" \
        "$port" \
        'POST /upload/dupcl2.txt HTTP/1.1\r\nHost: localhost\r\nContent-Length: 5\r\nContent-Length: 6\r\n\r\nhello!'
}

test_content_length_too_large_413() {
    local port="$1"
    section "3.6.3 Content-Length 过大 -> 413"

    expect_status_from_nc \
        "huge Content-Length rejected with 413 on $port" \
        "413" \
        "$port" \
        'POST /upload/toolarge.txt HTTP/1.1\r\nHost: localhost\r\nContent-Length: 999999999\r\nConnection: close\r\n\r\nx'

    expect_status_from_nc \
        "boundary-over-limit Content-Length rejected with 413 on $port" \
        "413" \
        "$port" \
        'POST /upload/toolarge2.txt HTTP/1.1\r\nHost: localhost\r\nContent-Length: 512001\r\nConnection: close\r\n\r\n'
}

test_transfer_encoding_not_chunked_501() {
    local port="$1"
    section "3.6.4 Transfer-Encoding 非 chunked -> 501"

    expect_status_from_nc \
        "Transfer-Encoding: gzip rejected with 501 on $port" \
        "501" \
        "$port" \
        'POST /upload/raw3.txt HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: gzip\r\n\r\n'

    expect_status_from_nc \
        "Transfer-Encoding: gzip, compress rejected with 501 on $port" \
        "501" \
        "$port" \
        'POST /upload/raw4.txt HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: gzip, compress\r\n\r\n'
}

test_duplicate_transfer_encoding_400() {
    local port="$1"
    section "3.6.5 Transfer-Encoding 重复 -> 400"

    expect_status_from_nc \
        "duplicate Transfer-Encoding rejected on $port" \
        "400" \
        "$port" \
        'POST /upload/dupte.txt HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\nTransfer-Encoding: chunked\r\n\r\n0\r\n\r\n'
}

test_te_and_cl_together_400() {
    local port="$1"
    section "3.6.6 TE: chunked + Content-Length -> 400"

    expect_status_from_nc \
        "chunked + Content-Length rejected on $port" \
        "400" \
        "$port" \
        'POST /upload/tecl.txt HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\nContent-Length: 5\r\n\r\n5\r\nhello\r\n0\r\n\r\n'
}

test_chunked_normal_upload() {
    local port="$1"
    section "3.6.7 chunked 正常上传"

    clean_upload_file "chunk1.txt"

    local out
    out="$(nc_raw "$port" 'POST /upload/chunk1.txt HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n5\r\nhello\r\n6\r\n world\r\n0\r\n\r\n')"

    if echo "$out" | grep -q '^HTTP/1\.1 201 '; then
        print_ok "chunked upload returns 201 on $port"
    else
        print_fail "chunked upload returns 201 on $port" "expected 201"
        echo "---- nc output ----"
        echo "$out"
        return
    fi

    check_upload_content_exact "chunk1.txt" "hello world"
}

test_chunk_hex_size() {
    local port="$1"
    section "3.6.8 chunk size 大小写十六进制"

    clean_upload_file "chunk_hex.txt"

    local out
    out="$(nc_raw "$port" 'POST /upload/chunk_hex.txt HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\nA\r\n0123456789\r\n0\r\n\r\n')"

    if echo "$out" | grep -q '^HTTP/1\.1 201 '; then
        print_ok "hex chunk size upload returns 201 on $port"
    else
        print_fail "hex chunk size upload returns 201 on $port" "expected 201"
        echo "---- nc output ----"
        echo "$out"
        return
    fi

    check_upload_content_exact "chunk_hex.txt" "0123456789"
}

test_chunk_extension_supported() {
    local port="$1"
    section "3.6.9 chunk extension"

    clean_upload_file "chunk_ext.txt"

    local out
    out="$(nc_raw "$port" 'POST /upload/chunk_ext.txt HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n5;ext=1\r\nhello\r\n0\r\n\r\n')"

    if echo "$out" | grep -q '^HTTP/1\.1 201 '; then
        print_ok "chunk extension upload returns 201 on $port"
    else
        print_fail "chunk extension upload returns 201 on $port" "expected 201"
        echo "---- nc output ----"
        echo "$out"
        return
    fi

    check_upload_content_exact "chunk_ext.txt" "hello"
}

test_chunk_incomplete_and_badsize() {
    local port="$1"
    section "3.6.10 chunked 缺少结尾 / 非法 chunk size"

    clean_upload_file "chunk_incomplete.txt"

    local out_incomplete
    out_incomplete="$(nc_raw "$port" 'POST /upload/chunk_incomplete.txt HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n5\r\nhello\r\n')"

    if echo "$out_incomplete" | grep -q '^HTTP/1\.1 201 '; then
        print_fail "incomplete chunked request should not succeed on $port" "got 201 unexpectedly"
        echo "---- nc output ----"
        echo "$out_incomplete"
    else
        print_ok "incomplete chunked request does not falsely return 201 on $port"
    fi

    clean_upload_file "chunk_ok.txt"
    local out_ok
    out_ok="$(nc_raw "$port" 'POST /upload/chunk_ok.txt HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n5\r\nhello\r\n0\r\n\r\n')"

    if echo "$out_ok" | grep -q '^HTTP/1\.1 201 '; then
        print_ok "complete chunked request succeeds on $port"
    else
        print_fail "complete chunked request succeeds on $port" "expected 201"
        echo "---- nc output ----"
        echo "$out_ok"
    fi

    clean_upload_file "chunk_badsize.txt"
    local out_bad
    out_bad="$(nc_raw "$port" 'POST /upload/chunk_badsize.txt HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\nZZ\r\nhello\r\n0\r\n\r\n')"

    if echo "$out_bad" | grep -q '^HTTP/1\.1 400 '; then
        print_ok "invalid chunk size rejected with 400 on $port"
    else
        print_fail "invalid chunk size rejected with 400 on $port" "expected 400"
        echo "---- nc output ----"
        echo "$out_bad"
    fi
}

test_transfer_encoding_mixed_tokens() {
    local port="$1"
    section "3.6.11 混合 Transfer-Encoding token"

    clean_upload_file "te_mix.txt"

    local out
    out="$(nc_raw "$port" 'POST /upload/te_mix.txt HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: gzip, chunked\r\n\r\n3\r\nabc\r\n0\r\n\r\n')"

    if echo "$out" | grep -q '^HTTP/1\.1 '; then
        print_ok "mixed Transfer-Encoding returns a stable HTTP response on $port"
    else
        print_fail "mixed Transfer-Encoding returns a stable HTTP response on $port" "no HTTP response"
        echo "---- nc output ----"
        echo "$out"
    fi
}

test_content_length_nondigit_400() {
    local port="$1"
    section "3.6.12 Content-Length 非数字 -> 400"

    expect_status_from_nc \
        "non-digit Content-Length rejected on $port" \
        "400" \
        "$port" \
        'POST /upload/cl_nondigit.txt HTTP/1.1\r\nHost: localhost\r\nContent-Length: abc\r\n\r\n'
}

test_duplicate_content_type_400() {
    local port="$1"
    section "3.6.13 重复 Content-Type -> 400"

    expect_status_from_nc \
        "duplicate Content-Type rejected on $port" \
        "400" \
        "$port" \
        'POST /upload/test.txt HTTP/1.1\r\nHost: localhost\r\nContent-Type: text/plain\r\nContent-Type: application/json\r\nContent-Length: 5\r\nConnection: close\r\n\r\nhello'

    expect_status_from_nc \
        "duplicate Content-Type with different case rejected on $port" \
        "400" \
        "$port" \
        'POST /upload/test2.txt HTTP/1.1\r\nHost: localhost\r\nContent-Type: text/plain\r\ncontent-type: application/json\r\nContent-Length: 5\r\nConnection: close\r\n\r\nhello'

    expect_status_from_nc \
        "duplicate identical Content-Type rejected on $port" \
        "400" \
        "$port" \
        'POST /upload/test3.txt HTTP/1.1\r\nHost: localhost\r\nContent-Type: text/plain\r\nContent-Type: text/plain\r\nContent-Length: 5\r\nConnection: close\r\n\r\nhello'
}
# ---------------- 3.7 ----------------

test_fixed_body_incremental() {
    local port="$1"
    section "3.7.1 固定长度 body 分段输入"

    clean_upload_file "inc_fixed.txt"

    local out
    out="$(HOST="$HOST" PORT="$port" python3 - <<'PY'
import os, socket, time
host=os.environ.get("HOST","127.0.0.1")
port=int(os.environ.get("PORT","8080"))

req_head = (
    "POST /upload/inc_fixed.txt HTTP/1.1\r\n"
    "Host: localhost\r\n"
    "Content-Length: 12\r\n"
    "Connection: close\r\n"
    "\r\n"
)
parts = [b"hel", b"lo ", b"wor", b"ld!"]  # hello world!
s=socket.create_connection((host,port))
s.sendall(req_head.encode())
time.sleep(0.2)

for p in parts:
    s.sendall(p)
    time.sleep(0.2)

resp = s.recv(4096).decode(errors="replace")
print(resp.split("\r\n\r\n",1)[0])
s.close()
PY
)"

    if echo "$out" | grep -q '^HTTP/1\.1 201 '; then
        print_ok "incremental fixed-length body upload returns 201 on $port"
    else
        print_fail "incremental fixed-length body upload returns 201 on $port" "expected 201"
        echo "---- python output ----"
        echo "$out"
        return
    fi

    check_upload_content_exact "inc_fixed.txt" "hello world!"
}

test_chunk_extension_supported_body() {
    local port="$1"
    section "3.7.2 chunk extension（再次验证）"

    clean_upload_file "chunk_ext.txt"

    local out
    out="$(nc_raw "$port" 'POST /upload/chunk_ext.txt HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n5;ext=1\r\nhello\r\n0\r\n\r\n')"

    if echo "$out" | grep -q '^HTTP/1\.1 201 '; then
        print_ok "chunk extension body upload returns 201 on $port"
    else
        print_fail "chunk extension body upload returns 201 on $port" "expected 201"
        echo "---- nc output ----"
        echo "$out"
        return
    fi

    check_upload_content_exact "chunk_ext.txt" "hello"
}

test_chunked_incomplete_waits() {
    local port="$1"
    section "3.7.3 chunked 未结束时应等待更多数据"

    expect_no_early_response_from_python \
        "incomplete chunked body does not respond early on $port" \
        "$port" \
'import os, socket, time
host=os.environ.get("HOST","127.0.0.1")
port=int(os.environ.get("PORT","8080"))

req = (
    "POST /upload/chunk_incomplete.txt HTTP/1.1\r\n"
    "Host: localhost\r\n"
    "Transfer-Encoding: chunked\r\n"
    "\r\n"
    "5\r\nhe"
)
s=socket.create_connection((host,port))
s.settimeout(1.0)
s.sendall(req.encode())

time.sleep(0.3)
try:
    data = s.recv(4096)
    print("Server responded early:")
    print(data.decode(errors="replace").split("\\r\\n",1)[0])
except socket.timeout:
    print("[OK] no early response (parser likely waiting for more data)")

time.sleep(0.7)
try:
    data = s.recv(4096)
    print("Server responded within 1s:")
    print(data.decode(errors="replace").split("\\r\\n",1)[0])
except socket.timeout:
    print("[OK] still waiting (complet=false expected)")

s.close()'
}

test_lf_only_stuck_body() {
    local port="$1"
    section "3.7.4 LF-only 短时间内不应立即响应"

    expect_no_early_response_from_python \
        "LF-only request does not get an immediate response on $port" \
        "$port" \
'import os, socket, time
host=os.environ.get("HOST","127.0.0.1")
port=int(os.environ.get("PORT","8080"))

req = "GET / HTTP/1.1\nHost: localhost\n\n"
s=socket.create_connection((host,port))
s.settimeout(1.0)
s.sendall(req.encode())

time.sleep(0.3)
try:
    data = s.recv(4096)
    print("Server responded:")
    print(data.decode(errors="replace").split("\\r\\n",1)[0])
except socket.timeout:
    print("[OK] no response (likely stuck waiting for CRLF)")

s.close()'
}

test_te_chunked_plus_cl_body() {
    local port="$1"
    section "3.7.5 TE chunked + Content-Length -> 400（再次验证）"

    expect_status_from_nc \
        "chunked plus Content-Length rejected with 400 on $port" \
        "400" \
        "$port" \
        'POST /upload/raw4.txt HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\nContent-Length: 3\r\n\r\n3\r\nabc\r\n0\r\n\r\n'
}

test_chunk_size_mismatch() {
    local port="$1"
    section "3.7.6 chunk size 与数据不匹配"

    local short_out
    short_out="$(nc_raw "$port" 'POST /upload/chunk_short.txt HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n5\r\nhell\r\n')"
    if echo "$short_out" | grep -q '^HTTP/1\.1 201 '; then
        print_fail "short chunk should not falsely succeed on $port" "got 201 unexpectedly"
        echo "---- nc output ----"
        echo "$short_out"
    else
        print_ok "short chunk does not falsely return 201 on $port"
    fi

    local long_out
    long_out="$(nc_raw "$port" 'POST /upload/chunk_long.txt HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n5\r\nhelloo\r\n0\r\n\r\n')"
    if echo "$long_out" | grep -q '^HTTP/1\.1 400 '; then
        print_ok "long chunk rejected with 400 on $port"
    else
        print_fail "long chunk rejected with 400 on $port" "expected 400"
        echo "---- nc output ----"
        echo "$long_out"
    fi
}

test_chunk_missing_crlf_after_data() {
    local port="$1"
    section "3.7.7 chunk 后缺少 CRLF"

    local out
    out="$(nc_raw "$port" 'POST /upload/chunk_nocrlf.txt HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n5\r\nhello0\r\n\r\n')"

    if echo "$out" | grep -q '^HTTP/1\.1 400 '; then
        print_ok "missing CRLF after chunk-data rejected with 400 on $port"
    else
        print_fail "missing CRLF after chunk-data rejected with 400 on $port" "expected 400"
        echo "---- nc output ----"
        echo "$out"
    fi
}

test_chunk_data_contains_crlf() {
    local port="$1"
    section "3.7.8 chunked body 中数据本身包含 CRLF"

    clean_upload_file "chunk_crlf.txt"

    local out1
    out1="$(nc_raw "$port" 'POST /upload/chunk_crlf.txt HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\nC\r\nhello\r\nworld\r\n0\r\n\r\n')"

    if echo "$out1" | grep -q '^HTTP/1\.1 201 '; then
        print_ok "single chunk containing CRLF returns 201 on $port"
    else
        print_fail "single chunk containing CRLF returns 201 on $port" "expected 201"
        echo "---- nc output ----"
        echo "$out1"
        return
    fi

    check_upload_content_exact_bytes "chunk_crlf.txt" 'b"hello\r\nworld"'

    clean_upload_file "chunk_crlf2.txt"

    local out2
    out2="$(nc_raw "$port" 'POST /upload/chunk_crlf2.txt HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n4\r\na\r\nb\r\n4\r\nc\r\nd\r\n0\r\n\r\n')"

    if echo "$out2" | grep -q '^HTTP/1\.1 201 '; then
        print_ok "multiple chunks containing CRLF return 201 on $port"
    else
        print_fail "multiple chunks containing CRLF return 201 on $port" "expected 201"
        echo "---- nc output ----"
        echo "$out2"
        return
    fi

    check_upload_content_exact_bytes "chunk_crlf2.txt" 'b"a\r\nbc\r\nd"'
}

test_chunk_fake_boundary_text_in_data() {
    local port="$1"
    section "3.7.8 chunk data 中伪造 chunk 边界文本"

    clean_upload_file "chunk_fake.txt"

    local out
    out="$(nc_raw "$port" 'POST /upload/chunk_fake.txt HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\nF\r\n5\r\nhello\r\n0\r\n\r\n\r\n0\r\n\r\n')"

    if echo "$out" | grep -q '^HTTP/1\.1 201 '; then
        print_ok "fake chunk-boundary text inside data returns 201 on $port"
    else
        print_fail "fake chunk-boundary text inside data returns 201 on $port" "expected 201"
        echo "---- nc output ----"
        echo "$out"
        return
    fi

    check_upload_content_exact_bytes "chunk_fake.txt" 'b"5\r\nhello\r\n0\r\n\r\n"'
}

# ---------------- 3.8 ----------------
mkdir -p www
printf "hello world" > www/hello_len.txt


test_error_405_has_allow() {
    local port="$1"
    local base="http://$HOST:$port"

    section "3.8.1 ErrorResponse 405 包含 Allow"

    echo "cmd: curl -v -X PUT \"$base/\" -H \"Host: localhost\" --http1.1"
    local status
    status="$(curl -sS -D /tmp/hdr405a.$$ -o /tmp/body405a.$$ -w '%{http_code}' \
        -X PUT "$base/" -H "Host: localhost" --http1.1 2>/tmp/err405a.$$ || true)"

    if [ "$status" = "405" ]; then
        print_ok "PUT / returns 405 on $port"
    else
        print_fail "PUT / returns 405 on $port" "expected 405 got $status"
        echo "---- headers ----"
        cat /tmp/hdr405a.$$ 2>/dev/null || true
        echo "---- body ----"
        cat /tmp/body405a.$$ 2>/dev/null || true
    fi

    expect_header_present_in_curl_response \
        "405 response contains Allow header on $port" \
        /tmp/hdr405a.$$ \
        "allow"

    if grep -iq '^connection:' /tmp/hdr405a.$$; then
        print_ok "405 response contains Connection header on $port"
    else
        print_fail "405 response contains Connection header on $port" "missing Connection header"
        echo "---- headers ----"
        cat /tmp/hdr405a.$$ 2>/dev/null || true
    fi

    echo "cmd:"
    printf 'PUT / HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n\n'
    echo "| nc -v $HOST $port"

    local out
    out="$(nc_raw "$port" 'PUT / HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n')"

    if echo "$out" | grep -q '^HTTP/1\.1 405 '; then
        if echo "$out" | grep -iq '^Allow:'; then
            print_ok "nc 405 response also contains Allow on $port"
        else
            print_fail "nc 405 response also contains Allow on $port" "missing Allow header"
            echo "---- nc output ----"
            echo "$out"
        fi
    else
        print_fail "nc PUT / returns 405 on $port" "expected 405"
        echo "---- nc output ----"
        echo "$out"
    fi
}

test_responsebuilder_content_length() {
    local port="$1"
    local base="http://$HOST:$port"

    section "3.8.2 ResponseBuilder 自动 Content-Length"

    echo "cmd: curl -sS -D /tmp/hdr_len.$$ \"$base/hello_len.txt\" -H \"Host: localhost\" --http1.1 -o /tmp/body_len.$$"
    curl -sS -D /tmp/hdr_len.$$ "$base/hello_len.txt" -H "Host: localhost" --http1.1 -o /tmp/body_len.$$ 2>/tmp/err_len.$$ || true
    expect_content_length_matches_body \
        "200 static file Content-Length matches body on $port" \
        /tmp/hdr_len.$$ \
        /tmp/body_len.$$

    echo "cmd: curl -sS -D /tmp/hdr404.$$ \"$base/this_file_should_not_exist_404\" -H \"Host: localhost\" --http1.1 -o /tmp/body404.$$"
    curl -sS -D /tmp/hdr404.$$ "$base/this_file_should_not_exist_404" -H "Host: localhost" --http1.1 -o /tmp/body404.$$ 2>/tmp/err404.$$ || true
    expect_content_length_matches_body \
        "404 error response Content-Length matches body on $port" \
        /tmp/hdr404.$$ \
        /tmp/body404.$$

    echo "cmd: curl -sS -D /tmp/hdr405b.$$ -X PUT \"$base/\" -H \"Host: localhost\" --http1.1 -o /tmp/body405b.$$"
    curl -sS -D /tmp/hdr405b.$$ -X PUT "$base/" -H "Host: localhost" --http1.1 -o /tmp/body405b.$$ 2>/tmp/err405b.$$ || true
    expect_header_present_in_curl_response \
        "405 response contains Allow header (builder path) on $port" \
        /tmp/hdr405b.$$ \
        "allow"
    expect_content_length_matches_body \
        "405 error response Content-Length matches body on $port" \
        /tmp/hdr405b.$$ \
        /tmp/body405b.$$
}

test_no_hanging_without_length() {
    local port="$1"

    section "3.8.2 响应结束性，不应 hang"

    echo "cmd:"
    printf 'GET /hello.txt HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n\n'
    echo "| nc -v $HOST $port"

    local out
    out="$(nc_raw "$port" 'GET /hello.txt HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n')"

    if echo "$out" | grep -q '^HTTP/1\.1 '; then
        if echo "$out" | grep -iq '^Content-Length:' || echo "$out" | grep -iq '^Transfer-Encoding:[[:space:]]*chunked'; then
            print_ok "response framing is explicit and does not hang on $port"
        else
            print_fail "response framing is explicit and does not hang on $port" "missing both Content-Length and Transfer-Encoding: chunked"
            echo "---- nc output ----"
            echo "$out"
        fi
    else
        print_fail "GET /hello.txt returns a valid HTTP response on $port" "no HTTP response"
        echo "---- nc output ----"
        echo "$out"
    fi
}

test_empty_body_response_content_length_zero() {
    local port="$1"
    local base="http://$HOST:$port"

    section "3.8.3 空 body 响应是否给 Content-Length: 0"

    echo "cmd: curl -sS -D /tmp/hdr_del.$$ -X DELETE \"$base/upload/nope.txt\" -H \"Host: localhost\" --http1.1 -o /tmp/body_del.$$"
    local status
    status="$(curl -sS -D /tmp/hdr_del.$$ -X DELETE "$base/upload/nope.txt" -H "Host: localhost" --http1.1 -o /tmp/body_del.$$ -w '%{http_code}' 2>/tmp/err_del.$$ || true)"

    # 这条不强绑具体状态，只检查如果 body 为空，CL 是否合理
    local body_sz
    body_sz="$(body_size_of_file /tmp/body_del.$$)"

    if [ "$body_sz" = "0" ]; then
        local cl
        cl="$(header_value_from_file /tmp/hdr_del.$$ "content-length")"
        if [ "$cl" = "0" ]; then
            print_ok "empty-body response carries Content-Length: 0 on $port"
        else
            print_fail "empty-body response carries Content-Length: 0 on $port" "body is empty but Content-Length is [$cl]"
            echo "---- headers ----"
            cat /tmp/hdr_del.$$ 2>/dev/null || true
        fi
    else
        print_ok "DELETE non-existent response has non-empty body on $port (Content-Length checked elsewhere)"
    fi
}

test_error_content_type() {
    local port="$1"
    local base="http://$HOST:$port"

    section "3.8.4 错误响应是否设置正确的 Content-Type"

    echo "cmd: curl -isS --http1.1 \"$base/hello.txt\" -H \"Host: localhost\""
    curl -isS --http1.1 "$base/hello.txt" -H "Host: localhost" > /tmp/hdr_ct_ok.$$ 2>/tmp/err_ct_ok.$$ || true

    if grep -iq '^content-type:' /tmp/hdr_ct_ok.$$; then
        print_ok "normal response contains Content-Type on $port"
    else
        print_fail "normal response contains Content-Type on $port" "missing Content-Type"
        echo "---- response ----"
        cat /tmp/hdr_ct_ok.$$ 2>/dev/null || true
    fi

    echo "cmd: curl -isS --http1.1 \"$base/this_file_should_not_exist_404\" -H \"Host: localhost\""
    curl -isS --http1.1 "$base/this_file_should_not_exist_404" -H "Host: localhost" > /tmp/hdr_ct_err.$$ 2>/tmp/err_ct_err.$$ || true

    local ct
    ct="$(header_value_from_file /tmp/hdr_ct_err.$$ "content-type")"
    if printf '%s\n' "$ct" | grep -Eiq '^(text/plain|text/html)'; then
        print_ok "error response Content-Type is text/plain or text/html on $port"
    else
        print_fail "error response Content-Type is text/plain or text/html on $port" "got [$ct]"
        echo "---- response ----"
        cat /tmp/hdr_ct_err.$$ 2>/dev/null || true
    fi
}
# ---------------- 3.9 redirection ----------------

test_redirect_301_absolute() {
    local port="$1"
    local base="http://$HOST:$port"

    section "3.9.1 301 绝对地址重定向"

    echo "cmd: curl -isS --http1.1 \"$base/redirection/\" | sed -n '1,30p'"
    local status
    status="$(curl -sS -D /tmp/h_redir301.$$ "$base/redirection/" --http1.1 \
        -o /tmp/b_redir301.$$ 2>/tmp/e_redir301.$$ -w '%{http_code}' || true)"

    if [ "$status" = "301" ]; then
        print_ok "GET /redirection/ returns 301 on $port"
    else
        print_fail "GET /redirection/ returns 301 on $port" "expected 301 got $status"
        echo "---- headers ----"
        cat /tmp/h_redir301.$$ 2>/dev/null || true
        echo "---- body ----"
        cat /tmp/b_redir301.$$ 2>/dev/null || true
        return
    fi

    expect_header_present_in_curl_response \
        "301 response contains Location header on $port" \
        /tmp/h_redir301.$$ \
        "location"

    expect_header_value_equals \
        "301 Location points to expected absolute URL on $port" \
        /tmp/h_redir301.$$ \
        "location" \
        "https://42.fr/en/homepage/"

    expect_content_length_matches_body \
        "301 response Content-Length matches body on $port" \
        /tmp/h_redir301.$$ \
        /tmp/b_redir301.$$
}
test_redirect_302_relative() {
    local port="$1"
    local base="http://$HOST:$port"

    section "3.9.2 302 站内相对路径重定向"

    echo "cmd: curl -isS --http1.1 \"$base/oldplace/\" | sed -n '1,30p'"
    local status
    status="$(curl -sS -D /tmp/h_redir302.$$ "$base/oldplace/" --http1.1 \
        -o /tmp/b_redir302.$$ 2>/tmp/e_redir302.$$ -w '%{http_code}' || true)"

    if [ "$status" = "302" ]; then
        print_ok "GET /oldplace/ returns 302 on $port"
    else
        print_fail "GET /oldplace/ returns 302 on $port" "expected 302 got $status"
        echo "---- headers ----"
        cat /tmp/h_redir302.$$ 2>/dev/null || true
        echo "---- body ----"
        cat /tmp/b_redir302.$$ 2>/dev/null || true
        return
    fi

    expect_header_present_in_curl_response \
        "302 response contains Location header on $port" \
        /tmp/h_redir302.$$ \
        "location"

    expect_header_value_equals \
        "302 Location points to expected relative path on $port" \
        /tmp/h_redir302.$$ \
        "location" \
        "/newplace/"

    expect_content_length_matches_body \
        "302 response Content-Length matches body on $port" \
        /tmp/h_redir302.$$ \
        /tmp/b_redir302.$$
}
test_redirect_follow_relative() {
    local port="$1"
    local base="http://$HOST:$port"

    section "3.9.3 curl -L 跟随 302 到目标页面"

    echo "cmd: curl -isS -L --http1.1 \"$base/oldplace/\" | sed -n '1,40p'"
    local status
    status="$(curl -sS -D /tmp/h_redir_follow.$$ -L "$base/oldplace/" --http1.1 \
        -o /tmp/b_redir_follow.$$ 2>/tmp/e_redir_follow.$$ -w '%{http_code}' || true)"

    if [ "$status" = "200" ]; then
        print_ok "curl -L follows /oldplace/ to final 200 on $port"
    else
        print_fail "curl -L follows /oldplace/ to final 200 on $port" "expected 200 got $status"
        echo "---- headers ----"
        cat /tmp/h_redir_follow.$$ 2>/dev/null || true
        echo "---- body ----"
        cat /tmp/b_redir_follow.$$ 2>/dev/null || true
        return
    fi

    if grep -q '<h1>NEW PLACE</h1>' /tmp/b_redir_follow.$$; then
        print_ok "curl -L final body matches redirected target on $port"
    else
        print_fail "curl -L final body matches redirected target on $port" "expected redirected target body"
        echo "---- body ----"
        cat /tmp/b_redir_follow.$$ 2>/dev/null || true
    fi
}
test_redirect_post_behavior() {
    local port="$1"

    section "3.9.4 POST 到 redirect 路径的当前行为"

    expect_status_from_nc \
        "POST /redirection/ without length returns 411 on $port" \
        "411" \
        "$port" \
        'POST /redirection/ HTTP/1.1\r\nHost: localhost\r\n\r\n'

    expect_status_from_nc \
        "POST /redirection/ with Content-Length: 0 returns redirect on $port" \
        "301" \
        "$port" \
        'POST /redirection/ HTTP/1.1\r\nHost: localhost\r\nContent-Length: 0\r\n\r\n'
}
test_redirect_delete_behavior() {
    local port="$1"

    section "3.9.5 DELETE 到 redirect 路径"

    expect_status_from_nc \
        "DELETE /redirection/ returns 301 on $port" \
        "301" \
        "$port" \
        'DELETE /redirection/ HTTP/1.1\r\nHost: localhost\r\n\r\n'
}
# ---------------- 4. HTTP_Method / 4.1 GET ----------------

test_get_existing_file() {
    local port="$1"
    local base="http://$HOST:$port"

    section "4.1.1 GET 已存在文件 /hello.txt"

    echo "cmd: curl -isS --http1.1 \"$base/hello.txt\" | sed -n '1,20p'"
    local status
    status="$(curl -sS -D /tmp/h_get_hello.$$ "$base/hello.txt" --http1.1 -o /tmp/b_get_hello.$$ 2>/tmp/e_get_hello.$$ -w '%{http_code}' || true)"

    if [ "$status" = "200" ]; then
        print_ok "GET /hello.txt returns 200 on $port"
    else
        print_fail "GET /hello.txt returns 200 on $port" "expected 200 got $status"
        echo "---- headers ----"
        cat /tmp/h_get_hello.$$ 2>/dev/null || true
        echo "---- body ----"
        cat /tmp/b_get_hello.$$ 2>/dev/null || true
        return
    fi

    local ct
    ct="$(header_value_from_file /tmp/h_get_hello.$$ "content-type")"
    if printf '%s\n' "$ct" | grep -Eiq '^text/plain'; then
        print_ok "GET /hello.txt content-type is text/plain-like on $port"
    else
        print_fail "GET /hello.txt content-type is text/plain-like on $port" "got [$ct]"
    fi

    expect_content_length_matches_body \
        "GET /hello.txt Content-Length matches body on $port" \
        /tmp/h_get_hello.$$ \
        /tmp/b_get_hello.$$

    local body_sz
    body_sz="$(body_size_of_file /tmp/b_get_hello.$$)"
    if [ "$body_sz" = "6" ]; then
        print_ok "GET /hello.txt body length is 6 bytes on $port"
    else
        print_fail "GET /hello.txt body length is 6 bytes on $port" "expected 6 got $body_sz"
    fi
}

test_get_missing_file_404() {
    local port="$1"
    local base="http://$HOST:$port"

    section "4.1.2 GET 不存在文件 -> 404"

    echo "cmd: curl -isS --http1.1 \"$base/nope.txt\" | sed -n '1,30p'"
    local status
    status="$(curl -sS -D /tmp/h_get_nope.$$ "$base/nope.txt" --http1.1 -o /tmp/b_get_nope.$$ 2>/tmp/e_get_nope.$$ -w '%{http_code}' || true)"

    if [ "$status" = "404" ]; then
        print_ok "GET /nope.txt returns 404 on $port"
    else
        print_fail "GET /nope.txt returns 404 on $port" "expected 404 got $status"
        echo "---- headers ----"
        cat /tmp/h_get_nope.$$ 2>/dev/null || true
        echo "---- body ----"
        cat /tmp/b_get_nope.$$ 2>/dev/null || true
        return
    fi

    if grep -iq '^content-length:' /tmp/h_get_nope.$$ || grep -iq '^transfer-encoding:[[:space:]]*chunked' /tmp/h_get_nope.$$; then
        print_ok "404 response has explicit framing on $port"
    else
        print_fail "404 response has explicit framing on $port" "missing both Content-Length and Transfer-Encoding: chunked"
        echo "---- headers ----"
        cat /tmp/h_get_nope.$$ 2>/dev/null || true
    fi
}

test_get_directory_autoindex() {
    local port="$1"
    local base="http://$HOST:$port"

    section "4.1.3 GET 目录无 index"

    rm -f www/dir/index.html

    echo "cmd: curl -isS --http1.1 \"$base/dir/\" | sed -n '1,60p'"
    local status
    status="$(curl -sS -D /tmp/h_dir_auto.$$ "$base/dir/" --http1.1 -o /tmp/b_dir_auto.$$ 2>/tmp/e_dir_auto.$$ -w '%{http_code}' || true)"

    if [ "$status" = "200" ]; then
        print_ok "GET /dir/ without index returns 200 on $port"

        local ct
        ct="$(header_value_from_file /tmp/h_dir_auto.$$ "content-type")"
        if printf '%s\n' "$ct" | grep -Eiq '^text/html'; then
            print_ok "directory listing content-type is text/html on $port"
        else
            print_fail "directory listing content-type is text/html on $port" "got [$ct]"
        fi

        if grep -Eiq '<ul>|<li>|href=' /tmp/b_dir_auto.$$; then
            print_ok "directory listing body looks like autoindex HTML on $port"
        else
            print_fail "directory listing body looks like autoindex HTML on $port" "body does not look like listing HTML"
            echo "---- body ----"
            cat /tmp/b_dir_auto.$$ 2>/dev/null || true
        fi

    elif [ "$status" = "403" ]; then
        print_ok "GET /dir/ without index returns 403 on $port (directory listing disabled)"

        local ct
        ct="$(header_value_from_file /tmp/h_dir_auto.$$ "content-type")"
        if printf '%s\n' "$ct" | grep -Eiq '^(text/plain|text/html)'; then
            print_ok "403 directory response content-type is acceptable on $port"
        else
            print_fail "403 directory response content-type is acceptable on $port" "got [$ct]"
        fi

    else
        print_fail "GET /dir/ without index returns acceptable status on $port" "expected 200 or 403 got $status"
        echo "---- headers ----"
        cat /tmp/h_dir_auto.$$ 2>/dev/null || true
        echo "---- body ----"
        cat /tmp/b_dir_auto.$$ 2>/dev/null || true
        return
    fi
}

test_get_directory_index_file() {
    local port="$1"
    local base="http://$HOST:$port"

    section "4.1.4 GET 目录有 index.html"

    printf "<h1>INDEX</h1>\n" > www/dir/index.html

    echo "cmd: curl -isS --http1.1 \"$base/dir/\" | sed -n '1,40p'"
    local status
    status="$(curl -sS -D /tmp/h_dir_index.$$ "$base/dir/" --http1.1 -o /tmp/b_dir_index.$$ 2>/tmp/e_dir_index.$$ -w '%{http_code}' || true)"

    if [ "$status" = "200" ]; then
        print_ok "GET /dir/ with index returns 200 on $port"
    else
        print_fail "GET /dir/ with index returns 200 on $port" "expected 200 got $status"
        echo "---- headers ----"
        cat /tmp/h_dir_index.$$ 2>/dev/null || true
        echo "---- body ----"
        cat /tmp/b_dir_index.$$ 2>/dev/null || true
        return
    fi

    local ct
    ct="$(header_value_from_file /tmp/h_dir_index.$$ "content-type")"
    if printf '%s\n' "$ct" | grep -Eiq '^text/html'; then
        print_ok "index file content-type is text/html on $port"
    else
        print_fail "index file content-type is text/html on $port" "got [$ct]"
    fi

    if grep -q '<h1>INDEX</h1>' /tmp/b_dir_index.$$; then
        print_ok "index file body is returned on $port"
    else
        print_fail "index file body is returned on $port" "expected <h1>INDEX</h1>"
        echo "---- body ----"
        cat /tmp/b_dir_index.$$ 2>/dev/null || true
    fi
}

test_get_basic_matrix() {
    local port="$1"
    local base="http://$HOST:$port"

    section "4.1.5 GET 基础矩阵"

    local p status
    for p in "/hello.txt" "/nope.txt" "/dir/"; do
        echo "cmd: curl -is --http1.1 \"$base$p\" | sed -n '1,30p'"
        status="$(curl -sS -D /tmp/h_matrix.$$ "$base$p" --http1.1 -o /tmp/b_matrix.$$ 2>/tmp/e_matrix.$$ -w '%{http_code}' || true)"

        case "$p" in
            "/hello.txt")
                if [ "$status" = "200" ]; then
                    print_ok "matrix $p -> 200 on $port"
                else
                    print_fail "matrix $p -> 200 on $port" "got $status"
                fi
                ;;
            "/nope.txt")
                if [ "$status" = "404" ]; then
                    print_ok "matrix $p -> 404 on $port"
                else
                    print_fail "matrix $p -> 404 on $port" "got $status"
                fi
                ;;
            "/dir/")
                if [ "$status" = "200" ] || [ "$status" = "403" ]; then
                    print_ok "matrix $p returns acceptable status $status on $port"
                else
                    print_fail "matrix $p returns acceptable status on $port" "expected 200 or 403 got $status"
                fi
                ;;
        esac

        if grep -iq '^content-length:' /tmp/h_matrix.$$ || grep -iq '^transfer-encoding:[[:space:]]*chunked' /tmp/h_matrix.$$; then
            print_ok "matrix $p has framing header on $port"
        else
            print_fail "matrix $p has framing header on $port" "missing both Content-Length and Transfer-Encoding"
        fi

        if grep -iq '^content-type:' /tmp/h_matrix.$$; then
            print_ok "matrix $p has Content-Type on $port"
        else
            print_fail "matrix $p has Content-Type on $port" "missing Content-Type"
        fi

        if grep -iq '^connection:' /tmp/h_matrix.$$; then
            print_ok "matrix $p has Connection header on $port"
        else
            echo "note: matrix $p has no explicit Connection header on $port"
        fi
    done
}
test_directory_behavior_matrix() {
    local port="$1"
    local base="http://$HOST:$port"

    section "4.1.x 目录请求三种行为矩阵"

    mkdir -p www/emptydir
    rm -f www/emptydir/index.html

    mkdir -p www/upload
    rm -f www/upload/index.html
    printf "HELLO\n" > www/upload/hello.txt

    # 1. 目录存在 + autoindex on -> 200
    echo "cmd: curl -isS --http1.1 \"$base/upload/\" | sed -n '1,60p'"
    local status_auto
    status_auto="$(curl -sS -D /tmp/h_dir_auto_on.$$ "$base/upload/" --http1.1 -o /tmp/b_dir_auto_on.$$ 2>/tmp/e_dir_auto_on.$$ -w '%{http_code}' || true)"

    if [ "$status_auto" = "200" ]; then
        print_ok "directory exists with autoindex enabled returns 200 on $port"
    else
        print_fail "directory exists with autoindex enabled returns 200 on $port" "expected 200 got $status_auto"
        echo "---- headers ----"
        cat /tmp/h_dir_auto_on.$$ 2>/dev/null || true
        echo "---- body ----"
        cat /tmp/b_dir_auto_on.$$ 2>/dev/null || true
    fi

    if grep -iq '^content-length:' /tmp/h_dir_auto_on.$$ || grep -iq '^transfer-encoding:[[:space:]]*chunked' /tmp/h_dir_auto_on.$$; then
        print_ok "autoindex-on directory response has framing header on $port"
    else
        print_fail "autoindex-on directory response has framing header on $port" "missing both Content-Length and Transfer-Encoding"
    fi

    if grep -iq '^content-type:' /tmp/h_dir_auto_on.$$; then
        print_ok "autoindex-on directory response has Content-Type on $port"
    else
        print_fail "autoindex-on directory response has Content-Type on $port" "missing Content-Type"
    fi

    if grep -Eiq '<ul>|<li>|href=' /tmp/b_dir_auto_on.$$; then
        print_ok "autoindex-on directory body looks like listing HTML on $port"
    else
        print_fail "autoindex-on directory body looks like listing HTML on $port" "body does not look like directory listing"
        echo "---- body ----"
        cat /tmp/b_dir_auto_on.$$ 2>/dev/null || true
    fi

    # 2. 目录存在 + no index + autoindex off -> 403
    echo "cmd: curl -isS --http1.1 \"$base/emptydir/\" | sed -n '1,60p'"
    local status_forbidden
    status_forbidden="$(curl -sS -D /tmp/h_dir_forbid.$$ "$base/emptydir/" --http1.1 -o /tmp/b_dir_forbid.$$ 2>/tmp/e_dir_forbid.$$ -w '%{http_code}' || true)"

    if [ "$status_forbidden" = "403" ]; then
        print_ok "directory exists without autoindex returns 403 on $port"
    else
        print_fail "directory exists without autoindex returns 403 on $port" "expected 403 got $status_forbidden"
        echo "---- headers ----"
        cat /tmp/h_dir_forbid.$$ 2>/dev/null || true
        echo "---- body ----"
        cat /tmp/b_dir_forbid.$$ 2>/dev/null || true
    fi

    if grep -iq '^content-length:' /tmp/h_dir_forbid.$$ || grep -iq '^transfer-encoding:[[:space:]]*chunked' /tmp/h_dir_forbid.$$; then
        print_ok "no-autoindex directory response has framing header on $port"
    else
        print_fail "no-autoindex directory response has framing header on $port" "missing both Content-Length and Transfer-Encoding"
    fi

    if grep -iq '^content-type:' /tmp/h_dir_forbid.$$; then
        print_ok "no-autoindex directory response has Content-Type on $port"
    else
        print_fail "no-autoindex directory response has Content-Type on $port" "missing Content-Type"
    fi

    # 3. 目录不存在 -> 404
    echo "cmd: curl -isS --http1.1 \"$base/no_such_dir__/\" | sed -n '1,60p'"
    local status_missing
    status_missing="$(curl -sS -D /tmp/h_dir_missing.$$ "$base/no_such_dir__/" --http1.1 -o /tmp/b_dir_missing.$$ 2>/tmp/e_dir_missing.$$ -w '%{http_code}' || true)"

    if [ "$status_missing" = "404" ]; then
        print_ok "nonexistent directory returns 404 on $port"
    else
        print_fail "nonexistent directory returns 404 on $port" "expected 404 got $status_missing"
        echo "---- headers ----"
        cat /tmp/h_dir_missing.$$ 2>/dev/null || true
        echo "---- body ----"
        cat /tmp/b_dir_missing.$$ 2>/dev/null || true
    fi

    if grep -iq '^content-length:' /tmp/h_dir_missing.$$ || grep -iq '^transfer-encoding:[[:space:]]*chunked' /tmp/h_dir_missing.$$; then
        print_ok "missing directory response has framing header on $port"
    else
        print_fail "missing directory response has framing header on $port" "missing both Content-Length and Transfer-Encoding"
    fi

    if grep -iq '^content-type:' /tmp/h_dir_missing.$$; then
        print_ok "missing directory response has Content-Type on $port"
    else
        print_fail "missing directory response has Content-Type on $port" "missing Content-Type"
    fi
}


test_get_directory_without_trailing_slash() {
    local port="$1"
    local base="http://$HOST:$port"

    section "4.1.6 GET 目录不带尾斜杠 /dir"

    echo "cmd: curl -isS --http1.1 \"$base/dir\" | sed -n '1,20p'"
    local status
    status="$(curl -sS -D /tmp/h_dir_noslash.$$ "$base/dir" --http1.1 -o /tmp/b_dir_noslash.$$ 2>/tmp/e_dir_noslash.$$ -w '%{http_code}' || true)"

    if [ "$status" = "301" ] || [ "$status" = "302" ] || [ "$status" = "307" ] || [ "$status" = "308" ] || [ "$status" = "200" ]; then
        print_ok "GET /dir without trailing slash returns acceptable status on $port"
    else
        print_fail "GET /dir without trailing slash returns acceptable status on $port" "expected redirect or 200, got $status"
        echo "---- headers ----"
        cat /tmp/h_dir_noslash.$$ 2>/dev/null || true
    fi
}

test_get_range_ignored_or_supported() {
    local port="$1"
    local base="http://$HOST:$port"

    section "4.1.7 GET Range 请求"

    mkdir -p www/upload
    printf "HELLO\n" > www/upload/hello.txt

    echo "cmd: curl -isS --http1.1 \"$base/upload/hello.txt\" -H \"Host: localhost\" -H \"Range: bytes=0-2\" | sed -n '1,30p'"
    local status
    status="$(curl -sS -D /tmp/h_range.$$ "$base/upload/hello.txt" \
        --http1.1 \
        -H "Host: localhost" \
        -H "Range: bytes=0-2" \
        -o /tmp/b_range.$$ 2>/tmp/e_range.$$ -w '%{http_code}' || true)"

    if [ "$status" = "206" ]; then
        print_ok "Range request returns 206 on $port"
    elif [ "$status" = "200" ]; then
        print_ok "Range request ignored and returned 200 on $port"
    else
        print_fail "Range request handled acceptably on $port" "expected 206 or 200, got $status"
        echo "---- headers ----"
        cat /tmp/h_range.$$ 2>/dev/null || true
        echo "---- body ----"
        cat /tmp/b_range.$$ 2>/dev/null || true
        echo "---- curl stderr ----"
        cat /tmp/e_range.$$ 2>/dev/null || true
    fi
}

# ---------------- 4. HTTP_Method / 4.2 POST ----------------

test_post_raw_upload_basic() {
    local port="$1"
    local base="http://$HOST:$port"

    section "4.2.1 POST raw body 到 /upload/<filename>"

    clean_upload_file "test.txt"
    clean_upload_file "raw2.txt"

    echo "cmd: curl -isS --http1.1 -X POST --data-binary @www/hello.txt \"$base/upload/test.txt\" | sed -n '1,40p'"
    local status1
    status1="$(curl -sS -D /tmp/h_post_raw1.$$ -X POST --data-binary @www/hello.txt \
        "$base/upload/test.txt" --http1.1 -o /tmp/b_post_raw1.$$ 2>/tmp/e_post_raw1.$$ -w '%{http_code}' || true)"

    if [ "$status1" = "201" ]; then
        print_ok "raw upload /upload/test.txt returns 201 on $port"
    else
        print_fail "raw upload /upload/test.txt returns 201 on $port" "expected 201 got $status1"
        echo "---- headers ----"
        cat /tmp/h_post_raw1.$$ 2>/dev/null || true
        echo "---- body ----"
        cat /tmp/b_post_raw1.$$ 2>/dev/null || true
    fi

    check_upload_content_exact "test.txt" "HELLO"

    echo "cmd: curl -is --http1.1 -X POST --data-binary @www/hello.txt \"$base/upload/raw2.txt\" | sed -n '1,40p'"
    local status2
    status2="$(curl -sS -D /tmp/h_post_raw2.$$ -X POST --data-binary @www/hello.txt \
        "$base/upload/raw2.txt" --http1.1 -o /tmp/b_post_raw2.$$ 2>/tmp/e_post_raw2.$$ -w '%{http_code}' || true)"

    if [ "$status2" = "201" ]; then
        print_ok "raw upload /upload/raw2.txt returns 201 on $port"
    else
        print_fail "raw upload /upload/raw2.txt returns 201 on $port" "expected 201 got $status2"
        echo "---- headers ----"
        cat /tmp/h_post_raw2.$$ 2>/dev/null || true
        echo "---- body ----"
        cat /tmp/b_post_raw2.$$ 2>/dev/null || true
    fi

    if [ -f "$UPLOAD_DIR/raw2.txt" ]; then
        print_ok "raw2.txt exists after upload"
    else
        print_fail "raw2.txt exists after upload" "missing $UPLOAD_DIR/raw2.txt"
    fi

    check_upload_content_exact "raw2.txt" "HELLO"
}

test_post_wrong_path_404() {
    local port="$1"
    local base="http://$HOST:$port"

    section "4.2.2 POST 错误路径 -> 404"

    echo "cmd: curl -i -X POST --data \"abc\" \"$base/foo.txt\""
    local status
    status="$(curl -sS -D /tmp/h_post_wrong.$$ -X POST --data "abc" \
        "$base/foo.txt" --http1.1 -o /tmp/b_post_wrong.$$ 2>/tmp/e_post_wrong.$$ -w '%{http_code}' || true)"

    if [ "$status" = "404" ]; then
        print_ok "POST /foo.txt returns 404 on $port"
    else
        print_fail "POST /foo.txt returns 404 on $port" "expected 404 got $status"
        echo "---- headers ----"
        cat /tmp/h_post_wrong.$$ 2>/dev/null || true
        echo "---- body ----"
        cat /tmp/b_post_wrong.$$ 2>/dev/null || true
    fi
}

test_post_multipart_upload_ok() {
    local port="$1"
    local base="http://$HOST:$port"

    section "4.2.3 multipart/form-data 上传到 /upload"

    clean_upload_file "hello.txt"

    echo "cmd: curl -isS --http1.1 -F \"file=@www/hello.txt\" \"$base/upload\" | sed -n '1,80p'"
    local status
    status="$(curl -sS -D /tmp/h_mp_ok.$$ -F "file=@www/hello.txt" \
        "$base/upload" --http1.1 -o /tmp/b_mp_ok.$$ 2>/tmp/e_mp_ok.$$ -w '%{http_code}' || true)"

    if [ "$status" = "201" ]; then
        print_ok "multipart POST /upload returns 201 on $port"
    else
        print_fail "multipart POST /upload returns 201 on $port" "expected 201 got $status"
        echo "---- headers ----"
        cat /tmp/h_mp_ok.$$ 2>/dev/null || true
        echo "---- body ----"
        cat /tmp/b_mp_ok.$$ 2>/dev/null || true
        return
    fi

    local ct
    ct="$(header_value_from_file /tmp/h_mp_ok.$$ "content-type")"
    if printf '%s\n' "$ct" | grep -Eiq '^text/html'; then
        print_ok "multipart upload response content-type is text/html on $port"
    else
        print_fail "multipart upload response content-type is text/html on $port" "got [$ct]"
    fi

    if grep -Eiq 'Saved as:|Upload OK|hello\.txt' /tmp/b_mp_ok.$$; then
        print_ok "multipart upload response body contains expected text on $port"
    else
        print_fail "multipart upload response body contains expected text on $port" "missing expected HTML text"
        echo "---- body ----"
        cat /tmp/b_mp_ok.$$ 2>/dev/null || true
    fi

    if [ -f "$UPLOAD_DIR/hello.txt" ]; then
        print_ok "multipart uploaded hello.txt exists"
    else
        print_fail "multipart uploaded hello.txt exists" "missing $UPLOAD_DIR/hello.txt"
    fi

    check_upload_content_exact "hello.txt" "HELLO"
}

test_post_multipart_wrong_endpoint_415() {
    local port="$1"
    local base="http://$HOST:$port"

    section "4.2.4 multipart 但路径错误 -> 415"

    echo "cmd: curl -i -F \"file=@www/hello.txt\" \"$base/upload/hello.txt\""
    local status
    status="$(curl -sS -D /tmp/h_mp_bad.$$ -F "file=@www/hello.txt" \
        "$base/upload/hello.txt" --http1.1 -o /tmp/b_mp_bad.$$ 2>/tmp/e_mp_bad.$$ -w '%{http_code}' || true)"

    if [ "$status" = "415" ]; then
        print_ok "multipart POST /upload/hello.txt returns 415 on $port"
    else
        print_fail "multipart POST /upload/hello.txt returns 415 on $port" "expected 415 got $status"
        echo "---- headers ----"
        cat /tmp/h_mp_bad.$$ 2>/dev/null || true
        echo "---- body ----"
        cat /tmp/b_mp_bad.$$ 2>/dev/null || true
    fi
}

test_post_upload_not_multipart_415() {
    local port="$1"
    local base="http://$HOST:$port"

    section "4.2.5 POST /upload 但不是 multipart -> 415"

    echo "cmd: curl -is --http1.1 -X POST --data \"abc\" \"$base/upload\" | sed -n '1,30p'"
    local status
    status="$(curl -sS -D /tmp/h_post_upload415.$$ -X POST --data "abc" \
        "$base/upload" --http1.1 -o /tmp/b_post_upload415.$$ 2>/tmp/e_post_upload415.$$ -w '%{http_code}' || true)"

    if [ "$status" = "415" ]; then
        print_ok "POST /upload non-multipart returns 415 on $port"
    else
        print_fail "POST /upload non-multipart returns 415 on $port" "expected 415 got $status"
        echo "---- headers ----"
        cat /tmp/h_post_upload415.$$ 2>/dev/null || true
        echo "---- body ----"
        cat /tmp/b_post_upload415.$$ 2>/dev/null || true
    fi
}

test_post_missing_length_411_method_group() {
    local port="$1"

    section "4.2.6 没有 CL / 没有 TE 的 POST -> 411"

    expect_status_from_nc \
        "POST / without CL/TE rejected with 411 on $port" \
        "411" \
        "$port" \
        'POST / HTTP/1.1\r\nHost: x\r\n\r\n'

    expect_status_from_nc \
        "POST / without CL/TE and Connection: close rejected with 411 on $port" \
        "411" \
        "$port" \
        'POST / HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n'
}


# ---------------- 4. HTTP_Method / 4.3 DELETE ----------------

test_delete_existing_file() {
    local port="$1"
    local base="http://$HOST:$port"

    section "4.3.1 删除存在文件"

    printf "HELLO\n" > www/hello.txt

    echo "cmd: curl -i -X DELETE \"$base/hello.txt\""
    local status
    status="$(curl -sS -D /tmp/h_del_exist.$$ -X DELETE \
        "$base/hello.txt" --http1.1 -o /tmp/b_del_exist.$$ 2>/tmp/e_del_exist.$$ -w '%{http_code}' || true)"

    if [ "$status" = "200" ] || [ "$status" = "204" ]; then
        print_ok "DELETE /hello.txt returns 200/204 on $port"
    else
        print_fail "DELETE /hello.txt returns 200/204 on $port" "expected 200 or 204 got $status"
        echo "---- headers ----"
        cat /tmp/h_del_exist.$$ 2>/dev/null || true
        echo "---- body ----"
        cat /tmp/b_del_exist.$$ 2>/dev/null || true
    fi

    if [ ! -f "www/hello.txt" ]; then
        print_ok "www/hello.txt deleted"
    else
        print_fail "www/hello.txt deleted" "file still exists"
    fi
}

test_delete_existing_file_copy() {
    local port="$1"
    local base="http://$HOST:$port"

    section "4.3.1b 删除存在文件（复制版）"

    if [ ! -f "$UPLOAD_DIR/raw2.txt" ]; then
        printf "HELLO\n" > "$UPLOAD_DIR/raw2.txt"
    fi
    cp "$UPLOAD_DIR/raw2.txt" "www/todel.txt"

    local status
    status="$(curl -sS -D /tmp/h_del_todel.$$ -X DELETE \
        "$base/todel.txt" --http1.1 -o /tmp/b_del_todel.$$ 2>/tmp/e_del_todel.$$ -w '%{http_code}' || true)"

    if [ "$status" = "200" ] || [ "$status" = "204" ]; then
        print_ok "DELETE /todel.txt returns 200/204 on $port"
    else
        print_fail "DELETE /todel.txt returns 200/204 on $port" "expected 200 or 204 got $status"
        echo "---- headers ----"
        cat /tmp/h_del_todel.$$ 2>/dev/null || true
        echo "---- body ----"
        cat /tmp/b_del_todel.$$ 2>/dev/null || true
    fi

    if [ ! -f "www/todel.txt" ]; then
        print_ok "www/todel.txt deleted"
    else
        print_fail "www/todel.txt deleted" "file still exists"
    fi
}

test_delete_missing_file_404() {
    local port="$1"
    local base="http://$HOST:$port"

    section "4.3.2 删除不存在文件 -> 404"

    local status
    status="$(curl -sS -D /tmp/h_del_missing.$$ -X DELETE \
        "$base/no_such_file.txt" --http1.1 -o /tmp/b_del_missing.$$ 2>/tmp/e_del_missing.$$ -w '%{http_code}' || true)"

    if [ "$status" = "404" ]; then
        print_ok "DELETE /no_such_file.txt returns 404 on $port"
    else
        print_fail "DELETE /no_such_file.txt returns 404 on $port" "expected 404 got $status"
        echo "---- headers ----"
        cat /tmp/h_del_missing.$$ 2>/dev/null || true
        echo "---- body ----"
        cat /tmp/b_del_missing.$$ 2>/dev/null || true
    fi
}

test_delete_directory_rejected() {
    local port="$1"
    local base="http://$HOST:$port"

    section "4.3.3 删除目录应被拒绝"

    mkdir -p www/dir
    touch www/dir/a.txt

    local status1
    status1="$(curl -sS -D /tmp/h_del_dir1.$$ -X DELETE \
        "$base/dir/" --http1.1 -o /tmp/b_del_dir1.$$ 2>/tmp/e_del_dir1.$$ -w '%{http_code}' || true)"

    if [ "$status1" = "405" ] || [ "$status1" = "403" ]; then
        print_ok "DELETE /dir/ returns rejecting status on $port"
    else
        print_fail "DELETE /dir/ returns rejecting status on $port" "expected 405 or 403 got $status1"
        echo "---- headers ----"
        cat /tmp/h_del_dir1.$$ 2>/dev/null || true
        echo "---- body ----"
        cat /tmp/b_del_dir1.$$ 2>/dev/null || true
    fi

    local status2
    status2="$(curl -sS -D /tmp/h_del_dir2.$$ -X DELETE \
        "$base/dir" --http1.1 -o /tmp/b_del_dir2.$$ 2>/tmp/e_del_dir2.$$ -w '%{http_code}' || true)"

    if [ "$status2" = "405" ] || [ "$status2" = "403" ] || [ "$status2" = "301" ] || [ "$status2" = "302" ] || [ "$status2" = "307" ] || [ "$status2" = "308" ]; then
        print_ok "DELETE /dir returns acceptable status on $port"
    else
        print_fail "DELETE /dir returns acceptable status on $port" "expected 405/403/redirect got $status2"
        echo "---- headers ----"
        cat /tmp/h_del_dir2.$$ 2>/dev/null || true
        echo "---- body ----"
        cat /tmp/b_del_dir2.$$ 2>/dev/null || true
    fi
}

test_delete_permission_denied_403() {
    local port="$1"
    local base="http://$HOST:$port"

    section "4.3.4 DELETE 没权限的文件 -> 403"

    mkdir -p "$UPLOAD_DIR"
    printf "hi\n" > "$UPLOAD_DIR/protect2.txt"
    chmod u-w "$UPLOAD_DIR"

    local status
    status="$(curl -sS -D /tmp/h_del_protect.$$ -X DELETE \
        "$base/upload/protect2.txt" -H "Host: localhost" --http1.1 \
        -o /tmp/b_del_protect.$$ 2>/tmp/e_del_protect.$$ -w '%{http_code}' || true)"

    chmod u+w "$UPLOAD_DIR"

    if [ "$status" = "403" ] || [ "$status" = "405" ]; then
        print_ok "DELETE permission-protected file returns rejecting status on $port"
    else
        print_fail "DELETE permission-protected file returns rejecting status on $port" "expected 403 or 405 got $status"
        echo "---- headers ----"
        cat /tmp/h_del_protect.$$ 2>/dev/null || true
        echo "---- body ----"
        cat /tmp/b_del_protect.$$ 2>/dev/null || true
    fi
}

test_delete_directory_allow_header() {
    local port="$1"
    local base="http://$HOST:$port"

    section "4.3.5 DELETE 目录 405 时应包含 Allow"

    local status
    status="$(curl -sS -D /tmp/h_del_allow.$$ -X DELETE \
        "$base/dir/" -H "Host: localhost" --http1.1 \
        -o /tmp/b_del_allow.$$ 2>/tmp/e_del_allow.$$ -w '%{http_code}' || true)"

    if [ "$status" = "405" ]; then
        print_ok "DELETE /dir/ returns 405 on $port"
        expect_header_present_in_curl_response \
            "DELETE /dir/ 405 contains Allow header on $port" \
            /tmp/h_del_allow.$$ \
            "allow"
    else
        print_skip "DELETE /dir/ 405 Allow check on $port" "server returned $status instead of 405"
    fi
}

test_delete_global_ok() {
    local port="$1"
    local base="http://$HOST:$port"

    section "4.3.6 DELETE 正常删除 upload 文件"

    chmod u+w "$UPLOAD_DIR" 2>/dev/null || true
    printf "hi\n" > "$UPLOAD_DIR/okdel.txt"

    local status
    status="$(curl -sS -D /tmp/h_del_ok.$$ -X DELETE \
        "$base/upload/okdel.txt" -H "Host: localhost" --http1.1 \
        -o /tmp/b_del_ok.$$ 2>/tmp/e_del_ok.$$ -w '%{http_code}' || true)"

    if [ "$status" = "200" ] || [ "$status" = "204" ]; then
        print_ok "DELETE /upload/okdel.txt returns 200/204 on $port"
    else
        print_fail "DELETE /upload/okdel.txt returns 200/204 on $port" "expected 200 or 204 got $status"
        echo "---- headers ----"
        cat /tmp/h_del_ok.$$ 2>/dev/null || true
        echo "---- body ----"
        cat /tmp/b_del_ok.$$ 2>/dev/null || true
    fi

    if [ ! -f "$UPLOAD_DIR/okdel.txt" ]; then
        print_ok "okdel.txt deleted from upload dir"
    else
        print_fail "okdel.txt deleted from upload dir" "file still exists"
    fi
}

# ---------------- CGI env / body / URI ----------------

test_cgi_env_basic() {
    local port="$1"
    local base="http://$HOST:$port"

    section "CGI env 基本变量"

    echo "cmd: curl -v \"$base/cgi-bin/test_env.sh\""
    local status
    status="$(curl -sS -D /tmp/h_cgi_env.$$ "$base/cgi-bin/test_env.sh" --http1.1 -o /tmp/b_cgi_env.$$ 2>/tmp/e_cgi_env.$$ -w '%{http_code}' || true)"

    if [ "$status" = "200" ]; then
        print_ok "CGI env GET returns 200 on $port"
    else
        print_fail "CGI env GET returns 200 on $port" "expected 200 got $status"
        echo "---- headers ----"
        cat /tmp/h_cgi_env.$$ 2>/dev/null || true
        echo "---- body ----"
        cat /tmp/b_cgi_env.$$ 2>/dev/null || true
        return
    fi

    grep -q '^REQUEST_METHOD=GET$' /tmp/b_cgi_env.$$ \
        && print_ok "CGI env REQUEST_METHOD=GET on $port" \
        || { print_fail "CGI env REQUEST_METHOD=GET on $port" "missing REQUEST_METHOD=GET"; cat /tmp/b_cgi_env.$$; }

    grep -q '^SCRIPT_NAME=/cgi-bin/test_env.sh$' /tmp/b_cgi_env.$$ \
        && print_ok "CGI env SCRIPT_NAME set on $port" \
        || { print_fail "CGI env SCRIPT_NAME set on $port" "missing SCRIPT_NAME=/cgi-bin/test_env.sh"; cat /tmp/b_cgi_env.$$; }

    grep -q '^QUERY_STRING=$' /tmp/b_cgi_env.$$ \
        && print_ok "CGI env empty QUERY_STRING on $port" \
        || { print_fail "CGI env empty QUERY_STRING on $port" "QUERY_STRING not empty"; cat /tmp/b_cgi_env.$$; }

    grep -q '^PATH_INFO=$' /tmp/b_cgi_env.$$ \
        && print_ok "CGI env empty PATH_INFO on $port" \
        || { print_fail "CGI env empty PATH_INFO on $port" "PATH_INFO not empty"; cat /tmp/b_cgi_env.$$; }
}

test_cgi_echo_body_post() {
    local port="$1"
    local base="http://$HOST:$port"

    section "CGI body POST"

    echo "cmd: curl -v -X POST \"$base/cgi-bin/echo_body.sh\" --data \"hello=world\""
    local status
    status="$(curl -sS -D /tmp/h_cgi_body.$$ -X POST "$base/cgi-bin/echo_body.sh" \
        --data "hello=world" --http1.1 -o /tmp/b_cgi_body.$$ 2>/tmp/e_cgi_body.$$ -w '%{http_code}' || true)"

    if [ "$status" = "200" ]; then
        print_ok "CGI POST body returns 200 on $port"
    else
        print_fail "CGI POST body returns 200 on $port" "expected 200 got $status"
        echo "---- headers ----"
        cat /tmp/h_cgi_body.$$ 2>/dev/null || true
        echo "---- body ----"
        cat /tmp/b_cgi_body.$$ 2>/dev/null || true
        return
    fi

    local body
    body="$(cat /tmp/b_cgi_body.$$ 2>/dev/null || true)"
    if [ "$body" = "hello=world" ]; then
        print_ok "CGI POST body echoed correctly on $port"
    else
        print_fail "CGI POST body echoed correctly on $port" "expected [hello=world] got [$body]"
    fi
}

test_cgi_uri_query_basic() {
    local port="$1"
    local base="http://$HOST:$port"

    section "CGI URI / query string"

    echo "cmd: curl -v \"$base/cgi-bin/test_env.sh?x=1&y=2\""
    local status1
    status1="$(curl -sS -D /tmp/h_cgi_q1.$$ "$base/cgi-bin/test_env.sh?x=1&y=2" --http1.1 -o /tmp/b_cgi_q1.$$ 2>/tmp/e_cgi_q1.$$ -w '%{http_code}' || true)"

    if [ "$status1" = "200" ]; then
        print_ok "CGI query x=1&y=2 returns 200 on $port"
    else
        print_fail "CGI query x=1&y=2 returns 200 on $port" "expected 200 got $status1"
        echo "---- body ----"
        cat /tmp/b_cgi_q1.$$ 2>/dev/null || true
    fi

    grep -q '^QUERY_STRING=x=1&y=2$' /tmp/b_cgi_q1.$$ \
        && print_ok "CGI QUERY_STRING preserves simple query on $port" \
        || { print_fail "CGI QUERY_STRING preserves simple query on $port" "unexpected QUERY_STRING"; cat /tmp/b_cgi_q1.$$; }

    echo "cmd: curl -v \"$base/cgi-bin/test_env.sh?x=1%202&y=%E4%B8%AD\""
    local status2
    status2="$(curl -sS -D /tmp/h_cgi_q2.$$ "$base/cgi-bin/test_env.sh?x=1%202&y=%E4%B8%AD" --http1.1 -o /tmp/b_cgi_q2.$$ 2>/tmp/e_cgi_q2.$$ -w '%{http_code}' || true)"

    if [ "$status2" = "200" ]; then
        print_ok "CGI encoded query returns 200 on $port"
    else
        print_fail "CGI encoded query returns 200 on $port" "expected 200 got $status2"
        echo "---- body ----"
        cat /tmp/b_cgi_q2.$$ 2>/dev/null || true
    fi

    grep -q '^QUERY_STRING=x=1%202&y=%E4%B8%AD$' /tmp/b_cgi_q2.$$ \
        && print_ok "CGI QUERY_STRING keeps raw encoded bytes on $port" \
        || { print_fail "CGI QUERY_STRING keeps raw encoded bytes on $port" "unexpected QUERY_STRING"; cat /tmp/b_cgi_q2.$$; }

    echo "cmd: curl -v \"$base/cgi-bin/test_env.sh?=\""
    local status3
    status3="$(curl -sS -D /tmp/h_cgi_q3.$$ "$base/cgi-bin/test_env.sh?=" --http1.1 -o /tmp/b_cgi_q3.$$ 2>/tmp/e_cgi_q3.$$ -w '%{http_code}' || true)"

    if [ "$status3" = "200" ]; then
        print_ok "CGI query '?=' returns 200 on $port"
    else
        print_fail "CGI query '?=' returns 200 on $port" "expected 200 got $status3"
        echo "---- body ----"
        cat /tmp/b_cgi_q3.$$ 2>/dev/null || true
    fi

    grep -q '^QUERY_STRING==$' /tmp/b_cgi_q3.$$ \
        && print_ok "CGI QUERY_STRING handles '?=' on $port" \
        || { print_fail "CGI QUERY_STRING handles '?=' on $port" "unexpected QUERY_STRING"; cat /tmp/b_cgi_q3.$$; }
}

test_cgi_path_info() {
    local port="$1"
    local base="http://$HOST:$port"

    section "CGI PATH_INFO"

    echo "cmd: curl -v --path-as-is \"$base/cgi-bin/test_env.sh/aaa/bbb?x=1\""
    local status
    status="$(curl -sS -D /tmp/h_cgi_pi.$$ --path-as-is "$base/cgi-bin/test_env.sh/aaa/bbb?x=1" --http1.1 -o /tmp/b_cgi_pi.$$ 2>/tmp/e_cgi_pi.$$ -w '%{http_code}' || true)"

    if [ "$status" = "200" ]; then
        print_ok "CGI PATH_INFO request returns 200 on $port"
    else
        print_fail "CGI PATH_INFO request returns 200 on $port" "expected 200 got $status"
        echo "---- headers ----"
        cat /tmp/h_cgi_pi.$$ 2>/dev/null || true
        echo "---- body ----"
        cat /tmp/b_cgi_pi.$$ 2>/dev/null || true
        return
    fi

    grep -q '^SCRIPT_NAME=/cgi-bin/test_env.sh$' /tmp/b_cgi_pi.$$ \
        && print_ok "CGI SCRIPT_NAME preserved with PATH_INFO on $port" \
        || { print_fail "CGI SCRIPT_NAME preserved with PATH_INFO on $port" "unexpected SCRIPT_NAME"; cat /tmp/b_cgi_pi.$$; }

    grep -q '^PATH_INFO=/aaa/bbb$' /tmp/b_cgi_pi.$$ \
        && print_ok "CGI PATH_INFO extracted correctly on $port" \
        || { print_fail "CGI PATH_INFO extracted correctly on $port" "unexpected PATH_INFO"; cat /tmp/b_cgi_pi.$$; }

    grep -q '^QUERY_STRING=x=1$' /tmp/b_cgi_pi.$$ \
        && print_ok "CGI QUERY_STRING preserved with PATH_INFO on $port" \
        || { print_fail "CGI QUERY_STRING preserved with PATH_INFO on $port" "unexpected QUERY_STRING"; cat /tmp/b_cgi_pi.$$; }
}

test_cgi_nonexistent_with_path_info() {
    local port="$1"
    local base="http://$HOST:$port"

    section "CGI 不存在脚本 + PATH_INFO"

    echo "cmd: curl -v --path-as-is \"$base/cgi-bin/nope.sh/aaa?x=1\""
    local status
    status="$(curl -sS -D /tmp/h_cgi_nope.$$ --path-as-is "$base/cgi-bin/nope.sh/aaa?x=1" --http1.1 -o /tmp/b_cgi_nope.$$ 2>/tmp/e_cgi_nope.$$ -w '%{http_code}' || true)"

    if [ "$status" = "404" ]; then
        print_ok "nonexistent CGI script with PATH_INFO returns 404 on $port"
    else
        print_fail "nonexistent CGI script with PATH_INFO returns 404 on $port" "expected 404 got $status"
        echo "---- headers ----"
        cat /tmp/h_cgi_nope.$$ 2>/dev/null || true
        echo "---- body ----"
        cat /tmp/b_cgi_nope.$$ 2>/dev/null || true
    fi
}

test_cgi_empty_post_behavior() {
    local port="$1"

    section "CGI empty POST 行为"

    expect_status_from_nc \
        "CGI POST without length info returns 411 on $port" \
        "411" \
        "$port" \
        'POST /cgi-bin/echo_body.sh HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n'

    expect_status_from_nc \
        "CGI POST with Content-Length: 0 returns 200 on $port" \
        "200" \
        "$port" \
        'POST /cgi-bin/echo_body.sh HTTP/1.1\r\nHost: localhost\r\nContent-Length: 0\r\nConnection: close\r\n\r\n'
}

run_for_port() {
    local port="$1"

    if [ "$START_SERVER" = "1" ]; then
        SERVER_PID=""
        start_server "$port"
    else
        section "using existing server on port $port"
    fi

    test_listen "$port"

    prepare_global_fixtures
    
    section "3.1 请求行（Request Line）"
    test_valid_get "$port"
    test_keep_alive_two_requests "$port"
    test_http_10_505 "$port"
    test_method_405 "$port"
    test_request_line_extra_token "$port"
    test_request_line_too_few_tokens "$port"
    test_empty_method_path_version "$port"
    test_illegal_method_chars "$port"
    test_absolute_uri_no_crash "$port"

    section "3.2 Host 头相关"
    test_missing_host "$port"
    test_host_with_port "$port"
    test_duplicate_host_case_insensitive "$port"
    test_empty_host_value "$port"
    test_blank_host_value "$port"
    test_duplicate_host_same_value "$port"
    test_comma_separated_host "$port"
    test_ipv6_host_with_port "$port"

    section "3.3 URI"
    test_absolute_form_host_match "$port"
    test_absolute_form_host_mismatch "$port"
    test_uri_too_long "$port"
    test_invalid_uri_chars "$port"
    test_path_dotdot_forbidden "$port"
    test_percent_encoded_dotdot_no_crash "$port"
    test_percent20_behavior "$port"
    test_query_path_split "$port"

    section "3.4 Connection / Keep-Alive"
    test_connection_close_keepalive_combo "$port"
    test_connection_close_case_space "$port"
    test_connection_only_close "$port"
    test_connection_unknown_tokens "$port"
    test_connection_spaces_commas "$port"

    section "3.5 Header 语法与大小限制"
    test_header_too_large_431 "$port"
    test_header_no_colon "$port"
    test_header_empty_value_allowed "$port"
    test_header_invalid_name "$port"
    test_header_empty_key "$port"
    # test_bare_lf_only "$port"

    section "3.6 Content-Length / Transfer-Encoding"
    test_post_missing_length_411 "$port"
    test_duplicate_content_length_400 "$port"
    test_content_length_too_large_413 "$port"
    test_transfer_encoding_not_chunked_501 "$port"
    test_duplicate_transfer_encoding_400 "$port"
    test_te_and_cl_together_400 "$port"
    test_chunked_normal_upload "$port"
    test_chunk_hex_size "$port"
    test_chunk_extension_supported "$port"
    test_chunk_incomplete_and_badsize "$port"
    test_transfer_encoding_mixed_tokens "$port"
    test_content_length_nondigit_400 "$port"
    test_duplicate_content_type_400 "$port"

    section "3.7 Body"
    test_fixed_body_incremental "$port"
    test_chunk_extension_supported_body "$port"
    test_chunked_incomplete_waits "$port"
    test_lf_only_stuck_body "$port"
    test_te_chunked_plus_cl_body "$port"
    test_chunk_size_mismatch "$port"
    test_chunk_missing_crlf_after_data "$port"
    test_chunk_data_contains_crlf "$port"
    test_chunk_fake_boundary_text_in_data "$port"

    section "3.8 响应构造"
    test_error_405_has_allow "$port"
    test_responsebuilder_content_length "$port"
    test_no_hanging_without_length "$port"
    test_empty_body_response_content_length_zero "$port"
    test_error_content_type "$port"

    section "3.9 Redirect"
    prepare_redirect_fixtures
    test_redirect_301_absolute "$port"
    test_redirect_302_relative "$port"
    test_redirect_follow_relative "$port"
    test_redirect_post_behavior "$port"
    test_redirect_delete_behavior "$port"
    
    section "4. HTTP_Method / 4.1 GET"
    prepare_http_method_get_fixtures
    test_get_existing_file "$port"
    test_get_missing_file_404 "$port"
    test_get_directory_autoindex "$port"
    test_get_directory_index_file "$port"
    test_get_basic_matrix "$port"
    test_directory_behavior_matrix "$port"
    test_get_directory_without_trailing_slash "$port"
    test_get_range_ignored_or_supported "$port"

    section "4. HTTP_Method / 4.2 POST / 4.3 DELETE"
    prepare_http_method_post_delete_fixtures
    test_post_raw_upload_basic "$port"
    test_post_wrong_path_404 "$port"
    test_post_multipart_upload_ok "$port"
    test_post_multipart_wrong_endpoint_415 "$port"
    test_post_upload_not_multipart_415 "$port"
    test_post_missing_length_411_method_group "$port"

    test_delete_existing_file "$port"
    test_delete_existing_file_copy "$port"
    test_delete_missing_file_404 "$port"
    test_delete_directory_rejected "$port"
    test_delete_permission_denied_403 "$port"
    test_delete_directory_allow_header "$port"
    test_delete_global_ok "$port"

    section "CGI env / body / URI"
    prepare_cgi_env_fixtures
    test_cgi_env_basic "$port"
    test_cgi_echo_body_post "$port"
    test_cgi_empty_post_behavior "$port"
    test_cgi_uri_query_basic "$port"
    test_cgi_path_info "$port"
    test_cgi_nonexistent_with_path_info "$port"

    if [ -n "${SERVER_PID:-}" ]; then
        kill "$SERVER_PID" >/dev/null 2>&1 || true
        wait "$SERVER_PID" 2>/dev/null || true
        SERVER_PID=""
    fi
}

main() {
    check_tools

    echo -e "${BOLD}HTTP parser phase-1 black-box tests${NC}"
    echo "HOST=$HOST"
    echo "PORTS=${PORTS[*]}"
    echo "SERVER_BIN=$SERVER_BIN"
    echo "SERVER_ARGS=$SERVER_ARGS"
    echo "HOST_HEADER=$HOST_HEADER"
    echo "START_SERVER=$START_SERVER"
    echo "UPLOAD_DIR=$UPLOAD_DIR"

    local port
    for port in "${PORTS[@]}"; do
        run_for_port "$port"
    done

    echo
    echo -e "${BOLD}==== Summary ====${NC}"
    echo -e "${GREEN}ok${NC}:   $PASS"
    echo -e "${RED}fail${NC}: $FAIL"
    echo -e "${YELLOW}skip${NC}: $SKIP"

    if [ "$FAIL" -eq 0 ]; then
        exit 0
    else
        exit 1
    fi
}

main "$@"