#include "Method_Handle/hpp/DirectoryHandle.hpp"
#include "Method_Handle/hpp/FileUtils.hpp"
#include <dirent.h>
#include <vector>
#include <algorithm>
#include <sstream>


static std::string htmlEscape(const std::string& s)
{
    std::string out;
    for (std::size_t i = 0; i < s.size(); ++i)
    {
        char c = s[i];
        if (c == '&') out += "&amp;";
        else if (c == '<') out += "&lt;";
        else if (c == '>') out += "&gt;";
        else if (c == '"') out += "&quot;";
        else out += c;
    }
    return (out);
}

bool DirectoryHandle::resolveIndex(const std::string& dirFsPath, const std::string& indexName, std::string& outIndexPath)
{
    std::string p = dirFsPath;
    if (!p.empty() && p[p.size() - 1] != '/')
        p += "/";
    p += indexName;
    if (FileUtils::exists(p) && !FileUtils::isDirectory(p))
    {
        outIndexPath = p;
        return (true);
    }
    return (false);
}

bool DirectoryHandle::generateAutoIndexHtml(const std::string& urlPath, const std::string& dirFsPath, std::string& outHtml)
{
    DIR* d = opendir(dirFsPath.c_str());
    if (!d)
        return (false);
    std::vector<std::string> names;
    for (;;)
    {
        struct dirent* ent = readdir(d);
        if (!ent)
            break;

        std::string name(ent->d_name);
        if (name == "." || name == "..") continue;
        // 通常 autoindex 会显示 "..", 在这里暂时隐藏
        names.push_back(name);
    }
    closedir(d);
    std::sort(names.begin(), names.end());
    std::string base = urlPath;
    if (base.empty()) base = "/";
    if (base[base.size() - 1] != '/')
        base += "/";
    std::ostringstream oss;
    oss << "<!doctype html>\n"
        << "<html><head><meta charset=\"utf-8\">"
        << "<title>Index of " << htmlEscape(base) << "</title>"
        << "</head><body>"
        << "<h1>Index of " << htmlEscape(base) << "</h1>"
        << "<ul>";
    for (std::size_t i = 0; i < names.size(); ++i)
    {
        const std::string& n = names[i];
        // 构建链接
        std::string href = base + n;

        // 如果是目录，给链接补一个 '/'
        std::string fsChild = dirFsPath;
        if (!fsChild.empty() && fsChild[fsChild.size() - 1] != '/')
            fsChild += "/";
        fsChild += n;
        bool isDir = FileUtils::isDirectory(fsChild);
        if (isDir)
            href += "/";
        oss << "<li><a href=\"" << htmlEscape(href) << "\">"
            << htmlEscape(n) << (isDir ? "/" : "")
            << "</a></li>";
    }
    oss << "</ul></body></html>\n";
    outHtml = oss.str();
    return (true);
}
