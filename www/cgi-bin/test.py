#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
import sys
import time

print("Content-Type: text/html")
print("")  # ⚠️ 必须有空行

print("<html><body>")
print("<h1>Python CGI Test</h1>")

print("<h2>Request Info</h2>")
print("<ul>")
print("<li>REQUEST_METHOD: {}</li>".format(os.environ.get("REQUEST_METHOD", "")))
print("<li>QUERY_STRING: {}</li>".format(os.environ.get("QUERY_STRING", "")))
print("<li>CONTENT_LENGTH: {}</li>".format(os.environ.get("CONTENT_LENGTH", "")))
print("</ul>")

if os.environ.get("REQUEST_METHOD") == "POST":
    length = int(os.environ.get("CONTENT_LENGTH", 0))
    if length > 0:
        body = sys.stdin.read(length)
        print("<h2>POST Data</h2>")
        print("<pre>{}</pre>".format(body))

time.sleep(1)

print("</body></html>")
