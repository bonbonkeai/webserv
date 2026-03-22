#!/bin/bash
echo "Content-type: text/plain"
echo ""
dd if=/dev/urandom bs=524 count=64 2>/dev/null | base64
