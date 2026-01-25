// test_http.cpp - UNIT(parser/response) + INTEGRATION(methods) in one binary (C++98)

#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/time.h>

#include "HTTP/hpp/HTTPRequestParser.hpp"
#include "HTTP/hpp/ErrorResponse.hpp"
#include "HTTP/hpp/ResponseBuilder.hpp"

// ---------------- tiny assert ----------------
static int g_fail = 0;
static int g_skip = 0;

static void ok(const std::string& name) {
    std::cout << "[OK]   " << name << "\n";
}
static void skip(const std::string& name, const std::string& msg) {
    ++g_skip;
    std::cout << "[SKIP] " << name << " (" << msg << ")\n";
}
static void fail(const std::string& name, const std::string& msg) {
    ++g_fail;
    std::cerr << "[FAIL] " << name << "\n  " << msg << "\n";
}
static bool contains(const std::string& s, const std::string& sub) {
    return s.find(sub) != std::string::npos;
}
static std::string lower(const std::string& s) {
    std::string r = s;
    for (size_t i = 0; i < r.size(); ++i)
        if (r[i] >= 'A' && r[i] <= 'Z') r[i] = char(r[i] - 'A' + 'a');
    return r;
}

static long now_ms()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long)tv.tv_sec * 1000L + (long)tv.tv_usec / 1000L;
}

// ---------------- UNIT helpers ----------------
static bool feed_parser(HTTPRequestParser& p, const std::vector<std::string>& chunks) {
    bool r = false;
    for (size_t i = 0; i < chunks.size(); ++i) r = p.dejaParse(chunks[i]);
    return r;
}

static void expect_parser(const std::string& name,
                          const std::vector<std::string>& chunks,
                          bool exp_done, bool exp_bad, int exp_err,
                          const std::string& exp_method,
                          const std::string& exp_path,
                          bool exp_keep_alive, bool exp_chunked,
                          const std::string& exp_body)
{
    HTTPRequestParser p;
    bool done = feed_parser(p, chunks);
    const HTTPRequest& r = p.getRequest();

    std::ostringstream why;
    bool pass = true;

    (void)done;
    // if (done != exp_done) { pass = false; why << "done exp " << exp_done << " got " << done << ". "; }
    if (r.complet != exp_done) { pass = false; why << "complet exp " << exp_done << " got " << r.complet << ". "; }
    if (r.bad_request != exp_bad) { pass = false; why << "bad exp " << exp_bad << " got " << r.bad_request << ". "; }

    if (exp_bad) {
        if (r.error_code != exp_err) { pass = false; why << "err exp " << exp_err << " got " << r.error_code << ". "; }
    } else {
        if (!exp_method.empty() && r.method != exp_method) { pass = false; why << "method exp " << exp_method << " got " << r.method << ". "; }
        if (!exp_path.empty() && r.path != exp_path) { pass = false; why << "path exp " << exp_path << " got " << r.path << ". "; }
        if (r.keep_alive != exp_keep_alive) { pass = false; why << "keep_alive exp " << exp_keep_alive << " got " << r.keep_alive << ". "; }
        if (r.chunked != exp_chunked) { pass = false; why << "chunked exp " << exp_chunked << " got " << r.chunked << ". "; }
        if (!exp_body.empty() && r.body != exp_body) { pass = false; why << "body mismatch. "; }
    }

    if (pass) ok(name);
    else {
        std::ostringstream dbg;
        dbg << why.str()
            << " (complet=" << r.complet
            << " method=" << r.method
            << " uri=" << r.uri
            << " version=" << r.version
            << ")";
        fail(name, dbg.str());
    }
}

// LF-only：没有 \r\n => stuck，但不是 bad_request
static void expect_lf_only_no_progress()
{
    HTTPRequestParser p;
    bool done = p.dejaParse("GET / HTTP/1.1\nHost: a\n\n");
    const HTTPRequest& r = p.getRequest();
    if (!done) fail("UNIT: LF-only stuck", "expected dejaParse true (no progress)");
    else if (r.complet) fail("UNIT: LF-only stuck", "expected complet false");
    else if (r.bad_request) fail("UNIT: LF-only stuck", "expected bad_request false");
    else ok("UNIT: LF-only stuck (documented behavior)");
}

