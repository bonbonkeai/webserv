#!/usr/bin/env python3
import os
import sys

body = sys.stdin.buffer.read()

print("Content-Type: text/html")
print()

print("<html><body>")
print("<h1>CGI Debug</h1>")
print("<ul>")
for k in [
    "REQUEST_METHOD",
    "QUERY_STRING",
    "CONTENT_LENGTH",
    "CONTENT_TYPE",
    "PATH_INFO",
    "SCRIPT_NAME",
    "REQUEST_URI"
]:
    print(f"<li>{k}: {os.environ.get(k, '')}</li>")
print("</ul>")

print("<h2>Body length</h2>")
print(f"<pre>{len(body)}</pre>")

preview = body[:300]
try:
    preview_text = preview.decode("utf-8", errors="replace")
except:
    preview_text = str(preview)

print("<h2>Body preview</h2>")
print(f"<pre>{preview_text}</pre>")
print("</body></html>")