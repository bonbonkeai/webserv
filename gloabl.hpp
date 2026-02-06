#ifndef GLOBAL_HPP
#define GLOBAL_HPP

#include <signal.h>


class Server;

extern Server*  g_server;
extern volatile sig_atomic_t    g_running;

#endif