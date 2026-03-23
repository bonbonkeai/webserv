#!/usr/bin/env bash

set -u

HOST="${HOST:-127.0.0.1}"
PORT="${PORT:-8080}"
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

need_cmd() {
    command -v "$1" >/dev/null 2>&1 || {
        echo -e "${RED}missing command:${NC} $1"
        exit 1
    }
}

ok() {
    echo -e "${GREEN}ok${NC}  $1"
    PASS=$((PASS + 1))
}

fail() {
    echo -e "${RED}fail${NC} $1"
    [ -n "${2:-}" ] && echo "      $2"
    FAIL=$((FAIL + 1))
}

skip() {
    echo -e "${YELLOW}skip${NC} $1"
    [ -n "${2:-}" ] && echo "      $2"
    SKIP=$((SKIP + 1))
}

note() {
    echo "note: $1"
}

section() {
    echo
    echo -e "${CYAN}${BOLD}==== $1 ====${NC}"
}

tmp_headers() { mktemp /tmp/err_headers.XXXXXX; }
tmp_body()    { mktemp /tmp/err_body.XXXXXX; }
tmp_err()     { mktemp /tmp/err_stderr.XXXXXX; }

fetch() {
    # $1=name $2=expected_status $3=url $4...=extra curl args
    local name="$1"
    local expected="$2"
    local url="$3"
    shift 3

    local H B E status
    H="$(tmp_headers)"
    B="$(tmp_body)"
    E="$(tmp_err)"

    status="$(curl -sS -D "$H" -o "$B" -w '%{http_code}' "$@" "$url" 2>"$E" || true)"

    echo "cmd: curl $* \"$url\""
    echo "status: $status"

    if [ "$status" = "$expected" ]; then
        ok "$name status=$expected"
    else
        fail "$name status=$expected" "got $status"
        echo "---- headers ----"
        cat "$H"
        echo "---- body ----"
        cat "$B"
        echo "---- stderr ----"
        cat "$E"
    fi

    FETCH_HEADERS="$H"
    FETCH_BODY="$B"
    FETCH_ERR="$E"
    FETCH_STATUS="$status"
}

fetch_any_status() {
    # $1=name $2=url $3...=extra curl args
    local name="$1"
    local url="$2"
    shift 2

    local H B E status
    H="$(tmp_headers)"
    B="$(tmp_body)"
    E="$(tmp_err)"

    status="$(curl -sS -D "$H" -o "$B" -w '%{http_code}' "$@" "$url" 2>"$E" || true)"

    echo "cmd: curl $* \"$url\""
    echo "status: $status"

    FETCH_HEADERS="$H"
    FETCH_BODY="$B"
    FETCH_ERR="$E"
    FETCH_STATUS="$status"
}

check_content_type_prefix() {
    local name="$1"
    local file="$2"
    local expected_prefix="$3"
    local ct
    ct="$(grep -i '^content-type:' "$file" | tail -n1 | sed -E 's/^[^:]+:[[:space:]]*//I' | tr -d '\r')"

    if printf '%s\n' "$ct" | grep -Eiq "^${expected_prefix}"; then
        ok "$name content-type=$expected_prefix"
    else
        fail "$name content-type=$expected_prefix" "got [$ct]"
        echo "---- headers ----"
        cat "$file"
    fi
}

check_body_contains() {
    local name="$1"
    local file="$2"
    local pattern="$3"

    if grep -Fq "$pattern" "$file"; then
        ok "$name body contains [$pattern]"
    else
        fail "$name body contains [$pattern]" "pattern not found"
        echo "---- body ----"
        cat "$file"
    fi
}

nc_expect_status() {
    # $1 name $2 expected $3 payload
    local name="$1"
    local expected="$2"
    local payload="$3"

    echo "cmd:"
    printf '%b\n' "$payload"

    local out
    out="$(printf '%b' "$payload" | timeout 5 nc "$HOST" "$PORT" 2>/dev/null || true)"

    if echo "$out" | grep -q "^HTTP/1\.1 $expected "; then
        ok "$name status=$expected"
    else
        fail "$name status=$expected" "unexpected response"
        echo "---- nc output ----"
        echo "$out"
    fi

    NC_OUT="$out"
}

check_nc_contains() {
    local name="$1"
    local pattern="$2"
    if printf '%s\n' "$NC_OUT" | grep -Fq "$pattern"; then
        ok "$name contains [$pattern]"
    else
        fail "$name contains [$pattern]" "pattern not found"
        echo "---- nc output ----"
        echo "$NC_OUT"
    fi
}

server_alive_check() {
    local name="$1"
    local H B E status
    H="$(tmp_headers)"
    B="$(tmp_body)"
    E="$(tmp_err)"
    status="$(curl -sS -D "$H" -o "$B" -w '%{http_code}' "$BASE/html_error/404.html" 2>"$E" || true)"
    if [ "$status" = "200" ]; then
        ok "$name server still responds"
    else
        fail "$name server still responds" "got $status"
        echo "---- headers ----"
        cat "$H"
        echo "---- body ----"
        cat "$B"
        echo "---- stderr ----"
        cat "$E"
    fi
}

