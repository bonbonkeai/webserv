#endif

纯数据结构：
method
path
query
version
headers
body

//不会包含业务逻辑，只负责存储解析后的结果。
//只是一个容器存HTTP Parser解析后的结果
//ex:
class HTTPRequest
{
public:
    std::string method;                  // GET / POST / DELETE
    std::string path;                    // "/upload/image.png"
    std::string query;                   // "user=alice&id=5"
    std::string version;                 // "HTTP/1.1"
    std::map<std::string, std::string> headers; // Host/User-Agent/Content-Type...
    std::string body;                    // 请求体（CGI 或 POST 上传用）
};

