// #include <iostream>
// #include "HTTP/hpp/HTTPRequestParser.hpp"
// #include "HTTP/hpp/HTTPResponse.hpp"
// #include "HTTP/hpp/ResponseBuilder.hpp"
// #include "HTTP/hpp/ErrorResponse.hpp"
// #include "HTTP/hpp/HTTPUtils.hpp"

// static void test_good_request()
// {
//     HTTPRequestParser parser;

//     std::string raw =
//         "GET /hello?x=1 HTTP/1.1\r\n"
//         "Host: localhost\r\n"
//         "Connection: close\r\n"
//         "\r\n";

//     bool ok = parser.dejaParse(raw);
//     const HTTPRequest& req = parser.getRequest();

//     std::cout << "[good] dejaParse returned: " << (ok ? "true" : "false") << "\n";
//     std::cout << "[good] complet: " << (req.complet ? "true" : "false") << "\n";
//     std::cout << "[good] method=" << req.method << " uri=" << req.uri
//               << " path=" << req.path << " query=" << req.query << "\n";

//     if (!req.complet)
//         std::cerr << "Parser did not reach complet state\n";

//     HTTPResponse resp;
//     resp.statusCode = 200;
//     resp.statusText = "OK";
//     resp.body = "Hello from macOS test\n";
//     resp.headers["content-type"] = "text/plain; charset=utf-8";
//     resp.headers["content-length"] = toString(resp.body.size());
//     resp.headers["connection"] = "close";

//     std::string out = ResponseBuilder::build(resp);
//     std::cout << "\n=== 200 Response ===\n" << out << "\n";
// }

// static void test_bad_request()
// {
//     HTTPRequestParser parser;

//     // 故意给一个非法首行（缺字段）
//     std::string raw = "GET /only_two_tokens\r\n\r\n";

//     bool ok = parser.dejaParse(raw);
//     const HTTPRequest& req = parser.getRequest();

//     std::cout << "[bad] dejaParse returned: " << (ok ? "true" : "false") << "\n";
//     std::cout << "[bad] bad_request: " << (req.bad_request ? "true" : "false") << "\n";

//     HTTPResponse err = buildErrorResponse(400);
//     std::string out = ResponseBuilder::build(err);
//     std::cout << "\n=== 400 Response ===\n" << out << "\n";
// }

// int main()
// {
//     test_good_request();
//     std::cout << "-----------------------------\n";
//     test_bad_request();
//     return (0);
// }


#include <iostream>
#include <string>
#include "HTTP/hpp/HTTPRequestParser.hpp"

static void assertTrue(bool cond, const std::string& name)
{
    if (cond)
        std::cout << "[OK]   " << name << "\n";
    else
        std::cout << "[FAIL] " << name << "\n";
}

static void assertEqStr(const std::string& a, const std::string& b, const std::string& name)
{
    if (a == b)
        std::cout << "[OK]   " << name << "\n";
    else
        std::cout << "[FAIL] " << name << " expected='" << b << "' got='" << a << "'\n";
}

static void assertEqInt(int a, int b, const std::string& name)
{
    if (a == b)
        std::cout << "[OK]   " << name << "\n";
    else
        std::cout << "[FAIL] " << name << " expected=" << b << " got=" << a << "\n";
}

static void test_missing_host_http11()
{
    HTTPRequestParser p;
    std::string raw =
        "GET / HTTP/1.1\r\n"
        "\r\n";
    bool ok = p.dejaParse(raw);
    const HTTPRequest& r = p.getRequest();

    assertTrue(ok == false, "HTTP/1.1 missing Host -> dejaParse false");
    assertTrue(r.bad_request == true, "HTTP/1.1 missing Host -> bad_request true");
    assertEqInt(r.error_code, 400, "HTTP/1.1 missing Host -> 400");
}

static void test_duplicate_host()
{
    HTTPRequestParser p;
    std::string raw =
        "GET / HTTP/1.1\r\n"
        "Host: a\r\n"
        "Host: b\r\n"
        "\r\n";
    bool ok = p.dejaParse(raw);
    const HTTPRequest& r = p.getRequest();

    assertTrue(ok == false, "Duplicate Host -> dejaParse false");
    assertTrue(r.bad_request == true, "Duplicate Host -> bad_request true");
    assertEqInt(r.error_code, 400, "Duplicate Host -> 400");
}

