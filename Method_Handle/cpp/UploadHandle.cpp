#include "Method_Handle/hpp/UploadHandle.hpp"
#include "Method_Handle/hpp/FileUtils.hpp"
#include "HTTP/hpp/ErrorResponse.hpp"
#include "HTTP/hpp/HTTPUtils.hpp"
#include <sstream>

// bool UploadHandle::extractBoundary(const std::string& contentType, std::string& outBoundary)
// {
//     // ex: multipart/form-data; boundary=----WebKitFormBoundaryabc123
//     // boundary 可能带引号 boundary="xxx"
//     std::string ct = contentType;
//     // 项目里已有 toLowerInPlace，但这里不能把整个 value lower 掉，因为 boundary 区分大小写
//     // 所以只用 find("boundary=") 在原字符串上查两种情况：boundary= 与 Boundary= 通常不会出现大写，MVP 直接查小写
//     std::string key = "boundary=";
//     std::size_t pos = ct.find(key);
//     if (pos == std::string::npos)
//         return (false);
//     std::string b = ct.substr(pos + key.size());
//     ltrimSpaces(b);
//     rtrimSpaces(b);
//     if (!b.empty() && b[0] == '"')
//     {
//         // boundary="xxx"
//         std::size_t end = b.find('"', 1);
//         if (end == std::string::npos)
//             return false;
//         b = b.substr(1, end - 1);
//     }
//     else
//     {
//         // boundary=xxx; other=...
//         std::size_t semi = b.find(';');
//         if (semi != std::string::npos)
//             b = b.substr(0, semi);
//         rtrimSpaces(b);
//     }
//     if (b.empty())
//         return (false);
//     outBoundary = b;
//     return (true);
// }

bool UploadHandle::extractBoundary(const std::string& contentType, std::string& outBoundary)
{
    // 支持：
    // multipart/form-data; boundary=xxx
    // multipart/form-data; Boundary=xxx
    // multipart/form-data; boundary = xxx
    // multipart/form-data; boundary="xxx"
    std::size_t pos = std::string::npos;
    std::string key;

    // 1) 先找最常见的 "boundary="
    pos = contentType.find("boundary=");
    if (pos != std::string::npos)
    {
        key = "boundary=";
    }
    else
    {
        // 2) 再找 "Boundary="
        pos = contentType.find("Boundary=");
        if (pos != std::string::npos)
            key = "Boundary=";
        else
        {
            // 3) 再找 "boundary ="
            pos = contentType.find("boundary");
            if (pos == std::string::npos)
                return (false);
            // boundary 后面允许空格，再允许 '='
            std::size_t i = pos + std::string("boundary").size();
            // 跳过空格
            while (i < contentType.size() && (contentType[i] == ' ' || contentType[i] == '\t'))
                ++i;
            if (i >= contentType.size() || contentType[i] != '=')
                return (false);
            // key 的长度我们用 i-pos+1 来描述（boundary[spaces]=）
            key = contentType.substr(pos, (i - pos) + 1); // 形如 "boundary   ="
        }
    }
    std::string b = contentType.substr(pos + key.size());
    ltrimSpaces(b);
    rtrimSpaces(b);

    if (!b.empty() && b[0] == '"')
    {
        // boundary="xxx"
        std::size_t end = b.find('"', 1);
        if (end == std::string::npos)
            return (false);
        b = b.substr(1, end - 1);
    }
    else
    {
        // boundary=xxx; other=...
        std::size_t semi = b.find(';');
        if (semi != std::string::npos)
            b = b.substr(0, semi);
        rtrimSpaces(b);
    }
    if (b.empty())
        return (false);
    outBoundary = b;
    return (true);
}


static std::string getParamQuoted(const std::string& s, const std::string& key)
{
    // 在 Content-Disposition 一行里找 key="..."
    // 返回引号内内容，不存在则 ""
    std::string needle = key + "=\"";
    std::size_t p = s.find(needle);
    if (p == std::string::npos)
        return ("");
    p += needle.size();
    std::size_t end = s.find('"', p);
    if (end == std::string::npos)
        return ("");
    return (s.substr(p, end - p));
}

