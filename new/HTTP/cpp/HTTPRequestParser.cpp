#include "HTTP/hpp/HTTPRequestParser.hpp"

HTTPRequestParser::HTTPRequestParser() : _state(WAIT_REQUEST_LINE),
                                         _chunk_waiting_size(true),
                                         _chunk_expected_size(0)
{
}

HTTPRequestParser::HTTPRequestParser(const HTTPRequestParser &copy) : _req(copy._req),
                                                                      _state(copy._state),
                                                                      _buffer(copy._buffer),
                                                                      _chunk_waiting_size(copy._chunk_waiting_size),
                                                                      _chunk_expected_size(copy._chunk_expected_size)
{
}

HTTPRequestParser &HTTPRequestParser::operator=(const HTTPRequestParser &copy)
{
    if (this != &copy)
    {
        _req = copy._req;
        _state = copy._state;
        _buffer = copy._buffer;
        _chunk_expected_size = copy._chunk_expected_size;
        _chunk_waiting_size = copy._chunk_waiting_size;
    }
    return (*this);
}

HTTPRequestParser::~HTTPRequestParser() {}

const HTTPRequest &HTTPRequestParser::getRequest() const
{
    return (_req);
}

void HTTPRequestParser::resetForNextRequest()
{
    // 不清 _buffer，让 pipelined 的下一条请求留着继续解析
    _req = HTTPRequest();
    _state = WAIT_REQUEST_LINE;
    _chunk_waiting_size = true;
    _chunk_expected_size = 0;
}

bool HTTPRequestParser::hasBufferedData() const
{
    return (!_buffer.empty());
}

void HTTPRequestParser::reset()
{
    _req = HTTPRequest();
    _state = WAIT_REQUEST_LINE;
    _buffer.clear();
    _chunk_waiting_size = true;
    _chunk_expected_size = 0;
}

bool HTTPRequestParser::dejaParse(const std::string &newData)
{
    const std::size_t MAX_HEADER_SIZE = 8192; // 后续接 config
    _buffer += newData;
    if (_state == WAIT_REQUEST_LINE || _state == WAIT_HEADERS)
    {
        // header还没结束才限制，避免把body算进去
        if (_buffer.find("\r\n\r\n") == std::string::npos && _buffer.size() > MAX_HEADER_SIZE)
            return (fail(431));
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

void HTTPRequestParser::splitUri()
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

static int hexValue(char c)
{
    if (c >= '0' && c <= '9')
        return (c - '0');
    if (c >= 'A' && c <= 'F')
        return (10 + c - 'A');
    if (c >= 'a' && c <= 'f')
        return (10 + c - 'a');
    return (-1);
}

static bool isUnreservedChar(unsigned char c)
{
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))
        return (true);
    if (c >= '0' && c <= '9')
        return (true);
    return (c == '-' || c == '.' || c == '_' || c == '~');
}

bool HTTPRequestParser::percentDecodePath(std::string &path)
{
    std::string out;
    for (std::size_t i = 0; i < path.size(); ++i)
    {
        if (path[i] != '%')
        {
            out += path[i];
            continue;
        }
        if (i + 2 >= path.size())
            return (false);

        int hi = hexValue(path[i + 1]);
        int lo = hexValue(path[i + 2]);
        if (hi < 0 || lo < 0)
            return (false);

        unsigned char decoded = static_cast<unsigned char>(hi * 16 + lo);

        // 只解码 unreserved characters
        // 这样 %2e -> '.'，但不会把 %2f 提前变成 '/'
        if (isUnreservedChar(decoded))
            out += static_cast<char>(decoded);
        else
            out.append(path, i, 3);

        i += 2;
    }
    path = out;
    return (true);
}

