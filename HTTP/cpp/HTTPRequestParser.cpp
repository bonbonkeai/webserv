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
	const std::size_t MAX_HEADER_SIZE = 8192; // 后续接 config
	_buffer += newData;
	if (_state == WAIT_REQUEST_LINE || _state == WAIT_HEADERS)
    {
        //header还没结束才限制，避免把body算进去
        if (_buffer.find("\r\n\r\n") == std::string::npos && _buffer.size() > MAX_HEADER_SIZE)
            return (fail(400));
    }
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

bool HTTPRequestParser::isValidHeaderName(const std::string& key)
{
    if (key.empty())
        return (false);
    for (std::size_t i = 0; i < key.size(); ++i)
    {
        unsigned char c = static_cast<unsigned char>(key[i]);
        if (c <= 32 || c == 127)
            return (false);
        if (key[i] == ':')
            return (false);
    }
    return true;
}

//严格解析Content-Length: 纯数字、无符号、无溢出
//max_body可先传一个很大的值，之后用config的client_max_body_size
bool HTTPRequestParser::parseContentLengthStrict(const std::string& v, std::size_t& out, std::size_t max_body)
{
    if (v.empty())
        return (false);
    std::size_t acc = 0;
    for (std::size_t i = 0; i < v.size(); ++i)
    {
        char c = v[i];
        if (c < '0' || c > '9')
            return (false);
        std::size_t digit = static_cast<std::size_t>(c - '0');

        //acc = acc * 10 + digit;之前做溢出保护
        if (acc > (static_cast<std::size_t>(-1) - digit) / 10)
            return (false);
        acc = acc * 10 + digit;

        //做上限拦截（防止读超大 body）
        if (acc > max_body)
            return (false); //之后返回特殊状态改成413
    }
    out = acc;
    return (true);
}

bool HTTPRequestParser::fail(int code)
{
	//暂时都用400
    _req.bad_request = true;
    if (code > 0)
		_req.error_code = code;
	else
		_req.error_code = 400;
    _req.complet = true;
    _state = PARSE_DONE;
    return (false);
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

	if (!isTokenUpperAlpha(_req.method))
		return (fail(400));
	if (_req.method.empty() || _req.uri.empty() || _req.version.empty())
		return (fail(400));
	if (_req.version != "HTTP/1.1")
		return (fail(505));
	_req.keep_alive = true;
	splitUri();
	_state = WAIT_HEADERS;
	return (true);

}

// bool	HTTPRequestParser::parseRequestLine()
// {
// 	size_t	pos = _buffer.find("\r\n");
// 	if (pos == std::string::npos)
// 		return (true);
// 	//non complet

// 	std::string	line = _buffer.substr(0, pos);
// 	_buffer.erase(0, pos + 2);
// 	std::istringstream iss(line);
// 	iss >> _req.method >> _req.uri >> _req.version;

// 	if (!isTokenUpperAlpha(_req.method))
//     {
//         _req.bad_request = true;
//         _req.error_code = 400;
//         _state = PARSE_DONE;
//         return (false);
//     }
// 	if (_req.method.empty() || _req.uri.empty() || _req.version.empty())
// 	{
// 		_req.bad_request = true;
// 		_req.bad_request = 400;
// 		_state = PARSE_DONE;
// 		return (false);
// 	}
// 	if (_req.version != "HTTP/1.1")
// 	{
// 		_req.bad_request = true;
// 		_req.error_code = 400;
// 		_state = PARSE_DONE;
// 		return (false);
// 	}
// 	_req.keep_alive = true;
// 	splitUri();
// 	_state = WAIT_HEADERS;
// 	return (true);

// }

