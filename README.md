# **Webserv**

This project was developed as part of the **42 curriculum** by **Jdu**, **Yujin**, and **Jmen**.

---

##  Description

**Webserv** is a lightweight HTTP/1.1 web server written in **C++**, inspired by **Nginx**.

The goal of this project is to gain a deep understanding of:

* How web servers work internally
* HTTP request parsing and response generation
* Socket programming and client-server communication
* Event-driven architectures for handling multiple connections

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
* RFC 7230–7235 — HTTP/1.1 Specification
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

###  Features

* Supports **GET**, **POST**, and **DELETE** methods
* Handles **persistent and non-persistent connections**
* Fully **configurable via a configuration file**
* **CGI support** for dynamic content (e.g., Python scripts)
* **Multipart file upload** handling
* **Custom error pages**
* **Directory listing (autoindex)** *(optional)*
* **Non-blocking I/O** using `poll()` or `select()`

---
