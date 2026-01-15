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

#endif
