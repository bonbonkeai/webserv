#include "Method_Handle/hpp/ErrorRequest.hpp"
#include "HTTP/hpp/ErrorResponse.hpp"

ErrorRequest::ErrorRequest(const HTTPRequest& req, int code) : _req(req), _code(code) {}
ErrorRequest::~ErrorRequest() {}

HTTPResponse ErrorRequest::handle()
{
    HTTPResponse r = buildErrorResponse(_code);

    // 用现有 buildErrorResponse 固定 close，这里按 parser 的 keep_alive 覆盖更合理
    r.headers["connection"] = (_req.keep_alive ? "keep-alive" : "close");
    return (r);
}
