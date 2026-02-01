#include "Config/hpp/ConfigTokenizer.hpp"
#include <fstream>
#include <string>
#include <iterator>
#include <cctype>

bool    ConfigTokenizer::read_file(const std::string& filename)
{
    std::fstream    file(filename.c_str());

    if (!file.is_open())
    {
        std::cerr << "Error: failed to open the file" << std::endl;
        return false;
    }
    std::string content_file((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    //std::cout << content_file << std::endl;
    file.close();
    return tokenise_string(content_file);
}


void    ConfigTokenizer::skip_space(const std::string& str, size_t& pos)
{
    while (pos < str.length() && std::isspace(str[pos]))
    {
        if (str[pos] == '\n')
        {
            current_line++;
            current_col = 1;
        }
        else 
        {
            current_col++;
        }
        pos++;
    }
}

Token   ConfigTokenizer::get_next_token(const std::string& str, size_t& pos)
{
    if (pos >= str.length())
        return Token(TYPE_EOF, "", current_line, current_col);

    char    c = str[pos];
    size_t  tmp_col = current_col;
    size_t  tmp_line = current_line;

    if (c == '/' && (pos + 1) < str.length() && str[pos + 1] == '/')
    {
        while (pos < str.length() && str[pos] != '\n')
        {
            pos++;
            current_col++;
        }
        return Token(TYPE_COMMENT, "", tmp_line, tmp_col);
    }
    if (is_word(c))
        return Token(TYPE_WORD, read_word(str, pos), tmp_line, tmp_col);
    if (is_number(c))
        return Token(TYPE_NUM, read_number(str, pos), tmp_line, tmp_col);
    switch (c)
    {
        case ';':
            current_col++;
            pos++;
            return Token(TYPE_SEMICOLON, ";", tmp_line, tmp_col);
        case '{':
            current_col++;
            pos++;
            return Token(TYPE_L_CURLY, "{", tmp_line, tmp_col);
        case '}':
            current_col++;
            pos++;
            return Token(TYPE_R_CURLY, "}", tmp_line, tmp_col);
        case '#':
            while (pos < str.length() && str[pos] != '\n')
            { 
                pos++;
                current_col++; 
            }
            return Token(TYPE_COMMENT, "", tmp_line, tmp_col);
    }
    /* Avancer le caractere??? sinon boucle infini*/
    current_col++;
    pos++;
    return Token(TYPE_EOF, std::string(1, c), tmp_line, tmp_col);
}

bool    ConfigTokenizer::is_word(char c)
{
    return (std::isalpha(c) || c == ':' || c == '/' || c == '_' || c == '-' || c == '.');
}

bool    ConfigTokenizer::is_number(char c)
{
    return (std::isdigit(c));
}

std::string ConfigTokenizer::read_word(const std::string& str, size_t& pos)
{
    std::string resultat;
    while (pos < str.length() && is_word(str[pos]))
    {
        resultat += str[pos];
        pos++;
        current_col++;
    }
    return resultat;
}

std::string ConfigTokenizer::read_number(const std::string& str, size_t& pos)
{
    std::string resultat;
    
    /*
    read only number: 127.0.0.1 or 8080:80
    */
    while (pos < str.length() && (is_number(str[pos]) || str[pos] == '.' || str[pos] == ':'))
    {
        resultat += str[pos];
        pos++;
        current_col++;
    }

    //read 20s, 
    if (pos < str.length())
    {
        char    next_char = str[pos];
        if (next_char == 'k' || next_char == 'm')
        {
            resultat += next_char;
            pos++;
            current_col++;
        }
    }
    return resultat;
}

void ConfigTokenizer::print_tokens() const
{
    for (size_t i = 0; i < _tokens.size(); i++)
    {
        const Token& t = _tokens[i];

        std::cout << "[" << i << "] "
                  << "Type: " << t.type << ", "
                  << "Value: \"" << t.value << "\", "
                  << "Line: " << t.line << ", "
                  << "Col: " << t.col
                  << std::endl;
    }
}
/* tokenise the string, put the elements in the container of tokens */
bool    ConfigTokenizer::tokenise_string(const std::string& str)
{
    size_t  pos = 0;
    _tokens.clear();
    current_col = 1;
    current_line = 1;

    while (pos < str.length())
    {
        skip_space(str, pos);
        if (pos >= str.length())
            break; 
        Token   token = get_next_token(str, pos);
        if (token.type != TYPE_COMMENT)
            _tokens.push_back(token);
        if (token.type == TYPE_ERROR)
        {
            std::cerr << "Error at line " << token.line << ", col " << token.col <<" invalide token " << token.value << std::endl;
            return false;
        }
    }
    _tokens.push_back(Token(TYPE_EOF, "", current_line, current_col));
    return true;
}
