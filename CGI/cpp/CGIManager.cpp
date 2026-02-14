#include "CGI/hpp/CGIManager.hpp"

#define EXECUTION_TIMEOUT 10000ULL
#define START_TIMEOUT 5000ULL

CGIManager::CGIManager() {}

CGIManager::~CGIManager()
{
    for (std::vector<CGI_Process*>::iterator it = _all_process.begin();
            it != _all_process.end(); ++it )
    {
        CGI_Process*  proc = *it;
        delete proc;
    }
    _all_process.clear();
    _fd_to_process.clear();
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

CGI_Process*    CGIManager::get_process_by_fd(int fd)
{
    std::map<int, CGI_Process*>::const_iterator it = _fd_to_process.find(fd);
    if (it != _fd_to_process.end())
        return it->second;
    return NULL;
}

bool    CGIManager::is_cgi_fd(int fd)
{
    return _fd_to_process.find(fd) != _fd_to_process.end();
}
void CGIManager::registe_fd(int fd, CGI_Process* proc)
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

/*void CGIManager::find_all_cgi_timeout(unsigned long long now)
{    
    _timeout_cgi.clear();
    for (std::vector<CGI_Process*>::iterator it = _all_process.begin();
            it != _all_process.end(); ++it) 
    {
        CGI_Process* proc = *it;
        if (proc->is_running() && proc->check_timeout(now))
        {
            proc->_state = CGI_Process::TIMEOUT;
            _timeout_cgi.push_back(proc);     
        }
    }
}*/

void CGIManager::cleanup_process()
{
    std::vector<CGI_Process*> to_delete;
    
    for (std::vector<CGI_Process*>::iterator it = _all_process.begin();
            it != _all_process.end(); ++it) 
    {
        CGI_Process *proc = *it;
        if (!proc->is_running()) {
            to_delete.push_back(proc);
        }
    }
    
    for(size_t i = 0; i < to_delete.size(); ++i)
    {
        CGI_Process *proc = to_delete[i];
        remove_process(proc);
        delete proc;
    }
}

