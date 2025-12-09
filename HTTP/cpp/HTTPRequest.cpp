#include "HTTP/hpp/HTTPRequest.hpp"

HTTPRequest::HTTPRequest() : 
			contentLength(0),
			complet(false),
			bad_request(false),
			has_body(false),
			keep_alive(false)
			{}

HTTPRequest::HTTPRequest(const HTTPRequest& copy) :
			method(copy.method),
			uri(copy.uri),
			path(copy.path),
			query(copy.query),
			version(copy.version),
			body(copy.body),
			headers(copy.headers),
			contentLength(copy.contentLength),
			complet(copy.complet),
			bad_request(copy.bad_request),
			has_body(copy.has_body),
			keep_alive(copy.keep_alive)
			{}

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
		complet = copy.complet;
		bad_request = copy.bad_request;
		has_body = copy.has_body;
		keep_alive = copy.keep_alive;
	}
	return (*this);
}

HTTPRequest::~HTTPRequest() {}