// chunked 不完整时：应当 not complet、not bad_request
static void expect_chunked_incomplete_not_error()
{
    HTTPRequestParser p;
    (void)p.dejaParse("POST /upload/u.txt HTTP/1.1\r\nHost: a\r\nTransfer-Encoding: chunked\r\n\r\n");
    (void)p.dejaParse("5\r\nhe"); // incomplete data + missing \r\n
    const HTTPRequest& r = p.getRequest();
    if (r.bad_request) fail("UNIT: chunked incomplete not error", "unexpected bad_request");
    else if (r.complet) fail("UNIT: chunked incomplete not error", "unexpected complet true");
    else ok("UNIT: chunked incomplete -> not complet, not error");
}

static void expect_chunked_trailer_incomplete_not_error()
{
    HTTPRequestParser p;
    (void)p.dejaParse("POST /upload/u.txt HTTP/1.1\r\nHost: a\r\nTransfer-Encoding: chunked\r\n\r\n");
    (void)p.dejaParse("1\r\na\r\n");
    (void)p.dejaParse("0\r\nX-T: v\r\n"); // trailer not finished (\r\n missing)
    const HTTPRequest& r = p.getRequest();
    if (r.bad_request) fail("UNIT: chunked trailer incomplete not error", "unexpected bad_request");
    else if (r.complet) fail("UNIT: chunked trailer incomplete not error", "unexpected complet true");
    else ok("UNIT: chunked trailer incomplete -> not complet, not error");
}

// ---------------- INTEGRATION: raw TCP client ----------------
static int connect_tcp(const std::string& host, const std::string& port, int timeout_ms) {
    struct addrinfo hints;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo* res = NULL;
    int rc = getaddrinfo(host.c_str(), port.c_str(), &hints, &res);
    if (rc != 0 || !res) return -1;

    int fd = -1;
    for (struct addrinfo* p = res; p; p = p->ai_next) {
        fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (fd < 0) continue;

        int flags = fcntl(fd, F_GETFL, 0);
        if (flags >= 0) fcntl(fd, F_SETFL, flags | O_NONBLOCK);

        int c = ::connect(fd, p->ai_addr, p->ai_addrlen);
        if (c == 0) { freeaddrinfo(res); return fd; }
        if (errno != EINPROGRESS) { close(fd); fd = -1; continue; }

        fd_set wfds;
        FD_ZERO(&wfds); FD_SET(fd, &wfds);
        struct timeval tv;
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        int sel = select(fd + 1, NULL, &wfds, NULL, &tv);
        if (sel > 0 && FD_ISSET(fd, &wfds)) {
            int soerr = 0; socklen_t sl = sizeof(soerr);
            getsockopt(fd, SOL_SOCKET, SO_ERROR, &soerr, &sl);
            if (soerr == 0) { freeaddrinfo(res); return fd; }
        }
        close(fd); fd = -1;
    }
    freeaddrinfo(res);
    return -1;
}

static bool send_all(int fd, const std::string& s) {
    size_t off = 0;
    while (off < s.size()) {
        ssize_t n = send(fd, s.data() + off, s.size() - off, 0);
        if (n < 0) { if (errno == EINTR) continue; return false; }
        off += (size_t)n;
    }
    return true;
}

struct HttpResp {
    int status;
    std::map<std::string,std::string> headers;
    std::string head;
    std::string body;
};

static void parse_headers(HttpResp& r) {
    r.headers.clear();
    std::istringstream iss(r.head);
    std::string line;
    std::getline(iss, line); // status line
    while (std::getline(iss, line)) {
        if (!line.empty() && line[line.size()-1] == '\r') line.erase(line.size()-1);
        if (line.empty()) break;
        size_t p = line.find(':');
        if (p == std::string::npos) continue;
        std::string k = lower(line.substr(0, p));
        std::string v = line.substr(p+1);
        while (!v.empty() && (v[0]==' '||v[0]=='\t')) v.erase(0,1);
        r.headers[k] = v;
    }
}

