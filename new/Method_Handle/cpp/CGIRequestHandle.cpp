#include "Method_Handle/hpp/CGIRequestHandle.hpp"

CGIRequestHandle::CGIRequestHandle(const HTTPRequest &req, EffectiveConfig& cfg) : _process(NULL), _req(req),
                                                                                _config(cfg),
                                                            completed(false), succes(false)
{
}

CGIRequestHandle::~CGIRequestHandle() {}
/*==========================================*/
/* Prepare env elements for child           */
/*==========================================*/

std::string CGI_ENV::format_header_key(const std::string &key)
{
    std::string resultat = "HTTP_";
    for (size_t i = 0; i < key.size(); i++)
    {
        if (key[i] == '-')
            resultat += '_';
        else
            resultat += (char)std::toupper(key[i]);
    }
    return resultat;
}
CGI_ENV CGI_ENV::get_env_from_request(const HTTPRequest &req, const EffectiveConfig &config)
{
    CGI_ENV env;
    // basique cgi env
    env.env_str.push_back("GATEWAY_INTERFACE=CGI/1.1");
    env.env_str.push_back("SERVER_SOFTWARE=webserv/1.0");
    env.env_str.push_back("REDIRECT_STATUS=200");

    env.env_str.push_back("REQUEST_METHOD=" + req.method);
    env.env_str.push_back("SERVER_PROTOCOL=HTTP/1.1");
    env.env_str.push_back("QUERY_STRING=" + req.query);
    env.env_str.push_back("REMOTE_ADDR=127.0.0.1");

    // server name port
    env.env_str.push_back("SERVER_NAME=" + config.server_name);
    env.env_str.push_back("SERVER_PORT=" + toString(config.server_port));

    // request uri
    env.env_str.push_back("REQUEST_URI=" + req.uri);
    env.env_str.push_back("DOCUMENT_URI=" + req.path);
    env.env_str.push_back("DOCUMENT_ROOT=" + config.root);
    // path
    env.env_str.push_back("PATH=/usr/bin:/bin");
    // script
    env.env_str.push_back("SCRIPT_NAME=" + req._rout.script_name);
    env.env_str.push_back("PATH_INFO=" + req._rout.path_info);
    env.env_str.push_back("SCRIPT_FILENAME=" + req._rout.fs_path);

    if (req.method == "POST" && req.has_body)
    {
        env.env_str.push_back("CONTENT_LENGTH=" + toString(req.contentLength));
        std::map<std::string, std::string>::const_iterator it = req.headers.find("content-type");
        if (it != req.headers.end())
            env.env_str.push_back("CONTENT_TYPE=" + req.headers.at("content-type"));
        else
            env.env_str.push_back("CONTENT_TYPE=");
    }
    else
    {
        env.env_str.push_back("CONTENT_TYPE=");
        env.env_str.push_back("CONTENT_LENGTH=");
    }

    for (std::map<std::string, std::string>::const_iterator it = req.headers.begin(); it != req.headers.end(); it++)
    {
        if (it->first == "content-type" || it->first == "content-length")
            continue;
        env.env_str.push_back(format_header_key(it->first) + "=" + it->second);
    }
    // translated
    std::string translated;
    if (!req._rout.path_info.empty())
    {
        std::string script_dir = req._rout.fs_path;
        size_t last_slash = script_dir.find_last_of('/');
        if (last_slash != std::string::npos)
        {
            script_dir = script_dir.substr(0, last_slash + 1);
        }
        translated = script_dir + req._rout.path_info.substr(1);
    }
    env.env_str.push_back("PATH_TRANSLATED=" + translated);
    env.final_env();

    return env;
}

bool    CGIRequestHandle::start(CGIManager& manager, const EffectiveConfig &config, 
    const HTTPRequest &req, Client* c)
{
    _process = new CGI_Process();
    _manager = &manager;

    if (!_process->execute(config, req, c))
    {
        delete _process;
        _process = NULL;
        completed = true;
        succes = false;
        return false;
    }

    manager.add_process(_process);
    return true;
}

/*uint32_t    CGIRequestHandle::get_events() const
{
    if(!_process || !_process->is_running())
        return 0;
    uint32_t    events = 0;
    if (_process->_read_fd >= 0)
        events |= EPOLLIN;
    if (_process->_write_fd >= 0 && _process->write_pos < _req.body.size())
        events |= EPOLLOUT;
    return events;
}*/

bool    CGIRequestHandle::handle_read()
{
    if (!_process || !_process->is_running())
        return false;
    
    std::string new_data;
    bool    has_data = _process->read_output(new_data);

    if (has_data)
        _output_buffer += new_data;
    if (!_process->is_running())
    {
        completed = true;
        succes = (_process->is_finished() && !_output_buffer.empty());
        return false;
    }
    return true;
}

bool    CGIRequestHandle::handle_write()
{
    if (!_process || !_process->is_running())
        return false;
    
    bool    write_done = _process->write_body(_req.body);

    if (!_process->is_running())
    {
        completed = true;
        succes = false;
        return false;
    }
    return !write_done;
}

HTTPResponse CGIRequestHandle::get_response() const
{
    if (succes)
        return HTTPResponse().buildResponseFromCGIOutput(_output_buffer, true);
    else if (_process && _process->is_timeout())
        return buildErrorResponse(504);
    else
        return buildErrorResponse(500);
}
int CGIRequestHandle::get_read_fd() const 
{
    return _process ? _process->_read_fd : -1;
}
int CGIRequestHandle::get_write_fd() const 
{
    return _process ? _process->_write_fd : -1;
}