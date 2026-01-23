#include <iostream>
#include "Event/hpp/Server.hpp"

int main()
{
    int port = 8090;

    try
    {
        Server s(port);
        s.init_sockets();
        std::cout << "Listen on http://localhost:" << port << std::endl;
        s.run();
    }
    catch(std::exception& e)
    {
        std::cerr << e.what() << std::endl;
        return 1;
    }
    return 0;
}