bool UploadHandle::parsePartHeaders(const std::string& headersBlock, std::string& outName, std::string& outFilename, std::string& outPartContentType)
{
    // headersBlock: 多行，已不包含最后的 \r\n\r\n
    // 找到 content-disposition 行，从中提取 name/filename
    // 以及可选 content-type
    outName = "";
    outFilename = "";
    outPartContentType = "";

    std::size_t start = 0;
    while (start < headersBlock.size())
    {
        std::size_t end = headersBlock.find("\r\n", start);
        if (end == std::string::npos)
            end = headersBlock.size();
        std::string line = headersBlock.substr(start, end - start);
        start = (end == headersBlock.size()) ? end : end + 2;

        // 拆 header: key: value
        std::size_t colon = line.find(':');
        if (colon == std::string::npos)
            continue;

        std::string key = line.substr(0, colon);
        std::string val = line.substr(colon + 1);
        rtrimSpaces(key);
        toLowerInPlace(key);
        ltrimSpaces(val);
        rtrimSpaces(val);

        if (key == "content-disposition")
        {
            // val 例: form-data; name="file"; filename="a.png"
            outName = getParamQuoted(val, "name");
            outFilename = getParamQuoted(val, "filename");
        }
        else if (key == "content-type")
        {
            outPartContentType = val;
        }
    }
    return (true);
}

std::string UploadHandle::sanitizeFilename(const std::string& filename)
{
    // 去掉路径分隔符与 ..，避免目录穿越
    if (filename.empty())
        return ("");
    std::string f = filename;

    // Windows 路径 "C:\a\b.png" 或浏览器带全路径的情况
    // 取最后一个 '/' 或 '\'
    std::size_t slash = f.find_last_of("/\\");
    if (slash != std::string::npos)
        f = f.substr(slash + 1);

    if (f.find("..") != std::string::npos)
        return ("");

    // 禁止再包含分隔符
    if (f.find('/') != std::string::npos || f.find('\\') != std::string::npos)
        return ("");
    // 过滤奇怪字符，MVP 只做最基本
    if (f.find('\0') != std::string::npos)
        return ("");
    return (f);
}

std::string UploadHandle::buildSuccessHtml(const std::string& savedAs)
{
    std::ostringstream oss;
    oss << "<!doctype html>\n<html><head><meta charset=\"utf-8\">"
        << "<title>Upload OK</title></head><body>"
        << "<h1>Upload OK</h1>"
        << "<p>Saved as: " << savedAs << "</p>"
        << "</body></html>\n";
    return (oss.str());
}

