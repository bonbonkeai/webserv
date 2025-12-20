#include "HTTP/hpp/HTTPUtils.hpp" 

std::string toString(std::size_t n)
{
    std::ostringstream oss;
    oss << n;
    return oss.str();
}