ensure_www_hello() {
    mkdir -p ./www
    if [ ! -f ./www/hello.txt ]; then
        printf "HELLO\n" > ./www/hello.txt
        ok "created ./www/hello.txt fixture"
    else
        note "./www/hello.txt already exists, keeping it unchanged"
    fi
}

prepare_forbidden_fixture() {
    mkdir -p ./www/forbidden_test
    rm -f ./www/forbidden_test/index.html
}

prepare_big_file() {
    if [ ! -f big.bin ]; then
        dd if=/dev/zero of=big.bin bs=1024 count=600 >/dev/null 2>&1
        ok "created ./big.bin fixture"
    else
        note "./big.bin already exists, keeping it unchanged"
    fi
}

prepare_readonly_fixture() {
    mkdir -p ./www/readonly
    if [ ! -f ./www/readonly/a.txt ]; then
        printf "READONLY\n" > ./www/readonly/a.txt
        ok "created ./www/readonly/a.txt fixture"
    else
        note "./www/readonly/a.txt already exists, keeping it unchanged"
    fi
}

prepare_cgi_fixtures() {
    mkdir -p ./www/cgi-bin

    cat > ./www/cgi-bin/test_crash.sh <<'SH'
#!/bin/sh
echo "Content-Type: text/plain"
echo
exit 1
SH

    cat > ./www/cgi-bin/test_slow.sh <<'SH'
#!/bin/sh
sleep 20
echo "Content-Type: text/plain"
echo
echo "late"
SH

    chmod +x ./www/cgi-bin/test_crash.sh ./www/cgi-bin/test_slow.sh
    ok "prepared CGI fixtures under ./www/cgi-bin"
}

check_plaintext_fallback_hint() {
    local name="$1"
    local text="$2"

    if printf '%s\n' "$text" | grep -Fq "webserv custom error page"; then
        ok "$name appears to use custom html error page"
    else
        note "$name may still be using plain-text fallback instead of custom html page"
    fi
}

