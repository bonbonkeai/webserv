#endif

表示一个客户端连接：
成员包括：
readBuffer / writeBuffer
HTTPRequestParser 实例
是否 headerComplete / bodyComplete
keepAlive 状态
lastActive 时间戳
与 CGI 交互时的 cgiFd