#!/bin/bash
echo 'Content-Type: text/html'
echo ''
echo '<html><body>'
echo '<h1>CGI Test Page</h1>'
echo '<p>Request Method: '$REQUEST_METHOD'</p>'
echo '<p>Query String: '$QUERY_STRING'</p>'
echo '<p>Server Name: '$SERVER_NAME'</p>'
echo '<p>Server Port: '$SERVER_PORT'</p>'
if [ "$REQUEST_METHOD" = "POST" ]; then
  echo '<h2>POST Data:</h2>'
  echo '<pre>'
  cat
  echo '</pre>'
fi
echo '</body></html>'
