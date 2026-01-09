#!/usr/bin/env python3
import os

print("Content-Type: text/plain")
print()
print("Hello CGI GET")
print("QUERY_STRING =", os.environ.get("QUERY_STRING"))
print("METHOD =", os.environ.get("REQUEST_METHOD"))
