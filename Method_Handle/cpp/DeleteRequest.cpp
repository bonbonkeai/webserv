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
        // HTTPResponse r = buildErrorResponse(400);
        HTTPResponse r = buildConfiguredErrorResponse(400, _req.effective);
        r.headers["connection"] = (_req.keep_alive ? "keep-alive" : "close");
        return (r);
    }
    std::cerr << "[DELETE] _req.path = [" << _req.path << "]\n";
    std::cerr << "[DELETE] effective.root = [" << _req.effective.root << "]\n";
    std::cerr << "[DELETE] routed fs_path = [" << _req._rout.fs_path << "]\n";

    // std::string fullPath = FileUtils::joinPath(ROOT, _req.path);
    // std::string fullPath = _req._rout.fs_path;
    std::string fullPath;
    if (FileUtils::startsWith(_req.path, "/upload/"))
        fullPath = _req._rout.fs_path;
    
    if (fullPath.empty())
        fullPath = FileUtils::joinPath(_req.effective.root, _req.path);
    std::cerr << "[DELETE] final fullPath = [" << fullPath << "]\n";
    std::cerr << "[DELETE] exists = [" << FileUtils::exists(fullPath) << "]\n";
    if (!FileUtils::exists(fullPath))
    {
        // HTTPResponse r = buildErrorResponse(404);
        HTTPResponse r = buildConfiguredErrorResponse(404, _req.effective);
        r.headers["connection"] = (_req.keep_alive ? "keep-alive" : "close");
        return (r);
    }
    if (FileUtils::isDirectory(fullPath))
    {
        // HTTPResponse r = buildErrorResponse(405);
        HTTPResponse r = buildConfiguredErrorResponse(405, _req.effective);
        r.headers["allow"] = "GET";
        r.headers["connection"] = (_req.keep_alive ? "keep-alive" : "close");
        return (r);
    }
    int e = 0;
    if (!FileUtils::removeFileErrno(fullPath, e))
    {
        int code = 500;
        if (e == ENOENT) code = 404;
        else if (e == EACCES || e == EPERM) code = 403;
        // HTTPResponse r = buildErrorResponse(code);
        HTTPResponse r = buildConfiguredErrorResponse(code, _req.effective);
        r.headers["connection"] = (_req.keep_alive ? "keep-alive" : "close");
        return (r);
    }

    // HTTPResponse resp;
    // resp.statusCode = 200;
    // resp.statusText = "OK";
    // resp.body = "Deleted\n";
    // resp.headers["content-type"] = "text/plain; charset=utf-8";
    // resp.headers["content-length"] = toString(resp.body.size());
    // resp.headers["connection"] = (_req.keep_alive ? "keep-alive" : "close");
    // return (resp);
    HTTPResponse resp;
    resp.statusCode = 204;
    resp.statusText = "No Content";
    resp.body = "";
    resp.headers["content-length"] = "0";
    resp.headers["connection"] = (_req.keep_alive ? "keep-alive" : "close");
    return resp;
}
