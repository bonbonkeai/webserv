#include <iostream>
#include "Event/hpp/Server.hpp"
#include <csignal>
#include <cstdlib>
#include <stdio.h>
#include <string.h>

void signal_handler(int signum)
{
    std::cout << "\n[Signal] Caught signal " << signum << ", shutting down..." << std::endl;
    // Cleanup will happen in destructor
    exit(0);
}

void print_usage(const char *prog_name)
{
    std::cout << "Usage: " << prog_name <<#include "Server.hpp" " <port>" << std::endl;
    std::cout << "Example: " << prog_name << " 8080" << std::endl;
}
void setup_test_cgi_script()
{
    std::cout << "[Setup] Creating test CGI scripts..." << std::endl;

    // Create a simple test.cgi script
    const char *test_script =
        "#!/bin/bash\n"
        "echo 'Content-Type: text/html'\n"
        "echo ''\n"
        "echo '<html><body>'\n"
        "echo '<h1>CGI Test Page</h1>'\n"
        "echo '<p>Request Method: '$REQUEST_METHOD'</p>'\n"
        "echo '<p>Query String: '$QUERY_STRING'</p>'\n"
        "echo '<p>Server Name: '$SERVER_NAME'</p>'\n"
        "echo '<p>Server Port: '$SERVER_PORT'</p>'\n"
        "if [ \"$REQUEST_METHOD\" = \"POST\" ]; then\n"
        "  echo '<h2>POST Data:</h2>'\n"
        "  echo '<pre>'\n"
        "  cat\n"
        "  echo '</pre>'\n"
        "fi\n"
        "echo '</body></html>'\n";

    system("mkdir -p ./cgi-bin");
    FILE *f = fopen("./cgi-bin/test.cgi", "w");
    if (f)
    {
        fwrite(test_script, 1, strlen(test_script), f);
        fclose(f);
        system("chmod +x ./cgi-bin/test.cgi");
        std::cout << "[Setup] Created ./cgi-bin/test.cgi" << std::endl;
    }

    // Create a slow CGI script to test timeout handling
    const char *slow_script =
        "#!/bin/bash\n"
        "echo 'Content-Type: text/plain'\n"
        "echo ''\n"
        "echo 'Starting slow process...'\n"
        "sleep 3\n"
        "echo 'Done after 3 seconds'\n";

    f = fopen("./cgi-bin/slow.cgi", "w");
    if (f)
    {
        fwrite(slow_script, 1, strlen(slow_script), f);
        fclose(f);
        system("chmod +x ./cgi-bin/slow.cgi");
        std::cout << "[Setup] Created ./cgi-bin/slow.cgi" << std::endl;
    }

    // Create an echo POST CGI script
    const char *echo_script =
        "#!/bin/bash\n"
        "echo 'Content-Type: text/plain'\n"
        "echo ''\n"
        "echo 'You sent:'\n"
        "cat\n";

    f = fopen("./cgi-bin/echo.cgi", "w");
    if (f)
    {
        fwrite(echo_script, 1, strlen(echo_script), f);
        fclose(f);
        system("chmod +x ./cgi-bin/echo.cgi");
        std::cout << "[Setup] Created ./cgi-bin/echo.cgi" << std::endl;
    }
}

