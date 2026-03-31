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

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m'

section() { echo -e "\n${CYAN}${BOLD}━━━ $1 ━━━${NC}"; }

check_code() {
    local desc="$1" expected="$2" actual="$3"
    if [ "$actual" = "$expected" ]; then
        echo -e "  ${GREEN}PASS${NC} $desc (got $actual)"
        PASS=$((PASS+1))
    else
        echo -e "  ${RED}FAIL${NC} $desc (expected $expected, got $actual)"
        FAIL=$((FAIL+1))
    fi
}

check_body() {
    local desc="$1" pattern="$2" body="$3"
    if echo "$body" | grep -q "$pattern"; then
        echo -e "  ${GREEN}PASS${NC} $desc"
        PASS=$((PASS+1))
    else
        echo -e "  ${RED}FAIL${NC} $desc (pattern '$pattern' not found)"
        echo -e "       body: $(echo "$body" | head -3)"
        FAIL=$((FAIL+1))
    fi
}

check_file_exists() {
    local desc="$1" path="$2"
    if [ -f "$path" ]; then
        echo -e "  ${GREEN}PASS${NC} $desc"
        PASS=$((PASS+1))
    else
        echo -e "  ${RED}FAIL${NC} $desc (not found: $path)"
        FAIL=$((FAIL+1))
    fi
}

check_file_absent() {
    local desc="$1" path="$2"
    if [ ! -f "$path" ]; then
        echo -e "  ${GREEN}PASS${NC} $desc"
        PASS=$((PASS+1))
    else
        echo -e "  ${RED}FAIL${NC} $desc (file still exists: $path)"
        FAIL=$((FAIL+1))
    fi
}

check_file_content() {
    local desc="$1" expected="$2" actual="$3"
    if [ "$actual" = "$expected" ]; then
        echo -e "  ${GREEN}PASS${NC} $desc"
        PASS=$((PASS+1))
    else
        echo -e "  ${RED}FAIL${NC} $desc"
        echo -e "       expected: $(echo "$expected" | head -1)"
        echo -e "       got:      $(echo "$actual"   | head -1)"
        FAIL=$((FAIL+1))
    fi
}

skip() {
    echo -e "  ${YELLOW}SKIP${NC} $1 ($2)"
    SKIP=$((SKIP+1))
}

# 统一禁用 Expect: 100-continue
# 服务器正确处理 Expect 后可移除 -H "Expect:"
http_code() { curl -s -H "Expect:" -o /dev/null -w '%{http_code}' "$@"; }
http_body() { curl -s -H "Expect:" "$@"; }

# ============================================================
# Setup
# ============================================================

