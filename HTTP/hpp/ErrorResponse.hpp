#ifndef ERRORRESPONSE_HPP
#define ERRORRESPONSE_HPP

#include "HTTP/hpp/HTTPResponse.hpp"
#include "HTTP/hpp/HTTPUtils.hpp" 

HTTPResponse buildErrorResponse(int statusCode);

#endif
