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
			keep_alive(false)
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
      keep_alive(copy.keep_alive)
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
    }
    return (*this);
}

HTTPRequest::~HTTPRequest() {}