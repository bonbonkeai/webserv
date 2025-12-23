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



