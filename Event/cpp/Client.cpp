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
    last_activity_ms = now_ms();
    if (_cgi)
    {
        _cgi->reset();
        delete _cgi;
        _cgi = NULL;
    }
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
    _cgi_manager.clear();
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

void ClientManager::del_cgi_fd(int pipe_fd)
{
    std::map<int, Client *>::iterator it = _cgi_manager.find(pipe_fd);
    if (it == _cgi_manager.end())
        return;
    Client *c = it->second;
    _cgi_manager.erase(it);
    if (!c || !c->_cgi)
    {
        close(pipe_fd); // 防御：至少关掉传进来的 fd
        return;
    }
    CGI_Process *cgi = c->_cgi;
    // 1) 关闭 CGI 的 pipe（只在这里做一次）
    // pipe_fd 应当等于 cgi->_read_fd，但防御一下
    if (cgi->_read_fd >= 0)
    {
        close(cgi->_read_fd);
        cgi->_read_fd = -1;
    }
    else
    {
        // 如果read_fd已经被关过，确保pipe_fd也不泄漏
        close(pipe_fd);
    }
    if (cgi->_write_fd >= 0)
    {
        close(cgi->_write_fd);
        cgi->_write_fd = -1;
    }
    // 2) 不 kill / 不 waitpid（由timeout或正常finish逻辑负责）
    // 3) 只重置与 CGI 状态相关的标记
    c->is_cgi = false;

    // 可选：如果你们 reset 只清 buffer 不 kill，可以改成 reset_no_kill()
    cgi->reset_no_kill();
}

// void    ClientManager::bind_cgi_fd(int pipe_fd, int client_fd)
// {
//     Client* c = _clients.at(client_fd);
//     if (c)
//     {
//         _cgi_manager[pipe_fd] = c;
//         c->is_cgi = true;
//     }
// }
void ClientManager::bind_cgi_fd(int pipe_fd, int client_fd)
{
    std::map<int, Client *>::iterator it = _clients.find(client_fd);
    if (it == _clients.end())
        return;

    Client *c = it->second;
    if (!c)
        return;

    _cgi_manager[pipe_fd] = c;
    c->is_cgi = true;

    // 最好把 CGIProcess 的 read fd 同步为 pipe_fd，保证“key == read_fd”这条约定成立
    if (c->_cgi)
        c->_cgi->_read_fd = pipe_fd;
}