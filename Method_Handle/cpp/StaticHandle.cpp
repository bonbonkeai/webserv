#include "Method_Handle/hpp/StaticHandle.hpp"
#include "Method_Handle/hpp/FileUtils.hpp"
#include "HTTP/hpp/ErrorResponse.hpp"
#include "HTTP/hpp/HTTPUtils.hpp"

HTTPResponse StaticHandle::serveFile(const HTTPRequest& req, const std::string& fullPath)
{
    if (!FileUtils::exists(fullPath))
    {
        HTTPResponse r = buildErrorResponse(404);
        r.headers["connection"] = (req.keep_alive ? "keep-alive" : "close");
        return (r);
    }
    if (FileUtils::isDirectory(fullPath))
    {
        HTTPResponse r = buildErrorResponse(403);
        r.headers["connection"] = (req.keep_alive ? "keep-alive" : "close");
        return (r);
    }

    std::string content;
    if (!FileUtils::readAll(fullPath, content))
    {
        HTTPResponse r = buildErrorResponse(500);
        r.headers["connection"] = (req.keep_alive ? "keep-alive" : "close");
        return (r);
    }

    HTTPResponse resp;
    resp.statusCode = 200;
    resp.statusText = "OK";
    resp.body = content;
    resp.headers["content-type"] = FileUtils::guessContentType(fullPath);
    resp.headers["content-length"] = toString(resp.body.size());
    resp.headers["connection"] = (req.keep_alive ? "keep-alive" : "close");
    return (resp);
}
