#include "CGI/hpp/CGIManager.hpp"

#define EXECUTION_TIMEOUT 10000ULL
#define START_TIMEOUT 5000ULL

CGIManager::CGIManager() {}

CGIManager::~CGIManager()
{
    for (std::vector<CGI_Process *>::iterator it = _all_process.begin();
         it != _all_process.end(); ++it)
    {
        CGI_Process *proc = *it;
        if (proc)
        {
            proc->terminate();
            delete proc;
        }
        _all_process.clear();
        _fd_to_process.clear();
    }
}

void CGIManager::add_process(CGI_Process *proc)
{
    if (proc)
    {
        _all_process.push_back(proc);
        if (proc->_read_fd >= 0)
            registe_fd(proc->_read_fd, proc);
        if (proc->_write_fd >= 0)
            registe_fd(proc->_write_fd, proc);
    }
}

void CGIManager::remove_process(CGI_Process *proc)
{
    if (!proc)
        return;
    if (proc->_read_fd >= 0)
        unregiste_fd(proc->_read_fd);
    if (proc->_write_fd >= 0)
        unregiste_fd(proc->_write_fd);

    for (std::vector<CGI_Process *>::iterator it = _all_process.begin(); it != _all_process.end(); ++it)
    {
        if (*it == proc)
        {
            _all_process.erase(it);
            break;
        }
    }
}

CGI_Process *CGIManager::get_process_by_fd(int fd)
{
    std::map<int, CGI_Process *>::const_iterator it = _fd_to_process.find(fd);
    if (it != _fd_to_process.end())
        return it->second;
    return NULL;
}

bool CGIManager::is_cgi_fd(int fd)
{
    return _fd_to_process.find(fd) != _fd_to_process.end();
}
void CGIManager::registe_fd(int fd, CGI_Process *proc)
{
    if (fd >= 0 && proc)
    {
        _fd_to_process[fd] = proc;
    }
}

void CGIManager::unregiste_fd(int fd)
{
    _fd_to_process.erase(fd);
}

bool    CGIManager::is_known(CGI_Process *proc)
{
    for(size_t i = 0; i < _all_process.size(); ++i)
    {
        if (_all_process[i] == proc)
            return true;
    }
    return false;
}

void CGIManager::cleanup_process()
{
    std::vector<CGI_Process *> to_delete;

    for (std::vector<CGI_Process *>::iterator it = _all_process.begin();
         it != _all_process.end(); ++it)
    {
        CGI_Process *proc = *it;
        if (!proc->is_running())
        {
            to_delete.push_back(proc);
        }
    }

    for (size_t i = 0; i < to_delete.size(); ++i)
    {
        CGI_Process *proc = to_delete[i];
        remove_process(proc);
        delete proc;
    }
}

void CGIManager::remove_and_delete(CGI_Process *proc)
{
    if (!proc)
        return;
    remove_process(proc);
    delete proc;
}

void CGIManager::kill_and_remove(CGI_Process *proc)
{
    if (!proc)
        return;
    proc->terminate();
    remove_and_delete(proc);
    delete proc;
}

std::vector<CGI_Process *> &CGIManager::all_processes()
{
    return _all_process;
}