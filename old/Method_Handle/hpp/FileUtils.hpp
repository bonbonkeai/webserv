#ifndef FILEUTILS_HPP
#define FILEUTILS_HPP

#include <string>
#include <fcntl.h>   // open, O_RDONLY
#include <unistd.h>  // read, close
#include <sys/stat.h>
#include <fstream>
#include <cstdio>
#include <errno.h>

class FileUtils
{
public:
        static bool isSafePath(const std::string& urlPath);
        static std::string joinPath(const std::string& root, const std::string& urlPath);
        static bool exists(const std::string& path);
        static bool isDirectory(const std::string& path);
        // static bool readAll(const std::string& path, std::string& out);
        // 返回 true 表示成功
        // 返回 false 表示失败，同时 outErrno 保存 errno
        static bool readAll(const std::string& path,
                        std::string& out,
                        int& outErrno);

        static bool ensureDirRecursive(const std::string& dir, int mode);
        static bool writeAllBinaryErrno(const std::string& path, const std::string& data, int& outErrno);
        static bool writeAllBinary(const std::string& path, const std::string& data);
        
        static bool removeFileErrno(const std::string& path, int& outErrno);

        static bool removeFile(const std::string& path);
        static std::string guessContentType(const std::string& path);
        static bool startsWith(const std::string& s, const std::string& prefix);

        // FileUtils.hpp
        static bool fileSize(const std::string& path, std::size_t& outSize, int& outErrno);
        
        static std::string trimCopy(std::string s);
        static std::string mimeMainLower(const std::string& ct);

        

};

#endif
