#ifndef UPLOADHANDLE_HPP
#define UPLOADHANDLE_HPP

#include <string>
#include "HTTP/hpp/HTTPRequest.hpp"
#include "HTTP/hpp/HTTPResponse.hpp"

class UploadHandle
{
public:
        // 解析 multipart/form-data，并把文件写入 uploadDir
        // 返回 true 表示成功并填充 outResp，false 表示失败并填充 outResp（错误响应）
        static bool handleMultipart(const HTTPRequest& req, const std::string& uploadDir, HTTPResponse& outResp);

private:
        static bool extractBoundary(const std::string& contentType, std::string& outBoundary);
        static bool parsePartHeaders(const std::string& headersBlock,
                             std::string& outName,
                             std::string& outFilename,
                             std::string& outPartContentType);
        static std::string sanitizeFilename(const std::string& filename);
        static std::string buildSuccessHtml(const std::string& savedAs);
        // 工具：大小写不敏感 startsWith（MVP 用）
        static bool startsWith(const std::string& s, const std::string& prefix);
};

#endif


/*负责 multipart/form-data 上传：
解析 boundary
找出文件字段
保存到 upload_path
返回成功页面*/