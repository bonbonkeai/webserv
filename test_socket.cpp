#include <iostream>
#include "Event/hpp/Server.hpp"
#include "Event/hpp/EpollManager.hpp"

int main()
{
    Server  s(8080);

    s.init_sockets();
    s.run();
    return 0;
}

/**
 * test:
 * 在一个terminal当中make，运行以后，在另一个terminal里面输入curl -I http://localhost::8080 
 * 会得到do read当中process request的结果，临时test socket没问题 
 * 
 */