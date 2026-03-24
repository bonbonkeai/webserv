#include "HTTP/hpp/ErrorResponse.hpp"


static std::string reasonPhrase(int code)
{
    switch (code)
    {
        case 400: return ("Bad Request");//解析失败
        case 403: return ("Forbidden");
        case 404: return ("Not Found");
        case 405: return ("Method Not Allowed");
        case 408: return ("Request Timeout");//读请求超时（通常在 socket 层计时，不在 parser）
        case 411: return ("Length Required");//服务器要求该请求必须明确给出消息体长度，但客户端没有提供。
        case 413: return ("Payload Too Large");//body 太大超出
        case 414: return ("URI Too Long");//uri 太长
        case 415: return ("Unsupported Media Type");//上传但 content-type 不支持（业务层）
        case 431: return ("Request Header Fields Too Large");//header 太大超出
        case 500: return ("Internal Server Error");
        case 501: return ("Not Implemented");//transfor-encoding
        case 502: return ("Bad Gateway");//cgi出错
        case 504: return ("Gateway Timeout");
        case 505: return ("HTTP Version Not Supported");
        default:  return ("Error: Unknown Status");
    }
}

static void AddAllowHeader(HTTPResponse& r)
{
    if (r.statusCode == 405)
        r.headers["allow"] = "GET, POST, DELETE";
}

HTTPResponse buildErrorResponse(int statusCode)
{
    HTTPResponse r;
    r.statusCode = statusCode;
    r.statusText = reasonPhrase(statusCode);

    std::ostringstream body;
    body << statusCode << " " << r.statusText << "\n";
    r.body = body.str();

    //parser header key 是小写，所以这里也用小写
    r.headers["content-type"] = "text/plain; charset=utf-8";
    r.headers["content-length"] = toString(r.body.size());
    r.headers["connection"] = "close";
    AddAllowHeader(r);
    return (r);
}
//
static bool readWholeFile(const std::string& path, std::string& out)
{
    std::ifstream ifs(path.c_str(), std::ios::in | std::ios::binary);
    if (!ifs.is_open())
        return false;
    std::ostringstream ss;
    ss << ifs.rdbuf();
    if (!ifs.good() && !ifs.eof())
        return false;
    out = ss.str();
    return true;
}

static std::string guessContentType(const std::string& path)
{
    std::size_t dot = path.rfind('.');
    if (dot == std::string::npos)
        return "text/plain; charset=utf-8";

    std::string ext = path.substr(dot + 1);
    for (std::size_t i = 0; i < ext.size(); ++i)
        ext[i] = static_cast<char>(std::tolower(ext[i]));
    if (ext == "html" || ext == "htm")
        return "text/html; charset=utf-8";
    if (ext == "css")
        return "text/css; charset=utf-8";
    if (ext == "txt")
        return "text/plain; charset=utf-8";
    return "application/octet-stream";
}

static bool uriToPathForErrorPage(const EffectiveConfig& cfg,
                                  const std::string& uri,
                                  std::string& outPath)
{
    if (cfg.root.empty() || uri.empty())
        return false;
    if (uri[0] != '/')
        return false;
    if (uri.find("..") != std::string::npos)
        return false;
    if (cfg.root[cfg.root.size() - 1] == '/')
        outPath = cfg.root.substr(0, cfg.root.size() - 1) + uri;
    else
        outPath = cfg.root + uri;
    return true;
}

HTTPResponse buildConfiguredErrorResponse(int statusCode, const EffectiveConfig& cfg)
{
    HTTPResponse fallback = buildErrorResponse(statusCode);
    std::map<int, ErrorPageRule>::const_iterator it = cfg.error_pages.find(statusCode);
    if (it == cfg.error_pages.end())
        return fallback;
    const ErrorPageRule& rule = it->second;
    if (rule.uri.empty())
        return fallback;
    std::string filePath;
    if (!uriToPathForErrorPage(cfg, rule.uri, filePath))
        return fallback;
    std::string fileBody;
    if (!readWholeFile(filePath, fileBody))
        return fallback;
    HTTPResponse r;
    r.statusCode = statusCode;
    r.statusText = fallback.statusText;
    r.body = fileBody;
    r.headers["content-type"] = guessContentType(filePath);
    r.headers["content-length"] = toString(r.body.size());
    r.headers["connection"] = "close";
    AddAllowHeader(r);
    return r;
}