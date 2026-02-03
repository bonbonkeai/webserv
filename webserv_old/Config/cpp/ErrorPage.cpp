#include "Config/hpp/ErrorPage.hpp"
#include <sstream>
#include <fstream>
#include "HTTP/hpp/ErrorResponse.hpp"

//这里有很多问题，暂时没用，目前还是先以buildErrorResponse(code) 固定生成 HTML body 的方式

std::string ErrorPage::load_error_file(const std::string& path)
{
    std::ifstream file(path.c_str());
    if (!file.is_open())
        return "";

    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

std::string ErrorPage::get_error_page_path(int status, const ServerConfig& server,LocationConfig* location)
{
    std::string str = toString(status);
    /*location override*/
    if (location)
    {
        std::map<std::string, std::vector<std::string> >::iterator it;
        it = location->directives.find("error_page");
        if (it != location->directives.end())
        {
            const std::vector<std::string>& values = it->second;
            for(size_t i =0; i+1 < values.size(); i+=2)
            {
                if(values[i] == str)
                    return values[i+1];
            }
        }
    }

    /* server*/
    std::map<std::string, std::vector<std::string> >::iterator it;
    it = location->directives.find("error_page");
    if (it != server.directives.end())
    {
        const std::vector<std::string>& values = it->second;
        for (size_t i=0; i+1 < values.size(); i += 2)
        {
            if (values[i] == str)
                return values[i+1];
        }
    }
    /*pas de fichier*/
    return "";
}

std::string ErrorPage::default_error_page(int status)
{
    std::stringstream s;

    s << "<!DOCTYPE html>\n"
       << "<html>\n"
       << "<head>\n"
       << "<title>" << status << " Error</title>\n"
       << "<style>\n"
       << "body { font-family: Arial; text-align: center; padding-top: 50px; }\n"
       << "h1 { font-size: 48px; }\n"
       << "</style>\n"
       << "</head>\n"
       << "<body>\n"
       << "<h1>" << status << "</h1>\n"
       << "<p>Webserv error</p>\n"
       << "</body>\n"
       << "</html>\n";

    return s.str();
}


/*API public*/
HTTPResponse ErrorPage::generate(int status, const ServerConfig& s, LocationConfig* l)
{
    std::string body;
    std::string path = get_error_page_path(status, s, l);

    if (!path.empty())
        body = load_error_file(path);

    if (body.empty())
        body = default_error_page(status);
    HTTPResponse res;
    res.statusCode = status;
    buildErrorResponse(res.statusCode);
    res.body = body;
    return res;
}
