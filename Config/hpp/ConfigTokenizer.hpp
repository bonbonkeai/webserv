#ifndef CONFIGTOKENIZER_HPP
#define CONFIGTOKENIZER_HPP

#include <iostream>
#include <string>
#include <vector>

enum    Tokentype
{
    TYPE_EOF, //la fin du file
    TYPE_COMMENT, //commentaire
    TYPE_SEMICOLON, //;
    TYPE_L_CURLY, //{
    TYPE_R_CURLY, //}
    TYPE_WORD, //elements cle
    TYPE_NUM, //numbers
    TYPE_ERROR, 
};

struct  Token
{
    Tokentype   type;
    std::string value;
    size_t  line;
    size_t  col;

/*initialisation token type*/
    Token(Tokentype t = TYPE_EOF, const std::string& v = "", size_t l = 1, size_t c = 1): 
        type(t), value(v), line(l), col(c){};
};

class   ConfigTokenizer
{
    private:
        std::string _content;
        std::vector<Token>  _tokens;
        size_t  current_col;
        size_t  current_line;
    public:
        ConfigTokenizer(){};
        ~ConfigTokenizer(){};

        /*read file*/
        bool read_file(const std::string& filename);
        bool    tokenise_string(const std::string& str);

        void    skip_space(const std::string& str, size_t& pos);
        Token   get_next_token(const std::string& str, size_t& pos);

        bool    is_word(char c);
        bool    is_number(char c);
        std::string read_word(const std::string& str, size_t& pos);
        std::string read_number(const std::string& str, size_t& pos);

        //debug
        void    print_tokens() const;
};


#endif


/*读取配置文件

词法分析 tokenize
语法分析（解析 server { ... } 结构）
构造：
ServerConfig
LocationConfig
检查语法错误、缺失字段

最后得到std::vector<ServerConfig> servers;*/