#ifndef ERRORRESPONSE_HPP
#define ERRORRESPONSE_HPP

#include "HTTP/hpp/HTTPResponse.hpp"
#include "HTTP/hpp/HTTPUtils.hpp" 
#include "Config/hpp/EffectiveConfig.hpp"

#include <fstream>
#include <sstream>
#include <map>

HTTPResponse buildErrorResponse(int statusCode);
HTTPResponse buildConfiguredErrorResponse(int statusCode, const EffectiveConfig& cfg);

#endif