bool UploadHandle::handleMultipart(const HTTPRequest& req, const std::string& uploadDir, HTTPResponse& outResp)
{
    // 1) Content-Type / boundary
    if (!req.headers.count("content-type"))
    {
        outResp = buildErrorResponse(400);
        outResp.headers["connection"] = (req.keep_alive ? "keep-alive" : "close");
        return (false);
    }

    const std::string& ct = req.headers.find("content-type")->second;
    // ps：ct 可能带大小写差异，MVP 用查找 "multipart/form-data"
    if (ct.find("multipart/form-data") == std::string::npos)
    {
        outResp = buildErrorResponse(400);
        outResp.headers["connection"] = (req.keep_alive ? "keep-alive" : "close");
        return (false);
    }

    std::string boundary;
    if (!extractBoundary(ct, boundary))
    {
        outResp = buildErrorResponse(400);
        outResp.headers["connection"] = (req.keep_alive ? "keep-alive" : "close");
        return (false);
    }

    // multipart 的边界串在 body 里表现为：
    // --<boundary>\r\n ... \r\n\r\n <data> \r\n--<boundary>...
    std::string delim = "--" + boundary;
    std::string delimEnd = "--" + boundary + "--";

    const std::string& body = req.body;

    // 2) 定位第一个 boundary
    std::size_t pos = body.find(delim);
    if (pos == std::string::npos)
    {
        outResp = buildErrorResponse(400);
        outResp.headers["connection"] = (req.keep_alive ? "keep-alive" : "close");
        return (false);
    }

    // 3) 逐 part 扫描，找到第一个带 filename 的 part（MVP）
    //    你也可以扩展为多文件循环保存
    bool saved = false;
    std::string savedName;

    while (pos != std::string::npos)
    {
        // 结束 boundary
        if (body.compare(pos, delimEnd.size(), delimEnd) == 0)
            break;

        // 跳过 delim 行
        pos += delim.size();

        // boundary 后面必须是 \r\n
        if (pos + 2 > body.size() || body.compare(pos, 2, "\r\n") != 0)
        {
            outResp = buildErrorResponse(400);
            outResp.headers["connection"] = (req.keep_alive ? "keep-alive" : "close");
            return (false);
        }
        pos += 2;

        // part headers 到 \r\n\r\n
        std::size_t headerEnd = body.find("\r\n\r\n", pos);
        if (headerEnd == std::string::npos)
        {
            outResp = buildErrorResponse(400);
            outResp.headers["connection"] = (req.keep_alive ? "keep-alive" : "close");
            return (false);
        }

        std::string headersBlock = body.substr(pos, headerEnd - pos);
        pos = headerEnd + 4; // 跳过 \r\n\r\n

        // part data 直到下一个 \r\n--boundary
        std::string nextNeedle = "\r\n" + delim;
        std::size_t next = body.find(nextNeedle, pos);
        if (next == std::string::npos)
        {
            outResp = buildErrorResponse(400);
            outResp.headers["connection"] = (req.keep_alive ? "keep-alive" : "close");
            return (false);
        }

        std::string partData = body.substr(pos, next - pos);
        pos = next + 2; // 让 pos 指向 "--boundary" 的开头（跳过前导 \r\n 的 \r\n）

        std::string name, filename, partCt;
        parsePartHeaders(headersBlock, name, filename, partCt);

        if (!filename.empty())
        {
            std::string safe = sanitizeFilename(filename);
            if (safe.empty())
            {
                outResp = buildErrorResponse(403);
                outResp.headers["connection"] = (req.keep_alive ? "keep-alive" : "close");
                return (false);
            }

            // 拼存储路径
            std::string path = uploadDir;
            if (!path.empty() && path[path.size() - 1] != '/')
                path += "/";
            path += safe;

            // 写文件
            if (!FileUtils::writeAllBinary(path, partData))
            {
                outResp = buildErrorResponse(500);
                outResp.headers["connection"] = (req.keep_alive ? "keep-alive" : "close");
                return (false);
            }

            saved = true;
            savedName = safe;
            break; // MVP: 只保存第一个文件字段
        }

        // 找下一个 boundary（pos 当前指向 "--boundary..."）
        // 直接继续 while，pos 已经在 boundary 开头
        // 不用再 find，下一轮从当前 pos 开始即可
        if (pos >= body.size())
            break;
        if (body.compare(pos, delim.size(), delim) != 0)
        {
            // 若没对齐，尝试 find
            pos = body.find(delim, pos);
        }
    }

    if (!saved)
    {
        // 没找到 filename 字段
        outResp = buildErrorResponse(400);
        outResp.headers["connection"] = (req.keep_alive ? "keep-alive" : "close");
        return (false);
    }

    // 4) 成功响应（HTML）
    outResp.statusCode = 201;
    outResp.statusText = "Created";
    outResp.body = buildSuccessHtml(savedName);
    outResp.headers["content-type"] = "text/html; charset=utf-8";
    outResp.headers["content-length"] = toString(outResp.body.size());
    outResp.headers["connection"] = (req.keep_alive ? "keep-alive" : "close");
    return (true);
}
