#include <iostream>
#include <string>
#include <stdexcept>
#include "Event/hpp/Server.hpp"
#include "Event/hpp/EpollManager.hpp"
#include "Event/hpp/Client.hpp"

// int main(int ac, char **av)
// {
//     try
//     {
//         int port = 8080;
//         if (ac == 2)
//             port = std::atoi(av[1]);
//         Server s(port);
//         if (!s.init_sockets())
//             throw std::runtime_error("init_sockets() failed");
//         std::cout << "Init succes, entering event loop" << std::endl;
//         s.load_config(av[1]);
//         s.run();
//         return (0);
//     }
//     catch(const std::exception& e)
//     {
//         std::cerr << "Failed" << e.what() << std::endl;
//         return (1);
//     }
// }

#include <iostream>
#include <string>
#include <stdexcept>
#include <cstdlib> // atoi
#include "Event/hpp/Server.hpp"

int main(int ac, char **av)
{
    try
    {
        int port = 8080;
        std::string cfg = "config_defaut/default.conf";

        if (ac == 2)
        {
            // 约定：一个参数时当作 config path
            cfg = av[1];
        }
        else if (ac == 3)
        {
            port = std::atoi(av[1]);
            cfg = av[2];
        }
        else if (ac != 1)
        {
            throw std::runtime_error("Usage: ./webserv [config_path] OR ./webserv <port> <config_path>");
        }

        Server s(port);
        // load_config 再 init_sockets
        s.load_config(cfg);
        if (!s.init_sockets())
            throw std::runtime_error("init_sockets() failed");
        std::cout << "Init success, entering event loop" << std::endl;
        s.run();
        return (0);
    }
    catch (const std::exception &e)
    {
        std::cerr << "Failed: " << e.what() << std::endl;
        return (1);
    }
}
