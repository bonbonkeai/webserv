#include "Event/hpp/Client.hpp"
#include <utility>
#include <iterator>

ClientManager::ClientManager()
{
}
ClientManager::~ClientManager()
{
    std::map<int, Client*>::const_iterator  it;
    for(it = _clients.begin(); it != _clients.end(); ++it)
        delete it->second;
}

void ClientManager::add_client(int fd)
{
    if (_clients.find(fd) == _clients.end())
        _clients[fd] = new Client(fd);
}

Client* ClientManager::get_client(int fd)
{
    std::map<int, Client*>::iterator it = _clients.find(fd);
    if (it != _clients.end())
        return it->second;
    return  NULL;
}

void ClientManager::remove_client(int fd)
{
    std::map<int, Client*>::iterator    it = _clients.find(fd);
    if (it != _clients.end())
    {
        delete it->second;
        _clients.erase(it);
    }
}

Client* ClientManager::get_client_by_cgi_fd(int pipe_fd)
{
    std::map<int, Client*>::const_iterator  it = _cgi_manager.find(pipe_fd);
    if (it != _cgi_manager.end())
        return it->second;
    return  NULL;
}

bool    ClientManager::is_cgi_pipe(int pipe_fd)
{
    return  _cgi_manager.find(pipe_fd) != _cgi_manager.end();
}

void    ClientManager::del_cgi_fd(int pipe_fd)
{
    std::map<int, Client*>::iterator    it = _cgi_manager.find(pipe_fd);
    if (it != _cgi_manager.end())
    {
        close(pipe_fd);
        it->second->_cgi.reset();
        _cgi_manager.erase(it);
    }
}

void    ClientManager::bind_cgi_fd(int pipe_fd, int client_fd)
{
    Client* c = _clients.at(client_fd);
    if (c)
    {
        _cgi_manager[pipe_fd] = c;
        c->is_cgi = true;
    }
}