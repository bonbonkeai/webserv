#include "HTTP/hpp/Session.hpp"
#include <ctime>
Session_manager::Session_manager()
{
}

Session_manager::~Session_manager()
{
}


std::string Session_manager::generate_id() const
{
    std::string str;
    std::string s = "0123456789ABCDEF";

    for (int i = 0; i < 16; i++)
        str += s[rand() % 16];
    return str;
}
/**
 * get_session:
 *  1. session find and not expired: update acces
 *  2. session find but expired: manager delet this session
 *  3. not find: create a new one 
 */

Session *Session_manager::get_session(const std::string &name, bool &is_new_session)
{
    std::map<std::string, Session>::iterator    it = _cookies.find(name);

    if (it != _cookies.end() && !it->second.is_expired())
    {
        it->second.update_acces();
        return &(it->second);
    }
    if (it != _cookies.end() && it->second.is_expired())
        _cookies.erase(it);
    
    //create a new one ;
    std::string id = generate_id();
    Session s(id);
    _cookies[s._id] = s;
    is_new_session = true;
    return &(_cookies[s._id]);
}

void Session_manager::clean_up()
{
    time_t now = std::time(0);
    for (std::map<std::string, Session>::iterator it = _cookies.begin();
         it != _cookies.end();)
    {
        if ((now - it->second.last_acces) > SESSION_TIMEOUT)
            _cookies.erase(it);
        else
            ++it;
    }
}