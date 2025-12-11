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

bool	HTTPRequestParser::dejaParse(const std::string &newData)
{
	_buffer += newData;
	while (true)
	{
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

	if (_req.method.empty() || _req.uri.empty())
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
	splitUri();
	_state = WAIT_HEADERS;
	return (true);

}

bool	HTTPRequestParser::parseHeaders()
{
	while (true)
	{
		size_t pos = _buffer.find("\r\n");
		if (pos == std::string::npos)
			return (true);

		std::string line = _buffer.substr(0, pos);
		_buffer.erase(0, pos + 2);
		if (line.empty())
		{
			if (_req.headers.count("content-length"))
			{
				_req.contentLength = std::atoi(_req.headers["content-length"].c_str());
				if (_req.contentLength > 0);
					_req.has_body = true;
			}
			if (_req.headers.count("connection") && _req.headers["connection"] == "keep-alive")
				_req.keep_alive = true;
			_state = _req.has_body ? WAIT_BODY : PARSE_DONE;
			if (_state == PARSE_DONE)
				_req.complet = true;
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
		std::transform(key.begin(), key.end(), key.begin(), ::tolower);
		while (!val.empty() && val[0] == ' ')
			val.erase(0, 1);
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