#!/bin/sh
echo "Content-type: text/plain"
echo ""
if [ -n "$CONTENT_LENGTH" ]; then
    dd bs=1 count="$CONTENT_LENGTH" 2>/dev/null
fi
