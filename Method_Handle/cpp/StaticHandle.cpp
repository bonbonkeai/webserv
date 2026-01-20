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

    const std::size_t MAX_STATIC_FILE = 1024 * 1024 * 10; // 10MB 示例，后续接 config
    std::size_t sz = 0;
    int err = 0;
    if (!FileUtils::fileSize(fullPath, sz, err))
    {
        // stat 都失败了：权限/IO/不存在（理论上 exists 已做过，这里仍防御一下）
        int code = (err == EACCES ? 403 : 500);
        HTTPResponse r = buildErrorResponse(code);
        r.headers["connection"] = (req.keep_alive ? "keep-alive" : "close");
        return (r);
    }
    if (sz > MAX_STATIC_FILE)
    {
        HTTPResponse r = buildErrorResponse(413);
        r.headers["connection"] = (req.keep_alive ? "keep-alive" : "close");
        return (r);
    }

    // std::string content;
    // if (!FileUtils::readAll(fullPath, content))
    // {
    //     HTTPResponse r = buildErrorResponse(500);
    //     r.headers["connection"] = (req.keep_alive ? "keep-alive" : "close");
    //     return (r);
    // }
    std::string content;
    int err = 0;
    if (!FileUtils::readAll(fullPath, content, err))
    {
        int code;
        if (err == EACCES)
            code = 403;
        else
            code = 500;
        HTTPResponse r = buildErrorResponse(code);
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