int HTTPRequestParser::normalizePathInPlace(std::string &path)
{
    std::vector<std::string> segments;

    // 记录原始路径是否显式以 '/' 结尾
    bool had_trailing_slash = false;
    if (path.size() > 1 && path[path.size() - 1] == '/')
        had_trailing_slash = true;

    std::size_t i = 0;
    while (i < path.size())
    {
        while (i < path.size() && path[i] == '/')
            ++i;

        std::size_t start = i;
        while (i < path.size() && path[i] != '/')
            ++i;

        std::string seg = path.substr(start, i - start);
        if (seg.empty() || seg == ".")
            continue;
        if (seg == "..")
            return (403);

        segments.push_back(seg);
    }
    path = "/";
    for (std::size_t j = 0; j < segments.size(); ++j)
    {
        if (j > 0)
            path += "/";
        path += segments[j];
    }
    // 保留原始尾斜杠，但根路径 "/" 不额外追加
    if (had_trailing_slash && path != "/")
        path += "/";

    return (0);
}

bool HTTPRequestParser::isValidHeaderName(const std::string &key)
{
    if (key.empty())
        return (false);
    for (size_t i = 0; i < key.size(); ++i)
    {
        unsigned char c = static_cast<unsigned char>(key[i]);
        if (!isTChar(c))
            return (false);
    }
    return (true);
}

int HTTPRequestParser::parseContentLengthStrict(const std::string &v, std::size_t &out, std::size_t max_body)
{
    if (v.empty())
        return (400);

    std::size_t acc = 0;
    for (std::size_t i = 0; i < v.size(); ++i)
    {
        char c = v[i];
        if (c < '0' || c > '9')
            return (400);
        std::size_t digit = static_cast<std::size_t>(c - '0');
        // overflow protection
        if (acc > (static_cast<std::size_t>(-1) - digit) / 10)
            return (400);
        acc = acc * 10 + digit;
        if (acc > max_body)
            return (413);
    }
    out = acc;
    return (0);
}

bool HTTPRequestParser::fail(int code)
{
    _req.bad_request = true;
    if (code > 0)
        _req.error_code = code;
    else
        _req.error_code = 400;
    _req.complet = true;
    _state = PARSE_DONE;
    _req.keep_alive = false;
    // 防止 chunked 状态残留
    _chunk_waiting_size = true;
    _chunk_expected_size = 0;
    return (false);
}

bool HTTPRequestParser::isWaitingBody() const
{
    return (_state == WAIT_BODY);
}

static bool hasBareLF(const std::string &s)
{
    for (std::size_t i = 0; i < s.size(); ++i)
    {
        if (s[i] == '\n')
        {
            if (i == 0 || s[i - 1] != '\r')
                return true;
        }
    }
    return false;
}

