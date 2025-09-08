#pragma once
#include "Token.hpp"
#include "Common.hpp"

class Lexer
{
public:
    explicit Lexer(const std::string &source) : Source(source), Position(0) {}

    std::vector<Token> Tokenize()
    {
        std::vector<Token> Tokens;

        while (Position < Source.size())
        {
            char Current = Peek();
            if (std::isspace(Current))
            {
                Advance();
            }
            else if (std::isalpha(Current) || Current == '_') // <-- allow underscore as valid start char
                Tokens.push_back(ReadIdentifierOrKeyword());
            else if (Current == '"' || Current == '\'')
                Tokens.push_back(ReadStringLiteral());
            else if (std::isdigit(Current))
                Tokens.push_back(ReadNumber());
            else
            {
                switch (Current)
                {
                case '=':
                    if (Peek(1) == '=')
                    {
                        Tokens.emplace_back(TokenType::DoubleEquals, "==", Location);
                        Advance(2);
                    }
                    else if (Peek(1) == '>')
                    {
                        Tokens.emplace_back(TokenType::RArrowThick, "=>", Location);
                        Advance(2);
                    }
                    else
                    {
                        Tokens.emplace_back(TokenType::Equals, "=", Location);
                        Advance();
                    }
                    break;
                case ':':
                    if (Peek(1) == ':')
                    {
                        Tokens.emplace_back(TokenType::DoubleColon, "::", Location);
                        Advance(2);
                    }
                    else
                    {
                        Tokens.emplace_back(TokenType::Colon, ":", Location);
                        Advance();
                    }
                    break;
                case '.':
                    if (Peek(1) == '.' && Peek(2) == '.')
                    {
                        Tokens.emplace_back(TokenType::DotDotDot, "...", Location);
                        Advance(3);
                    }
                    else
                    {
                        Tokens.emplace_back(TokenType::Dot, ".", Location);
                        Advance();
                    }
                    break;
                case '!':
                    if (Peek(1) == '=')
                    {
                        Tokens.emplace_back(TokenType::ExclamationEquals, "!=", Location);
                        Advance(2);
                    }
                    else
                    {
                        Tokens.emplace_back(TokenType::Exclamation, "!", Location);
                        Advance();
                    }
                    break;
                case '(':
                    Tokens.emplace_back(TokenType::LParen, "(", Location);
                    Advance();
                    break;
                case ')':
                    Tokens.emplace_back(TokenType::RParen, ")", Location);
                    Advance();
                    break;
                case '{':
                    Tokens.emplace_back(TokenType::LBrace, "{", Location);
                    Advance();
                    break;
                case '}':
                    Tokens.emplace_back(TokenType::RBrace, "}", Location);
                    Advance();
                    break;
                case '[':
                    Tokens.emplace_back(TokenType::LBracket, "[", Location);
                    Advance();
                    break;
                case ']':
                    Tokens.emplace_back(TokenType::RBracket, "]", Location);
                    Advance();
                    break;
                case ',':
                    Tokens.emplace_back(TokenType::Comma, ",", Location);
                    Advance();
                    break;
                case '?':
                    Tokens.emplace_back(TokenType::QuestionMark, "?", Location);
                    Advance();
                    break;
                case '|':
                    Tokens.emplace_back(TokenType::Pipe, "|", Location);
                    Advance();
                    break;
                case '&':
                    Tokens.emplace_back(TokenType::Ampersand, "&", Location);
                    Advance();
                    break;
                case '$':
                    Tokens.emplace_back(TokenType::DollarSign, "$", Location);
                    Advance();
                    break;
                case '#':
                    Tokens.emplace_back(TokenType::Hash, "#", Location);
                    Advance();
                    break;
                case ';':
                    Tokens.emplace_back(TokenType::SemiColon, ";", Location);
                    Advance();
                    break;
                case '^':
                    Tokens.emplace_back(TokenType::Caret, "^", Location);
                    Advance();
                    break;
                case '<':
                    Tokens.emplace_back(TokenType::LAngle, "<", Location);
                    Advance();
                    break;
                case '>':
                    Tokens.emplace_back(TokenType::RAngle, ">", Location);
                    Advance();
                    break;
                case '+':
                    Tokens.emplace_back(TokenType::Plus, "+", Location);
                    Advance();
                    break;
                case '-':
                    if (Peek(1) == '>')
                    {
                        Tokens.emplace_back(TokenType::RArrowThin, "->", Location);
                        Advance(2);
                    }
                    else
                    {
                        Tokens.emplace_back(TokenType::Minus, "-", Location);
                        Advance();
                    }
                    break;
                case '*':
                    Tokens.emplace_back(TokenType::Star, "*", Location);
                    Advance();
                    break;
                case '~':
                    if (Peek(1) == '>')
                    {
                        Tokens.emplace_back(TokenType::RArrowWavy, "~>", Location);
                        Advance(2);
                    }
                    else
                    {
                        Tokens.emplace_back(TokenType::Tilde, "~", Location);
                        Advance();
                    }
                    break;
                // Ignore comments
                case '/':
                    if (Peek(1) == '/')
                    {
                        while (Peek() != '\n' && !IsAtEnd())
                            Advance();

                        if (!IsAtEnd())
                        {
                            Advance();
                        }
                    }
                    else
                    {
                        Tokens.emplace_back(TokenType::Slash, "/", Location);
                        Advance();
                    }
                    break;
                default:
                    Advance(); // Skip unrecognized character
                    break;
                }
            }
        }

        Tokens.emplace_back(TokenType::Eof, "", Location);
        return Tokens;
    }

public:
    ScriptLocation Location;

private:
    const std::string &Source;
    size_t Position;

