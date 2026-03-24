#include "HTTP/hpp/HTTPRequest.hpp"

HTTPRequest::HTTPRequest() : method(""),
			uri(""),
			path(""),
			query(""),
			version(""),
			body(""),
			headers(),
			contentLength(0),
			has_body(false),
			complet(false),
			bad_request(false),
			keep_alive(false),
			has_content_length(false),
			error_code(200),
            has_transfer_encoding(false),
            chunked(false),
            max_body_size(1024 * 1024 * 10), // 先临时 10MB
            authority(""),
            has_effective(false),
            _rout()      
{
}

HTTPRequest::HTTPRequest(const HTTPRequest& copy)
    : method(copy.method),
      uri(copy.uri),
      path(copy.path),
      query(copy.query),
      version(copy.version),
      body(copy.body),
      headers(copy.headers),
      contentLength(copy.contentLength),
      has_body(copy.has_body),
      complet(copy.complet),
      bad_request(copy.bad_request),
      keep_alive(copy.keep_alive),
	  has_content_length(copy.has_content_length),
	  error_code(copy.error_code),
      has_transfer_encoding(copy.has_transfer_encoding),
      chunked(copy.chunked),
      max_body_size(copy.max_body_size),
      authority(copy.authority),
      effective(copy.effective),
      has_effective(copy.has_effective),
      _rout(copy._rout)
{
}

HTTPRequest& HTTPRequest::operator=(const HTTPRequest& copy)
{
    if (this != &copy)
    {
        method = copy.method;
        uri = copy.uri;
        path = copy.path;
        query = copy.query;
        version = copy.version;
        body = copy.body;
        headers = copy.headers;
        contentLength = copy.contentLength;
        has_body = copy.has_body;
        complet = copy.complet;
        bad_request = copy.bad_request;
        keep_alive = copy.keep_alive;
		has_content_length = copy.has_content_length;
		error_code = copy.error_code;
        has_transfer_encoding = copy.has_transfer_encoding;
        chunked = copy.chunked;
        max_body_size = copy.max_body_size;
        authority = copy.authority;
        effective = copy.effective;
        has_effective = copy.has_effective;
        _rout = copy._rout;
    }
    return (*this);
}

HTTPRequest::~HTTPRequest() {}

bool HTTPRequest::is_cgi_request() const
{
    if (path.find("/cgi-bin/") == 0)
        return true;
    // 2. 或者根据文件扩展名判断
    size_t dot = path.rfind('.');
    if (dot != std::string::npos)
    {
        std::string ext = path.substr(dot);
        if (ext == ".sh" || ext == ".py" || ext == ".php" || ext == ".cgi")
            return true;
    }

    // 3. 其他情况认为不是 CGI
    return false;
}