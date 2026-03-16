#!/bin/bash

# ============================================================
# CGI & Upload Test Suite
# Usage: ./cgi_test.sh [host] [port] [cgi_dir] [upload_dir] [www_root]
# Example: ./cgi_test.sh localhost 8080 ./www/cgi-bin ./www/upload ./www
# ============================================================

HOST="${1:-localhost}"
PORT="${2:-8080}"
CGI_DIR="${3:-./www/cgi-bin}"
UPLOAD_DIR="${4:-./www/upload}"
WWW_ROOT="${5:-./www}"
BASE="http://$HOST:$PORT"

PASS=0
FAIL=0
SKIP=0

# ============================================================
# Helpers
# ============================================================

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m'

section() { echo -e "\n${CYAN}${BOLD}━━━ $1 ━━━${NC}"; }

check_code() {
    local desc="$1"
    local expected="$2"
    local actual="$3"
    if [ "$actual" = "$expected" ]; then
        echo -e "  ${GREEN}PASS${NC} $desc (got $actual)"
        PASS=$((PASS+1))
    else
        echo -e "  ${RED}FAIL${NC} $desc (expected $expected, got $actual)"
        FAIL=$((FAIL+1))
    fi
}

check_body() {
    local desc="$1"
    local pattern="$2"
    local body="$3"
    if echo "$body" | grep -q "$pattern"; then
        echo -e "  ${GREEN}PASS${NC} $desc"
        PASS=$((PASS+1))
    else
        echo -e "  ${RED}FAIL${NC} $desc (pattern '$pattern' not found)"
        echo -e "       body: $(echo "$body" | head -3)"
        FAIL=$((FAIL+1))
    fi
}

skip() {
    echo -e "  ${YELLOW}SKIP${NC} $1 ($2)"
    SKIP=$((SKIP+1))
}

http_code() { curl -s -o /dev/null -w '%{http_code}' "$@"; }
http_body() { curl -s "$@"; }

# ============================================================
# Setup: create test scripts if missing
# ============================================================

setup_scripts() {
    section "Setup: creating test scripts"

    mkdir -p "$CGI_DIR"
    mkdir -p "$UPLOAD_DIR"

    # --- test_env.sh ---
    if [ ! -f "$CGI_DIR/test_env.sh" ]; then
        cat > "$CGI_DIR/test_env.sh" << 'SCRIPT'
#!/bin/bash
echo "Content-type: text/plain"
echo ""
echo "REQUEST_METHOD=$REQUEST_METHOD"
echo "QUERY_STRING=$QUERY_STRING"
echo "CONTENT_TYPE=$CONTENT_TYPE"
echo "CONTENT_LENGTH=$CONTENT_LENGTH"
echo "SCRIPT_NAME=$SCRIPT_NAME"
echo "PATH_INFO=$PATH_INFO"
echo "SERVER_PORT=$SERVER_PORT"
echo "REMOTE_ADDR=$REMOTE_ADDR"
if [ "$REQUEST_METHOD" = "POST" ] && [ -n "$CONTENT_LENGTH" ] && [ "$CONTENT_LENGTH" -gt 0 ]; then
    echo "BODY=$(dd bs=1 count=$CONTENT_LENGTH 2>/dev/null)"
fi
SCRIPT
        chmod +x "$CGI_DIR/test_env.sh"
        echo "  created test_env.sh"
    fi

    # --- test_query.sh ---
    if [ ! -f "$CGI_DIR/test_query.sh" ]; then
        cat > "$CGI_DIR/test_query.sh" << 'SCRIPT'
#!/bin/bash
echo "Content-type: text/plain"
echo ""
echo "QUERY=$QUERY_STRING"
SCRIPT
        chmod +x "$CGI_DIR/test_query.sh"
        echo "  created test_query.sh"
    fi

    # --- test_slow.sh (timeout test) ---
    if [ ! -f "$CGI_DIR/test_slow.sh" ]; then
        cat > "$CGI_DIR/test_slow.sh" << 'SCRIPT'
#!/bin/bash
sleep 30
echo "Content-type: text/plain"
echo ""
echo "should not reach here"
SCRIPT
        chmod +x "$CGI_DIR/test_slow.sh"
        echo "  created test_slow.sh"
    fi

    # --- test_crash.sh ---
    if [ ! -f "$CGI_DIR/test_crash.sh" ]; then
        cat > "$CGI_DIR/test_crash.sh" << 'SCRIPT'
#!/bin/bash
exit 1
SCRIPT
        chmod +x "$CGI_DIR/test_crash.sh"
        echo "  created test_crash.sh"
    fi

    # --- test_big.sh ---
    if [ ! -f "$CGI_DIR/test_big.sh" ]; then
        cat > "$CGI_DIR/test_big.sh" << 'SCRIPT'
#!/bin/bash
echo "Content-type: text/plain"
echo ""
dd if=/dev/urandom bs=524 count=64 2>/dev/null | base64
SCRIPT
        chmod +x "$CGI_DIR/test_big.sh"
        echo "  created test_big.sh"
    fi

    echo "  done."
}