    char Peek(size_t Offset = 0) const
    {
        return (Position + Offset < Source.size()) ? Source[Position + Offset] : '\0';
    }

    void Advance(size_t Amount = 1)
    {
        for (size_t i = 0; i < Amount; ++i)
        {
            char c = Peek(); // get the current character
            if (c == '\n')
            {
                Location.Line += 1;
                Location.Column = 1;
            }
            else
            {
                Location.Column += 1;
            }

            Position += 1;
        }
    }

    Token ReadIdentifierOrKeyword()
    {
        size_t Start = Position;
        while (std::isalnum(Peek()) || Peek() == '_')
            Advance();
        std::string Text = Source.substr(Start, Position - Start);

        static std::unordered_map<std::string, TokenType> Keywords = {
            {"fn", TokenType::Function},
            {"do", TokenType::Do},
            {"esc", TokenType::End},
            {"lib", TokenType::Library},
            {"if", TokenType::If},
            {"else", TokenType::Else},
            {"for", TokenType::For},
            {"of", TokenType::Of},
            {"var", TokenType::Var},
            {"use", TokenType::Use},
            {"with", TokenType::With},
            {"class", TokenType::Class},
            {"break", TokenType::Break},

            {"null", TokenType::Null},
            {"true", TokenType::True},
            {"false", TokenType::False},
            {"string", TokenType::StringType},
            {"int", TokenType::IntType},
            {"float", TokenType::FloatType},
            {"bool", TokenType::BoolType},
            {"any", TokenType::AnyType}
        };

        auto It = Keywords.find(Text);
        TokenType Type = (It != Keywords.end()) ? It->second : TokenType::Identifier;
        return Token(Type, Text, Location);
    }

    Token ReadStringLiteral()
    {
        const char QuoteChar = Peek();
        Advance();

        size_t Start = Position;
        std::string Text;
        while (!(Peek() == QuoteChar))
        {
            if (Peek() == '\\')
            {
                Advance();

                switch (Peek())
                {
                case '\\':
                    Text += '\\';
                    break;
                case '\'':
                    Text += '\'';
                    break;
                case '"':
                    Text += '"';
                    break;
                case 'n':
                    Text += '\n';
                    break;

                default:
                    throw std::runtime_error("Invalid escape character");
                }

                Advance();
                continue;
            }

            Text += Peek();
            Advance();
        }

        // if (QuoteChar == '\'' && Text.length() != 1)
        //     throw std::runtime_error("Character literal must contain 1 character");

        Advance();
        return Token(TokenType::StringLiteral, Text, Location);
    }

    Token ReadNumber()
    {
        size_t Start = Position;

        while (std::isdigit(Peek()))
            Advance();

        if (Peek() == '.' && std::isdigit(Peek(1)))
        {
            Advance(); // consume '.'

            while (std::isdigit(Peek()))
                Advance();
        }

        std::string Text = Source.substr(Start, Position - Start);
        return Token(TokenType::Number, Text, Location);
    }

    bool IsAtEnd() const
    {
        return Position >= Source.size();
    }
};
