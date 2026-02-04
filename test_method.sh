#!/bin/bash
set -e

PORT=8080

echo "[1] prepare dirs/files"
mkdir -p www upload www/dir www/emptydir
echo "HELLO" > www/hello.txt
echo "<h1>INDEX</h1>" > www/dir/index.html
echo "A" > www/dir/a.txt

echo "[2] GET static"
curl -is http://127.0.0.1:${PORT}/hello.txt | head -n 20

echo "[3] GET 404"
curl -is http://127.0.0.1:${PORT}/nope.txt | head -n 20

echo "[4] GET dir index"
curl -is http://127.0.0.1:${PORT}/dir/ | head -n 30

echo "[5] GET dir autoindex"
curl -is http://127.0.0.1:${PORT}/emptydir/ | head -n 30

echo "[6] POST raw upload"
curl -is -X POST --data-binary @www/hello.txt http://127.0.0.1:${PORT}/upload/raw.txt | head -n 30
test -f upload/raw.txt

echo "[7] POST multipart upload"
curl -is -F "file=@www/hello.txt" http://127.0.0.1:${PORT}/upload | head -n 30
test -f upload/hello.txt

echo "[8] POST /upload non-multipart -> 415"
curl -is -X POST --data "abc" http://127.0.0.1:${PORT}/upload | head -n 20

echo "[9] DELETE existing"
curl -is -X DELETE http://127.0.0.1:${PORT}/hello.txt | head -n 20
test ! -f www/hello.txt

echo "ALL OK"
