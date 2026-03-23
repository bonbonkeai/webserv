
#include "Event/hpp/Client.hpp"
#include <utility>
#include <iterator>

void Client::reset()
{
    _state = READING;
    read_buffer.clear();
    write_buffer.clear();
    write_pos = 0;
    is_keep_alive = false;
    // parser.reset();
    parser.resetForNextRequest();
    is_cgi = false;
    cgi_request = HTTPRequest();
    last_activity_ms = now_ms();
    // if (_cgi)
    // {
    //     _cgi->terminate();
    //     delete _cgi;
    //     _cgi = NULL;
    // }
    // if (cgi_handler)
    //     cgi_handler = NULL;
    if(_cgi)
        _cgi = NULL;
}

ClientManager::ClientManager()
{
}
ClientManager::~ClientManager()
{
    std::map<int, Client *>::const_iterator it;
    for (it = _clients.begin(); it != _clients.end(); ++it)
    {
        it->second->reset();
        delete it->second;
    }
    _clients.clear();
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


// void    ClientManager::del_cgi_fd(int pipe_fd)
// {
//     std::map<int, Client*>::iterator    it = _cgi_manager.find(pipe_fd);
//     if (it != _cgi_manager.end())
//     {
//         close(pipe_fd);
//         it->second->_cgi->reset();
//         _cgi_manager.erase(it);
//     }
// }


// void    ClientManager::bind_cgi_fd(int pipe_fd, int client_fd)
// {
//     Client* c = _clients.at(client_fd);
//     if (c)
//     {
//         _cgi_manager[pipe_fd] = c;
//         c->is_cgi = true;
//     }
// }


void    ClientManager::clear_all_clients()
{
    for(std::map<int, Client*>::iterator it = _clients.begin(); it != _clients.end(); ++it)
    {
        delete it->second;
    }
    _clients.clear();
}
