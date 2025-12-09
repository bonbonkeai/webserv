#endif

把 HTTPResponse 对象转成原始字节串
ex:

HTTP/1.1 200 OK\r\n
Content-Length: 123\r\n
...\r\n
\r\n
<body>

//这就是写入 Client.writeBuffer 的内容。