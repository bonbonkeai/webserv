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

check_location() {
    local desc="$1"
    local expected="$2"
    local actual="$3"
    if echo "$actual" | grep -qi "location: $expected"; then
        echo "✅ PASS — $desc"
        PASS=$((PASS+1))
    else
        echo "❌ FAIL — $desc (期望Location:$expected，实际: $(echo "$actual" | grep -i 'location:'))"
        FAIL=$((FAIL+1))
    fi
}

echo "=== 重定向测试 ==="
echo ""

# ---------- 301 外部跳转 ----------

# 1. GET /redirection/ → 301
R=$(curl -si --http1.1 -o /dev/null -D - "http://$HOST:$PORT/redirection/" 2>&1)
check "GET /redirection/ 返回301" "301" "$R"
check_location "GET /redirection/ Location正确" "https://42.fr/en/homepage/" "$R"

# 2. POST /redirection/ → 301（方法不影响重定向）
R=$(curl -si --http1.1 -X POST -o /dev/null -D - "http://$HOST:$PORT/redirection/" 2>&1)
check "POST /redirection/ 也返回301" "301" "$R"

# 3. 不跟随跳转，确认没有直接返回200
R=$(curl -si --http1.1 "http://$HOST:$PORT/redirection/" 2>&1)
if echo "$R" | grep -q "HTTP/1.1 200"; then
    echo "❌ FAIL — /redirection/ 不应直接返回200（应该是301）"
    FAIL=$((FAIL+1))
else
    echo "✅ PASS — /redirection/ 未直接返回200"
    PASS=$((PASS+1))
fi

echo ""

# ---------- 302 内部跳转 ----------

# 4. GET /oldplace/ → 302
R=$(curl -si --http1.1 -o /dev/null -D - "http://$HOST:$PORT/oldplace/" 2>&1)
check "GET /oldplace/ 返回302" "302" "$R"
check_location "GET /oldplace/ Location指向/newplace/" "/newplace/" "$R"

# 5. 跟随跳转，最终应该到 /newplace/（200或其他有效响应）
R=$(curl -si --http1.1 -L "http://$HOST:$PORT/oldplace/" 2>&1)
if echo "$R" | grep -q "HTTP/1.1 200\|HTTP/1.1 404"; then
    echo "✅ PASS — 跟随302跳转后到达/newplace/ ($(echo "$R" | grep -o 'HTTP/1.1 [0-9]*' | tail -1))"
    PASS=$((PASS+1))
else
    echo "❌ FAIL — 跟随跳转后响应异常: $(echo "$R" | grep -o 'HTTP/1.1 [0-9]*' | tail -1)"
    FAIL=$((FAIL+1))
fi

# 6. 确认302不缓存（301会缓存，302不应该）
R=$(curl -si --http1.1 -o /dev/null -D - "http://$HOST:$PORT/oldplace/" 2>&1)
if echo "$R" | grep -qi "cache-control: no-store\|cache-control: no-cache"; then
    echo "✅ PASS — 302包含no-cache头"
    PASS=$((PASS+1))
else
    echo "⚠️  INFO — 302未显式设置cache-control（可选）"
fi

echo ""

# ---------- 边界情况 ----------

# 7. /redirection 不带尾斜杠（看你的路由是否处理）
R=$(curl -si --http1.1 -o /dev/null -D - "http://$HOST:$PORT/redirection" 2>&1)
echo "ℹ️  INFO — GET /redirection (无尾斜杠): $(echo "$R" | grep -o 'HTTP/1.1 [0-9]*' | head -1)"

# 8. /oldplace 不带尾斜杠
R=$(curl -si --http1.1 -o /dev/null -D - "http://$HOST:$PORT/oldplace" 2>&1)
echo "ℹ️  INFO — GET /oldplace (无尾斜杠): $(echo "$R" | grep -o 'HTTP/1.1 [0-9]*' | head -1)"

echo ""
echo "结果: ${PASS} passed, ${FAIL} failed"
```
```