bool HTTPRequestParser::parseHeaders()
{
    //先临时给一个上限，后续接入 config 的 client_max_body_size
    const std::size_t MAX_BODY = 1024 * 1024 * 10;
    while (true)
    {
        std::size_t pos = _buffer.find("\r\n");
        if (pos == std::string::npos)
            return (true);
		//need more data

        std::string line = _buffer.substr(0, pos);
        _buffer.erase(0, pos + 2);

        //header结束：空行
        if (line.empty())
        {
            //1)HTTP/1.1 必须有 Host
            if (_req.version == "HTTP/1.1")
            {
				if (_req.headers.find("host") == _req.headers.end())
					return (fail(400));
            }
            //2)chunked 与 content-length 不能同时存在->目前先不处理chunked
            // if (_req.chunked && _req.has_content_length)
            //     return (fail(400));
            //3)如果有 content-length（且没 chunked），决定 body
			// if (_req.has_content_length && !_req.chunked && _req.contentLength > 0)
			if (_req.has_content_length && _req.contentLength > 0)
				_req.has_body = true;
            // connection: close 覆写 keep-alive
            if (_req.headers.count("connection"))
            {
                std::string conn = _req.headers["connection"];
                toLowerInPlace(conn);
                ltrimSpaces(conn);
                rtrimSpaces(conn);
                if (conn == "close")
                    _req.keep_alive = false;
            }
            _state = _req.has_body ? WAIT_BODY : PARSE_DONE;
            if (_state == PARSE_DONE)
                _req.complet = true;
            return (true);
        }
        //parse header line
        std::size_t colon = line.find(':');
        if (colon == std::string::npos)
            return (fail(400));
        std::string key = line.substr(0, colon);
        std::string val = line.substr(colon + 1);
        //key / val trim
        rtrimSpaces(key);
        toLowerInPlace(key);
        ltrimSpaces(val);
        rtrimSpaces(val);
        //4)header name 合法性
        if (!isValidHeaderName(key))
            return (fail(400));
        //5)Host 唯一性（重复 host -> 400）
        if (key == "host")
        {
            if (_req.headers.count("host"))
                return (fail(400));
        }
        //6)Content-Length 严格校验（纯数字、无溢出、唯一性）
        if (key == "content-length")
        {
            if (_req.has_content_length)
                return (fail(400));
            std::size_t n = 0;
            if (!parseContentLengthStrict(val, n, MAX_BODY))
                return (fail(400));
			// MAX_BODY 返回 413, 先全部设置400；等接入 config 再细分
            _req.contentLength = n;
            _req.has_content_length = true;
            _req.headers[key] = val;
            continue;
        }
        //7)Transfer-Encoding 只支持 chunked，且唯一性
        if (key == "transfer-encoding")
			return (fail(400)); //暂不支持 chunked
        //默认：保存 header
        _req.headers[key] = val;
    }
	return (true);
}

// bool	HTTPRequestParser::parseHeaders()
// {
// 	while (true)
// 	{
// 		size_t pos = _buffer.find("\r\n");
// 		if (pos == std::string::npos)
// 			return (true);//need more data

// 		std::string line = _buffer.substr(0, pos);
// 		_buffer.erase(0, pos + 2);

// 		//header 结束-》空行
// 		if (line.empty())
// 		{
// 			if (_req.version == "HTTP/1.1")
// 			{
// 				if (_req.headers.find("host") == _req.headers.end())
// 				{
// 					_req.bad_request = true;
// 					_req.error_code = 400;
// 					_req.complet = true;
// 					_state = PARSE_DONE;
// 					return (false);
// 				}
// 			}
// 			//content-length
// 			if (_req.headers.count("content-length"))
//             {
//                 const std::string& v = _req.headers["content-length"];
//                 int n = std::atoi(v.c_str());
//                 if (n < 0)
//                 {
//                     _req.bad_request = true;
//                     _state = PARSE_DONE;
//                     return (false);
//                 }
//                 _req.contentLength = static_cast<std::size_t>(n);
//                 if (_req.contentLength > 0)//分号
//                     _req.has_body = true;
//             }
// 			// connection: close覆写keep-alive
//             if (_req.headers.count("connection"))
//             {
//                 std::string conn = _req.headers["connection"];
//                 toLowerInPlace(conn);
//                 if (conn == "close")
//                     _req.keep_alive = false;
//             }
//             _state = _req.has_body ? WAIT_BODY : PARSE_DONE;
//             if (_state == PARSE_DONE)
//                 _req.complet = true;
// 			return (true);
// 		}
// 		//parse headers
// 		size_t	colon = line.find(":");
// 		if (colon == std::string::npos)
// 		{
// 			_req.bad_request = true;
// 			_state = PARSE_DONE;
// 			return (false);
// 		}
// 		std::string key = line.substr(0, colon);
// 		std::string val = line.substr(colon + 1);
// 		toLowerInPlace(key);
//         ltrimSpaces(val);
// 		_req.headers[key] = val;
// 	}
// }

bool HTTPRequestParser::parseBody()
{
    //WAIT_BODY 必须有 Content-Length
    if (!_req.has_content_length)
        return fail(400);
    //等待足够的 body 数据
    if (_buffer.size() < _req.contentLength)
        return (true);
    //精确读取 body（可能为 0，一般不会在 0 时进入 WAIT_BODY）
    if (_req.contentLength > 0)
        _req.body = _buffer.substr(0, _req.contentLength);
    else
        _req.body.clear();
    _buffer.erase(0, _req.contentLength);
    //完成
    _req.complet = true;
    _state = PARSE_DONE;
    return (true);
}

// bool	HTTPRequestParser::parseBody()
// {
// 	if (!_req.has_content_length)
// 		return (fail(400));
// 	if (_buffer.size() < _req.contentLength)
// 		return (true);
// 	//body non arrive
// 	_req.body = _buffer.substr(0, _req.contentLength);
// 	_buffer.erase(0, _req.contentLength);
// 	_req.complet = true;
// 	_state = PARSE_DONE;
// 	return (true);
// }