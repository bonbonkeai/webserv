
WebServe

This project has been created as part of the 42 curriculum by Jdu, Yujin, Jmen. 

Description
A Nginx-like webserver in C++

Instructions
Compilation

The project includes a Makefile with the following standard rules:
make        # Compiles the server binary
make clean  # Removes object files
make fclean # Removes object files and the binary
make re     # Recompiles the entire project

The server is written in C++98 and uses no external libraries aside from the standard C++ and POSIX libraries.

Dependencies

 • C++98-compliant compiler (clang++/g++)

 • Make

 • POSIX-compliant operating system (Linux, macOS)

Execution

To start the server, provide a configuration file as an argument:

./webserve [configuration_file]

If no configuration file is specified, the server will attempt to use a default default.conf in the current directory.

Resources

Documentation & References

Nginx Configuration Documentation


Additional Sections
Features

    Supports GET, POST, and DELETE methods

    Handles persistent and non-persistent connections

    Configurable via a flexible configuration file

    CGI support for dynamic content (e.g., Python scripts)

    Multipart file upload handling

    Custom error pages

    Directory listing support (optional)

    Non-blocking I/O with poll() or select()

