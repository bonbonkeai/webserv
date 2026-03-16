#include "HTTP/hpp/RequestFactory.hpp"
#include "Method_Handle/hpp/ErrorRequest.hpp"
#include "Method_Handle/hpp/GetRequest.hpp"
#include "Method_Handle/hpp/PostRequest.hpp"
#include "Method_Handle/hpp/DeleteRequest.hpp"

IRequest* RequestFactory::create(const HTTPRequest& req)
{
    if (req.bad_request)
        return (new ErrorRequest(req, req.error_code));
    if (req.method == "GET")
        return (new GetRequest(req));
    if (req.method == "POST")
        return (new PostRequest(req));
    if (req.method == "DELETE")
        return (new DeleteRequest(req));
    return (new ErrorRequest(req, 405));
}
