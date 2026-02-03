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
            has_effective(false)
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
      has_effective(copy.has_effective)
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
    }
    return (*this);
}

HTTPRequest::~HTTPRequest() {}

bool HTTPRequest::is_cgi_request() const
{
    // return (false);
    return (path.compare(0, 9, "/cgi-bin/") == 0);
    //
    // ps:后续要接入config,那就需要在 EffectiveConfig 里加一项->例如 cgi_pass 或 cgi_extensions, 然后
    // location/server 配置里存 cgi_*
    // resolve 后写进 req.effective
    // is_cgi_request() 读 effective 决策
    // 但现在的 EffectiveConfig.hpp 里还没有 CGI 相关字段，所以先做 path 前缀规则。
    //
}