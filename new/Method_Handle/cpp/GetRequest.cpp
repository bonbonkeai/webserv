#include "Method_Handle/hpp/GetRequest.hpp"
#include "Method_Handle/hpp/FileUtils.hpp"
#include "Method_Handle/hpp/DirectoryHandle.hpp"
#include "Method_Handle/hpp/StaticHandle.hpp"
#include "Method_Handle/hpp/RedirectHandle.hpp"
#include "HTTP/hpp/ErrorResponse.hpp"
#include "HTTP/hpp/HTTPUtils.hpp"

GetRequest::GetRequest(const HTTPRequest &req) : _req(req) {}
GetRequest::~GetRequest() {}

HTTPResponse GetRequest::handle()
{
    const bool AUTO_INDEX = _req.effective.autoindex;
    std::string INDEX_NAME = "index.html";
    if (!_req.effective.index.empty())
        INDEX_NAME = _req.effective.index[0];

    if (!FileUtils::isSafePath(_req.path))
    {
        // HTTPResponse r = buildErrorResponse(400);
        HTTPResponse r = buildConfiguredErrorResponse(400, _req.effective);
        r.headers["connection"] = (_req.keep_alive ? "keep-alive" : "close");
        return r;
    }

    std::string fullPath = _req._rout.fs_path;
    if (fullPath.empty())
        fullPath = FileUtils::joinPath(_req.effective.root, _req.path);

    if (_req.path == "/" || _req.path.empty())
    {
        // 先尝试找 index.html
        std::string indexPath = fullPath + "/index.html";
        if (FileUtils::exists(indexPath) && !FileUtils::isDirectory(indexPath))
        {
            return StaticHandle::serveFile(_req, indexPath);
        }

        // 如果没有 index.html，返回一个简单的欢迎页面（而不是 403）
        HTTPResponse resp;
        resp.statusCode = 200;
        resp.statusText = "OK";
        resp.body = "<!DOCTYPE html><html><head><title>Welcome</title></head><body><h1>Welcome to WebServer</h1><p>Root directory</p></body></html>";
        resp.headers["content-type"] = "text/html; charset=utf-8";
        resp.headers["content-length"] = toString(resp.body.size());
        resp.headers["connection"] = (_req.keep_alive ? "keep-alive" : "close");
        return resp;
    }
    if (FileUtils::isDirectory(fullPath))
    {
        std::string indexPath;
        if (DirectoryHandle::resolveIndex(fullPath, INDEX_NAME, indexPath))
            return StaticHandle::serveFile(_req, indexPath);

        if (!AUTO_INDEX)
        {
            // HTTPResponse r = buildErrorResponse(403);
            HTTPResponse r = buildConfiguredErrorResponse(403, _req.effective);
            r.headers["connection"] = (_req.keep_alive ? "keep-alive" : "close");
            return r;
        }

        std::string html;
        if (!DirectoryHandle::generateAutoIndexHtml(_req.path, fullPath, html))
        {
            // HTTPResponse r = buildErrorResponse(500);
            HTTPResponse r = buildConfiguredErrorResponse(500, _req.effective);
            r.headers["connection"] = (_req.keep_alive ? "keep-alive" : "close");
            return r;
        }

        HTTPResponse resp;
        resp.statusCode = 200;
        resp.statusText = "OK";
        resp.body = html;
        resp.headers["content-type"] = "text/html; charset=utf-8";
        resp.headers["content-length"] = toString(resp.body.size());
        resp.headers["connection"] = (_req.keep_alive ? "keep-alive" : "close");
        return resp;
    }

    return StaticHandle::serveFile(_req, fullPath);
}