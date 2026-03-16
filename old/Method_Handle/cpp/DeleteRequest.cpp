#include "Method_Handle/hpp/DeleteRequest.hpp"
#include "Method_Handle/hpp/FileUtils.hpp"
#include "HTTP/hpp/ErrorResponse.hpp"
#include "HTTP/hpp/HTTPUtils.hpp"

DeleteRequest::DeleteRequest(const HTTPRequest& req) : _req(req) {}
DeleteRequest::~DeleteRequest() {}

HTTPResponse DeleteRequest::handle()
{
    // const std::string ROOT = "./www";
    const std::string ROOT = _req.effective.root;

    if (!FileUtils::isSafePath(_req.path))
    {
        HTTPResponse r = buildErrorResponse(400);
        r.headers["connection"] = (_req.keep_alive ? "keep-alive" : "close");
        return (r);
    }

    std::string fullPath = FileUtils::joinPath(ROOT, _req.path);
    if (!FileUtils::exists(fullPath))
    {
        HTTPResponse r = buildErrorResponse(404);
        r.headers["connection"] = (_req.keep_alive ? "keep-alive" : "close");
        return (r);
    }
    if (FileUtils::isDirectory(fullPath))
    {
        HTTPResponse r = buildErrorResponse(405);
        r.headers["allow"] = "GET"; // 覆盖 buildErrorResponse 的默认 allow->不然要改 buildErrorResponse的传参很麻烦
        r.headers["connection"] = (_req.keep_alive ? "keep-alive" : "close");
        return (r);
    }
    // if (!FileUtils::removeFile(fullPath))
    // {
    //     HTTPResponse r = buildErrorResponse(500);
    //     r.headers["connection"] = (_req.keep_alive ? "keep-alive" : "close");
    //     return (r);
    // }
    int e = 0;
    if (!FileUtils::removeFileErrno(fullPath, e))
    {
        int code = 500;
        if (e == ENOENT) code = 404;
        else if (e == EACCES || e == EPERM) code = 403;
        HTTPResponse r = buildErrorResponse(code);
        r.headers["connection"] = (_req.keep_alive ? "keep-alive" : "close");
        return (r);
    }

    HTTPResponse resp;
    resp.statusCode = 200;
    resp.statusText = "OK";
    resp.body = "Deleted\n";
    resp.headers["content-type"] = "text/plain; charset=utf-8";
    resp.headers["content-length"] = toString(resp.body.size());
    resp.headers["connection"] = (_req.keep_alive ? "keep-alive" : "close");
    return (resp);
}
