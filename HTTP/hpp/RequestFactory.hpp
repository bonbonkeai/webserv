#endif

根据 HTTPRequest.method 返回正确的类型：
GET → GetRequest
POST → PostRequest
DELETE → DeleteRequest
否则 → ErrorRequest

用于把协议层(C)连接到业务层(Handle)。

