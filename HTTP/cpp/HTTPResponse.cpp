#include "HTTP/hpp/HTTPResponse.hpp"
#include "HTTP/hpp/HTTPUtils.hpp" 

HTTPResponse::HTTPResponse() : statusCode(200), statusText("OK") {}
		
HTTPResponse::HTTPResponse(const HTTPResponse& copy) : 
				statusCode(copy.statusCode),
				statusText(copy.statusText),
				headers(copy.headers),
				body(copy.body)
				{}

		
HTTPResponse& HTTPResponse::operator=(const HTTPResponse& copy)
{
	if (this != &copy)
	{
		statusCode = copy.statusCode;
		statusText = copy.statusText;
		headers = copy.headers;
		body = copy.body;
	}
	return (*this);
}
		
HTTPResponse::~HTTPResponse() {}

void HTTPResponse::clear()
{
	statusCode = 200;
	statusText = "OK";
	headers.clear();
	body.clear();
}

static bool splitHeaderBody(const std::string& out, std::string& head, std::string& body)
{
    std::size_t pos = out.find("\r\n\r\n");
    std::size_t sep = 4;
    if (pos == std::string::npos)
    {
        pos = out.find("\n\n");
        sep = 2;
    }
    if (pos == std::string::npos)
        return false;
    head = out.substr(0, pos);
    body = out.substr(pos + sep);
    return true;
}

HTTPResponse HTTPResponse::buildResponseFromCGIOutput(const std::string& out, bool keep_alive)
{
    HTTPResponse resp;
    resp.statusCode = 200;
    resp.statusText = "OK";

    std::string head, body;
    if (!splitHeaderBody(out, head, body))
    {
        // CGI 输出不含头部，按 body 直接返回 200
        resp.body = out;
        resp.headers["content-length"] = toString(resp.body.size());
        resp.headers["content-type"] = "text/plain; charset=utf-8";
        resp.headers["connection"] = keep_alive ? "keep-alive" : "close";
        return resp;
    }

    // parse headers
    std::istringstream iss(head);
    std::string line;
    while (std::getline(iss, line))
    {
        if (!line.empty() && line[line.size() - 1] == '\r')
            line.erase(line.size() - 1);

        if (line.empty())
            continue;

        std::size_t colon = line.find(':');
        if (colon == std::string::npos)
            continue;

        std::string key = line.substr(0, colon);
        std::string val = line.substr(colon + 1);
        rtrimSpaces(key);
        toLowerInPlace(key);
        ltrimSpaces(val);
        rtrimSpaces(val);

        if (key == "status")
        {
            // val: "302 Found"
            std::istringstream s(val);
            int code;
            std::string text;
            s >> code;
            std::getline(s, text);
            ltrimSpaces(text);
            if (code > 0)
            {
                resp.statusCode = code;
                resp.statusText = text.empty() ? "OK" : text;
            }
            continue;
        }

        resp.headers[key] = val;
    }

    resp.body = body;

    // 如果 CGI 没给 Content-Length，我们补上
    if (resp.headers.find("content-length") == resp.headers.end())
        resp.headers["content-length"] = toString(resp.body.size());

    // CGI 常见：给 Location 但没给 Status，则按 302
    if (resp.statusCode == 200 && resp.headers.find("location") != resp.headers.end())
    {
        resp.statusCode = 302;
        resp.statusText = "Found";
    }

    resp.headers["connection"] = keep_alive ? "keep-alive" : "close";
    return (resp);
}
