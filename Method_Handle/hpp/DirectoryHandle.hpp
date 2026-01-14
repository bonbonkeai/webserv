/*处理 “请求的路径指向的是一个目录f”的情况
查找 index 文件
如果没有 index
autoindex=on → 生成目录列表
autoindex=off → 返回错误*/

#ifndef DIRECTORYHANDLE_HPP
#define DIRECTORYHANDLE_HPP

#include <string>

class DirectoryHandle
{
public:
        // 尝试在目录下找 index 文件，成功返回 true，并把 outIndexPath 填成完整路径
        static bool resolveIndex(const std::string& dirFsPath, const std::string& indexName, std::string& outIndexPath);

        // 生成 autoindex 的 HTML 内容
        // urlPath: 请求中的路径（例如 "/imgs/"）
        // dirFsPath: 文件系统目录（例如 "./www/imgs"）
        static bool generateAutoIndexHtml(const std::string& urlPath, const std::string& dirFsPath, std::string& outHtml);
};

#endif