static bool recv_response(int fd, HttpResp& out, int timeout_ms, int hard_ms) {
    out.status = -1;
    std::string buf;
    long start = now_ms();

    while (true) {
        long now = now_ms();
        if (now - start > hard_ms) return false;

        fd_set rfds;
        FD_ZERO(&rfds); FD_SET(fd, &rfds);
        struct timeval tv;
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        int sel = select(fd + 1, &rfds, NULL, NULL, &tv);
        if (sel < 0) { if (errno == EINTR) continue; return false; }
        if (sel == 0) continue;

        char tmp[4096];
        ssize_t n = recv(fd, tmp, sizeof(tmp), 0);
        if (n < 0) { if (errno == EINTR) continue; return false; }
        if (n == 0) break;
        buf.append(tmp, tmp + n);

        size_t pos = buf.find("\r\n\r\n");
        if (pos != std::string::npos) {
            out.head = buf.substr(0, pos + 4);
            out.body = buf.substr(pos + 4);
            parse_headers(out);

            std::istringstream sl(out.head);
            std::string hv; sl >> hv >> out.status;

            std::map<std::string,std::string>::iterator it = out.headers.find("content-length");
            if (it != out.headers.end()) {
                long cl = std::atol(it->second.c_str());
                while ((long)out.body.size() < cl) {
                    fd_set rf2;
                    FD_ZERO(&rf2); FD_SET(fd, &rf2);
                    struct timeval tv2;
                    tv2.tv_sec = timeout_ms / 1000;
                    tv2.tv_usec = (timeout_ms % 1000) * 1000;
                    int s2 = select(fd + 1, &rf2, NULL, NULL, &tv2);
                    if (s2 <= 0) continue;
                    ssize_t n2 = recv(fd, tmp, sizeof(tmp), 0);
                    if (n2 <= 0) break;
                    out.body.append(tmp, tmp + n2);
                }
                if ((long)out.body.size() > cl) out.body.resize((size_t)cl);
                return true;
            }
            return true;
        }
    }
    return out.status != -1;
}

static bool file_exists(const std::string& p) {
    struct stat st;
    return stat(p.c_str(), &st) == 0;
}

static void expect_http(const std::string& name,
                        const std::string& host, const std::string& port,
                        const std::string& req,
                        int exp_status,
                        const std::string& must_header_k,
                        const std::string& must_header_substr,
                        const std::string& body_substr)
{
    int fd = connect_tcp(host, port, 300);
    if (fd < 0) { fail(name, "cannot connect to server"); return; }

    if (!send_all(fd, req)) { close(fd); fail(name, "send failed"); return; }

    HttpResp r;
    bool okrecv = recv_response(fd, r, 200, 3000);
    close(fd);

    if (!okrecv) { fail(name, "recv/timeout"); return; }
    if (r.status != exp_status) {
        std::ostringstream msg;
        msg << "expected " << exp_status << " got " << r.status;
        fail(name, msg.str());
        return;
    }
    if (!must_header_k.empty()) {
        std::map<std::string,std::string>::iterator it = r.headers.find(lower(must_header_k));
        if (it == r.headers.end() || (!must_header_substr.empty() && it->second.find(must_header_substr) == std::string::npos)) {
            fail(name, "missing/invalid header: " + must_header_k);
            return;
        }
    }
    if (!body_substr.empty() && r.body.find(body_substr) == std::string::npos) {
        fail(name, "body does not contain expected substring");
        return;
    }
    ok(name);
}

static void expect_keepalive_pair(const std::string& name,
                                 const std::string& host, const std::string& port,
                                 const std::string& req1, int st1,
                                 const std::string& req2, int st2)
{
    int fd = connect_tcp(host, port, 300);
    if (fd < 0) { fail(name, "cannot connect"); return; }

    if (!send_all(fd, req1)) { close(fd); fail(name, "send1 failed"); return; }
    HttpResp r1;
    if (!recv_response(fd, r1, 200, 3000)) { close(fd); fail(name, "recv1 failed"); return; }

    if (!send_all(fd, req2)) { close(fd); fail(name, "send2 failed"); return; }
    HttpResp r2;
    if (!recv_response(fd, r2, 200, 3000)) { close(fd); fail(name, "recv2 failed"); return; }

    close(fd);
    if (r1.status != st1 || r2.status != st2) {
        std::ostringstream msg;
        msg << "expected " << st1 << "," << st2 << " got " << r1.status << "," << r2.status;
        fail(name, msg.str());
        return;
    }
    ok(name);
}