bool HTTPRequestParser::parseRequestLine()
{
    // if (hasBareLF(_buffer))
    //     return fail(400);

    // size_t	pos = _buffer.find("\r\n");
    // if (pos == std::string::npos)
    // 	return (true);
    std::size_t pos = _buffer.find("\r\n");
    if (pos == std::string::npos)
    {
        //// 请求行还没收全时，只要当前 buffer 里出现裸 LF，就说明请求行本身非法
        //if (hasBareLF(_buffer))
        //{
        //    std::cout << "has bar lf in parserequest" << std::endl;
        //    return fail(400);
        //}
        size_t  lf_pos = _buffer.find('\n');
        if (lf_pos != std::string::npos)
        {
            if (lf_pos == 0 || _buffer[lf_pos - 1] != '\r')
            {
                std::cout << "lf only line, wait for more data" << std::endl;
                if (_buffer.size() > 8192)
                    return fail(400);
                return true;
            }
        }
        return true;
    }
    // non complet

    std::string line = _buffer.substr(0, pos);
    _buffer.erase(0, pos + 2);
    std::istringstream iss(line);
    iss >> _req.method >> _req.uri >> _req.version;

    //
    std::string extra;
    if (!iss || (iss >> extra))
        return fail(400);
    //

    static const std::size_t MAX_URI_LENGTH = 4096;
    if (!isTokenUpperAlpha(_req.method))
        return (fail(400));
    if (_req.method.empty() || _req.uri.empty() || _req.version.empty())
        return (fail(400));
    if (_req.version != "HTTP/1.1")
        return (fail(505));
    if (_req.method != "GET" && _req.method != "POST" && _req.method != "DELETE")
        return (fail(405));

    // URI 长度
    if (_req.uri.size() > MAX_URI_LENGTH)
        return (fail(414));
    // URI 字符集合法性
    if (!isValidUriChar(_req.uri))
        return fail(400);
    // 如果是 absolute-form，把 authority 取出来，把 uri 改写成 origin-form（从 / 开始）
    if (_req.uri.compare(0, 7, "http://") == 0)
    {
        size_t slash = _req.uri.find('/', 7);
        if (slash == std::string::npos)
            return (fail(400)); // 没有 path
        std::string authority = _req.uri.substr(7, slash - 7);
        std::string rest = _req.uri.substr(slash); // 从 '/' 开始（含 query）
        // authority 不能为空
        if (authority.empty())
            return (fail(400));
        // authority: host[:port]
        std::string host = authority;
        size_t colon = authority.find(':');
        if (colon != std::string::npos)
        {
            host = authority.substr(0, colon);
            std::string port_str = authority.substr(colon + 1);

            int port_num = 0;
            if (!parsePort(port_str, port_num))
                return (fail(400));
            // 还需要检查端口是否等于 server listen port（依赖 config）
            // (需要把 server_port 注入 parser)
        }
        // if (!isValidDomainLike(host))
        //         return (fail(400));
        if (!isValidDomainLike(host) && !isValidIp(host))
            return fail(400);
        _req.authority = authority;
        _req.uri = rest;
        // 改写后再次检查长度（防御）
        if (_req.uri.size() > MAX_URI_LENGTH)
            return (fail(414));
    }
    // origin-form must begin with '/'
    if (_req.uri.empty() || _req.uri[0] != '/')
    {
        return (fail(400));
    }
    // block '..' => 403
    // if (_req.uri.find("..") != std::string::npos)
    // {
    //     return (fail(403));
    // }
    _req.keep_alive = true;
    splitUri();
    if (!percentDecodePath(_req.path))
        return fail(400);
    int norm = normalizePathInPlace(_req.path); // 0 ok, 403 out-of-root
    if (norm == 403)
        return fail(403);
    _chunk_waiting_size = true;
    _chunk_expected_size = 0;
    _state = WAIT_HEADERS;
    return (true);
}

static bool hasBareLFInPrefix(const std::string &s, std::size_t len)
{
    for (std::size_t i = 0; i < len; ++i)
    {
        if (s[i] == '\n')
        {
            if (i == 0 || s[i - 1] != '\r')
                return true;
        }
    }
    return false;
}

