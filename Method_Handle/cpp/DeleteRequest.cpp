#include "Method_Handle/hpp/DeleteRequest.hpp"
#include "Method_Handle/hpp/FileUtils.hpp"
#include "HTTP/hpp/ErrorResponse.hpp"
#include "HTTP/hpp/HTTPUtils.hpp"

DeleteRequest::DeleteRequest(const HTTPRequest& req) : _req(req) {}
DeleteRequest::~DeleteRequest() {}

HTTPResponse DeleteRequest::handle()
{
    const std::string ROOT = "./www";

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
        HTTPResponse r = buildErrorResponse(403);
        r.headers["connection"] = (_req.keep_alive ? "keep-alive" : "close");
        return (r);
    }
    if (!FileUtils::removeFile(fullPath))
    {
        HTTPResponse r = buildErrorResponse(500);
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
