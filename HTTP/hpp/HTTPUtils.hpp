#ifndef HTTPUTILS_HPP
#define HTTPUTILS_HPP

#include <string>
#include <sstream>
#include <algorithm>
#include <cctype>

std::string toString(std::size_t n);
void toLowerInPlace(std::string& s);
void ltrimSpaces(std::string& s);
bool isTokenUpperAlpha(const std::string& s);
void rtrimSpaces(std::string& s);
bool isTChar(unsigned char c);

//增加的针对URI的parser
static bool uriCharset(char c);
static bool isValidUriChar(const std::string& s);
static bool isValidHostChar(char c);
static bool isValidDomainLike(const std::string& host);
static bool parsePort(const std::string& s, int& port_out);
static bool isValidIp(const std::string& host);

#endif
