#include "CGIProcess.hpp"

CGI_Process::CGI_Process() : _pid(-1), _read_fd(-1)
{}

CGI_Process::~CGI_Process()
{
    reset();
}

std::string CGI_Process::format_header_key(const std::string &key)
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

CGI_ENV CGI_Process::get_env_from_request(HTTPRequest &req)
{
    CGI_ENV env;

    env.env_str.push_back("GATEWAY_INTERFACE=CGI/1.1");
    env.env_str.push_back("REQUEST_METHOD=" + req.method);
    env.env_str.push_back("SERVER_PROTOCOL=HTTP/1.1");

    env.env_str.push_back("QUERY_STRING=" + req.query);

    env.env_str.push_back("SCRIPT_NAME=" +);
    env.env_str.push_back("SERVER_NAME=" +);
    env.env_str.push_back("SERVER_PORT=" +);
    env.env_str.push_back("SERVER_SOFTWARE" +);
    env.env_str.push_back("REMOTE_ADDR=" +);

    if (req.method == "POST")
    {
        env.env_str.push_back("CONTENT_LENGTH=" + req.contentLength);
        env.env_str.push_back("CONTENT_TYPE=" + req.headers["content-type"]);
    }

    for (std::map<std::string, std::string>::const_iterator it = req.headers.begin(); it != req.headers.end(); it++)
    {
        if (it->first == "content-type" || it->first == "content-length")
            continue;
        env.env_str.push_back(format_header_key(it->first) + "=" + it->second);
    }
    env.final_env();
    return env;
}

bool CGI_Process::execute(const std::string &script_path, HTTPRequest &req)
{
    int _pipe_in[2];
    int _pipe_out[2];

    if (pipe(_pipe_in) < 0 || pipe(_pipe_out) < 0)
        return false;
    _pid = fork();
    if (_pid < 0)
    {
        close(_pipe_in[0]);
        close(_pipe_in[1]);
        close(_pipe_out[0]);
        close(_pipe_out[1]);
        return false;
    }
    if (_pid == 0)
    {
        dup2(_pipe_in[0], STDIN_FILENO);
        dup2(_pipe_out[1], STDOUT_FILENO);
        close(_pipe_in[1]);
        close(_pipe_out[0]);
        // add env

        CGI_ENV envp = get_env_from_request(req);

        char *argv[] = {const_cast<char *>(script_path.c_str()), NULL};
        execve(script_path.c_str(), argv, envp.envp.data());
        exit(1);
    }
    close(_pipe_in[0]);
    close(_pipe_out[1]);
    if (req.method == "POST")
        write(_pipe_in[1], req.body.c_str(), req.body.size());
    close(_pipe_in[1]);
    _read_pid = _pipe_out[0];
    return true;
}