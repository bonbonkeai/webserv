#include <iostream>
#include <string>
#include <stdexcept>
#include "Event/hpp/Server.hpp"
#include "Config/hpp/ConfigTokenizer.hpp"
#include "Config/hpp/ConfigParser.hpp"
#include "Config/hpp/ConfigUtils.hpp"
#include "Config/hpp/EffectiveConfig.hpp"

int main(int ac, char** av)
{
    try
    {
        std::string configPath = "config_defaut/default.conf";
        if (ac == 2)
            configPath = av[1];
        else if (ac != 1)
        {
            std::cerr << "Usage: ./webserv [config_file]" << std::endl;
            return 1;
        }

        std::cout << "Using config file: " << configPath << std::endl;

        ConfigTokenizer tokenizer;
        if (!tokenizer.read_file(configPath))
        {
            std::cerr << "[ERROR] Failed to read config file" << std::endl;
            return 1;
        }

        ConfigParser parser(tokenizer.tokens());
        std::vector<ServerConfig> raw = parser.parse();
        ConfigUtils utils;
        utils.validate(raw);

        std::vector<ServerRuntimeConfig> servers = buildRuntime(raw);
        if (servers.empty())
        {
            std::cerr << "[ERROR] No valid server blocks found." << std::endl;
            return 1;
        }

        Server webserver(servers);
        if (!webserver.init_sockets())
            throw std::runtime_error("init_sockets() failed");

        std::cout << "Initialization complete. Entering event loop..." << std::endl;
        webserver.run();
        std::cout << "Server shutdown." << std::endl;
    }
    catch (const std::exception& e)
    {
        std::cerr << "[FATAL] " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