// ---------------- UNIT test suite ----------------
static void run_unit()
{
    // baseline
    expect_parser("UNIT: valid GET",
        std::vector<std::string>(1, "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n"),
        true,false,0,"GET","/",true,false,"");

    // 505 version
    expect_parser("UNIT: version != 1.1 -> 505",
        std::vector<std::string>(1, "GET / HTTP/1.0\r\nHost: localhost\r\n\r\n"),
        true,true,505,"","",true,false,"");

    // 405 method
    expect_parser("UNIT: method not allowed -> 405",
        std::vector<std::string>(1, "PUT / HTTP/1.1\r\nHost: localhost\r\n\r\n"),
        true,true,405,"","",true,false,"");

    // 400 missing host
    expect_parser("UNIT: missing Host -> 400",
        std::vector<std::string>(1, "GET / HTTP/1.1\r\n\r\n"),
        true,true,400,"","",true,false,"");

    // 411 POST missing CL/TE
    expect_parser("UNIT: POST missing length info -> 411",
        std::vector<std::string>(1, "POST / HTTP/1.1\r\nHost: a\r\n\r\n"),
        true,true,411,"","",true,false,"");

    // 501 TE without chunked
    expect_parser("UNIT: TE not including chunked -> 501",
        std::vector<std::string>(1, "POST / HTTP/1.1\r\nHost: a\r\nTransfer-Encoding: gzip\r\n\r\n"),
        true,true,501,"","",true,false,"");

    // extra token accepted (current behavior)
    expect_parser("UNIT: request line extra token accepted (current behavior)",
        std::vector<std::string>(1, "GET / HTTP/1.1 EXTRA\r\nHost: localhost\r\n\r\n"),
        true,false,0,"GET","/",true,false,"");

    // Host with port
    expect_parser("UNIT: Host with port accepted",
        std::vector<std::string>(1, "GET / HTTP/1.1\r\nHost: localhost:8080\r\n\r\n"),
        true,false,0,"GET","/",true,false,"");

    // Host duplicate with different case -> 400
    expect_parser("UNIT: duplicate Host different case -> 400",
        std::vector<std::string>(1, "GET / HTTP/1.1\r\nHost: a\r\nhOsT: b\r\n\r\n"),
        true,true,400,"","",true,false,"");

    // absolute-form with port OK
    expect_parser("UNIT: absolute-form OK (authority matches Host with port)",
        std::vector<std::string>(1, "GET http://example.com:8080/path HTTP/1.1\r\nHost: example.com:8080\r\n\r\n"),
        true,false,0,"GET","/path",true,false,"");

    // absolute-form mismatch -> 400
    expect_parser("UNIT: absolute-form mismatch -> 400",
        std::vector<std::string>(1, "GET http://example.com:8080/path HTTP/1.1\r\nHost: example.com\r\n\r\n"),
        true,true,400,"","",true,false,"");

    // Connection list not treated as close (current behavior)
    expect_parser("UNIT: Connection close, keep-alive NOT treated as close",
        std::vector<std::string>(1, "GET / HTTP/1.1\r\nHost: a\r\nConnection: close, keep-alive\r\n\r\n"),
        true,false,0,"GET","/",true,false,"");

    // Connection exact close treated as close
    expect_parser("UNIT: Connection Close -> keep_alive false",
        std::vector<std::string>(1, "GET / HTTP/1.1\r\nHost: a\r\nConnection:  Close \r\n\r\n"),
        true,false,0,"GET","/",false,false,"");

    // ===== 缺口补齐：414 URI too long =====
    std::string long_uri = "/";
    long_uri.append(5000, 'a');
    expect_parser("UNIT: URI too long -> 414",
        std::vector<std::string>(1, "GET " + long_uri + " HTTP/1.1\r\nHost: a\r\n\r\n"),
        true,true,414,"","",true,false,"");

    // ===== 缺口补齐：400 invalid URI char =====
    expect_parser("UNIT: invalid URI char -> 400",
        std::vector<std::string>(1, "GET /a<> HTTP/1.1\r\nHost: a\r\n\r\n"),
        true,true,400,"","",true,false,"");

    // ===== 缺口补齐：403 path traversal =====
    expect_parser("UNIT: path contains .. -> 403",
        std::vector<std::string>(1, "GET /a/../b HTTP/1.1\r\nHost: a\r\n\r\n"),
        true,true,403,"","",true,false,"");

    // ===== 缺口补齐：431 header too large (no \\r\\n\\r\\n) =====
    {
        HTTPRequestParser p;
        std::string big;
        big.append(9000, 'A');
        (void)p.dejaParse(big);
        const HTTPRequest& r = p.getRequest();
        if (!r.bad_request || r.error_code != 431)
            fail("UNIT: header too large -> 431", "expected 431");
        else
            ok("UNIT: header too large -> 431");
    }

    // ===== 缺口补齐：400 header line no colon =====
    expect_parser("UNIT: header line no colon -> 400",
        std::vector<std::string>(1, "GET / HTTP/1.1\r\nHost: a\r\nBadHeader\r\n\r\n"),
        true,true,400,"","",true,false,"");

    // ===== 缺口补齐：400 invalid header name =====
    expect_parser("UNIT: invalid header name -> 400",
        std::vector<std::string>(1, "GET / HTTP/1.1\r\nHost: a\r\nBad Name: x\r\n\r\n"),
        true,true,400,"","",true,false,"");

    // ===== 缺口补齐：duplicate Content-Length -> 400 =====
    expect_parser("UNIT: duplicate Content-Length -> 400",
        std::vector<std::string>(1, "POST / HTTP/1.1\r\nHost: a\r\nContent-Length: 1\r\nContent-Length: 1\r\n\r\na"),
        true,true,400,"","",true,false,"");

    // ===== 缺口补齐：Content-Length too large -> 413 =====
    expect_parser("UNIT: Content-Length too large -> 413",
        std::vector<std::string>(1, "POST / HTTP/1.1\r\nHost: a\r\nContent-Length: 999999999\r\n\r\n"),
        true,true,413,"","",true,false,"");

    // ===== 缺口补齐：TE repeated -> 400 =====
    expect_parser("UNIT: Transfer-Encoding repeated -> 400",
        std::vector<std::string>(1, "POST / HTTP/1.1\r\nHost: a\r\nTransfer-Encoding: chunked\r\nTransfer-Encoding: chunked\r\n\r\n0\r\n\r\n"),
        true,true,400,"","",true,false,"");

    // ===== 缺口补齐：TE + CL both present -> 400 =====
    expect_parser("UNIT: TE chunked + Content-Length -> 400",
        std::vector<std::string>(1, "POST / HTTP/1.1\r\nHost: a\r\nTransfer-Encoding: chunked\r\nContent-Length: 5\r\n\r\n0\r\n\r\n"),
        true,true,400,"","",true,false,"");

    // fixed body incremental
    {
        std::vector<std::string> inc;
        inc.push_back("POST /upload/u.txt HTTP/1.1\r\nHost: a\r\nContent-Length: 11\r\n\r\n");
        inc.push_back("hello ");
        inc.push_back("world");
        expect_parser("UNIT: fixed body incremental",
            inc, true,false,0,"POST","/upload/u.txt",true,false,"hello world");
    }

    // chunked OK
    {
        std::vector<std::string> c1;
        c1.push_back("POST /upload/u.txt HTTP/1.1\r\nHost: a\r\nTransfer-Encoding: chunked\r\n\r\n");
        c1.push_back("5\r\nhello\r\n");
        c1.push_back("6\r\n world\r\n");
        c1.push_back("0\r\n\r\n");
        expect_parser("UNIT: chunked OK",
            c1, true,false,0,"POST","/upload/u.txt",true,true,"hello world");
    }

    // chunk extension supported
    {
        std::vector<std::string> cext;
        cext.push_back("POST /upload/u.txt HTTP/1.1\r\nHost: a\r\nTransfer-Encoding: chunked\r\n\r\n");
        cext.push_back("5;ext=1\r\nhello\r\n");
        cext.push_back("0\r\n\r\n");
        expect_parser("UNIT: chunk extension supported",
            cext, true,false,0,"POST","/upload/u.txt",true,true,"hello");
    }

    // chunked incomplete / trailer incomplete
    expect_chunked_incomplete_not_error();
    expect_chunked_trailer_incomplete_not_error();

    // LF-only
    expect_lf_only_no_progress();

    // ErrorResponse / ResponseBuilder
    {
        HTTPResponse e405 = buildErrorResponse(405);
        if (e405.headers.find("allow") == e405.headers.end()) fail("UNIT: ErrorResponse 405 Allow", "missing allow");
        else ok("UNIT: ErrorResponse 405 Allow");

        HTTPResponse r;
        r.statusCode = 200; r.statusText="OK"; r.body="HI";
        std::string out = ResponseBuilder::build(r);
        if (!contains(out, "content-length: 2\r\n")) fail("UNIT: ResponseBuilder content-length", "missing");
        else ok("UNIT: ResponseBuilder content-length");
    }
}

