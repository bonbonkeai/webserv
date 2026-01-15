#ifndef HTTPUTILS_HPP
#define HTTPUTILS_HPP

#include <string>
#include <sstream>
#include <algorithm>

std::string toString(std::size_t n);
void toLowerInPlace(std::string& s);
void ltrimSpaces(std::string& s);
bool isTokenUpperAlpha(const std::string& s);
void rtrimSpaces(std::string& s);

std::string extrat_session_id(const std::string& cookie);
#endif
