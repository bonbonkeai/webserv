// #include "Event/hpp/Client.hpp"
// #include <utility>

// ClientManager::ClientManager()
// {
// }
// ClientManager::~ClientManager()
// {
// }

// void ClientManager::add_socket_client(int fd)
// {
//     // _clients.emplace(fd, Client(fd));//98不能用这个吧
//     _clients.insert(std::make_pair(fd, Client(fd)));
// }

// Client &ClientManager::get_client(int fd)
// {
//     std::map<int, Client>::iterator it = _clients.find(fd);
//     if (it != _clients.end())
//         return it->second;
//     throw std::runtime_error("Client not found");
// }

// void ClientManager::remove_socket_client(int fd)
// {
//     if (_clients.find(fd) != _clients.end())
//         _clients.erase(fd);
// }

#include "Event/hpp/Client.hpp"
#include <utility>
#include <iterator>

ClientManager::ClientManager()
{
}
ClientManager::~ClientManager()
{
    std::map<int, Client *>::const_iterator it;
    for (it = _clients.begin(); it != _clients.end(); ++it)
        delete it->second;
}

void ClientManager::add_socket_client(int fd)
{
    if (_clients.find(fd) == _clients.end())
        _clients[fd] = new Client(fd);
}

Client *ClientManager::get_socket_client_by_fd(int fd)
{
    std::map<int, Client *>::iterator it = _clients.find(fd);
    if (it != _clients.end())
        return it->second;
    return NULL;
}

void ClientManager::remove_socket_client(int fd)
{
    std::map<int, Client *>::iterator it = _clients.find(fd);
    if (it != _clients.end())
    {
        delete it->second;
        _clients.erase(it);
    }
}

Client *ClientManager::get_client_by_cgi_fd(int pipe_fd)
{
    std::map<int, Client *>::const_iterator it = _cgi_manager.find(pipe_fd);
    if (it != _cgi_manager.end())
        return it->second;
    return NULL;
}

bool ClientManager::is_cgi_pipe(int pipe_fd)
{
    return _cgi_manager.find(pipe_fd) != _cgi_manager.end();
}

void ClientManager::del_cgi_fd(int pipe_fd)
{
    std::map<int, Client *>::iterator it = _cgi_manager.find(pipe_fd);
    if (it != _cgi_manager.end())
    {
        close(pipe_fd);
        it->second->_cgi->reset();
        _cgi_manager.erase(it);
    }
}

void ClientManager::bind_cgi_fd(int pipe_fd, int client_fd)
{
    Client *c = _clients.at(client_fd);
    if (c)
    {
        _cgi_manager[pipe_fd] = c;
        c->is_cgi = true;
    }
}

void Client::reset()
{
    _state = READING;
    read_buffer.clear();
    write_buffer.clear();
    write_pos = 0;
    is_keep_alive = false;
    parser.reset();
    delete _cgi;
    _cgi = NULL;
    is_cgi = false;
    last_active = std::time(0);
}