// ---------------- INTEGRATION suite ----------------
static void run_integration(const std::string& host, const std::string& port,
                            const std::string& root_dir, const std::string& upload_dir)
{
    (void)root_dir; 
    // GET static 200
    expect_http("INT: GET /hello.txt -> 200",
        host, port,
        "GET /hello.txt HTTP/1.1\r\nHost: localhost\r\n\r\n",
        200, "content-length", "", "");

    // GET 404
    expect_http("INT: GET /nope -> 404",
        host, port,
        "GET /nope HTTP/1.1\r\nHost: localhost\r\n\r\n",
        404, "content-length", "", "");

    // GET directory index 200
    expect_http("INT: GET /dir/ -> 200",
        host, port,
        "GET /dir/ HTTP/1.1\r\nHost: localhost\r\n\r\n",
        200, "content-length", "", "");

    // autoindex off 403
    expect_http("INT: GET /emptydir/ -> 403",
        host, port,
        "GET /emptydir/ HTTP/1.1\r\nHost: localhost\r\n\r\n",
        403, "content-length", "", "");

    // ===== methods 缺口补齐：GET permission denied -> 403 =====
    // 需要 root_dir 下存在 /noperm.txt 且 chmod 000（由脚本准备）
    expect_http("INT: GET /noperm.txt -> 403",
        host, port,
        "GET /noperm.txt HTTP/1.1\r\nHost: localhost\r\n\r\n",
        403, "content-length", "", "");

    // POST upload CL success -> 201
    expect_http("INT: POST upload (CL) -> 201",
        host, port,
        "POST /upload/u.txt HTTP/1.1\r\nHost: localhost\r\nContent-Length: 12\r\n\r\nupload-test\n",
        201, "content-length", "", "");

    if (!upload_dir.empty()) {
        std::string fp = upload_dir + "/u.txt";
        if (!file_exists(fp)) fail("INT: upload side effect (CL)", "file not created: " + fp);
        else ok("INT: upload side effect (CL)");
    }

    // POST empty body -> 400
    expect_http("INT: POST empty body -> 400",
        host, port,
        "POST /upload/empty.txt HTTP/1.1\r\nHost: localhost\r\nContent-Length: 0\r\n\r\n",
        400, "content-length", "", "");

    // POST non-upload -> 403
    expect_http("INT: POST non-upload -> 403",
        host, port,
        "POST /hello.txt HTTP/1.1\r\nHost: localhost\r\nContent-Length: 1\r\n\r\na",
        403, "content-length", "", "");

    // ===== methods 缺口补齐：chunked POST upload -> 201 =====
    expect_http("INT: POST upload (chunked) -> 201",
        host, port,
        "POST /upload/chunk.txt HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n"
        "5\r\nhello\r\n"
        "6\r\n world\r\n"
        "0\r\n\r\n",
        201, "content-length", "", "");

    if (!upload_dir.empty()) {
        std::string fp2 = upload_dir + "/chunk.txt";
        if (!file_exists(fp2)) fail("INT: upload side effect (chunked)", "file not created: " + fp2);
        else ok("INT: upload side effect (chunked)");
    }

    // ===== methods 缺口补齐：upload dir not writable -> 403 =====
    // 脚本会在测试前短暂 chmod 000 upload_dir 并请求 /upload/forbidden.txt
    expect_http("INT: POST upload dir no-permission -> 403",
        host, port,
        "POST /upload/forbidden.txt HTTP/1.1\r\nHost: localhost\r\nContent-Length: 1\r\n\r\na",
        403, "content-length", "", "");

    // DELETE existing -> 204
    expect_http("INT: DELETE existing upload -> 204",
        host, port,
        "DELETE /upload/u.txt HTTP/1.1\r\nHost: localhost\r\n\r\n",
        204, "content-length", "", "");

    // DELETE missing -> 404
    expect_http("INT: DELETE missing -> 404",
        host, port,
        "DELETE /upload/nope.txt HTTP/1.1\r\nHost: localhost\r\n\r\n",
        404, "content-length", "", "");

    // DELETE directory -> 403
    expect_http("INT: DELETE directory -> 403",
        host, port,
        "DELETE /dir/ HTTP/1.1\r\nHost: localhost\r\n\r\n",
        403, "content-length", "", "");

    // ===== methods 缺口补齐：DELETE permission denied -> 403 =====
    // 脚本准备 /protected.txt 并 chmod 000
    expect_http("INT: DELETE protected -> 403",
        host, port,
        "DELETE /protected.txt HTTP/1.1\r\nHost: localhost\r\n\r\n",
        403, "content-length", "", "");

    // PUT -> 405 with Allow
    expect_http("INT: PUT -> 405",
        host, port,
        "PUT /hello.txt HTTP/1.1\r\nHost: localhost\r\nContent-Length: 0\r\n\r\n",
        405, "allow", "", "");

    // keep-alive: error then success in same socket (404 then 200)
    expect_keepalive_pair("INT: keep-alive (404 then 200)",
        host, port,
        "GET /nope HTTP/1.1\r\nHost: localhost\r\nConnection: keep-alive\r\n\r\n", 404,
        "GET /hello.txt HTTP/1.1\r\nHost: localhost\r\nConnection: keep-alive\r\n\r\n", 200);

    // keep-alive: 405 then 200
    expect_keepalive_pair("INT: keep-alive (405 then 200)",
        host, port,
        "PUT /hello.txt HTTP/1.1\r\nHost: localhost\r\nConnection: keep-alive\r\nContent-Length: 0\r\n\r\n", 405,
        "GET /hello.txt HTTP/1.1\r\nHost: localhost\r\nConnection: keep-alive\r\n\r\n", 200);

    // Optional redirect check (depends on config)
    {
        int fd = connect_tcp(host, port, 300);
        if (fd < 0) { skip("INT: redirect optional", "cannot connect"); }
        else {
            send_all(fd, "GET /redirect HTTP/1.1\r\nHost: localhost\r\n\r\n");
            HttpResp r;
            bool okrecv = recv_response(fd, r, 200, 1500);
            close(fd);
            if (!okrecv) skip("INT: redirect optional", "no response");
            else if (r.status >= 300 && r.status < 400) {
                if (r.headers.find("location") == r.headers.end())
                    fail("INT: redirect optional", "3xx but no Location header");
                else
                    ok("INT: redirect optional (3xx + Location)");
            } else {
                skip("INT: redirect optional", "no redirect configured at /redirect");
            }
        }
    }
}

static void usage() {
    std::cout << "Usage:\n"
              << "  ./test_http --unit\n"
              << "  ./test_http --integration <host> <port> <root_dir> <upload_dir>\n";
}

int main(int argc, char** argv)
{
    if (argc < 2) { usage(); return 2; }

    std::string mode = argv[1];

    if (mode == "--unit") {
        run_unit();
    } else if (mode == "--integration") {
        if (argc < 6) { usage(); return 2; }
        std::string host = argv[2];
        std::string port = argv[3];
        std::string root_dir = argv[4];
        std::string upload_dir = argv[5];
        run_integration(host, port, root_dir, upload_dir);
    } else {
        usage();
        return 2;
    }

    if (g_fail == 0) {
        std::cout << "\nALL PASS";
        if (g_skip) std::cout << " (SKIP: " << g_skip << ")";
        std::cout << "\n";
        return 0;
    }
    std::cout << "\nFAILURES: " << g_fail << " (SKIP: " << g_skip << ")\n";
    return 1;
}
