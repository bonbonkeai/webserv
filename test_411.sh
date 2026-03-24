#!/bin/bash
HOST="localhost"
PORT="8080"
PASS=0
FAIL=0

check() {
    local desc="$1"
    local expected="$2"
    local actual="$3"
    if echo "$actual" | grep -q "HTTP/1.1 $expected"; then
        echo "✅ PASS — $desc"
        PASS=$((PASS+1))
    else
        echo "❌ FAIL — $desc (期望$expected，实际: $(echo "$actual" | grep -o 'HTTP/1.1 [0-9]*' | head -1))"
        FAIL=$((FAIL+1))
    fi
}

echo "=== 411 测试 ==="

# 1. 裸POST无Content-Length → 411
R=$(python3 -c "
import socket
s = socket.socket()
s.connect(('$HOST', $PORT))
s.send(b'POST / HTTP/1.1\r\nHost: $HOST\r\n\r\n')
print(s.recv(4096).decode(errors='replace'))
s.close()
")
check "裸POST无Content-Length → 411" "411" "$R"

# 2. POST有Content-Length:0 → 不应该是411
R=$(curl -si --http1.1 -X POST "http://$HOST:$PORT/upload" \
    -H "Content-Type: text/plain" \
    -H "Content-Length: 0" 2>&1)
if echo "$R" | grep -q "HTTP/1.1 411"; then
    echo "❌ FAIL — CL:0不应触发411"
    FAIL=$((FAIL+1))
else
    echo "✅ PASS — CL:0不触发411 ($(echo "$R" | grep -o 'HTTP/1.1 [0-9]*' | head -1))"
    PASS=$((PASS+1))
fi

# 3. POST有body → 不应该是411
R=$(curl -si --http1.1 -X POST "http://$HOST:$PORT/upload/test.txt" \
    -H "Content-Type: text/plain" \
    -d "hello" 2>&1)
if echo "$R" | grep -q "HTTP/1.1 411"; then
    echo "❌ FAIL — 正常POST不应触发411"
    FAIL=$((FAIL+1))
else
    echo "✅ PASS — 正常POST不触发411 ($(echo "$R" | grep -o 'HTTP/1.1 [0-9]*' | head -1))"
    PASS=$((PASS+1))
fi

# 4. GET → 不应该是411
R=$(curl -si --http1.1 "http://$HOST:$PORT/" 2>&1)
if echo "$R" | grep -q "HTTP/1.1 411"; then
    echo "❌ FAIL — GET不应触发411"
    FAIL=$((FAIL+1))
else
    echo "✅ PASS — GET不触发411 ($(echo "$R" | grep -o 'HTTP/1.1 [0-9]*' | head -1))"
    PASS=$((PASS+1))
fi

# 5. chunked POST → 不应该是411
R=$(curl -si --http1.1 -X POST "http://$HOST:$PORT/upload" \
    -H "Transfer-Encoding: chunked" \
    -H "Content-Type: text/plain" \
    --data-raw "hello" 2>&1)
if echo "$R" | grep -q "HTTP/1.1 411"; then
    echo "❌ FAIL — chunked POST不应触发411"
    FAIL=$((FAIL+1))
else
    echo "✅ PASS — chunked不触发411 ($(echo "$R" | grep -o 'HTTP/1.1 [0-9]*' | head -1))"
    PASS=$((PASS+1))
fi

echo ""
echo "结果: ${PASS} passed, ${FAIL} failed"
```

```