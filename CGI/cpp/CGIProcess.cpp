#include "../hpp/CGIProcess.hpp"
#include "Event/hpp/Client.hpp"
#include <stdio.h>
#include <limits.h> // 用于 PATH_MAX
#include <stdlib.h> // 用于 realpath
#include <unistd.h> // 用于 access, getcwd
#include <string.h> // 用于 strerror
#define TRACE() std::cout << "[] " << __FILE__ << ":" << __LINE__ << std::endl;
#define EXECUTION_TIMEOUT 10000ULL
#define START_TIMEOUT 5000ULL

CGI_Process::CGI_Process() : _error_message(),
                             _pid(-1), _read_fd(-1), _write_fd(-1), _state(RUNNING),
                             _output_buffer(), start_time_ms(0), last_output_ms(0), _cgi_header_parsed(false),
                             _cgi_content_length(-1), _cgi_body_begin(0)
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

bool CGI_ENV::check_extension(const std::string &str, const std::set<std::string> &cgi_exetension)
{
    std::set<std::string>::const_iterator it = cgi_exetension.find(str);
    return it == cgi_exetension.end();
}

CGI_path CGI_ENV::final_script_paths(const HTTPRequest &req, const EffectiveConfig &config)
{
    const std::string &path = req.path;
    CGI_path out;
    // check path is in location
    if (path.compare(0, config.location_path.size(), config.location_path) != 0)
        throw std::runtime_error("cgi path not under location");

    std::string path_after_location = path.substr(config.location_path.size());
    if (path_after_location.empty() || path_after_location[0] != '/')
    {
        path_after_location = "/" + path_after_location;
    }

    std::string current_path;
    size_t pos = 1;

    while (true)
    {
        size_t flash = path_after_location.find('/', pos + 1);
        std::string str;
        if (flash == std::string::npos)
            str = path_after_location.substr(pos);
        else
            str = path_after_location.substr(pos, flash - pos);
        current_path += str;
        if (check_extension(str, config.cgi_extensions))
        {
            out.script_name = config.location_path + current_path;
            out.path_info = (flash == std::string::npos) ? "" : path_after_location.substr(flash);

            if (!config.alias.empty())
                out.script_filename = config.alias + current_path;
            else
                out.script_filename = config.root + out.script_name;
            return out;
        }
        if (flash == std::string::npos)
            break;
        pos = flash;
    }
    throw std::runtime_error("no cgi script find in path");
}

