#include "Method_Handle/hpp/PostRequest.hpp"
#include "Method_Handle/hpp/FileUtils.hpp"
#include "Method_Handle/hpp/UploadHandle.hpp"
#include "HTTP/hpp/ErrorResponse.hpp"
#include "HTTP/hpp/HTTPUtils.hpp"

static bool isUploadEndpoint(const std::string& path)
{
    return (path == "/upload" || path == "/upload/");
}

static std::string basenameUpload(const std::string& path)
{
    // ps：/upload/<filename> 且 filename 不允许包含 '/'
    const std::string prefix = "/upload/";
    if (!FileUtils::startsWith(path, prefix))
        return ("");

    std::string name = path.substr(prefix.size());
    if (name.empty())
        return ("");
    if (name.find('/') != std::string::npos)
        return ("");
    if (name.find("..") != std::string::npos)
        return ("");
    if (name.find('\0') != std::string::npos)
        return ("");
    return (name);
}

PostRequest::PostRequest(const HTTPRequest& req) : _req(req) {}
PostRequest::~PostRequest() {}

/*
POST /foo.txt → 404
POST /upload 非 multipart → 415
POST /upload/<file> raw → 201
POST /upload/<file> multipart → 404
*/

HTTPResponse PostRequest::handleRawUploadFallback()
{
    const std::string UPLOAD_DIR = "./upload";

    if (!FileUtils::isSafePath(_req.path))
    {
        HTTPResponse r = buildErrorResponse(400);
        r.headers["connection"] = (_req.keep_alive ? "keep-alive" : "close");
        return (r);
    }
    
    // 先判断是不是 /upload/ 前缀
    const std::string prefix = "/upload/";
    if (!FileUtils::startsWith(_req.path, prefix))
    {
        // POST 到非 upload 路径：404（或统一为 405）
        HTTPResponse r = buildErrorResponse(404);
        r.headers["connection"] = (_req.keep_alive ? "keep-alive" : "close");
        return (r);
    }

    // // POST 到 /upload/<file>
    // std::string filename = basenameUpload(_req.path);
    // if (filename.empty())
    // {
    //     HTTPResponse r = buildErrorResponse(403);
    //     r.headers["connection"] = (_req.keep_alive ? "keep-alive" : "close");
    //     return (r);
    // }

    // 再做 /upload/<filename> 的 basename 校验
    std::string filename = basenameUpload(_req.path);
    if (filename.empty())
    {
        // /upload/ 但文件名不合法：更合理是 400（格式不对）
        HTTPResponse r = buildErrorResponse(400);
        r.headers["connection"] = (_req.keep_alive ? "keep-alive" : "close");
        return (r);
    }

    std::string fullPath = UPLOAD_DIR;
    if (!fullPath.empty() && fullPath[fullPath.size() - 1] != '/')
        fullPath += "/";
    fullPath += filename;
    if (!FileUtils::writeAllBinary(fullPath, _req.body))
    {
        HTTPResponse r = buildErrorResponse(500);
        r.headers["connection"] = (_req.keep_alive ? "keep-alive" : "close");
        return (r);
    }
    HTTPResponse resp;
    resp.statusCode = 201;
    resp.statusText = "Created";
    resp.body = "Uploaded\n";
    resp.headers["content-type"] = "text/plain; charset=utf-8";
    resp.headers["content-length"] = toString(resp.body.size());
    resp.headers["connection"] = (_req.keep_alive ? "keep-alive" : "close");
    return (resp);
}

HTTPResponse PostRequest::handle()
{
    const bool hasCT = _req.headers.count("content-type");
    const std::string ct = hasCT ? _req.headers.find("content-type")->second : "";
    //multipart 判定应该大小写不敏感
    bool isMultipart = false;
    if (hasCT)
    {
        std::string main = FileUtils::mimeMainLower(ct);
        isMultipart = (main == "multipart/form-data");
    }
    // const bool isMultipart = hasCT && ct.find("multipart/form-data") != std::string::npos;
    // ---------- case 1：POST /upload ----------
    if (isUploadEndpoint(_req.path))
    {
        // 只允许 multipart
        if (!isMultipart)
        {
            HTTPResponse r = buildErrorResponse(415); // Unsupported Media Type
            r.headers["connection"] = (_req.keep_alive ? "keep-alive" : "close");
            return (r);
        }
        HTTPResponse resp;
        const std::string UPLOAD_DIR = "./upload";
        if (UploadHandle::handleMultipart(_req, UPLOAD_DIR, resp))
            return (resp);
        return (resp); // 错误已填充
    }
    // ---------- case 2：multipart 但不是 /upload ----------
    if (isMultipart)
    {
        HTTPResponse r = buildErrorResponse(404);
        r.headers["connection"] = (_req.keep_alive ? "keep-alive" : "close");
        return (r);
    }
    // ---------- case 3：fallback raw upload (/upload/<filename>) ----------
    return handleRawUploadFallback();
}


