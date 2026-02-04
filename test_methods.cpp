#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <cstdlib>
#include <cerrno>
#include <cstring>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

// ---- Your project headers (adjust paths if needed) ----
#include "HTTP/hpp/HTTPRequestParser.hpp"
#include "HTTP/hpp/HTTPResponse.hpp"
#include "HTTP/hpp/ResponseBuilder.hpp"
#include "HTTP/hpp/ErrorResponse.hpp"
#include "HTTP/hpp/RequestFactory.hpp"
#include "Method_Handle/hpp/IRequest.hpp"

// ------------------------------------------------------

static int setReuseAddr(int fd)
{
    int yes = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0)
        return -1;
    return 0;
}

static int createListenSocket(int port)
{
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
        return -1;

    if (setReuseAddr(fd) < 0)
    {
        ::close(fd);
        return -1;
    }

    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);  // 0.0.0.0
    addr.sin_port = htons((unsigned short)port);

    if (::bind(fd, (sockaddr*)&addr, sizeof(addr)) < 0)
    {
        ::close(fd);
        return -1;
    }

    if (::listen(fd, 128) < 0)
    {
        ::close(fd);
        return -1;
    }
    return fd;
}

static bool sendAll(int fd, const std::string& data)
{
    std::size_t total = 0;
    while (total < data.size())
    {
        ssize_t n = ::send(fd, data.data() + total, data.size() - total, 0);
        if (n < 0)
        {
            if (errno == EINTR)
                continue;
            return false;
        }
        if (n == 0)
            return false;
        total += (std::size_t)n;
    }
    return true;
}

static void handleOneConnection(int cfd)
{
    HTTPRequestParser parser;
    std::string recvBuf;
    recvBuf.reserve(8192);

    // We read until a complete request is parsed, then respond.
    // For keep-alive, loop to parse multiple requests from same connection.
    while (true)
    {
        char tmp[4096];
        ssize_t n = ::recv(cfd, tmp, sizeof(tmp), 0);
        if (n < 0)
        {
            if (errno == EINTR)
                continue;
            // recv error -> close
            return;
        }
        if (n == 0)
        {
            // client closed
            return;
        }

        bool ok = parser.dejaParse(std::string(tmp, (std::size_t)n));

        // parser.dejaParse returns:
        // - true when PARSE_DONE (complete) OR need-more-data (depending on your implementation)
        // In your parser: when PARSE_DONE it returns true; when need more data it returns true too.
        // So we check request.complet to know "complete request parsed".
        const HTTPRequest& req = parser.getRequest();

        if (!ok && req.bad_request)
        {
            // Parser error: build error response and close or keep-alive based on req.keep_alive
            HTTPResponse err = buildErrorResponse(req.error_code);
            err.headers["connection"] = (req.keep_alive ? "keep-alive" : "close");
            std::string out = ResponseBuilder::build(err);
            sendAll(cfd, out);
            if (!req.keep_alive)
                return;

            parser.reset();
            continue;
        }

        if (!req.complet)
        {
            // Need more data, continue recv
            continue;
        }

        // Complete request: dispatch to business layer via RequestFactory
        IRequest* h = RequestFactory::create(req);
        HTTPResponse resp;
        if (!h)
        {
            resp = buildErrorResponse(500);
            resp.headers["connection"] = (req.keep_alive ? "keep-alive" : "close");
        }
        else
        {
            resp = h->handle();
            delete h;
        }

        // Ensure connection header consistent with request (unless handler already set)
        // You already set connection in your handlers; this is just a safety net.
        if (resp.headers.find("connection") == resp.headers.end())
            resp.headers["connection"] = (req.keep_alive ? "keep-alive" : "close");

        std::string out = ResponseBuilder::build(resp);
        sendAll(cfd, out);

        if (!req.keep_alive)
            return;

        // Ready for next request on same connection
        parser.reset();
    }
}

int main(int ac, char** av)
{
    int port = 8080;
    if (ac == 2)
        port = std::atoi(av[1]);

    // Prepare folders expected by your handlers (optional but helpful)
    // We do not call mkdir here to avoid extra headers/permissions issues.

    int lfd = createListenSocket(port);
    if (lfd < 0)
    {
        std::cerr << "[FATAL] cannot listen on port " << port
                  << " : " << std::strerror(errno) << std::endl;
        return 1;
    }

    std::cout << "mini_webserv listening on 0.0.0.0:" << port << std::endl;
    std::cout << "root: ./www , upload: ./upload" << std::endl;

    while (true)
    {
        sockaddr_in cli;
        socklen_t len = sizeof(cli);
        int cfd = ::accept(lfd, (sockaddr*)&cli, &len);
        if (cfd < 0)
        {
            if (errno == EINTR)
                continue;
            std::cerr << "[WARN] accept error: " << std::strerror(errno) << std::endl;
            continue;
        }

        // Single-thread, blocking handling
        handleOneConnection(cfd);
        ::close(cfd);
    }

    ::close(lfd);
    return 0;
}
