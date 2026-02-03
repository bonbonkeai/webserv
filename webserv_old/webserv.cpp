#include <iostream>
#include <string>
#include <stdexcept>
#include "Event/hpp/Server.hpp"
#include "Event/hpp/EpollManager.hpp"
#include "Event/hpp/Client.hpp"

int main(int ac, char **av)
{
    try
    {
        int port = 8080;
        if (ac == 2)
            port = std::atoi(av[1]);
        Server s(port);
        if (!s.init_sockets())
            throw std::runtime_error("init_sockets() failed");
        std::cout << "Init succes, entering event loop" << std::endl;
        s.load_config(av[1]);
        s.run();
        return (0);
    }
    catch(const std::exception& e)
    {
        std::cerr << "Failed" << e.what() << std::endl;
        return (1);
    }
}