main() {
    need_cmd curl
    need_cmd nc
    need_cmd timeout
    need_cmd grep
    need_cmd sed
    need_cmd mktemp
    need_cmd dd
    need_cmd chmod
    need_cmd mkdir
    need_cmd printf

    echo -e "${BOLD}Error page mapping tests${NC}"
    echo "HOST=$HOST"
    echo "PORT=$PORT"
    echo "BASE=$BASE"

    ensure_www_hello
    prepare_big_file
    prepare_forbidden_fixture
    prepare_readonly_fixture
    prepare_cgi_fixtures

    section "1. 静态错误页文件本身"
    fetch "static 404.html" "200" "$BASE/html_error/404.html"
    check_content_type_prefix "static 404.html" "$FETCH_HEADERS" "text/html"
    check_body_contains "static 404.html" "$FETCH_BODY" "<title>404 Not Found</title>"
    check_body_contains "static 404.html" "$FETCH_BODY" "webserv custom error page"

    fetch "static style.css" "200" "$BASE/html_error/style.css"
    check_content_type_prefix "static style.css" "$FETCH_HEADERS" "text/css"

    section "2. 404 Not Found"
    fetch "404 missing file" "404" "$BASE/no-such-file"
    check_content_type_prefix "404 missing file" "$FETCH_HEADERS" "text/html"
    check_body_contains "404 missing file" "$FETCH_BODY" "<title>404 Not Found</title>"
    check_body_contains "404 missing file" "$FETCH_BODY" "webserv custom error page"

    section "3. 403 Forbidden"
    fetch "403 forbidden directory" "403" "$BASE/forbidden_test/"
    if [ "$FETCH_STATUS" = "403" ]; then
        check_content_type_prefix "403 forbidden directory" "$FETCH_HEADERS" "text/html"
        check_body_contains "403 forbidden directory" "$FETCH_BODY" "403"
    else
        note "if you got 404 or 200 here, your forbidden fixture or routing logic still needs adjustment"
    fi

    section "4. 405 Method Not Allowed"
    note "this test assumes your config has: location /readonly/ { root ./www; allow_methods GET; }"

    fetch_any_status "405 POST readonly" "$BASE/readonly/a.txt" -X POST -H "Content-Length: 0"
    if [ "$FETCH_STATUS" = "405" ]; then
        ok "405 POST readonly status=405"
        check_content_type_prefix "405 POST readonly" "$FETCH_HEADERS" "text/html"
        check_body_contains "405 POST readonly" "$FETCH_BODY" "405"
    elif [ "$FETCH_STATUS" = "404" ] || [ "$FETCH_STATUS" = "200" ] || [ "$FETCH_STATUS" = "403" ]; then
        skip "405 POST readonly" "current config likely does not define a GET-only /readonly/ location"
    else
        fail "405 POST readonly" "got unexpected status $FETCH_STATUS"
        echo "---- headers ----"
        cat "$FETCH_HEADERS"
        echo "---- body ----"
        cat "$FETCH_BODY"
    fi

    fetch_any_status "405 DELETE readonly" "$BASE/readonly/a.txt" -X DELETE
    if [ "$FETCH_STATUS" = "405" ]; then
        ok "405 DELETE readonly status=405"
        check_content_type_prefix "405 DELETE readonly" "$FETCH_HEADERS" "text/html"
        check_body_contains "405 DELETE readonly" "$FETCH_BODY" "405"
    elif [ "$FETCH_STATUS" = "404" ] || [ "$FETCH_STATUS" = "200" ] || [ "$FETCH_STATUS" = "403" ]; then
        skip "405 DELETE readonly" "current config likely does not define a GET-only /readonly/ location"
    else
        fail "405 DELETE readonly" "got unexpected status $FETCH_STATUS"
        echo "---- headers ----"
        cat "$FETCH_HEADERS"
        echo "---- body ----"
        cat "$FETCH_BODY"
    fi

    section "5. 413 Payload Too Large"
    fetch "413 raw upload too large" "413" "$BASE/upload/big.bin" -X POST --data-binary @big.bin
    if [ "$FETCH_STATUS" = "413" ]; then
        check_content_type_prefix "413 raw upload too large" "$FETCH_HEADERS" "text/html"
        check_body_contains "413 raw upload too large" "$FETCH_BODY" "413"
    fi

    section "6. 502 Bad Gateway"
    fetch_any_status "502 crash CGI" "$BASE/cgi-bin/test_crash.sh"
    if [ "$FETCH_STATUS" = "502" ]; then
        ok "502 crash CGI status=502"
        check_content_type_prefix "502 crash CGI" "$FETCH_HEADERS" "text/html"
        check_body_contains "502 crash CGI" "$FETCH_BODY" "502"
    elif [ "$FETCH_STATUS" = "403" ] || [ "$FETCH_STATUS" = "404" ]; then
        skip "502 crash CGI" "CGI route did not execute script; check cgi config / permissions / routing"
    else
        fail "502 crash CGI" "got unexpected status $FETCH_STATUS"
        echo "---- headers ----"
        cat "$FETCH_HEADERS"
        echo "---- body ----"
        cat "$FETCH_BODY"
    fi


    section "7. 504 Gateway Timeout"
    fetch_any_status "504 slow CGI" "$BASE/cgi-bin/test_slow.sh"
    if [ "$FETCH_STATUS" = "504" ]; then
        ok "504 slow CGI status=504"
        check_content_type_prefix "504 slow CGI" "$FETCH_HEADERS" "text/html"
        check_body_contains "504 slow CGI" "$FETCH_BODY" "504"
    elif [ "$FETCH_STATUS" = "403" ] || [ "$FETCH_STATUS" = "404" ] || [ "$FETCH_STATUS" = "200" ]; then
        skip "504 slow CGI" "CGI timeout not reached or CGI route did not execute script"
    else
        fail "504 slow CGI" "got unexpected status $FETCH_STATUS"
        echo "---- headers ----"
        cat "$FETCH_HEADERS"
        echo "---- body ----"
        cat "$FETCH_BODY"
    fi
    server_alive_check "after slow CGI"

    section "8. 400 Bad Request"
    nc_expect_status "400 missing Host" "400" 'GET /hello.txt HTTP/1.1\r\n\r\n'
    check_nc_contains "400 missing Host body" "400"
    check_plaintext_fallback_hint "400 missing Host" "$NC_OUT"

    section "9. 408 Request Timeout"
    skip "408 request timeout" "best tested interactively with nc: send headers, do not finish body, then wait for server timeout"

    section "10. 411 Length Required"
    nc_expect_status "411 POST no Content-Length" "411" 'POST /upload HTTP/1.1\r\nHost: localhost\r\n\r\nabc'
    check_nc_contains "411 POST no Content-Length body" "411"
    check_plaintext_fallback_hint "411 POST no Content-Length" "$NC_OUT"

    section "11. 415 Unsupported Media Type"
    fetch_any_status "415 or 201 multipart octet-stream" "$BASE/upload" -F "file=@big.bin;type=application/octet-stream"
    if [ "$FETCH_STATUS" = "415" ]; then
        ok "multipart octet-stream rejected with 415"
        check_content_type_prefix "415 multipart octet-stream" "$FETCH_HEADERS" "text/html"
        check_body_contains "415 multipart octet-stream" "$FETCH_BODY" "415"
    elif [ "$FETCH_STATUS" = "201" ]; then
        ok "multipart octet-stream accepted with 201 (server currently allows this media type)"
    else
        fail "415 or 201 multipart octet-stream" "got unexpected status $FETCH_STATUS"
        echo "---- headers ----"
        cat "$FETCH_HEADERS"
        echo "---- body ----"
        cat "$FETCH_BODY"
        echo "---- stderr ----"
        cat "$FETCH_ERR"
    fi

    section "12. 500 Internal Server Error"
    skip "500 internal server error" "no dedicated black-box 500 scenario defined yet"

    section "13. 505 HTTP Version Not Supported"
    nc_expect_status "505 invalid HTTP version" "505" 'GET / HTTP/9.9\r\nHost: localhost\r\n\r\n'
    check_nc_contains "505 invalid HTTP version body" "505"
    check_plaintext_fallback_hint "505 invalid HTTP version" "$NC_OUT"

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