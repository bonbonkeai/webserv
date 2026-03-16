#include "HTTP/hpp/ErrorResponse.hpp"

static std::string reasonPhrase(int code)
{
    switch (code)
    {
        case 400: return ("Bad Request");//解析失败
        case 403: return ("Forbidden");
        case 404: return ("Not Found");
        case 405: return ("Method Not Allowed");
        case 408: return ("Request Timeout");//读请求超时（通常在 socket 层计时，不在 parser）
        case 411: return ("Length Required");//服务器要求该请求必须明确给出消息体长度，但客户端没有提供。
        case 413: return ("Payload Too Large");//body 太大超出
        case 414: return ("URI Too Long");//uri 太长
        case 415: return ("Unsupported Media Type");//上传但 content-type 不支持（业务层）
        case 431: return ("Request Header Fields Too Large");//header 太大超出
        case 500: return ("Internal Server Error");
        case 501: return ("Not Implemented");//transfor-encoding
        case 502: return ("Bad Gateway");//cgi出错
        case 504: return ("Gateway Timeout");
        case 505: return ("HTTP Version Not Supported");
        default:  return ("Error: Unknown Status");
    }
}

static void AddAllowHeader(HTTPResponse& r)
{
    if (r.statusCode == 405)
        r.headers["allow"] = "GET, POST, DELETE";
}

HTTPResponse buildErrorResponse(int statusCode)
{
    HTTPResponse r;
    r.statusCode = statusCode;
    r.statusText = reasonPhrase(statusCode);

    std::ostringstream body;
    body << statusCode << " " << r.statusText << "\n";
    r.body = body.str();

    //parser header key 是小写，所以这里也用小写
    r.headers["content-type"] = "text/plain; charset=utf-8";
    r.headers["content-length"] = toString(r.body.size());
    r.headers["connection"] = "close";
    AddAllowHeader(r);
    return (r);
}
