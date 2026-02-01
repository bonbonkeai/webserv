#include "Method_Handle/hpp/RedirectHandle.hpp"
#include "HTTP/hpp/HTTPUtils.hpp"
#include <sstream>

static std::string phrase(int code)
{
    if (code == 301) return "Moved Permanently";
    if (code == 302) return "Found";
    return ("Redirect");
}

HTTPResponse RedirectHandle::buildRedirect(const HTTPRequest& req, int code, const std::string& location)
{
    HTTPResponse resp;
    resp.statusCode = code;
    resp.statusText = phrase(code);
    resp.headers["location"] = location;

    std::ostringstream oss;
    oss << "<html><body><h1>" << code << " " << resp.statusText
        << "</h1><a href=\"" << location << "\">" << location
        << "</a></body></html>\n";
    resp.body = oss.str();

    resp.headers["content-type"] = "text/html; charset=utf-8";
    resp.headers["content-length"] = toString(resp.body.size());
    resp.headers["connection"] = (req.keep_alive ? "keep-alive" : "close");
    return (resp);
}
