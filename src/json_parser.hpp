#ifndef SIMPLE_JSON_PARSER_H
#define SIMPLE_JSON_PARSER_H

#include <string>
#include <vector>
#include <unordered_map>
#include <any>
#include <stdexcept>
#include <cctype>
#include <sstream>

namespace xjson
{

    using json_object = std::unordered_map<std::string, std::any>;
    using json_array = std::vector<std::any>;

    class parser
    {
        const std::string &text_;
        size_t pos_ = 0;

    public:
        parser(const std::string &input) : text_(input) {}

        std::any parse()
        {
            skip_whitespace();
            auto val = parse_value();
            skip_whitespace();
            if (pos_ != text_.size())
                throw std::runtime_error("Extra data after JSON value");
            return val;
        }

    private:
        void skip_whitespace()
        {
            while (pos_ < text_.size() && std::isspace(text_[pos_]))
                pos_++;
        }

        char peek()
        {
            if (pos_ >= text_.size())
                throw std::runtime_error("Unexpected end");
            return text_[pos_];
        }

        char consume()
        {
            if (pos_ >= text_.size())
                throw std::runtime_error("Unexpected end");
            return text_[pos_++];
        }

        std::any parse_value()
        {
            skip_whitespace();
            char ch = peek();
            if (ch == '"')
                return parse_string();
            if (ch == '{')
                return parse_object();
            if (ch == '[')
                return parse_array();
            if (std::isdigit(ch) || ch == '-')
                return parse_number();
            if (text_.compare(pos_, 4, "true") == 0)
            {
                pos_ += 4;
                return true;
            }
            if (text_.compare(pos_, 5, "false") == 0)
            {
                pos_ += 5;
                return false;
            }
            if (text_.compare(pos_, 4, "null") == 0)
            {
                pos_ += 4;
                return std::any();
            }
            throw std::runtime_error("Invalid JSON value");
        }

        std::string parse_string()
        {
            consume(); // "
            std::ostringstream ss;
            while (true)
            {
                if (pos_ >= text_.size())
                    throw std::runtime_error("Unterminated string");
                char ch = consume();
                if (ch == '"')
                    break;
                if (ch == '\\')
                {
                    char esc = consume();
                    switch (esc)
                    {
                    case '"':
                        ss << '"';
                        break;
                    case '\\':
                        ss << '\\';
                        break;
                    case '/':
                        ss << '/';
                        break;
                    case 'b':
                        ss << '\b';
                        break;
                    case 'f':
                        ss << '\f';
                        break;
                    case 'n':
                        ss << '\n';
                        break;
                    case 'r':
                        ss << '\r';
                        break;
                    case 't':
                        ss << '\t';
                        break;
                    default:
                        throw std::runtime_error("Invalid escape");
                    }
                }
                else
                {
                    ss << ch;
                }
            }
            return ss.str();
        }

        std::any parse_number()
        {
            size_t start = pos_;
            if (peek() == '-')
                pos_++;
            while (std::isdigit(peek()))
                pos_++;
            bool is_float = false;
            if (peek() == '.')
            {
                is_float = true;
                pos_++;
                while (std::isdigit(peek()))
                    pos_++;
            }
            if (peek() == 'e' || peek() == 'E')
            {
                is_float = true;
                pos_++;
                if (peek() == '+' || peek() == '-')
                    pos_++;
                while (std::isdigit(peek()))
                    pos_++;
            }
            std::string num_str = text_.substr(start, pos_ - start);
            if (is_float)
                return std::stof(num_str);
            return std::stoi(num_str);
        }

        json_array parse_array()
        {
            json_array arr;
            consume(); // '['
            skip_whitespace();
            if (peek() == ']')
            {
                consume();
                return arr;
            }
            while (true)
            {
                arr.push_back(parse_value());
                skip_whitespace();
                if (peek() == ']')
                {
                    consume();
                    break;
                }
                if (consume() != ',')
                    throw std::runtime_error("Expected ','");
            }
            return arr;
        }

        json_object parse_object()
        {
            json_object obj;
            consume(); // '{'
            skip_whitespace();
            if (peek() == '}')
            {
                consume();
                return obj;
            }
            while (true)
            {
                std::string key = parse_string();
                skip_whitespace();
                if (consume() != ':')
                    throw std::runtime_error("Expected ':'");
                skip_whitespace();
                obj[key] = parse_value();
                skip_whitespace();
                if (peek() == '}')
                {
                    consume();
                    break;
                }
                if (consume() != ',')
                    throw std::runtime_error("Expected ','");
            }
            return obj;
        }
    };

    inline std::any parse_json(const std::string &json)
    {
        parser parser_(json);
        return parser_.parse();
    }

} // namespace xjson

#endif // SIMPLE_JSON_PARSER_H