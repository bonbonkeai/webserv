
#include <iostream>
#include <string>
#include "HTTP/hpp/HTTPRequestParser.hpp"

static void assertTrue(bool cond, const std::string& name)
{
    if (cond) std::cout << "[OK]   " << name << "\n";
    else      std::cout << "[FAIL] " << name << "\n";
}

static void assertEqStr(const std::string& a, const std::string& b, const std::string& name)
{
    if (a == b) std::cout << "[OK]   " << name << "\n";
    else        std::cout << "[FAIL] " << name << " expected='" << b << "' got='" << a << "'\n";
}

static void assertEqInt(int a, int b, const std::string& name)
{
    if (a == b) std::cout << "[OK]   " << name << "\n";
    else        std::cout << "[FAIL] " << name << " expected=" << b << " got=" << a << "\n";
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

// 这里改 expected: 413
static void test_content_length_too_large()
{
    HTTPRequestParser p;
    std::string raw =
        "POST / HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Length: 999999999999999999999999999999999999\r\n"
        "\r\n";
    bool ok = p.dejaParse(raw);
    const HTTPRequest& r = p.getRequest();

    assertTrue(ok == false, "Content-Length too large -> dejaParse false");
    assertTrue(r.bad_request == true, "Content-Length too large -> bad_request true");
    assertEqInt(r.error_code, 413, "Content-Length too large -> 413");
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

// 替换掉 rejected 测试：你现在的实现是“支持 chunked”
static void test_chunked_body_complete()
{
    HTTPRequestParser p;

    std::string part1 =
        "POST /upload HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Transfer-Encoding: chunked\r\n"
        "\r\n";
    bool ok1 = p.dejaParse(part1);
    const HTTPRequest& r1 = p.getRequest();

    assertTrue(ok1 == true, "chunked headers -> dejaParse true");
    assertTrue(r1.complet == false, "chunked headers -> complet false (need body)");
    assertTrue(r1.chunked == true, "chunked flag set");

    std::string part2 =
        "5\r\nhello\r\n"
        "0\r\n\r\n";
    bool ok2 = p.dejaParse(part2);
    const HTTPRequest& r2 = p.getRequest();

    assertTrue(ok2 == true, "chunked body -> dejaParse true");
    assertTrue(r2.complet == true, "chunked body -> complet true");
    assertEqStr(r2.body, "hello", "chunked body -> body matches");

    assertTrue(r2.bad_request == false, "chunked body -> bad_request false");
    assertEqInt(r2.error_code, 200, "chunked body -> error_code 200");
}

// 这里改 expected: 431
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
    assertEqInt(r.error_code, 431, "Header too large without terminator -> 431");
}

/*
CGI 相关测试（parser 级别）：
- parser 不执行 CGI，只验证 splitUri 与 body 的解析是否正确
*/
static void test_cgi_get_split_uri()
{
    HTTPRequestParser p;
    std::string raw =
        "GET /cgi-bin/hello.py?name=Jingyi&x=1 HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "\r\n";

    bool ok = p.dejaParse(raw);
    const HTTPRequest& r = p.getRequest();

    assertTrue(ok == true, "CGI GET -> dejaParse true");
    assertTrue(r.complet == true, "CGI GET -> complet true");
    assertEqStr(r.path, "/cgi-bin/hello.py", "CGI GET -> path split");
    assertEqStr(r.query, "name=Jingyi&x=1", "CGI GET -> query split");
}

static void test_cgi_post_body()
{
    HTTPRequestParser p;
    std::string raw =
        "POST /cgi-bin/echo.py HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Length: 9\r\n"
        "Content-Type: application/x-www-form-urlencoded\r\n"
        "\r\n"
        "a=1&b=two";

    bool ok = p.dejaParse(raw);
    const HTTPRequest& r = p.getRequest();

    assertTrue(ok == true, "CGI POST -> dejaParse true");
    assertTrue(r.complet == true, "CGI POST -> complet true");
    assertEqStr(r.path, "/cgi-bin/echo.py", "CGI POST -> path");
    assertEqStr(r.body, "a=1&b=two", "CGI POST -> body matches");
}

int main()
{
    test_missing_host_http11();
    std::cout << "-----------------------------\n";
    test_duplicate_host();
    std::cout << "-----------------------------\n";

    test_content_length_nondigit();
    std::cout << "-----------------------------\n";
    test_content_length_too_large();
    std::cout << "-----------------------------\n";
    test_duplicate_content_length();
    std::cout << "-----------------------------\n";

    test_post_with_body_complete();
    std::cout << "-----------------------------\n";
    test_post_body_incremental();
    std::cout << "-----------------------------\n";

    test_chunked_body_complete();
    std::cout << "-----------------------------\n";

    test_header_size_limit_no_terminator();
    std::cout << "-----------------------------\n";

    test_cgi_get_split_uri();
    std::cout << "-----------------------------\n";
    test_cgi_post_body();

    return (0);
}
