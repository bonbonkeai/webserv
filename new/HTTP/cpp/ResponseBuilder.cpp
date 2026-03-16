#include "HTTP/hpp/ResponseBuilder.hpp"

static void ensureRequiredHeaders(HTTPResponse& resp)
{
    // Content-Length: 必须存在
    if (resp.headers.find("content-length") == resp.headers.end())
        resp.headers["content-length"] = toString(resp.body.size());

    // Connection: 先默认 close，后续接 keep-alive 再覆盖
    if (resp.headers.find("connection") == resp.headers.end())
        resp.headers["connection"] = "close";

    // Content-Type: 默认一个
    if (resp.headers.find("content-type") == resp.headers.end())
        resp.headers["content-type"] = "text/plain; charset=utf-8";
}

std::string ResponseBuilder::buildStatusLine(const HTTPResponse &resp)
{
    std::ostringstream oss;
    oss << "HTTP/1.1 " << resp.statusCode << " " << resp.statusText << "\r\n";
    return (oss.str());
}

std::string ResponseBuilder::buildHeaders(const HTTPResponse &resp)
{
    std::ostringstream oss;
    for (std::map<std::string, std::string>::const_iterator it = resp.headers.begin();
         it != resp.headers.end(); ++it)
    {
        oss << it->first << ": " << it->second << "\r\n";
    }
    return (oss.str());
}

std::string ResponseBuilder::buildDataHeader()
{
    return ("\r\n");
}

std::string ResponseBuilder::build(const HTTPResponse& in)
{
    // in是const，所以得拷贝一份
    HTTPResponse resp = in;
    ensureRequiredHeaders(resp);
    std::string out;
    out += buildStatusLine(resp);
    out += buildHeaders(resp);
    out += buildDataHeader();
    out += resp.body;
    return (out);
}