#include "Method_Handle/hpp/FileUtils.hpp"
#include "HTTP/hpp/HTTPUtils.hpp" 

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

// bool FileUtils::readAll(const std::string& path, std::string& out)
// {
//     std::ifstream ifs(path.c_str(), std::ios::in | std::ios::binary);
//     if (!ifs.is_open())
//         return (false);

//     std::string buf;
//     char tmp[4096];
//     while (ifs.good())
//     {
//         ifs.read(tmp, sizeof(tmp));
//         std::streamsize n = ifs.gcount();
//         if (n > 0)
//             buf.append(tmp, (size_t)n);
//     }
//     out.swap(buf);
//     return (true);
// }

bool FileUtils::readAll(const std::string& path, std::string& out, int& outErrno)
{
    out.clear();
    outErrno = 0;
    int fd = open(path.c_str(), O_RDONLY);
    if (fd < 0)
    {
        outErrno = errno;
        return (false);
    }
    char buf[4096];
    ssize_t n;
    while ((n = read(fd, buf, sizeof(buf))) > 0)
    {
        out.append(buf, n);
    }
    if (n < 0)
    {
        outErrno = errno;
        close(fd);
        return (false);
    }
    close(fd);
    return (true);
}

// mkdir -p：支持 "./www/upload"、"www/upload"、"/tmp/x/y"
bool FileUtils::ensureDirRecursive(const std::string& dir, int mode)
{
    if (dir.empty())
        return (false);
    if (isDirectory(dir))
        return (true);

    std::string cur;
    cur.reserve(dir.size());

    for (std::size_t i = 0; i < dir.size(); ++i)
    {
        char c = dir[i];
        cur.push_back(c);

        if (c == '/')
        {
            // 忽略根 "/" 或 "./"
            if (cur == "/" || cur == "./")
                continue;

            if (!isDirectory(cur))
            {
                if (::mkdir(cur.c_str(), mode) != 0 && errno != EEXIST)
                    return (false);
            }
        }
    }
    // 最后一段如果不以 '/' 结尾，还需要 mkdir
    if (!isDirectory(cur))
    {
        if (::mkdir(cur.c_str(), mode) != 0 && errno != EEXIST)
            return (false);
    }
    return (isDirectory(dir));
}

static std::string parentDirOf(const std::string& path)
{
    std::size_t slash = path.find_last_of('/');
    if (slash == std::string::npos)
        return ("");
    if (slash == 0)
        return ("/");
    return (path.substr(0, slash));
}

bool FileUtils::writeAllBinaryErrno(const std::string& path, const std::string& data, int& outErrno)
{
    outErrno = 0;

    // 1) 确保父目录存在（ofstream 不会创建目录）
    std::string dir = parentDirOf(path);
    if (!dir.empty())
    {
        if (!ensureDirRecursive(dir, 0755))
        {
            outErrno = errno ? errno : ENOENT;
            return (false);
        }
    }

    // 2) 写文件
    std::ofstream ofs(path.c_str(), std::ios::out | std::ios::binary | std::ios::trunc);
    if (!ofs.is_open())
    {
        outErrno = errno ? errno : EACCES;
        return (false);
    }
    if (!data.empty())
        ofs.write(data.data(), (std::streamsize)data.size());
    if (!ofs.good())
    {
        outErrno = errno ? errno : EIO;
        return (false);
    }
    return (true);
}

bool FileUtils::writeAllBinary(const std::string& path, const std::string& data)
{
    // std::ofstream ofs(path.c_str(), std::ios::out | std::ios::binary | std::ios::trunc);
    // if (!ofs.is_open())
    //     return (false);
    // ofs.write(data.data(), (std::streamsize)data.size());
    // return (ofs.good());
    int e = 0;
    return (FileUtils::writeAllBinaryErrno(path, data, e));
}

bool FileUtils::removeFileErrno(const std::string& path, int& outErrno)
{
    outErrno = 0;
    if (::remove(path.c_str()) == 0)
        return (true);
    outErrno = errno;
    return (false);
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

bool FileUtils::fileSize(const std::string& path, std::size_t& outSize, int& outErrno)
{
    outErrno = 0;
    outSize = 0;
    struct stat st;
    if (stat(path.c_str(), &st) != 0)
    {
        outErrno = errno;
        return (false);
    }
    if (!S_ISREG(st.st_mode))
    {
        // 非普通文件，让上层按 isDirectory/403/404 处理
        outErrno = 0;
        outSize = 0;
        return (true);
    }
    if (st.st_size < 0)
    {
        outErrno = 0;
        outSize = 0;
        return (true);
    }
    outSize = static_cast<std::size_t>(st.st_size);
    return (true);
}

bool FileUtils::startsWith(const std::string& s, const std::string& prefix)
{
    return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

std::string FileUtils::trimCopy(std::string s)
{
    ltrimSpaces(s);
    rtrimSpaces(s);
    return (s);
}

std::string FileUtils::mimeMainLower(const std::string& ct)
{
    std::size_t semi = ct.find(';');
    std::string main = (semi == std::string::npos) ? ct : ct.substr(0, semi);
    main = trimCopy(main);
    toLowerInPlace(main);
    return (main);
}