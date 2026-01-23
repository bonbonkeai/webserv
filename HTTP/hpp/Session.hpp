#ifndef SESSION_HPP
#define SESSION_HPP

#include <iostream>
#include <map>
#include <iterator>
#include <cstdlib>
#include <ctime>
#include "HTTPRequest.hpp"
#include "Event/hpp/Client.hpp"

#define SESSION_TIMEOUT 300

struct  Session
{
    std::string _id;
    unsigned long long  last_acces;

    bool    is_expired()
    {
        return (std::time(0) - last_acces) > SESSION_TIMEOUT;
    }

    Session(): _id(""), last_acces(std::time(0))
    {}
    Session(const std::string& id): _id(id), last_acces(std::time(0))
    {}
    void    update_acces()
    {
        last_acces = std::time(0);
    }
};

class   Session_manager
{
    private:
        std::map<std::string, Session>  _cookies;

    public:
        Session_manager();
        ~Session_manager();

        std::string generate_id() const;

        Session*    get_session(const std::string& name, bool &is_new_session);
        void    clean_up();

};
#endif


/**
 * https://www.hackingarticles.in/beginner-guide-understand-cookies-session-management/
 * 
 */