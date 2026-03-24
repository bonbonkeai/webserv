// #include <iostream>
// #include <string>
// #include <stdexcept>
// #include "Event/hpp/new_Server.hpp"
// #include "Event/hpp/EpollManager.hpp"
// #include "Event/hpp/Client.hpp"
// #include <cstdlib> // atoi

// int main(int ac, char **av)
// {
//     try
//     {
//         std::string cfg = "config_defaut/default.conf";
//         if (ac == 2)
//             cfg = av[1];
//         else if (ac != 1)
//             throw std::runtime_error("Usage: ./webserv [config_path]");

//         Server s(0);  // port 由 config 决定
//         s.load_config(cfg);
//         if (!s.init_sockets())
//             throw std::runtime_error("init_sockets() failed");
//         std::cout << "Init success, entering event loop" << std::endl;
//         s.run();
//         return 0;
//     }
//     catch (const std::exception &e)
//     {
//         std::cerr << "Failed: " << e.what() << std::endl;
//         return 1;
//     }
// }#include <iostream>
// #include <string>
// #include <stdexcept>
// #include "Event/hpp/new_Server.hpp"
// #include "Event/hpp/EpollManager.hpp"
// #include "Event/hpp/Client.hpp"
// #include <cstdlib> // atoi

// int main(int ac, char **av)
// {
//     try
//     {
//         std::string cfg = "config_defaut/default.conf";
//         if (ac == 2)
//             cfg = av[1];
//         else if (ac != 1)
//             throw std::runtime_error("Usage: ./webserv [config_path]");

//         Server s(0);  // port 由 config 决定
//         s.load_config(cfg);
//         if (!s.init_sockets())
//             throw std::runtime_error("init_sockets() failed");
//         std::cout << "Init success, entering event loop" << std::endl;
//         s.run();
//         return 0;
//     }
//     catch (const std::exception &e)
//     {
//         std::cerr << "Failed: " << e.what() << std::endl;
//         return 1;
//     }
// }
#include <iostream>
#include <string>
#include <stdexcept>
#include "Event/hpp/new_Server.hpp"
#include "Event/hpp/EpollManager.hpp"
#include "Event/hpp/Client.hpp"
#include <cstdlib>

int main(int ac, char **av)
{
    try
    {
        std::string cfg = "config_defaut/default.conf";
        if (ac == 2)
            cfg = av[1];
        else if (ac != 1)
            throw std::runtime_error("Usage: ./webserv [config_path]");

        Server s(0);
        s.load_config(cfg);
        s.printStartupInfo(cfg);

        if (!s.init_sockets())
            throw std::runtime_error("init_sockets() failed");

        std::cout << "[OK] sockets initialized" << std::endl;
        std::cout << "[OK] entering event loop" << std::endl;
        std::cout << "========================================" << std::endl;

        s.run();
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << "[FAILED] " << e.what() << std::endl;
        return 1;
    }
}