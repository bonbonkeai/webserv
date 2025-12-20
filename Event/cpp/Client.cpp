#include "Event/hpp/Client.hpp"
#include <utility>

ClientManager::ClientManager()
{
}
ClientManager::~ClientManager()
{
}

void ClientManager::add_client(int fd)
{
    // _clients.emplace(fd, Client(fd));//98不能用这个吧
    _clients.insert(std::make_pair(fd, Client(fd)));
}

Client &ClientManager::get_client(int fd)
{
    std::map<int, Client>::iterator it = _clients.find(fd);
    if (it != _clients.end())
        return it->second;
    throw std::runtime_error("Client not found");
}

void ClientManager::remove_client(int fd)
{
    if (_clients.find(fd) != _clients.end())
        _clients.erase(fd);
}