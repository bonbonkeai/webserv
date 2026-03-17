#!/bin/bash
SERVER_URL="http://127.0.0.1:8080"

echo "==== 1. 测试 CGI 执行 ===="
for script in test_env.sh test_query.sh test_crash.sh nonexistent.sh; do
    echo "-> GET /cgi-bin/$script"
    curl -i -X GET "$SERVER_URL/cgi-bin/$script"
    echo -e "\n"
done

echo "==== 2. 准备上传测试文件 ===="
# 创建测试文件
echo "Hello World" > plain_upload.txt
echo -n "" > empty.bin
dd if=/dev/urandom of=large_upload.bin bs=1K count=600 2>/dev/null

echo "==== 3. 测试 二进制 上传 (POST) ===="
for file in plain_upload.txt empty.bin large_upload.bin; do
    echo "-> Upload $file with --data-binary"
    curl -i -X POST --data-binary @"$file" "$SERVER_URL/upload/$file"
    echo -e "\n"
done

echo "==== 4. 测试 multipart/form-data 上传 ===="
for file in plain_upload.txt empty.bin large_upload.bin; do
    echo "-> Upload $file with multipart/form-data"
    curl -i -X POST -F "file=@$file" "$SERVER_URL/upload/$file"
    echo -e "\n"
done

echo "==== 5. 测试 GET 下载文件 ===="
for file in plain_upload.txt empty.bin large_upload.bin; do
    echo "-> GET /upload/$file"
    curl -i -X GET "$SERVER_URL/upload/$file"
    echo -e "\n"
done

echo "==== 6. 测试 DELETE 文件 ===="
for file in plain_upload.txt empty.bin large_upload.bin; do
    echo "-> DELETE /upload/$file"
    curl -i -X DELETE "$SERVER_URL/upload/$file"
    echo -e "\n"
done

echo "==== 7. 测试超过最大上传限制 (expect 413) ===="
dd if=/dev/urandom of=too_big.bin bs=1K count=600 2>/dev/null
curl -i -X POST --data-binary @too_big.bin "$SERVER_URL/upload/too_big.bin"
rm -f too_big.bin
echo -e "\n"

echo "==== 8. 测试缺失 Content-Length POST (expect 411) ===="
# 发送 POST 不带 Content-Length
curl -i -X POST -H "Content-Type: application/x-www-form-urlencoded" "$SERVER_URL/cgi-bin/test_env.sh"
echo -e "\n"

echo "==== 9. 测试不支持的 HTTP 方法 (expect 405) ===="
curl -i -X PUT "$SERVER_URL/upload/unknown.txt"
echo -e "\n"

echo "==== 10. 清理本地测试文件 ===="
rm -f plain_upload.txt empty.bin large_upload.bin
echo "==== 测试完成 ===="