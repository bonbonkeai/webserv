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

}

bool	HTTPRequestParser::parseRequestLine()
{

}

bool	HTTPRequestParser::parseHeaders()
{

}

bool	HTTPRequestParser::parseBody()
{

}