#include "HTTP/hpp/HTTPUtils.hpp" 

std::string toString(std::size_t n)
{
    std::ostringstream oss;
    oss << n;
    return oss.str();
}

void toLowerInPlace(std::string& s)
{
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
}

void ltrimSpaces(std::string& s)
{
    while (!s.empty() && (s[0] == ' ' || s[0] == '\t'))
        s.erase(0, 1);
}

bool isTokenUpperAlpha(const std::string& s)
{
    if (s.empty())
        return (false);
    for (size_t i = 0; i < s.size(); ++i)
        if (!(s[i] >= 'A' && s[i] <= 'Z'))
            return (false);
    return (true);
}

void rtrimSpaces(std::string& s)
{
    while (!s.empty() && (s[s.size() - 1] == ' ' || s[s.size() - 1] == '\t'))
        s.erase(s.size() - 1, 1);
}

bool isTChar(unsigned char c)
{
    if (std::isalnum(c))
        return true;
    switch (c)
    {
        case '!': case '#': case '$': case '%': case '&':
        case '\'': case '*': case '+': case '-': case '.':
        case '^': case '_': case '`': case '|': case '~':
            return (true);
        default:
            return (false);
    }
}

bool uriCharset(char c)
{
    static const std::string allowed =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789"
        "-_.~"
        ":/?#[]@"
        "!$&'()*+,;=%";
    return (allowed.find(c) != std::string::npos);
}

bool isValidUriChar(const std::string& s)
{
    for (size_t i = 0; i < s.size(); ++i)
    {
        if (!uriCharset(s[i]))
            return (false);
    }
    return (true);
}

bool isValidHostChar(char c)
{
    return (std::isalnum(static_cast<unsigned char>(c)) || c == '.' || c == '-');
}

bool isValidDomainLike(const std::string& host)
{
    if (host.empty())
        return (false);
    // 允许 localhost
    if (host == "localhost")
        return (true);

    // 每个字符必须合法，且不能以 '-' 开头结尾
    if (host.front() == '-' || host.back() == '-')
        return (false);

    for (size_t i = 0; i < host.size(); ++i)
        if (!isValidHostChar(host[i]))
            return (false);

    // 要求至少有一个点（模仿 domain/ip 的常见形态）
    return (true);
}

bool parsePort(const std::string& s, int& port_out)
{
    if (s.empty() || s.size() > 5)
        return (false);
    int acc = 0;
    for (size_t i = 0; i < s.size(); ++i)
    {
        if (s[i] < '0' || s[i] > '9')
            return (false);
        acc = acc * 10 + (s[i] - '0');
    }
    if (acc < 1 || acc > 65535)
        return (false);
    port_out = acc;
    return (true);
}

bool isValidIp(const std::string& host)
{
    int parts = 0;
    size_t start = 0;
    while (true)
    {
        size_t dot = host.find('.', start);
        std::string part;
        if (dot == std::string::npos)
            part = host.substr(start);
        else
            part = host.substr(start, dot - start);
        // 每一段必须非空
        if (part.empty())
            return (false);
        // 每一段必须全是数字
        int value = 0;
        for (size_t i = 0; i < part.size(); ++i)
        {
            if (part[i] < '0' || part[i] > '9')
                return (false);
            value = value * 10 + (part[i] - '0');
        }
        // 0–255
        if (value < 0 || value > 255)
            return (false);
        parts++;
        if (dot == std::string::npos)
            break;
        start = dot + 1;
    }
    // IPv4 必须正好 4 段
    return (parts == 4);
}

