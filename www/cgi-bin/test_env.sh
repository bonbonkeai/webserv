#!/bin/bash
echo "Content-Type: text/plain"
echo ""
echo "QUERY_STRING=$QUERY_STRING"
echo "REQUEST_METHOD=$REQUEST_METHOD"
echo "CONTENT_TYPE=$CONTENT_TYPE"
echo "CONTENT_LENGTH=$CONTENT_LENGTH"
echo "SCRIPT_NAME=$SCRIPT_NAME"
echo "PATH_INFO=$PATH_INFO"
echo "SERVER_PORT=$SERVER_PORT"
echo "REMOTE_ADDR=$REMOTE_ADDR"
if [ "$REQUEST_METHOD" = "POST" ] && [ -n "$CONTENT_LENGTH" ] && [ "$CONTENT_LENGTH" -gt 0 ] 2>/dev/null; then
    body=$(dd bs=1 count="$CONTENT_LENGTH" 2>/dev/null)
    echo "BODY=$body"
else
    echo "BODY=(none)"
fi