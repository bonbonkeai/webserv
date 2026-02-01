#include "Method_Handle/hpp/GetRequest.hpp"
#include "Method_Handle/hpp/FileUtils.hpp"
#include "Method_Handle/hpp/DirectoryHandle.hpp"
#include "Method_Handle/hpp/StaticHandle.hpp"
#include "Method_Handle/hpp/RedirectHandle.hpp"
#include "HTTP/hpp/ErrorResponse.hpp"
#include "HTTP/hpp/HTTPUtils.hpp"

GetRequest::GetRequest(const HTTPRequest& req) : _req(req) {}
GetRequest::~GetRequest() {}

HTTPResponse GetRequest::handle(const EffectiveConfig& cfg)
{
    const std::string ROOT = cfg.root.empty() ? "./www" : cfg.root;
    const bool AUTO_INDEX = cfg.autoindex;
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
        bool foundIndex = false;
        if (!cfg.index.empty())
        {
            for (size_t i = 0; i < cfg.index.size(); ++i)
            {
                if (DirectoryHandle::resolveIndex(fullPath, cfg.index[i], indexPath))
                {
                    foundIndex = true;
                    break;
                }
            }
        }
        else if (DirectoryHandle::resolveIndex(fullPath, "index.html", indexPath))
        {
            foundIndex = true;
        }
        if (foundIndex)
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