setup_scripts() {
    section "Setup: creating test scripts"
    mkdir -p "$CGI_DIR"
    mkdir -p "$UPLOAD_DIR"

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
if [ "$REQUEST_METHOD" = "POST" ] && [ "$CONTENT_LENGTH" -gt 0 ] 2>/dev/null; then
    echo ""
    echo "BODY=$(cat)"
fi
SCRIPT
        chmod +x "$CGI_DIR/test_env.sh"
        echo "  created test_env.sh"
    fi

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

    if [ ! -f "$CGI_DIR/test_crash.sh" ]; then
        cat > "$CGI_DIR/test_crash.sh" << 'SCRIPT'
#!/bin/bash
exit 1
SCRIPT
        chmod +x "$CGI_DIR/test_crash.sh"
        echo "  created test_crash.sh"
    fi

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
# 3. POST
# ============================================================

test_post() {
    section "3. POST"

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

    code=$(http_code "$BASE/cgi-bin/test_env.php")
    check_code "unsupported extension .php returns 403" "403" "$code"

    code=$(http_code "$BASE/cgi-bin/test_env.rb")
    check_code "unsupported extension .rb returns 403" "403" "$code"

    code=$(http_code "$BASE/cgi-bin/nonexistent.sh")
    check_code "nonexistent script returns 404" "404" "$code"

    if [ -f "$CGI_DIR/test_env.sh" ]; then
        chmod -x "$CGI_DIR/test_env.sh"
        code=$(http_code "$BASE/cgi-bin/test_env.sh")
        check_code "no execute permission returns 403" "403" "$code"
        chmod +x "$CGI_DIR/test_env.sh"
    else
        skip "no-exec permission test" "test_env.sh not found"
    fi

    code=$(http_code "$BASE/cgi-bin/test_crash.sh")
    check_code "crashing script returns 502" "502" "$code"
}

# ============================================================
# 6. Timeout
# ============================================================

test_timeout() {
    section "6. Timeout (slow script, ~5s wait)"
    code=$(curl -s -H "Expect:" -o /dev/null -w '%{http_code}' \
        --max-time 15 "$BASE/cgi-bin/test_slow.sh")
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
    tmpdir=$(mktemp -d)
    for i in $(seq 1 10); do
        curl -s -H "Expect:" -o /dev/null -w '%{http_code}' \
            --max-time 10 \
            "$BASE/cgi-bin/test_env.sh" > "$tmpdir/$i" &
    done
    wait
    ok=0
    for i in $(seq 1 10); do
        [ "$(cat "$tmpdir/$i" 2>/dev/null)" = "200" ] && ok=$((ok+1))
    done
    if [ "$ok" -eq 10 ]; then
        echo -e "  ${GREEN}PASS${NC} 10/10 concurrent requests returned 200"
        PASS=$((PASS+1))
    else
        echo -e "  ${RED}FAIL${NC} only $ok/10 concurrent requests returned 200"
        FAIL=$((FAIL+1))
    fi
    rm -rf "$tmpdir"
}

# ============================================================
# 9. Keep-alive
# ============================================================

test_keepalive() {
    section "9. Keep-Alive Connection Reuse"
    results=$(curl -s -H "Expect:" -o /dev/null -w '%{http_code}\n' \
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

    section "10a. Upload — Raw POST"

    # 10-1 octet-stream
    f1=$(mktemp); echo "hello raw upload $(date)" > "$f1"
    name1="raw_$(date +%s%N).txt"
    code=$(curl -s -H "Expect:" -o /dev/null -w '%{http_code}' -X POST \
        -H "Content-Type: application/octet-stream" \
        --data-binary @"$f1" \
        "$BASE/upload/$name1")
    check_code "10-1 raw POST octet-stream returns 201" "201" "$code"
    check_file_exists "10-1 file written to disk" "$UPLOAD_DIR/$name1"

    # 10-2 text/plain
    f2=$(mktemp); echo "plain text upload" > "$f2"
    name2="plain_$(date +%s%N).txt"
    code=$(curl -s -H "Expect:" -o /dev/null -w '%{http_code}' -X POST \
        -H "Content-Type: text/plain" \
        --data-binary @"$f2" \
        "$BASE/upload/$name2")
    check_code "10-2 raw POST text/plain returns 201" "201" "$code"
    check_file_exists "10-2 text/plain file on disk" "$UPLOAD_DIR/$name2"

    # 10-3 GET back
    expected=$(cat "$f2")
    actual=$(curl -s -H "Expect:" "$BASE/upload/$name2")
    check_file_content "10-3 GET uploaded file returns correct content" "$expected" "$actual"

    # 10-4 binary + md5
    bin=$(mktemp)
    dd if=/dev/urandom bs=1024 count=4 2>/dev/null > "$bin"
    binname="binary_$(date +%s%N).bin"
    code=$(curl -s -H "Expect:" -o /dev/null -w '%{http_code}' -X POST \
        -H "Content-Type: application/octet-stream" \
        --data-binary @"$bin" \
        "$BASE/upload/$binname")
    check_code "10-4 binary raw upload returns 201" "201" "$code"
    orig_cksum=$(md5sum "$bin" | cut -d' ' -f1)
    got_cksum=$(curl -s -H "Expect:" "$BASE/upload/$binname" | md5sum | cut -d' ' -f1)
    if [ "$orig_cksum" = "$got_cksum" ]; then
        echo -e "  ${GREEN}PASS${NC} 10-4 binary content integrity (md5 match)"
        PASS=$((PASS+1))
    else
        echo -e "  ${RED}FAIL${NC} 10-4 binary content corrupted"
        FAIL=$((FAIL+1))
    fi

    # 10-5 overwrite
    ow=$(mktemp); echo "version 2" > "$ow"
    code=$(curl -s -H "Expect:" -o /dev/null -w '%{http_code}' -X POST \
        -H "Content-Type: text/plain" \
        --data-binary @"$ow" \
        "$BASE/upload/$name2")
    check_code "10-5 overwrite existing file returns 201" "201" "$code"
    actual=$(curl -s -H "Expect:" "$BASE/upload/$name2")
    check_file_content "10-5 overwritten content is new" "version 2" "$actual"

    # 10-6 oversized
    big=$(mktemp)
    dd if=/dev/urandom bs=1024 count=513 2>/dev/null > "$big"
    code=$(curl -s -H "Expect:" -o /dev/null -w '%{http_code}' -X POST \
        -H "Content-Type: application/octet-stream" \
        --data-binary @"$big" \
        "$BASE/upload/toobig.bin")
    check_code "10-6 oversized upload returns 413" "413" "$code"
    check_file_absent "10-6 oversized file NOT written to disk" "$UPLOAD_DIR/toobig.bin"

    # 10-7 empty body
    code=$(curl -s -H "Expect:" -o /dev/null -w '%{http_code}' -X POST \
        -H "Content-Type: application/octet-stream" \
        -H "Content-Length: 0" \
        "$BASE/upload/empty.bin")
    check_code "10-7 empty body returns 400" "400" "$code"

    # 10-8 missing filename
    code=$(curl -s -H "Expect:" -o /dev/null -w '%{http_code}' -X POST \
        -H "Content-Type: application/octet-stream" \
        --data-binary "data" \
        "$BASE/upload/")
    check_code "10-8 missing filename returns 400" "400" "$code"

    # 10-9 path traversal
    code=$(curl -s -H "Expect:" -o /dev/null -w '%{http_code}' -X POST \
        -H "Content-Type: application/octet-stream" \
        --data-binary "evil" \
        "$BASE/upload/../../etc/passwd")
    if [ "$code" = "400" ] || [ "$code" = "403" ] || [ "$code" = "404" ]; then
        echo -e "  ${GREEN}PASS${NC} 10-9 path traversal blocked (got $code)"
        PASS=$((PASS+1))
    else
        echo -e "  ${RED}FAIL${NC} 10-9 path traversal NOT blocked (got $code)"
        FAIL=$((FAIL+1))
    fi

    # 10-10 DELETE
    code=$(curl -s -H "Expect:" -o /dev/null -w '%{http_code}' \
        -X DELETE "$BASE/upload/$name1")
    if [ "$code" = "200" ] || [ "$code" = "204" ]; then
        echo -e "  ${GREEN}PASS${NC} 10-10 DELETE returns $code"
        PASS=$((PASS+1))
    else
        echo -e "  ${RED}FAIL${NC} 10-10 DELETE (expected 200/204, got $code)"
        FAIL=$((FAIL+1))
    fi
    check_file_absent "10-10 file removed from disk after DELETE" "$UPLOAD_DIR/$name1"

    # 10-11 DELETE non-existent
    code=$(curl -s -H "Expect:" -o /dev/null -w '%{http_code}' \
        -X DELETE "$BASE/upload/does_not_exist_xyz.txt")
    check_code "10-11 DELETE non-existent returns 404" "404" "$code"

    rm -f "$f1" "$f2" "$bin" "$ow" "$big"

    section "10b. Upload — multipart/form-data"

    # 10-12
    mf=$(mktemp /tmp/mfXXXXXX.txt); echo "multipart content here" > "$mf"
    mfname="multi_$(date +%s%N).txt"
    code=$(curl -s -H "Expect:" -o /dev/null -w '%{http_code}' -X POST \
        -F "file=@$mf;filename=$mfname" "$BASE/upload/")
    check_code "10-12 multipart upload returns 201" "201" "$code"
    check_file_exists "10-12 multipart file on disk" "$UPLOAD_DIR/$mfname"

    # 10-13
    actual=$(curl -s -H "Expect:" "$BASE/upload/$mfname")
    expected=$(cat "$mf")
    check_file_content "10-13 multipart file content correct" "$expected" "$actual"

    # 10-14 boundary edge case
    mf2=$(mktemp)
    printf -- '--boundary-like-content--\r\nmore data\r\n' > "$mf2"
    mfname2="edge_$(date +%s%N).bin"
    code=$(curl -s -H "Expect:" -o /dev/null -w '%{http_code}' -X POST \
        -F "file=@$mf2;filename=$mfname2" "$BASE/upload/")
    check_code "10-14 multipart dash-heavy content returns 201" "201" "$code"

    # 10-15 filename with spaces
    mf3=$(mktemp); echo "spaced" > "$mf3"
    code=$(curl -s -H "Expect:" -o /dev/null -w '%{http_code}' -X POST \
        -F "file=@$mf3;filename=my file.txt" "$BASE/upload/")
    if [ "$code" = "201" ] || [ "$code" = "400" ]; then
        echo -e "  ${GREEN}PASS${NC} 10-15 filename with spaces handled ($code)"
        PASS=$((PASS+1))
    else
        echo -e "  ${RED}FAIL${NC} 10-15 unexpected ($code)"
        FAIL=$((FAIL+1))
    fi

    # 10-16 no filename
    code=$(curl -s -H "Expect:" -o /dev/null -w '%{http_code}' -X POST \
        -F "field=justtext" "$BASE/upload/")
    check_code "10-16 multipart no filename returns 400" "400" "$code"

    # 10-17 oversized multipart
    bigmf=$(mktemp)
    dd if=/dev/urandom bs=1024 count=513 2>/dev/null > "$bigmf"
    code=$(curl -s -H "Expect:" -o /dev/null -w '%{http_code}' -X POST \
        -F "file=@$bigmf;filename=bigmulti.bin" "$BASE/upload/")
    check_code "10-17 multipart oversized returns 413" "413" "$code"

    rm -f "$mf" "$mf2" "$mf3" "$bigmf"

    section "10c. Upload — method restrictions"

    # 10-18 PUT
    code=$(curl -s -H "Expect:" -o /dev/null -w '%{http_code}' -X PUT \
        -H "Content-Type: application/octet-stream" \
        --data-binary "data" "$BASE/upload/puttest.txt")
    if [ "$code" = "405" ] || [ "$code" = "201" ]; then
        echo -e "  ${GREEN}PASS${NC} 10-18 PUT handled ($code)"
        PASS=$((PASS+1))
    else
        echo -e "  ${RED}FAIL${NC} 10-18 PUT unexpected ($code)"
        FAIL=$((FAIL+1))
    fi

    # 10-19 GET non-existent
    code=$(http_code "$BASE/upload/no_such_file_$(date +%s).txt")
    check_code "10-19 GET non-existent returns 404" "404" "$code"

    section "10d. Upload — concurrent (5 parallel)"

    tmpdir=$(mktemp -d)
    for i in $(seq 1 5); do
        cf=$(mktemp); echo "concurrent $i $(date +%s%N)" > "$cf"
        curl -s -H "Expect:" -o /dev/null -w '%{http_code}' \
            -X POST -H "Content-Type: text/plain" \
            --data-binary @"$cf" \
            "$BASE/upload/concurrent_${i}_$(date +%s%N).txt" > "$tmpdir/$i" &
    done
    wait
    ok=0
    for i in $(seq 1 5); do
        [ "$(cat "$tmpdir/$i" 2>/dev/null)" = "201" ] && ok=$((ok+1))
    done
    if [ "$ok" -eq 5 ]; then
        echo -e "  ${GREEN}PASS${NC} 10-20 5/5 concurrent uploads returned 201"
        PASS=$((PASS+1))
    else
        echo -e "  ${RED}FAIL${NC} 10-20 only $ok/5 concurrent uploads returned 201"
        FAIL=$((FAIL+1))
    fi
    rm -rf "$tmpdir"
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

echo -e "${BOLD}CGI & Upload Test Suite${NC}"
echo -e "Target:     ${CYAN}$BASE${NC}"
echo -e "CGI dir:    ${CYAN}$CGI_DIR${NC}"
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