# ============================================================
# 1. Basic GET
# ============================================================

test_basic_get() {
    section "1. Basic GET"

    code=$(http_code "$BASE/cgi-bin/test_env.sh")
    check_code "GET valid .sh script" "200" "$code"

    body=$(http_body "$BASE/cgi-bin/test_env.sh")
    check_body "body contains REQUEST_METHOD=GET" "REQUEST_METHOD=GET" "$body"
    check_body "body contains SERVER_PORT" "SERVER_PORT=$PORT" "$body"
    check_body "body contains REMOTE_ADDR" "REMOTE_ADDR=" "$body"
}

# ============================================================
# 2. Query string
# ============================================================

test_query_string() {
    section "2. Query String"

    body=$(http_body "$BASE/cgi-bin/test_env.sh?foo=bar&baz=qux")
    check_body "QUERY_STRING=foo=bar&baz=qux" "QUERY_STRING=foo=bar" "$body"

    body=$(http_body "$BASE/cgi-bin/test_query.sh?hello=world")
    check_body "query script echoes QUERY" "QUERY=hello=world" "$body"
}

# ============================================================
# 3. POST with body
# ============================================================

test_post() {
    section "3. POST"

    # POST form data
    code=$(http_code -X POST \
        -H "Content-Type: application/x-www-form-urlencoded" \
        -d "name=hello&value=world" \
        "$BASE/cgi-bin/test_env.sh")
    check_code "POST returns 200" "200" "$code"

    body=$(http_body -X POST \
        -H "Content-Type: application/x-www-form-urlencoded" \
        -d "name=hello&value=world" \
        "$BASE/cgi-bin/test_env.sh?foo=bar")
    check_body "POST: REQUEST_METHOD=POST" "REQUEST_METHOD=POST" "$body"
    check_body "POST: CONTENT_TYPE set" "CONTENT_TYPE=application/x-www-form-urlencoded" "$body"
    check_body "POST: CONTENT_LENGTH=22" "CONTENT_LENGTH=22" "$body"
    check_body "POST: QUERY_STRING=foo=bar" "QUERY_STRING=foo=bar" "$body"
    check_body "POST: body received" "BODY=name=hello" "$body"

    # POST no body
    code=$(http_code -X POST "$BASE/cgi-bin/test_env.sh")
    check_code "POST with no body returns 411" "411" "$code"
}

# ============================================================
# 4. PATH_INFO
# ============================================================

test_path_info() {
    section "4. PATH_INFO"

    body=$(http_body "$BASE/cgi-bin/test_env.sh/extra/path?x=1")
    check_body "SCRIPT_NAME correct" "SCRIPT_NAME=/cgi-bin/test_env.sh" "$body"
    check_body "PATH_INFO=/extra/path" "PATH_INFO=/extra/path" "$body"
    check_body "QUERY_STRING=x=1" "QUERY_STRING=x=1" "$body"
}

# ============================================================
# 5. Error cases
# ============================================================

test_errors() {
    section "5. Error Cases"

    # forbidden extension
    code=$(http_code "$BASE/cgi-bin/test_env.php")
    check_code "unsupported extension .php returns 403" "403" "$code"

    code=$(http_code "$BASE/cgi-bin/test_env.rb")
    check_code "unsupported extension .rb returns 403" "403" "$code"

    # file not found
    code=$(http_code "$BASE/cgi-bin/nonexistent.sh")
    check_code "nonexistent script returns 404" "404" "$code"

    # no execute permission
    if [ -f "$CGI_DIR/test_env.sh" ]; then
        chmod -x "$CGI_DIR/test_env.sh"
        code=$(http_code "$BASE/cgi-bin/test_env.sh")
        check_code "no execute permission returns 403" "403" "$code"
        chmod +x "$CGI_DIR/test_env.sh"
    else
        skip "no-exec permission test" "test_env.sh not found"
    fi

    # script crashes
    code=$(http_code "$BASE/cgi-bin/test_crash.sh")
    check_code "crashing script returns 500" "500" "$code"
}

# ============================================================
# 6. Timeout
# ============================================================

test_timeout() {
    section "6. Timeout (slow script, ~5s wait)"

    code=$(curl -s -o /dev/null -w '%{http_code}' \
        --max-time 15 \
        "$BASE/cgi-bin/test_slow.sh")
    check_code "slow script returns 504" "504" "$code"
}

# ============================================================
# 7. Large output
# ============================================================