bool HTTPRequestParser::parseHeaders()
{
    std::size_t header_end = _buffer.find("\r\n\r\n");
    if (header_end == std::string::npos)
    {
        if (hasBareLF(_buffer))
            return fail(400);
        return true; // need more data
    }
    // 这里只检查 header，不碰 body
    if (hasBareLFInPrefix(_buffer, header_end + 2))
        return fail(400);
    // 只拿 header 部分出来解析
    std::string headers_block = _buffer.substr(0, header_end + 4);
    while (true)
    {
        std::size_t pos = _buffer.find("\r\n");
        if (pos == std::string::npos)
            return (true);
        // need more data

        std::string line = _buffer.substr(0, pos);
        _buffer.erase(0, pos + 2);

        std::cout << "line: " << line << std::endl;
        // header结束：空行
        if (line.empty())
        {
            // 1)HTTP/1.1 必须有 Host
            if (_req.version == "HTTP/1.1")
            {
                if (_req.headers.find("host") == _req.headers.end())
                {
                    std::cerr << "[HDR FAIL] missing Host header" << std::endl;
                    return fail(400);
                }
            }
            // 2)chunked 与 content-length 不能同时存在
            //  if (_req.chunked && _req.has_content_length)
            //      return (fail(400));
            if (_req.chunked && _req.has_content_length)
            {
                std::cerr << "[HDR FAIL] both TE and CL present" << std::endl;
                return fail(400);
            }
            // 两者都没有 -> 411 Length Required（POST 必须有长度信息（Content-Length 或 chunked），否则 411）
            if (_req.method == "POST")
            {
                if (!_req.chunked && !_req.has_content_length)
                    return (fail(411));
            }
            // 3)如果有 content-length（且没 chunked），决定 body
            //  if (_req.has_content_length && _req.contentLength > 0)
            //  	_req.has_body = true;
            //  决定 body
            if (_req.chunked)
                _req.has_body = true;
            // else if (_req.has_content_length && _req.contentLength > 0)
            else if (_req.has_content_length) // content-length=0也进入wait-body,由怕rseFixedBody（）统一读空body
                _req.has_body = true;
            else
                _req.has_body = false;
            if (_req.chunked)
            {
                _chunk_waiting_size = true;
                _chunk_expected_size = 0;
            }
            if (_req.headers.count("connection"))
            {
                std::string conn = _req.headers["connection"];
                toLowerInPlace(conn);
                bool seen_close = false;
                std::stringstream ss(conn);
                std::string tok;
                while (std::getline(ss, tok, ','))
                {
                    ltrimSpaces(tok);
                    rtrimSpaces(tok);
                    if (tok == "close")
                        seen_close = true;
                }
                if (seen_close)
                    _req.keep_alive = false;
            }
            _state = _req.has_body ? WAIT_BODY : PARSE_DONE;
            if (_state == PARSE_DONE)
                _req.complet = true;
            return (true);
        }
        // parse header line
        std::size_t colon = line.find(':');
        // if (colon == std::string::npos)
        //     return (fail(400));
        if (colon == std::string::npos)
        {
            std::cerr << "[HDR FAIL] missing colon: " << line << std::endl;
            return fail(400);
        }
        std::string key = line.substr(0, colon);
        std::string val = line.substr(colon + 1);
        // key / val trim
        rtrimSpaces(key);
        toLowerInPlace(key);
        ltrimSpaces(val);
        rtrimSpaces(val);
        // 4)header name 合法性
        //  if (!isValidHeaderName(key))
        //      return (fail(400));
        if (!isValidHeaderName(key))
        {
            std::cerr << "[HDR FAIL] invalid header name: " << key << std::endl;
            return fail(400);
        }
        // 5)Host 唯一性（重复 host -> 400）
        if (key == "host")
        {
            //
            if (val.empty())
            {
                std::cerr << "[HDR FAIL] empty host " << std::endl;
                return (fail(400));
            }
            //
            // 1) Host 头重复 -> 400
            if (_req.headers.count("host"))
            {
                std::cerr << "[HDR FAIL] multi host " << std::endl;
                return (fail(400));
            }
            // 2) absolute-form 情况下，Host 必须与 authority 一致（大小写不敏感）
            if (!_req.authority.empty())
            {
                std::string host_hdr = val;
                std::string authority = _req.authority;
                toLowerInPlace(host_hdr);
                toLowerInPlace(authority);
                if (host_hdr != authority)
                {
                    std::cerr << "[HDR FAIL] host not autority " << std::endl;
                    return (fail(400));
                }
            }
        }
        // 6)Content-Length 严格校验（纯数字、无溢出、唯一性）
        if (key == "content-length")
        {
            if (_req.has_content_length)
            {
                std::cerr << "[HDR FAIL] has_content_length" << std::endl;
                return (fail(400));
            }
            std::size_t n = 0;
            // if (!parseContentLengthStrict(val, n, MAX_BODY))
            //     return (fail(400));
            int err = parseContentLengthStrict(val, n, _req.max_body_size);
            if (err != 0)
                return (fail(err));
            // MAX_BODY 返回 413, 先全部设置400；等接入 config 再细分
            _req.contentLength = n;
            _req.has_content_length = true;
            _req.headers[key] = val;
            continue;
        }
        // 7)Transfer-Encoding 只支持 chunked，且唯一性
        //  if (key == "transfer-encoding")
        //  {
        //      if (_req.has_transfer_encoding)
        //          return (fail(400));
        //      _req.has_transfer_encoding = true;
        //      std::string te = val;
        //      toLowerInPlace(te);
        //      ltrimSpaces(te);
        //      rtrimSpaces(te);
        //      // 只要包含 chunked 就认为 chunked
        //      if (te == "chunked")
        //          _req.chunked = true;
        //      else
        //          return fail(501);
        //      _req.headers[key] = val;
        //      continue;
        //  }
        if (key == "transfer-encoding")
        {
            if (_req.has_transfer_encoding)
            {
                std::cerr << "[HDR FAIL] has_transfer_encoding" << std::endl;
                return fail(400);
            }
            _req.has_transfer_encoding = true;
            std::string te = val;
            toLowerInPlace(te);
            bool found_chunked = false;
            std::stringstream ss(te);
            std::string token;
            while (std::getline(ss, token, ','))
            {
                ltrimSpaces(token);
                rtrimSpaces(token);
                if (token.empty())
                {
                    std::cerr << "[HDR FAIL] token empty" << std::endl;
                    return fail(400);
                }
                //     if (token == "chunked")
                //         found_chunked = true;
                // }
                if (token == "chunked")
                {
                    if (found_chunked)
                    {
                        std::cerr << "[HDR FAIL] multi chunked" << std::endl;
                        return fail(400); // 重复 chunked 也直接拒绝
                    }
                    found_chunked = true;
                }
                else
                {
                    // 任何非 chunked 的 transfer-coding 都不支持
                    return fail(501);
                }
            }
            if (!found_chunked)
                return (fail(501));
            _req.chunked = true;
            _req.headers[key] = val;
            continue;
        }
        // 默认：保存 header
        _req.headers[key] = val;
    }
    return (true);
}

