// #include <iostream>
// #include <string>
// #include <stdexcept>
// #include "Config/hpp/ConfigParser.hpp"
// #include "Event/hpp/Server.hpp"                        

// int main(int ac, char** av)
// {
//     try
//     {
//         // 1. 确定配置文件路径
//         std::string configPath;
//         if (ac == 1)
//         {
//             // 如果不传参数，使用默认配置文件
//             configPath = "config/default.conf";
//         }
//         else if (ac == 2)
//         {
//             configPath = av[1];
//         }
//         else
//         {
//             std::cerr << " Usage: ./webserv [config_file] " << std::endl;
//             return (1);
//         }
//         std::cout << " Using config file: " << configPath << std::endl;
//         // 2. 解析配置文件（A模块）
//         ConfigParser parser;
//         std::vector<ServerConfig> serverConfigs;
//         try
//         {
//             //调用 ConfigParser 解析配置 → 得到 server
//             serverConfigs = parser.parse(configPath);
//         }
//         catch (std::exception& e)
//         {
//             std::cerr << "[ERROR] Config error: " << e.what() << std::endl;
//             return (1);
//         }
//         if (serverConfigs.empty())
//         {
//             std::cerr << "[ERROR] No valid server blocks found." << std::endl;
//             return (1);
//         }
//         std::cout << " Config loaded. Servers: "
//                   << serverConfigs.size() << std::endl;
//         // 3. 初始化服务器（B模块）实例化 Server（事件循环的总控类）
//         Server webserver(serverConfigs);       
//         std::cout << " Initialization complete. Entering event loop..." << std::endl;
//         // 4. 进入事件循环（poll()）
//         webserver.run();
//         std::cout << " Server shutdown." << std::endl;
//     }
//     catch (std::exception& e)
//     {
//         std::cerr << "[FATAL] " << e.what() << std::endl;
//         return (1);
//     }
//     return (0);
// }


#include <iostream>
#include <string>
#include <stdexcept>
#include "Event/hpp/Server.hpp"  // 服务器主类
#include "Event/hpp/EpollManager.hpp"  // epoll 管理类
#include "Event/hpp/Client.hpp"  // 客户端管理类

class Server;
struct Client;
class HTTPRequest;
static Server *g_server = NULL;

static void signal_handler(int)
{
    std::cout << "\n[Signal] shutdown\n";
    if (g_server)
        g_server->cleanup();
    exit(0);
}

int main(void)
{
    int port = 8080;
   
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGPIPE, SIG_IGN);
    try
    {
        Server server(port);
        g_server = &server;
        server.init_sockets();
        std::cout << "Listening on http://localhost:" << port << std::endl;
        server.run();
    }
    catch (const std::exception &e)
    {
        std::cerr << "Fatal: " << e.what() << std::endl;
        return 1;
    }
}