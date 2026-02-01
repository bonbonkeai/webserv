#include "Method_Handle/hpp/GetRequest.hpp"
#include "Method_Handle/hpp/FileUtils.hpp"
#include "Method_Handle/hpp/DirectoryHandle.hpp"
#include "Method_Handle/hpp/StaticHandle.hpp"
#include "Method_Handle/hpp/RedirectHandle.hpp"
#include "HTTP/hpp/ErrorResponse.hpp"
#include "HTTP/hpp/HTTPUtils.hpp"

GetRequest::GetRequest(const HTTPRequest& req) : _req(req) {}
GetRequest::~GetRequest() {}

HTTPResponse GetRequest::handle()
{
    // const std::string ROOT = "./www";
    const std::string ROOT = _req.effective.root;
    // const bool AUTO_INDEX = true; // MVP 写死，后面接 config 替换
    const bool AUTO_INDEX = _req.effective.autoindex;
    const std::string INDEX_NAME = "index.html";
    if (!_req.effective.index.empty())
        INDEX_NAME = _req.effective.index[0];
    
    // 1) path 安全
    if (!FileUtils::isSafePath(_req.path))
    {
        HTTPResponse r = buildErrorResponse(400);
        r.headers["connection"] = (_req.keep_alive ? "keep-alive" : "close");
        return (r);
    }

    // 2) 拼出文件系统路径
    std::string fullPath = FileUtils::joinPath(ROOT, _req.path);

    // 3) 目录分支：index / autoindex
    if (FileUtils::isDirectory(fullPath))
    {
        std::string indexPath;
        if (DirectoryHandle::resolveIndex(fullPath, INDEX_NAME, indexPath))
        {
            // 找到 index，转为静态文件
            return (StaticHandle::serveFile(_req, indexPath));
        }

        // 没有 index：autoindex on/off
        if (!AUTO_INDEX)
        {
            HTTPResponse r = buildErrorResponse(403);
            r.headers["connection"] = (_req.keep_alive ? "keep-alive" : "close");
            return (r);
        }

        std::string html;
        if (!DirectoryHandle::generateAutoIndexHtml(_req.path, fullPath, html))
        {
            HTTPResponse r = buildErrorResponse(500);
            r.headers["connection"] = (_req.keep_alive ? "keep-alive" : "close");
            return (r);
        }

        HTTPResponse resp;
        resp.statusCode = 200;
        resp.statusText = "OK";
        resp.body = html;
        resp.headers["content-type"] = "text/html; charset=utf-8";
        resp.headers["content-length"] = toString(resp.body.size());
        resp.headers["connection"] = (_req.keep_alive ? "keep-alive" : "close");
        return (resp);
    }

    // 4) 非目录：统一交给 StaticHandle
    return (StaticHandle::serveFile(_req, fullPath));
}