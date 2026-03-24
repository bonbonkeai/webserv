#include "Method_Handle/hpp/PostRequest.hpp"
#include "Method_Handle/hpp/FileUtils.hpp"
#include "Method_Handle/hpp/UploadHandle.hpp"
#include "HTTP/hpp/ErrorResponse.hpp"
#include "HTTP/hpp/HTTPUtils.hpp"

static bool isMultipartUploadEndpoint(const std::string& path)
{
    return (path == "/upload");
}

static bool isUploadDirPath(const std::string& path)
{
    return (path == "/upload/");
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

static std::string resolveUploadDir(const HTTPRequest& req)
{
    if (req.effective.has_upload_path && !req.effective.upload_path.empty())
        return req.effective.upload_path;
    return FileUtils::joinPath(req.effective.root, "upload");
}

HTTPResponse PostRequest::handleRawUploadFallback()
{
    const std::string UPLOAD_DIR = resolveUploadDir(_req);

    if (!FileUtils::isSafePath(_req.path))
    {
        HTTPResponse r = buildConfiguredErrorResponse(400, _req.effective);
        r.headers["connection"] = (_req.keep_alive ? "keep-alive" : "close");
        return (r);
    }
    
    // 先判断是不是 /upload/ 前缀
    const std::string prefix = "/upload/";
    if (!FileUtils::startsWith(_req.path, prefix))
    {
        // POST 到非 upload 路径：404（或统一为 405）
        HTTPResponse r = buildConfiguredErrorResponse(404, _req.effective);
        r.headers["connection"] = (_req.keep_alive ? "keep-alive" : "close");
        return (r);
    }

    // 再做 /upload/<filename> 的 basename 校验
    std::string filename = basenameUpload(_req.path);
    if (filename.empty())
    {
        // /upload/ 但文件名不合法：更合理是 400（格式不对）
        HTTPResponse r = buildConfiguredErrorResponse(400, _req.effective);
        r.headers["connection"] = (_req.keep_alive ? "keep-alive" : "close");
        return (r);
    }
    //
    if (_req.body.empty())
    {
        HTTPResponse r = buildConfiguredErrorResponse(400, _req.effective);
        r.headers["connection"] = (_req.keep_alive ? "keep-alive" : "close");
        return (r);
    }

    if (_req.body.size() > _req.max_body_size)
    {
        HTTPResponse r = buildConfiguredErrorResponse(413, _req.effective);
        r.headers["connection"] = (_req.keep_alive ? "keep-alive" : "close");
        return (r);
    }
    //
    std::string fullPath = UPLOAD_DIR;
    if (!fullPath.empty() && fullPath[fullPath.size() - 1] != '/')
        fullPath += "/";
    fullPath += filename;
    int e = 0;
    if (!FileUtils::writeAllBinaryErrno(fullPath, _req.body, e))
    {
        int code = 500;
        if (e == EACCES || e == EPERM)
            code = 403;
        HTTPResponse r = buildConfiguredErrorResponse(code, _req.effective);
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
    
   // ---------- case 1：POST /upload ----------
    // 这是 multipart upload 的正式 endpoint
    if (isMultipartUploadEndpoint(_req.path))
    {
        if (!isMultipart)
        {
            HTTPResponse r = buildConfiguredErrorResponse(415, _req.effective);
            r.headers["connection"] = (_req.keep_alive ? "keep-alive" : "close");
            return r;
        }

        HTTPResponse resp;
        const std::string UPLOAD_DIR = resolveUploadDir(_req);
        if (UploadHandle::handleMultipart(_req, UPLOAD_DIR, resp))
            return resp;
        return resp;
    }

    // ---------- case 2：POST /upload/ ----------
    // 缺少 filename 的目录路径
    if (isUploadDirPath(_req.path))
    {
        if (isMultipart)
        {
            // 如果你们希望 /upload/ 也接受 multipart，这里可以复用 multipart handler
            HTTPResponse resp;
            const std::string UPLOAD_DIR = resolveUploadDir(_req);
            if (UploadHandle::handleMultipart(_req, UPLOAD_DIR, resp))
                return resp;
            return resp;
        }
        // raw body 发到 /upload/，属于格式错误：没有 filename
        HTTPResponse r = buildConfiguredErrorResponse(400, _req.effective);
        r.headers["connection"] = (_req.keep_alive ? "keep-alive" : "close");
        return r;
    }

    // ---------- case 3：multipart 但路径不是 /upload 或 /upload/ ----------
    if (isMultipart)
    {
        HTTPResponse r = buildConfiguredErrorResponse(415, _req.effective);
        r.headers["connection"] = (_req.keep_alive ? "keep-alive" : "close");
        return r;
    }

    // ---------- case 4：fallback raw upload (/upload/<filename>) ----------
    return handleRawUploadFallback();
}