// bool HTTPRequestParser::parseBody()
// {
//     //WAIT_BODY 必须有 Content-Length
//     if (!_req.has_content_length)
//         return (fail(400));
//     //等待足够的 body 数据
//     if (_buffer.size() < _req.contentLength)
//         return (true);
//     //精确读取 body（可能为 0，一般不会在 0 时进入 WAIT_BODY）
//     if (_req.contentLength > 0)
//         _req.body = _buffer.substr(0, _req.contentLength);
//     else
//         _req.body.clear();
//     _buffer.erase(0, _req.contentLength);
//     //完成
//     _req.complet = true;
//     _state = PARSE_DONE;
//     return (true);
// }

bool HTTPRequestParser::parseFixedBody()
{
    if (!_req.has_content_length)
        return (fail(400));
    if (_req.contentLength > _req.max_body_size)
        return (fail(413));
    if (_buffer.size() < _req.contentLength)
        return (true);
    if (_req.contentLength > 0)
        _req.body = _buffer.substr(0, _req.contentLength);
    else
        _req.body.clear();
    _buffer.erase(0, _req.contentLength);
    _req.complet = true;
    _state = PARSE_DONE;
    _chunk_waiting_size = true;
    _chunk_expected_size = 0;
    return (true);
}

static bool peekLineCRLF(const std::string &buf, std::string &outLine, std::size_t &lineTotalLen)
{
    std::size_t pos = buf.find("\r\n");
    if (pos == std::string::npos)
        return (false);
    outLine = buf.substr(0, pos);
    lineTotalLen = pos + 2; // 包含 \r\n
    return (true);
}

