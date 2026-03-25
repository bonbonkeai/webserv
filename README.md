# **Webserv**

*This project was developed as part of the 42 curriculum by Jdu, Yujin, and Jmen.*

---

##  Description

**Webserv** is a lightweight HTTP/1.1 web server written in **C++98**, inspired by **Nginx-style configuration and routing**, with configurable routing, CGI support, and event-driven connection handling.

The project aims to explore how a real web server works internally, including:

* HTTP request parsing and response generation
* Socket programming and client-server communication
* Event-driven handling of multiple simultaneous connections
* Configurable routing, CGI execution, uploads, redirects, and custom error pages

---

##  Instructions

###  Compilation

The project includes a **Makefile** with the following standard rules:

```bash id="c6v5gm"
make        # Compile the server
make clean  # Remove object files
make fclean # Remove object files and binary
make re     # Recompile the project
```

---

###  Dependencies

* **C++98-compliant compiler** (e.g., `clang++`, `g++`)
* **Make**
* **POSIX-compliant operating system** (Linux, macOS)

---

###  Execution

To start the server, run:

```bash id="a2l9wr"
./webserv [configuration_file]
```

If no configuration file is specified, the server will attempt to use a default `default.conf` in the current directory.

---

##  Resources

###  Documentation & References

* Nginx Configuration Documentation
* RFC 7230 and RFC 7231, along with related HTTP/1.1 RFCs (7232–7235)
* Beej’s Guide to Network Programming
* Linux manual pages (`man socket`, `bind`, `listen`, `accept`, `poll`, etc.)

---

###  Use of AI

AI tools (such as ChatGPT) were used for:

* Understanding HTTP protocol concepts
* Debugging and clarifying error messages
* Explaining networking and system calls
* Improving documentation and formatting

All implementation and design decisions were made by the authors.

---

##  Additional Sections

### Features

* Supports **GET**, **POST**, and **DELETE**
* Supports **persistent connections** when allowed by request/response handling
* **Configurable server and location blocks** inspired by Nginx-style configuration
* Static file serving with **index resolution**
* **Directory listing (autoindex)**
* **CGI execution** for dynamic content (e.g. `.py` and `.sh` scripts)
* **Multipart and raw upload** handling under upload routes
* **File deletion** through DELETE requests
* **Custom error pages** configured through `error_page`
* **Redirect handling** through `return` rules
* **Non-blocking event-driven I/O using epoll**

---
