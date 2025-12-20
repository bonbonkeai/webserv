#include "HTTP/hpp/ErrorResponse.hpp"

static std::string reasonPhrase(int code)
{
    switch (code)
    {
        case 400: return ("Bad Request");
        case 403: return ("Forbidden");
        case 404: return ("Not Found");
        case 405: return ("Method Not Allowed");
        case 500: return ("Internal Server Error");
        case 505: return ("HTTP Version Not Supported");
        default:  return ("Error");
    }
}

HTTPResponse buildErrorResponse(int statusCode)
{
    HTTPResponse r;
    r.statusCode = statusCode;
    r.statusText = reasonPhrase(statusCode);

    std::ostringstream body;
    body << statusCode << " " << r.statusText << "\n";
    r.body = body.str();

    // 你 parser header key 是小写，所以这里也用小写
    r.headers["content-type"] = "text/plain; charset=utf-8";
    r.headers["content-length"] = toString(r.body.size());
    r.headers["connection"] = "close";
    return (r);
}
