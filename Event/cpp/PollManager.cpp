#include "Event/hpp/PollManager.hpp"
#include <cstring>

Epoller::Epoller()
{}

Epoller::~Epoller()
{}

bool    Epoller::init(int max_size)
{
    epfd = epoll_create(1);
    if (epfd < 0)
        throw std::runtime_error("Epoll create failed");
    _events.resize(max_size);
    return true;
}

bool    Epoller::add_event(int fd, uint32_t events)
{
    if (fd < 0)
        return false;
    // struct epoll_event ev{};//98不允许
    struct epoll_event ev;
    std::memset(&ev, 0, sizeof(ev));
    ev.data.fd = fd;
    ev.events = events;
    return (epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev) == 0);
}

bool    Epoller::modif_event(int fd, uint32_t events)
{
    if (fd < 0)
        return false;
    //struct epoll_event ev{};//98应该不允许这样写
    struct epoll_event ev;
    std::memset(&ev, 0, sizeof(ev));
    ev.data.fd = fd;
    ev.events = events;
    return (epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &ev) == 0);
}

bool    Epoller::del_event(int fd)
{
    if (fd < 0)
        return false;
    // return (epoll_ctl(epfd, EPOLL_CTL_DEL, fd, nullptr) == 0);//98里没有nullptr
    return (epoll_ctl(epfd, EPOLL_CTL_DEL, fd, 0) == 0);
}

int Epoller::wait(int timeout)
{
    return epoll_wait(epfd, &_events[0], static_cast<int>(_events.size()), timeout);
}