CGI_ENV CGI_Process::get_env_from_request(HTTPRequest &req, const EffectiveConfig &config)
{
    CGI_ENV env;

    env.env_str.push_back("GATEWAY_INTERFACE=CGI/1.1");
    env.env_str.push_back("SERVER_SOFTWARE=webserv/1.0");
    env.env_str.push_back("REDIRECT_STATUS=200");

    env.env_str.push_back("REQUEST_METHOD=" + req.method);
    env.env_str.push_back("SERVER_PROTOCOL=HTTP/1.1");
    env.env_str.push_back("QUERY_STRING=" + req.query);

    env.env_str.push_back("SCRIPT_NAME=" + req.cgi_script_name);
    env.env_str.push_back("REMOTE_ADDR=127.0.0.1");

    // server name port
    env.env_str.push_back("SERVER_NAME=" + config.server_name);
    env.env_str.push_back("SERVER_PORT=" + toString(config.server_port));

    // request uri
    env.env_str.push_back("REQUEST_URI=" + req.uri);
    env.env_str.push_back("DOCUMENT_URI=" + req.path);

    env.env_str.push_back("DOCUMENT_ROOT=" + config.root);

    if (req.method == "POST" && req.has_body)
    {
        env.env_str.push_back("CONTENT_LENGTH=" + toString(req.contentLength));
        std::map<std::string, std::string>::const_iterator it = req.headers.find("content-type");
        if (it != req.headers.end())
            env.env_str.push_back("CONTENT_TYPE=" + req.headers["content-type"]);
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

    env.env_str.push_back("PATH=/usr/bin:/bin");
    return env;
}

void CGI_Process::reset()
{
    terminate();
    if (_write_fd >= 0)
    {
        close(_write_fd);
        _write_fd = -1;
    }

    _output_buffer.clear();
    _error_message.clear();
    start_time_ms = 0;
    last_output_ms = 0;
    _state = RUNNING;
}

void CGI_Process::append_output(const char *buf, size_t n)
{
    _output_buffer.append(buf, n);
}

bool CGI_Process::create_pipe(int pipe_in[2], int pipe_out[2])
{
    if (pipe(pipe_in) < 0 || pipe(pipe_out) < 0)
    {
        if (pipe_in[0] >= 0)
        {
            close(pipe_in[0]);
            close(pipe_in[1]);
        }
        return false;
    }
    return true;
}
void CGI_Process::close_pipes(int pipe_in[2], int pipe_out[2])
{
    close(pipe_in[0]);
    close(pipe_in[1]);
    close(pipe_out[0]);
    close(pipe_out[1]);
}

bool CGI_Process::execute(const EffectiveConfig &config, HTTPRequest &req)
{
    int pipe_in[2];
    int pipe_out[2];

    try
    {
        CGI_path paths = CGI_ENV::final_script_paths(req, config);
        script_path = paths.script_filename;

        req.cgi_path_info = paths.path_info;
        req.cgi_script_name = paths.script_name;
    }
    catch (const std::runtime_error &e)
    {
        _state = ERROR_CGI;
        _error_message = std::string("CGI path resolution failed: ") + e.what();
        return false;
    }

    if (access(script_path.c_str(), X_OK) != 0)
    {
        _state = ERROR_CGI;
        _error_message = "Script not executable: " + script_path;
        return false;
    }
    if (!create_pipe(pipe_in, pipe_out))
        return false;
    _pid = fork();
    if (_pid < 0)
    {
        close_pipes(pipe_in, pipe_out);
        return false;
    }
    if (_pid == 0)
        setup_child_process(pipe_in, pipe_out, config, req);
    return setup_parent_process(pipe_in, pipe_out, req);
}

bool CGI_Process::setup_child_process(int pipe_in[2], int pipe_out[2], const EffectiveConfig &config,
                                      HTTPRequest &req)
{
    close(pipe_in[1]);
    close(pipe_out[0]);

    if (dup2(pipe_in[0], STDIN_FILENO) < 0 || dup2(pipe_out[1], STDOUT_FILENO) < 0 ||
        dup2(pipe_out[1], STDERR_FILENO) < 0)
        _exit(1);
    close(pipe_in[0]);
    close(pipe_out[1]);

    char *abs_path = realpath(script_path.c_str(), NULL);
    if (!abs_path)
    {
        perror("realpath failed");
        _exit(1);
    }

    CGI_ENV env = get_env_from_request(req, config);
    env.env_str.push_back("SCRIPT_FILENAME=" + std::string(abs_path));
    env.env_str.push_back("PATH_INFO=" + req.cgi_path_info);
    env.env_str.push_back("SCRIPT_NAME=" + req.cgi_script_name);

    std::string translated;
    if (!req.cgi_path_info.empty())
    {
        std::string script_dir = abs_path;
        size_t  last_slash = script_dir.find_last_of('/');
        if (last_slash != std::string::npos)
        {
            script_dir = script_dir.substr(0, last_slash + 1);
        }
        translated = script_dir + req.cgi_path_info.substr(1);
    }
    env.env_str.push_back("PATH_TRANSLATED=" + translated);
    env.final_env();

    char *argv[] = {abs_path, NULL};
    execve(abs_path, argv, env.envp.data());

    perror("execve failed");
    free(abs_path);
    _exit(1);
    return false;
}

bool CGI_Process::setup_parent_process(int pipe_in[2], int pipe_out[2], HTTPRequest &req)
{

    close(pipe_in[0]);
    close(pipe_out[1]);

    _read_fd = pipe_out[0];
    _write_fd = pipe_in[1];
    if (!set_non_block_fd(_read_fd))
    {
        terminate();
        _state = ERROR_CGI;
        _error_message = "Failed to se non block";
        return false;
    }
    _state = RUNNING;
    start_time_ms = Client::now_ms();

    //
    if (req.method == "POST" && req.has_body)
    {
        if (!write_body(req.body))
        {
            terminate();
            _state = ERROR_CGI;
            _error_message = "failed to write POST body";
            close_pipes(pipe_in, pipe_out);
            return false;
        }
    }

    // stdin：不要 non-block，确保 body 写满，否则 CGI 可能一直等
    close(_write_fd);
    _write_fd = -1;
    return true;
}

void CGI_Process::reset_no_kill()
{
    // 清空 CGI 输出缓冲、状态机字段
    _output_buffer.clear();
    // 任何与 pid/fd 有关的都不要动
    start_time_ms = 0;
    last_output_ms = 0;
}

bool CGI_Process::set_non_block_fd(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1)
        return false;
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
        return false;
    return true;
}

