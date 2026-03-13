#!/bin/bash
echo "Content-type: text/plain"
echo ""
echo "=== Environment Variables ==="
echo "REQUEST_METHOD: $REQUEST_METHOD"
echo "CONTENT_TYPE: $CONTENT_TYPE"
echo "CONTENT_LENGTH: $CONTENT_LENGTH"
echo "QUERY_STRING: $QUERY_STRING"
echo "PATH_INFO: $PATH_INFO"
echo "SCRIPT_NAME: $SCRIPT_NAME"
echo "SCRIPT_FILENAME: $SCRIPT_FILENAME"
echo "PATH_INFO: $PATH_INFO"

echo "=== Body ==="
if [ "$REQUEST_METHOD" = "POST" ] && [ -n "$CONTENT_LENGTH" ] && [ "$CONTENT_LENGTH" -gt 0 ]; then
    dd bs=1 count=$CONTENT_LENGTH 2>/dev/null
else
    echo "(no body)"
fi