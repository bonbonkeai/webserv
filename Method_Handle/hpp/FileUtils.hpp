#ifndef FILEUTILS_HPP
#define FILEUTILS_HPP

#include <string>

class FileUtils
{
public:
        static bool isSafePath(const std::string& urlPath);
        static std::string joinPath(const std::string& root, const std::string& urlPath);
        static bool exists(const std::string& path);
        static bool isDirectory(const std::string& path);
        static bool readAll(const std::string& path, std::string& out);
        static bool writeAllBinary(const std::string& path, const std::string& data);
        static bool removeFile(const std::string& path);
        static std::string guessContentType(const std::string& path);
};

#endif