static bool parseHexSize(const std::string &s, std::size_t &out)
{
    // 允许 chunk-size 后跟 ;extension，先截断
    std::string t = s;
    std::size_t semi = t.find(';');
    if (semi != std::string::npos)
        t = t.substr(0, semi);
    if (t.empty())
        return (false);
    std::size_t acc = 0;
    for (std::size_t i = 0; i < t.size(); ++i)
    {
        char c = t[i];
        int v;
        if (c >= '0' && c <= '9')
            v = c - '0';
        else if (c >= 'a' && c <= 'f')
            v = 10 + (c - 'a');
        else if (c >= 'A' && c <= 'F')
            v = 10 + (c - 'A');
        else
            return (false);
        // overflow guard
        if (acc > (static_cast<std::size_t>(-1) - static_cast<std::size_t>(v)) / 16)
            return (false);
        acc = acc * 16 + static_cast<std::size_t>(v);
    }
    out = acc;
    return (true);
}

bool HTTPRequestParser::parseChunkedBody()
{
    while (true)
    {
        // 1) 等待读 chunk-size 行
        if (_chunk_waiting_size)
        {
            std::string line;
            std::size_t lineLen = 0;

            if (!peekLineCRLF(_buffer, line, lineLen))
                return (true); // need more data (连 size 行都不完整)

            std::size_t chunkSize = 0;
            if (!parseHexSize(line, chunkSize))
                return (fail(400));

            // 现在才真正消费掉 size 行（因为它已经完整）
            _buffer.erase(0, lineLen);

            _chunk_expected_size = chunkSize;
            // _chunk_waiting_size = false; // 下一步等 data+CRLF

            // chunkSize == 0 进入 trailer 处理
            if (_chunk_expected_size == 0)
            {
                // trailer 结束标志：空行 CRLF
                // 若 trailer 为空，此时 buffer 应该以 "\r\n" 开头
                // 若 trailer 非空，必须找到 "\r\n\r\n"
                // std::size_t end = _buffer.find("\r\n\r\n");
                // if (end == std::string::npos)
                //     return (true);              // trailer 未收全，不消费
                // _buffer.erase(0, end + 4);    // erase 到 \r\n\r\n 后

                // chunkSize == 0 后：允许空 trailer 直接以 CRLF 结束
                if (_buffer.size() >= 2 && _buffer.compare(0, 2, "\r\n") == 0)
                {
                    _buffer.erase(0, 2);
                }
                else
                {
                    std::size_t end = _buffer.find("\r\n\r\n");
                    if (end == std::string::npos)
                        return (true);
                    _buffer.erase(0, end + 4);
                }

                _req.contentLength = _req.body.size();
                _req.has_content_length = true;
                _req.complet = true;
                _state = PARSE_DONE;

                _chunk_waiting_size = true;
                _chunk_expected_size = 0;
                return (true);
            }
            _chunk_waiting_size = false; // 下一步等 data+CRLF
        }
        // 2) 等待读 chunk-data + CRLF
        //    这里必须“确认数据足够”后再消费
        std::size_t need = _chunk_expected_size + 2; // data + \r\n
        if (_buffer.size() < need)
            return (true); // need more data，这里不消费任何东西

        if (_req.body.size() + _chunk_expected_size > _req.max_body_size)
            return (fail(413));

        _req.body.append(_buffer.data(), _chunk_expected_size);
        _buffer.erase(0, _chunk_expected_size);

        // 必须跟着 CRLF
        // if (_buffer.size() < 2 || _buffer.substr(0, 2) != "\r\n")
        if (_buffer.size() < 2 || _buffer.compare(0, 2, "\r\n") != 0)
            return (fail(400));
        _buffer.erase(0, 2);
        // 当前 chunk 完成，回到读取下一块 size 行
        _chunk_waiting_size = true;
        _chunk_expected_size = 0;
    }
}

bool HTTPRequestParser::parseBody()
{
    if (_req.chunked)
        return parseChunkedBody();
    return parseFixedBody();
}