bool CGI_Process::write_body(const std::string &body)
{
    if (body.empty())
        return true;

    size_t total = body.size();
    size_t written = 0;

    while (written < total)
    {
        ssize_t n = write(_write_fd, body.c_str() + written, total - written);
        if (n > 0)
            written += n;
        else
            return false;
    }
    return true;
}

void CGI_Process::terminate()
{
    if (_pid > 0)
    {
        kill(_pid, SIGKILL);
        waitpid(_pid, NULL, 0);
        _pid = -1;
    }
    if (_read_fd >= 0)
    {
        close(_read_fd);
        _read_fd = -1;
    }
}

bool CGI_Process::check_timeout(unsigned long long now)
{
    unsigned long long diff = now - start_time_ms;
    if (_output_buffer.empty())
    {
        if (diff > START_TIMEOUT)
            return true;
        return false;
    }

    // partiel pipe
    if (last_output_ms > 0)
    {
        unsigned long long diff_time = now - last_output_ms;
        if (diff_time > EXECUTION_TIMEOUT)
            return true;
    }

    // total execution timeout
    if (diff > EXECUTION_TIMEOUT * 2)
        return true;
    return false;
}

bool CGI_Process::handle_output()
{
    if (_read_fd < 0)
        return true;
    if (_state == TIMEOUT || _state == ERROR_CGI)
        return true;
    char buf[4096];

    while (true)
    {
        ssize_t n = read(_read_fd, buf, sizeof(buf));
        if (n > 0)
        {
            append_output(buf, n);
            last_output_ms = Client::now_ms();
            if (!_cgi_header_parsed)
            {
                size_t pos = _output_buffer.find("\r\n\r\n");
                if (pos != std::string::npos)
                {
                    _cgi_header_parsed = true;
                    _cgi_body_begin += 4;

                    std::string header = _output_buffer.substr(0, pos);
                    std::istringstream  iss(header);
                    std::string line;
                    while (std::getline(iss, line))
                    {
                        if (!line.empty() && line[line.size() - 1] == '\r')
                            line.erase(line.size() - 1, 1);
                        size_t  colon = line.find(':');
                        if (colon == std::string::npos)
                            continue;
                        std::string key = line.substr(0, colon);
                        std::string value = line.substr(colon + 1);
                        toLowerInPlace(key);
                        ltrimSpaces(value);
                        rtrimSpaces(value);
                        if (key == "content-length")
                            _cgi_content_length = std::atoi(value.c_str());
                    }
                }
            }
            if (_cgi_header_parsed)
            {
                if (_cgi_content_length == -1)
                    return false;
                size_t body_len = _output_buffer.size() - _cgi_content_length;
                if (body_len >= (size_t)_cgi_content_length)
                {
                    get_exit_status();
                    close(_read_fd);
                    _read_fd = -1;
                    _state = FINISHED;
                    return true;
                }
            }
            
            continue;
        }
        else if (n == 0)
        {
            get_exit_status();
            _state = FINISHED;
            return true;
        }
        return false;
    }
}

void CGI_Process::get_exit_status()
{
    if (_pid > 0)
    {
        int status;
        if (waitpid(_pid, &status, WNOHANG) > 0)
        {
            _pid = -1;
            _state = (WIFEXITED(status) && WEXITSTATUS(status) == 0) ? FINISHED : ERROR_CGI;
        }
    }
}

void CGI_Process::handle_timeout()
{
    terminate();
    _state = TIMEOUT;
    _error_message = "CGI execution timeout";
}

void CGI_Process::handle_pipe_error()
{
    terminate();
    _state = ERROR_CGI;
    _error_message = "CGI pipe error";
}

HTTPResponse CGI_Process::build_response(bool keep_alive)
{
    HTTPResponse resp;

    switch (_state)
    {
    case FINISHED:
        if (_output_buffer.empty())
        {
            resp = buildErrorResponse(500);
        }
        else
        {
            resp = resp.buildResponseFromCGIOutput(_output_buffer, keep_alive);
        }
        break;

    case ERROR_CGI:
        resp = buildErrorResponse(500);
        resp.headers["content-type"] = "text/plain";
        resp.body = "CGI Error: " + _error_message;
        break;

    case TIMEOUT:
        resp = buildErrorResponse(504);
        break;

    default:
        // 如果有输出，尝试使用它
        if (!_output_buffer.empty())
        {
            resp = resp.buildResponseFromCGIOutput(_output_buffer, keep_alive);
        }
        else
        {
            resp = buildErrorResponse(500);
        }
        break;
    }

    return resp;
}