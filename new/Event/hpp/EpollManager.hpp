#ifndef EPOLLMANAGER_HPP
#define EPOLLMANAGER_HPP

#include <sys/epoll.h>
#include <exception>
#include <iostream>
#include <vector>
#include <unistd.h>

class   Epoller
{
    public:
        Epoller();
        ~Epoller();
        
        //fonction
        bool    init(int max_size);
        bool    add_event(int fd, uint32_t events);
        bool    modif_event(int fd, uint32_t events);
        bool    del_event(int fd);
        int     wait(int timeout);
        int get_event_fd(int i)
        {
            return _events[i].data.fd;
        }
        uint32_t    get_event_type(int i)
        {
            return _events[i].events;
        }
    private:
        int epfd;
        std::vector<epoll_event> _events;
};


#endif
/*封装 poll()
addFD(fd, events)
updateFD(fd, events)
removeFD(fd)
run poll(timeout)*/