static void test_content_length_nondigit()
{
    HTTPRequestParser p;
    std::string raw =
        "POST / HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Length: 12abc\r\n"
        "\r\n";
    bool ok = p.dejaParse(raw);
    const HTTPRequest& r = p.getRequest();

    assertTrue(ok == false, "Content-Length non-digit -> dejaParse false");
    assertTrue(r.bad_request == true, "Content-Length non-digit -> bad_request true");
    assertEqInt(r.error_code, 400, "Content-Length non-digit -> 400");
}

static void test_content_length_overflow()
{
    HTTPRequestParser p;
    std::string raw =
        "POST / HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Length: 999999999999999999999999999999999999\r\n"
        "\r\n";
    bool ok = p.dejaParse(raw);
    const HTTPRequest& r = p.getRequest();

    assertTrue(ok == false, "Content-Length overflow -> dejaParse false");
    assertTrue(r.bad_request == true, "Content-Length overflow -> bad_request true");
    assertEqInt(r.error_code, 400, "Content-Length overflow -> 400");
}

static void test_duplicate_content_length()
{
    HTTPRequestParser p;
    std::string raw =
        "POST / HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Length: 1\r\n"
        "Content-Length: 2\r\n"
        "\r\n";
    bool ok = p.dejaParse(raw);
    const HTTPRequest& r = p.getRequest();

    assertTrue(ok == false, "Duplicate Content-Length -> dejaParse false");
    assertTrue(r.bad_request == true, "Duplicate Content-Length -> bad_request true");
    assertEqInt(r.error_code, 400, "Duplicate Content-Length -> 400");
}

static void test_post_with_body_complete()
{
    HTTPRequestParser p;
    std::string raw =
        "POST /submit HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Length: 5\r\n"
        "\r\n"
        "hello";
    bool ok = p.dejaParse(raw);
    const HTTPRequest& r = p.getRequest();

    assertTrue(ok == true, "POST with body -> dejaParse true");
    assertTrue(r.complet == true, "POST with body -> complet true");
    assertEqStr(r.body, "hello", "POST with body -> body matches");
}

static void test_post_body_incremental()
{
    HTTPRequestParser p;

    std::string part1 =
        "POST /submit HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Length: 5\r\n"
        "\r\n"
        "he";
    bool ok1 = p.dejaParse(part1);
    const HTTPRequest& r1 = p.getRequest();

    assertTrue(ok1 == true, "Incremental body part1 -> dejaParse true (need more)");
    assertTrue(r1.complet == false, "Incremental body part1 -> complet false");

    std::string part2 = "llo";
    bool ok2 = p.dejaParse(part2);
    const HTTPRequest& r2 = p.getRequest();

    assertTrue(ok2 == true, "Incremental body part2 -> dejaParse true");
    assertTrue(r2.complet == true, "Incremental body part2 -> complet true");
    assertEqStr(r2.body, "hello", "Incremental body -> body matches");
}

static void test_transfer_encoding_rejected()
{
    HTTPRequestParser p;
    std::string raw =
        "POST / HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Transfer-Encoding: chunked\r\n"
        "\r\n";
    bool ok = p.dejaParse(raw);
    const HTTPRequest& r = p.getRequest();

    assertTrue(ok == false, "Transfer-Encoding chunked -> dejaParse false");
    assertTrue(r.bad_request == true, "Transfer-Encoding chunked -> bad_request true");
    assertEqInt(r.error_code, 400, "Transfer-Encoding chunked -> 400");
}

static void test_header_size_limit_no_terminator()
{
    HTTPRequestParser p;
    std::string raw = "GET / HTTP/1.1\r\nHost: localhost\r\nX-Fill: ";
    raw += std::string(9000, 'a'); // 超过 8192
    // 不加 \r\n\r\n

    bool ok = p.dejaParse(raw);
    const HTTPRequest& r = p.getRequest();

    assertTrue(ok == false, "Header too large without terminator -> dejaParse false");
    assertTrue(r.bad_request == true, "Header too large without terminator -> bad_request true");
    assertEqInt(r.error_code, 400, "Header too large without terminator -> 400");
}

int main()
{
    test_missing_host_http11();
    std::cout << "-----------------------------\n";
    test_duplicate_host();
    std::cout << "-----------------------------\n";

    test_content_length_nondigit();
    std::cout << "-----------------------------\n";
    test_content_length_overflow();
    std::cout << "-----------------------------\n";
    test_duplicate_content_length();
    std::cout << "-----------------------------\n";

    test_post_with_body_complete();
    std::cout << "-----------------------------\n";
    test_post_body_incremental();
    std::cout << "-----------------------------\n";

    test_transfer_encoding_rejected();
    std::cout << "-----------------------------\n";
    test_header_size_limit_no_terminator();

    return (0);
}