void print_test_instructions(int port)
{
    std::cout << "\n========================================" << std::endl;
    std::cout << "Server started successfully!" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Listening on port: " << port << std::endl;
    std::cout << "\nTest Commands:" << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    std::cout << "1. Basic GET request:" << std::endl;
    std::cout << "   curl http://localhost:" << port << "/" << std::endl;
    std::cout << "\n2. CGI GET request:" << std::endl;
    std::cout << "   curl http://localhost:" << port << "/cgi-bin/test.cgi" << std::endl;
    std::cout << "\n3. CGI with query string:" << std::endl;
    std::cout << "   curl 'http://localhost:" << port << "/cgi-bin/test.cgi?name=test&value=123'" << std::endl;
    std::cout << "\n4. CGI POST request:" << std::endl;
    std::cout << "   curl -X POST -d 'key1=value1&key2=value2' http://localhost:" << port << "/cgi-bin/echo.cgi" << std::endl;
    std::cout << "\n5. Slow CGI (3 second delay):" << std::endl;
    std::cout << "   curl http://localhost:" << port << "/cgi-bin/slow.cgi" << std::endl;
    std::cout << "\n6. Keep-Alive test (multiple requests):" << std::endl;
    std::cout << "   curl -H 'Connection: keep-alive' http://localhost:" << port << "/" << std::endl;
    std::cout << "\n7. Concurrent requests test:" << std::endl;
    std::cout << "   for i in {1..10}; do curl http://localhost:" << port << "/ & done; wait" << std::endl;
    std::cout << "\n8. Stress test with ab (Apache Bench):" << std::endl;
    std::cout << "   ab -n 1000 -c 50 http://localhost:" << port << "/" << std::endl;
    std::cout << "\n9. Load test with wrk:" << std::endl;
    std::cout << "   wrk -t4 -c100 -d10s http://localhost:" << port << "/" << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    std::cout << "Press Ctrl+C to stop the server" << std::endl;
    std::cout << "========================================\n"
              << std::endl;
}
int main(int ac, char **av)
{
    if (ac != 2)
    {
        std::cout << "Error: nbr of arguments error" << std::endl;
        return 1;
    }
    int port = std::atoi(av[1]);
    if (port <= 0 || port > 65535)
    {
        std::cerr << "Error: invalide port nbr" << std::endl;
        return 1;
    }
    // Setup signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGPIPE, SIG_IGN); // Ignore broken pipe

    try
    {
        setup_test_cgi_script();
        Server _s(port);
        _s.init_sockets();
        print_test_instructions(port);
        _s.run();
    }
    catch (const std::exception &e)
    {
        std::cerr << "[Error] Exception: " << e.what() << std::endl;
        return 1;
    }
    catch (...)
    {
        std::cerr << "[Error] Unknown exception occurred" << std::endl;
        return 1;
    }

    return 0;#include <iostream>
#include "Event/hpp/Server.hpp"
#include <csignal>
#include <cstdlib>
#include <stdio.h>
#include <string.h>
void signal_handler(int signum)
{
    std::cout << "\n[Signal] Caught signal " << signum << ", shutting down..." << std::endl;
    // Cleanup will happen in destructor
    exit(0);
}

void print_usage(const char *prog_name)
{
    std::cout << "Usage: " << prog_name << " <port>" << std::endl;
    std::cout << "Example: " << prog_name << " 8080" << std::endl;
}
void setup_test_cgi_script()
{
    std::cout << "[Setup] Creating test CGI scripts..." << std::endl;

    // Create a simple test.cgi script
    const char *test_script =
        "#!/bin/bash\n"
        "echo 'Content-Type: text/html'\n"
        "echo ''\n"
        "echo '<html><body>'\n"
        "echo '<h1>CGI Test Page</h1>'\n"
        "echo '<p>Request Method: '$REQUEST_METHOD'</p>'\n"
        "echo '<p>Query String: '$QUERY_STRING'</p>'\n"
        "echo '<p>Server Name: '$SERVER_NAME'</p>'\n"
        "echo '<p>Server Port: '$SERVER_PORT'</p>'\n"
        "if [ \"$REQUEST_METHOD\" = \"POST\" ]; then\n"
        "  echo '<h2>POST Data:</h2>'\n"
        "  echo '<pre>'\n"
        "  cat\n"
        "  echo '</pre>'\n"
        "fi\n"
        "echo '</body></html>'\n";

    system("mkdir -p ./cgi-bin");
    FILE *f = fopen("./cgi-bin/test.cgi", "w");
    if (f)
    {
        fwrite(test_script, 1, strlen(test_script), f);
        fclose(f);
        system("chmod +x ./cgi-bin/test.cgi");
        std::cout << "[Setup] Created ./cgi-bin/test.cgi" << std::endl;
    }

    // Create a slow CGI script to test timeout handling
    const char *slow_script =
        "#!/bin/bash\n"
        "echo 'Content-Type: text/plain'\n"
        "echo ''\n"
        "echo 'Starting slow process...'\n"
        "sleep 3\n"
        "echo 'Done after 3 seconds'\n";

    f = fopen("./cgi-bin/slow.cgi", "w");
    if (f)
    {
        fwrite(slow_script, 1, strlen(slow_script), f);
        fclose(f);
        system("chmod +x ./cgi-bin/slow.cgi");
        std::cout << "[Setup] Created ./cgi-bin/slow.cgi" << std::endl;
    }

    // Create an echo POST CGI script
    const char *echo_script =
        "#!/bin/bash\n"
        "echo 'Content-Type: text/plain'\n"
        "echo ''\n"
        "echo 'You sent:'\n"
        "cat\n";

    f = fopen("./cgi-bin/echo.cgi", "w");
    if (f)
    {
        fwrite(echo_script, 1, strlen(echo_script), f);
        fclose(f);
        system("chmod +x ./cgi-bin/echo.cgi");
        std::cout << "[Setup] Created ./cgi-bin/echo.cgi" << std::endl;
    }
}

void print_test_instructions(int port)
{
    std::cout << "\n========================================" << std::endl;
    std::cout << "Server started successfully!" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Listening on port: " << port << std::endl;
    std::cout << "\nTest Commands:" << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    std::cout << "1. Basic GET request:" << std::endl;
    std::cout << "   curl http://localhost:" << port << "/" << std::endl;
    std::cout << "\n2. CGI GET request:" << std::endl;
    std::cout << "   curl http://localhost:" << port << "/cgi-bin/test.cgi" << std::endl;
    std::cout << "\n3. CGI with query string:" << std::endl;
    std::cout << "   curl 'http://localhost:" << port << "/cgi-bin/test.cgi?name=test&value=123'" << std::endl;
    std::cout << "\n4. CGI POST request:" << std::endl;
    std::cout << "   curl -X POST -d 'key1=value1&key2=value2' http://localhost:" << port << "/cgi-bin/echo.cgi" << std::endl;
    std::cout << "\n5. Slow CGI (3 second delay):" << std::endl;
    std::cout << "   curl http://localhost:" << port << "/cgi-bin/slow.cgi" << std::endl;
    std::cout << "\n6. Keep-Alive test (multiple requests):" << std::endl;
    std::cout << "   curl -H 'Connection: keep-alive' http://localhost:" << port << "/" << std::endl;
    std::cout << "\n7. Concurrent requests test:" << std::endl;
    std::cout << "   for i in {1..10}; do curl http://localhost:" << port << "/ & done; wait" << std::endl;
    std::cout << "\n8. Stress test with ab (Apache Bench):" << std::endl;
    std::cout << "   ab -n 1000 -c 50 http://localhost:" << port << "/" << std::endl;
    std::cout << "\n9. Load test with wrk:" << std::endl;
    std::cout << "   wrk -t4 -c100 -d10s http://localhost:" << port << "/" << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    std::cout << "Press Ctrl+C to stop the server" << std::endl;
    std::cout << "========================================\n"
              << std::endl;
}
int main(int ac, char **av)
{
    if (ac != 2)
    {
        std::cout << "Error: nbr of arguments error" << std::endl;
        return 1;
    }
    int port = std::atoi(av[1]);
    if (port <= 0 || port > 65535)
    {
        std::cerr << "Error: invalide port nbr" << std::endl;
        return 1;
    }
    // Setup signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGPIPE, SIG_IGN); // Ignore broken pipe

    try
    {
        setup_test_cgi_script();
        Server _s(port);
        _s.init_sockets();
        print_test_instructions(port);
        _s.run();
    }
    catch (const std::exception &e)
    {
        std::cerr << "[Error] Exception: " << e.what() << std::endl;
        return 1;
    }
    catch (...)
    {
        std::cerr << "[Error] Unknown exception occurred" << std::endl;
        return 1;
    }

    return 0;
}
}