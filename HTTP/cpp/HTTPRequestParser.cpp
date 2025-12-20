#include "HTTP/hpp/HTTPRequestParser.hpp"

HTTPRequestParser::HTTPRequestParser() : _state(WAIT_REQUEST_LINE) {}

HTTPRequestParser::HTTPRequestParser(const HTTPRequestParser& copy) : 
					_req(copy._req),
					_state(copy._state),
					_buffer(copy._buffer)
					{}

HTTPRequestParser& HTTPRequestParser::operator=(const HTTPRequestParser& copy)
{
	if (this != &copy)
	{
		_req = copy._req;
		_state = copy._state;
		_buffer = copy._buffer;
	}
	return (*this);
}

HTTPRequestParser::~HTTPRequestParser() {}

const HTTPRequest&	HTTPRequestParser::getRequest() const
{
	return (_req);
}

void HTTPRequestParser::reset()
{
    _req = HTTPRequest();//重置所有字段与 flags
    _state = WAIT_REQUEST_LINE;
    _buffer.clear();
}

bool	HTTPRequestParser::dejaParse(const std::string &newData)
{
	_buffer += newData;
	while (true)
	{
		ParseState before = _state;
		std::size_t bufBefore = _buffer.size();
		if (_state == WAIT_REQUEST_LINE)
		{
			if (!parseRequestLine())
				return (false);
		}
		else if (_state == WAIT_HEADERS)
		{
			if (!parseHeaders())
				return (false);
		}
		else if (_state == WAIT_BODY)
		{
			if (!parseBody())
				return (false);
		}
		else if (_state == PARSE_DONE)
			return (true);
		if (_state == before && _buffer.size() == bufBefore)
            return (true);
	}
}

void	HTTPRequestParser::splitUri()
{
	size_t pos = _req.uri.find("?");
	if (pos == std::string::npos)
	{
		_req.path = _req.uri;
		_req.query = "";
	}
	else
	{
		_req.path = _req.uri.substr(0, pos);
		_req.query = _req.uri.substr(pos + 1);
	}
}

bool	HTTPRequestParser::parseRequestLine()
{
	size_t	pos = _buffer.find("\r\n");
	if (pos == std::string::npos)
		return (true);
	//non complet

	std::string	line = _buffer.substr(0, pos);
	_buffer.erase(0, pos + 2);
	std::istringstream iss(line);
	iss >> _req.method >> _req.uri >> _req.version;

	if (_req.method.empty() || _req.uri.empty() || _req.version.empty())
	{
		_req.bad_request = true;
		_state = PARSE_DONE;
		return (false);
	}

	if (_req.version != "HTTP/1.1")
	{
		_req.bad_request = true;
		_state = PARSE_DONE;
		return (false);
	}
	_req.keep_alive = true;
	splitUri();
	_state = WAIT_HEADERS;
	return (true);

}

static inline void toLowerInPlace(std::string& s)
{
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
}

static inline void ltrimSpaces(std::string& s)
{
    while (!s.empty() && (s[0] == ' ' || s[0] == '\t'))
        s.erase(0, 1);
}

bool	HTTPRequestParser::parseHeaders()
{
	while (true)
	{
		size_t pos = _buffer.find("\r\n");
		if (pos == std::string::npos)
			return (true);//need more data

		std::string line = _buffer.substr(0, pos);
		_buffer.erase(0, pos + 2);

		//header 结束-》空行
		if (line.empty())
		{
			//content-length
			if (_req.headers.count("content-length"))
            {
                const std::string& v = _req.headers["content-length"];
                int n = std::atoi(v.c_str());
                if (n < 0)
                {
                    _req.bad_request = true;
                    _state = PARSE_DONE;
                    return (false);
                }
                _req.contentLength = static_cast<std::size_t>(n);
                if (_req.contentLength > 0)//分号
                    _req.has_body = true;
            }
			// connection: close覆写keep-alive
            if (_req.headers.count("connection"))
            {
                std::string conn = _req.headers["connection"];
                toLowerInPlace(conn);
                if (conn == "close")
                    _req.keep_alive = false;
            }
            _state = _req.has_body ? WAIT_BODY : PARSE_DONE;
            if (_state == PARSE_DONE)
                _req.complet = true;
			return (true);
		}
		//parse headers
		size_t	colon = line.find(":");
		if (colon == std::string::npos)
		{
			_req.bad_request = true;
			_state = PARSE_DONE;
			return (false);
		}
		std::string key = line.substr(0, colon);
		std::string val = line.substr(colon + 1);
		toLowerInPlace(key);
        ltrimSpaces(val);
		_req.headers[key] = val;
	}
}

bool	HTTPRequestParser::parseBody()
{
	if (_buffer.size() < _req.contentLength)
		return (true);
	//body non arrive
	_req.body = _buffer.substr(0, _req.contentLength);
	_buffer.erase(0, _req.contentLength);
	_req.complet = true;
	_state = PARSE_DONE;
	return (true);
}