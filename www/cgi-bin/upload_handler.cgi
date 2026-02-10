#!/bin/bash

# 输出HTTP头部
echo "Content-type: text/html"
echo ""

# HTML页面开始
cat << HTML
<html>
<head><title>File Upload Test</title></head>
<body>
<h1>File Upload Test Result</h1>
HTML

# 显示环境变量
echo "<h2>Environment Variables</h2>"
echo "<table border='1'>"
echo "<tr><th>Variable</th><th>Value</th></tr>"
echo "<tr><td>REQUEST_METHOD</td><td>$REQUEST_METHOD</td></tr>"
echo "<tr><td>CONTENT_TYPE</td><td>$CONTENT_TYPE</td></tr>"
echo "<tr><td>CONTENT_LENGTH</td><td>$CONTENT_LENGTH</td></tr>"
echo "<tr><td>QUERY_STRING</td><td>$QUERY_STRING</td></tr>"
echo "</table>"

# 处理POST请求
if [ "$REQUEST_METHOD" = "POST" ]; then
    echo "<h2>Processing Upload</h2>"
    
    if [ -z "$CONTENT_LENGTH" ]; then
        echo "<p style='color:red'>Error: No CONTENT_LENGTH</p>"
    else
        echo "<p>Content length: $CONTENT_LENGTH bytes</p>"
        
        # 创建临时文件
        TMPFILE="/tmp/upload_$$.dat"
        
        # 读取POST数据到临时文件
        dd bs=1 count="$CONTENT_LENGTH" of="$TMPFILE" 2>/dev/null
        
        # 分析文件内容
        FILESIZE=$(wc -c < "$TMPFILE")
        echo "<p>Read $FILESIZE bytes to $TMPFILE</p>"
        
        # 显示文件信息
        echo "<h3>File Content Analysis</h3>"
        echo "<pre>"
        echo "First 300 bytes:"
        head -c 300 "$TMPFILE" | cat -v
        echo ""
        echo "..."
        echo ""
        echo "Last 100 bytes:"
        tail -c 100 "$TMPFILE" | cat -v
        echo "</pre>"
        
        # 尝试解析multipart/form-data
        echo "<h3>Multipart Form Data Analysis</h3>"
        echo "<pre>"
        # 提取boundary
        BOUNDARY=$(echo "$CONTENT_TYPE" | sed -n 's/.*boundary=//p')
        echo "Boundary: $BOUNDARY"
        
        # 显示是否找到文件部分
        if grep -q "filename=" "$TMPFILE"; then
            echo "Found filename in upload data"
            
            # 提取文件名
            FILENAME=$(grep -a "filename=" "$TMPFILE" | head -1 | sed 's/.*filename="//;s/".*//')
            echo "Uploaded filename: $FILENAME"
            
            # 提取Content-Type
            CTYPE=$(grep -a "Content-Type:" "$TMPFILE" | head -1 | sed 's/Content-Type: //')
            echo "Content-Type: $CTYPE"
        else
            echo "No filename found in upload data"
        fi
        echo "</pre>"
        
        # 保存上传的文件（简化版，实际需要正确解析）
        if [ -n "$FILENAME" ]; then
            SAVEPATH="/tmp/uploaded_${FILENAME}"
            # 这里应该正确解析multipart数据，但简单示例只复制
            cp "$TMPFILE" "$SAVEPATH"
            echo "<p>File saved to: $SAVEPATH ($(wc -c < "$SAVEPATH") bytes)</p>"
        fi
        
        # 清理临时文件
        rm -f "$TMPFILE"
    fi
else
    echo "<h2>Usage</h2>"
    echo "<p>This script accepts POST requests for file upload.</p>"
    echo "<p>Example using curl:</p>"
    echo "<pre>"
    echo "curl -X POST http://localhost:8080/cgi-bin/upload_handler.cgi \\"
    echo "  -F \"file=@yourfile.txt\""
    echo "</pre>"
fi

# HTML页面结束
echo "</body></html>"