test_large_output() {
    section "7. Large Output"

    bytes=$(http_body "$BASE/cgi-bin/test_big.sh" | wc -c)
    if [ "$bytes" -gt 5000 ]; then
        echo -e "  ${GREEN}PASS${NC} large output received ($bytes bytes)"
        PASS=$((PASS+1))
    else
        echo -e "  ${RED}FAIL${NC} large output too small ($bytes bytes)"
        FAIL=$((FAIL+1))
    fi
}

# ============================================================
# 8. Concurrent requests
# ============================================================

test_concurrent() {
    section "8. Concurrent Requests (10 parallel)"

    # 直接在终端跑，不经过测试脚本
    tmpdir=$(mktemp -d)
    for i in $(seq 1 10); do
        curl -s -o /dev/null -w '%{http_code}\n' \
            --max-time 10 \
            http://localhost:8080/cgi-bin/test_env.sh > "$tmpdir/$i" &
    done
    wait
    echo "Results:"
    for i in $(seq 1 10); do
        echo "  [$i] $(cat $tmpdir/$i 2>/dev/null || echo 'NO OUTPUT')"
    done
    rm -rf "$tmpdir"
}

# ============================================================
# 9. Keep-alive reuse
# ============================================================

test_keepalive() {
    section "9. Keep-Alive Connection Reuse"

    results=$(curl -s -o /dev/null -w '%{http_code}\n' \
        --http1.1 \
        "$BASE/cgi-bin/test_env.sh" \
        "$BASE/cgi-bin/test_env.sh" \
        "$BASE/cgi-bin/test_env.sh")

    count=$(echo "$results" | grep -c "^200$")
    if [ "$count" -eq 3 ]; then
        echo -e "  ${GREEN}PASS${NC} 3/3 requests on same connection returned 200"
        PASS=$((PASS+1))
    else
        echo -e "  ${RED}FAIL${NC} only $count/3 requests returned 200"
        FAIL=$((FAIL+1))
    fi
}

# ============================================================
# 10. Upload tests
# ============================================================

test_upload() {
    section "10. Upload"

    local test_file=$(mktemp)
    echo "upload test content $(date)" > "$test_file"
    local filename="testupload_$(date +%s).txt"

    # POST upload
    code=$(http_code -X POST \
        -H "Content-Type: text/plain" \
        --data-binary @"$test_file" \
        "$BASE/upload/$filename")
    check_code "POST upload returns 201" "201" "$code"

    # check file on disk
    if [ -f "$UPLOAD_DIR/$filename" ]; then
        echo -e "  ${GREEN}PASS${NC} file exists on disk: $UPLOAD_DIR/$filename"
        PASS=$((PASS+1))
    else
        echo -e "  ${RED}FAIL${NC} file not found on disk: $UPLOAD_DIR/$filename"
        FAIL=$((FAIL+1))
    fi

    # GET back
    body=$(http_body "$BASE/upload/$filename")
    expected=$(cat "$test_file")
    if [ "$body" = "$expected" ]; then
        echo -e "  ${GREEN}PASS${NC} GET returns correct content"
        PASS=$((PASS+1))
    else
        echo -e "  ${RED}FAIL${NC} GET content mismatch"
        echo "       expected: $expected"
        echo "       got:      $body"
        FAIL=$((FAIL+1))
    fi

    # oversized upload (513000 bytes > 512000 limit)
    # oversized upload
    big_file=$(mktemp)
    dd if=/dev/urandom bs=1024 count=501 2>/dev/null > "$big_file"
    code=$(curl -s -o /dev/null -w '%{http_code}' \
        -X POST \
        -H "Content-Type: application/octet-stream" \
        --data-binary @"$big_file" \
        "$BASE/upload/toobig.bin")
    check_code "oversized upload returns 413" "413" "$code"
    rm -f "$test_file" "$big_file"
}

# ============================================================
# Summary
# ============================================================

print_summary() {
    local total=$((PASS+FAIL+SKIP))
    echo -e "\n${BOLD}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo -e "${BOLD}Results: $total tests${NC}"
    echo -e "  ${GREEN}PASS${NC}: $PASS"
    echo -e "  ${RED}FAIL${NC}: $FAIL"
    echo -e "  ${YELLOW}SKIP${NC}: $SKIP"
    echo -e "${BOLD}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    if [ "$FAIL" -eq 0 ]; then
        echo -e "${GREEN}${BOLD}All tests passed!${NC}"
    else
        echo -e "${RED}${BOLD}$FAIL test(s) failed.${NC}"
    fi
}

# ============================================================
# Main
# ============================================================

echo -e "${BOLD}CGI Test Suite${NC}"
echo -e "Target: ${CYAN}$BASE${NC}"
echo -e "CGI dir: ${CYAN}$CGI_DIR${NC}"
echo -e "Upload dir: ${CYAN}$UPLOAD_DIR${NC}"

setup_scripts
test_basic_get
test_query_string
test_post
test_path_info
test_errors
test_timeout
test_large_output
test_concurrent
test_keepalive
test_upload
print_summary
