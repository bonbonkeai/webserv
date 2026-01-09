#include "../hpp/CGIProcess.hpp"

CGI_Process::CGI_Process() : _pid(-1), _read_fd(-1)
{
}

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
        std::map<std::string, std::string>::const_iterator it = req.headers.find("content-type");
        if (it != req.headers.end())
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

void CGI_Process::reset()
{
    if (_read_fd >= 0)
    {
        close(_read_fd);
        _read_fd = -1;
    }
    if (_write_fd >= 0)
    {
        close(_write_fd);
        _write_fd = -1;
    }
    if (_pid > 0)
    {
        kill(_pid, SIGKILL);
        waitpid(_pid, NULL, WNOHANG);
        _pid = -1;
    }
    _output_buffer.clear();
}
void CGI_Process::set_non_block_fd(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1)
        throw std::runtime_error("fcntl get flags failed");
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
        throw std::runtime_error("fcntl set flags failed");
}
void CGI_Process::append_output(const char *buf, size_t n)
{
    _output_buffer.append(buf, n);
}
bool CGI_Process::execute(const std::string &script_path, HTTPRequest &req)
{
    int pipe_in[2];
    int pipe_out[2];

    if (pipe(pipe_in) < 0 || pipe(pipe_out) < 0)
        return false;
    _pid = fork();
    if (_pid < 0)
    {
        close(pipe_in[0]);
        close(pipe_in[1]);
        close(pipe_out[0]);
        close(pipe_out[1]);
        return false;
    }
    if (_pid == 0)
    {
        dup2(pipe_in[0], STDIN_FILENO);
        dup2(pipe_out[1], STDOUT_FILENO);
        close(pipe_in[1]);
        close(pipe_out[0]);
        // add env

        CGI_ENV envp = get_env_from_request(req);

        char *argv[] = {const_cast<char *>(script_path.c_str()), NULL};
        execve(script_path.c_str(), argv, envp.envp.data());
        exit(1);
    }
    close(pipe_in[0]);
    close(pipe_out[1]);
    _read_fd = pipe_out[0];
    _write_fd = pipe_in[1];
    set_non_block_fd(_read_fd);
    set_non_block_fd(_write_fd);
    if (req.method == "POST" && req.has_body)
    //write the content in the body 
        write(pipe_in[1], req.body.c_str(), req.body.size());
    close(_write_fd);
    _write_fd = -1;
    return true;
}