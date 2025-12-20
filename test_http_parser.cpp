#include <iostream>
#include "HTTP/hpp/HTTPRequestParser.hpp"
#include "HTTP/hpp/HTTPResponse.hpp"
#include "HTTP/hpp/ResponseBuilder.hpp"
#include "HTTP/hpp/ErrorResponse.hpp"
#include "HTTP/hpp/HTTPUtils.hpp"

static void test_good_request()
{
    HTTPRequestParser parser;

    std::string raw =
        "GET /hello?x=1 HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Connection: close\r\n"
        "\r\n";

    bool ok = parser.dejaParse(raw);
    const HTTPRequest& req = parser.getRequest();

    std::cout << "[good] dejaParse returned: " << (ok ? "true" : "false") << "\n";
    std::cout << "[good] complet: " << (req.complet ? "true" : "false") << "\n";
    std::cout << "[good] method=" << req.method << " uri=" << req.uri
              << " path=" << req.path << " query=" << req.query << "\n";

    if (!req.complet)
        std::cerr << "Parser did not reach complet state\n";

    HTTPResponse resp;
    resp.statusCode = 200;
    resp.statusText = "OK";
    resp.body = "Hello from macOS test\n";
    resp.headers["content-type"] = "text/plain; charset=utf-8";
    resp.headers["content-length"] = toString(resp.body.size());
    resp.headers["connection"] = "close";

    std::string out = ResponseBuilder::build(resp);
    std::cout << "\n=== 200 Response ===\n" << out << "\n";
}

static void test_bad_request()
{
    HTTPRequestParser parser;

    // 故意给一个非法首行（缺字段）
    std::string raw = "GET /only_two_tokens\r\n\r\n";

    bool ok = parser.dejaParse(raw);
    const HTTPRequest& req = parser.getRequest();

    std::cout << "[bad] dejaParse returned: " << (ok ? "true" : "false") << "\n";
    std::cout << "[bad] bad_request: " << (req.bad_request ? "true" : "false") << "\n";

    HTTPResponse err = buildErrorResponse(400);
    std::string out = ResponseBuilder::build(err);
    std::cout << "\n=== 400 Response ===\n" << out << "\n";
}

int main()
{
    test_good_request();
    std::cout << "-----------------------------\n";
    test_bad_request();
    return (0);
}
