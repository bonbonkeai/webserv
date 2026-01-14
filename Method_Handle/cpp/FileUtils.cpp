#include "Method_Handle/hpp/FileUtils.hpp"
#include <sys/stat.h>
#include <fstream>
#include <cstdio>

bool FileUtils::isSafePath(const std::string& urlPath)
{
    if (urlPath.empty())
        return (false);
    if (urlPath[0] != '/')
        return (false);
    if (urlPath.find('\0') != std::string::npos)
        return (false);
    if (urlPath.find("..") != std::string::npos)
        return (false);
    return (true);
}

std::string FileUtils::joinPath(const std::string& root, const std::string& urlPath)
{
    if (root.empty())
        return (urlPath);
    if (urlPath.empty())
        return (root);

    if (root[root.size() - 1] == '/')
        return (root.substr(0, root.size() - 1) + urlPath);
    return (root + urlPath);
}

bool FileUtils::exists(const std::string& path)
{
    struct stat st;
    return ::stat(path.c_str(), &st) == 0;
}

bool FileUtils::isDirectory(const std::string& path)
{
    struct stat st;
    if (::stat(path.c_str(), &st) != 0)
        return (false);
    return (S_ISDIR(st.st_mode));
}

bool FileUtils::readAll(const std::string& path, std::string& out)
{
    std::ifstream ifs(path.c_str(), std::ios::in | std::ios::binary);
    if (!ifs.is_open())
        return (false);

    std::string buf;
    char tmp[4096];
    while (ifs.good())
    {
        ifs.read(tmp, sizeof(tmp));
        std::streamsize n = ifs.gcount();
        if (n > 0)
            buf.append(tmp, (size_t)n);
    }
    out.swap(buf);
    return (true);
}

bool FileUtils::writeAllBinary(const std::string& path, const std::string& data)
{
    std::ofstream ofs(path.c_str(), std::ios::out | std::ios::binary | std::ios::trunc);
    if (!ofs.is_open())
        return (false);
    ofs.write(data.data(), (std::streamsize)data.size());
    return (ofs.good());
}

bool FileUtils::removeFile(const std::string& path)
{
    return ::remove(path.c_str()) == 0;
}

static bool endsWith(const std::string& s, const std::string& suffix)
{
    if (s.size() < suffix.size())
        return (false);
    return s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::string FileUtils::guessContentType(const std::string& path)
{
    if (endsWith(path, ".html") || endsWith(path, ".htm")) return "text/html; charset=utf-8";
    if (endsWith(path, ".css"))  return "text/css; charset=utf-8";
    if (endsWith(path, ".js"))   return "application/javascript; charset=utf-8";
    if (endsWith(path, ".json")) return "application/json; charset=utf-8";
    if (endsWith(path, ".txt"))  return "text/plain; charset=utf-8";
    if (endsWith(path, ".png"))  return "image/png";
    if (endsWith(path, ".jpg") || endsWith(path, ".jpeg")) return "image/jpeg";
    if (endsWith(path, ".gif"))  return "image/gif";
    return ("application/octet-stream");
}
