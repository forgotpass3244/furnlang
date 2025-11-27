#pragma once
#include "Token.hpp"
#include "Common.hpp"
#include "CompileFlags.hpp"

#define _CheckCursorPos                                                   \
    if (Position == CmplFlags::CursorPosition && !Tokens.empty())                            \
    {                                                                     \
        Tokens.at(Tokens.size() - 1).IsCursor = true; \
    }

class Lexer
{
public:
    explicit Lexer(std::string &source) : Source(source), Position(0) {}

    std::vector<Token> ReadToken()
    {
        std::vector<Token> Tokens;

        char Current = Peek();
        _CheckCursorPos;

        while (std::isspace(Current))
        {
            Advance();
            Current = Peek();
            _CheckCursorPos;
        }
        _CheckCursorPos;
        if (!IsAtEnd() && Peek() == '#')
        {
            while (Peek() != '\n' && !IsAtEnd())
            {
                Advance();
                _CheckCursorPos;
            }
            if (!IsAtEnd())
            {
                Advance();
                _CheckCursorPos;
            }
            Current = Peek();
        }
        _CheckCursorPos;
        while (std::isspace(Current))
        {
            Advance();
            Current = Peek();
            _CheckCursorPos;
        }
        _CheckCursorPos;
        if (IsAtEnd())
        {
            return Tokens;
        }

        _CheckCursorPos;

        if (std::isalpha(Current) || Current == '_') // <-- allow underscore as valid start char
        {
            for (auto &Tok : ReadIdentifierOrKeyword())
                Tokens.push_back(Tok);
        }
        else if (Current == '"' || Current == '\'')
        {
            for (auto &Tok : ReadStringLiteral())
                Tokens.push_back(Tok);
        }
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
                else if (Peek(1) == '=')
                {
                    Tokens.emplace_back(TokenType::ColonEquals, ":=", Location);
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
                if (Peek(1) == '|')
                {
                    Tokens.emplace_back(TokenType::DoublePipe, "|", Location);
                    Advance(2);
                }
                else
                {
                    Tokens.emplace_back(TokenType::Pipe, "|", Location);
                    Advance();
                }
                break;
            case '&':
                if (Peek(1) == '&')
                {
                    Tokens.emplace_back(TokenType::DoubleAmpersand, "&&", Location);
                    Advance(2);
                }
                else
                {
                    Tokens.emplace_back(TokenType::Ampersand, "&", Location);
                    Advance();
                }
                break;
            case '$':
                Tokens.emplace_back(TokenType::DollarSign, "$", Location);
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
                if (Peek(1) == '=')
                {
                    Tokens.emplace_back(TokenType::LAngleEqual, "<=", Location);
                    Advance(2);
                }
                else
                {
                    Tokens.emplace_back(TokenType::LAngle, "<", Location);
                    Advance();
                }
                break;
            case '>':
                if (Peek(1) == '=')
                {
                    Tokens.emplace_back(TokenType::RAngleEqual, ">=", Location);
                    Advance(2);
                }
                else
                {
                    Tokens.emplace_back(TokenType::RAngle, ">", Location);
                    Advance();
                }
                break;
            case '+':
                if (Peek(1) == '+')
                {
                    Tokens.emplace_back(TokenType::PlusPlus, "++", Location);
                    Advance(2);
                }
                else
                {
                    Tokens.emplace_back(TokenType::Plus, "+", Location);
                    Advance();
                }
                break;
            case '-':
                if (Peek(1) == '>')
                {
                    Tokens.emplace_back(TokenType::RArrowThin, "->", Location);
                    Advance(2);
                }
                else if (Peek(1) == '-')
                {
                    Tokens.emplace_back(TokenType::MinusMinus, "--", Location);
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
            case '/':
                Tokens.emplace_back(TokenType::Slash, "/", Location);
                Advance();
                break;
            case '@':
                Tokens.emplace_back(TokenType::At, "@", Location);
                Advance();
                break;
            // comment
            case '#':
                while (Peek() != '\n' && !IsAtEnd())
                    Advance();
                if (!IsAtEnd())
                    Advance();
                break;
            default:
                throw std::runtime_error("Unsupported character " + std::to_string(Current));
                break;
            }
        }

        _CheckCursorPos;

        return Tokens;
    }

    std::vector<Token> Tokenize()
    {
        std::vector<Token> Tokens;

        while (Position < Source.size())
        {
            for (auto &Tok : ReadToken())
                Tokens.push_back(Tok);
        }

        Tokens.emplace_back(TokenType::Eof, "", Location);
        return Tokens;
    }

public:
    ScriptLocation Location;

    std::string &Source;
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

    std::vector<Token> ReadIdentifierOrKeyword()
    {
        // if ((Peek() == 'r' || Peek() == 'f') && (Peek(1) == '"' || Peek(1) == '\''))
        // {
        //     const char Modifier = Peek();
        //     Advance();
        //     return ReadStringLiteral(Modifier);
        // }
        
        size_t Start = Position;
        while (std::isalnum(Peek()) || Peek() == '_' || (Peek() == '-' && std::isalpha(Peek(1))) /* kebab-case support */)
            Advance();
        std::string Text = Source.substr(Start, Position - Start);

        static std::unordered_map<std::string, TokenType> Keywords = {
            // keywords
            {"if", TokenType::If},
            {"else", TokenType::Else},
            {"elif", TokenType::ElseIf},
            {"for", TokenType::For},
            {"while", TokenType::While},
            {"in", TokenType::In},
            {"as", TokenType::As},
            {"with", TokenType::With},
            {"break", TokenType::Break},
            {"return", TokenType::Return},
            {"raise", TokenType::Raise},

            // declaration
            {"defn", TokenType::Function},
            {"import", TokenType::Import},
            {"pkg", TokenType::Package},
            {"type", TokenType::Class},
            {"export", TokenType::Export},
            {"new", TokenType::New},
            {"immut", TokenType::Immutable},
            {"mut", TokenType::Mutable},

            // null/bool literals
            {"null", TokenType::Null},
            {"true", TokenType::True},
            {"false", TokenType::False},

            // data types
            {"int", TokenType::IntType},
            {"float", TokenType::FloatType},
            {"bool", TokenType::BoolType},
            {"double", TokenType::DoubleType},
            {"short", TokenType::ShortType},
            {"long", TokenType::LongType},
            {"char", TokenType::CharacterType},

            // other
            {"self", TokenType::This},
            {"not", TokenType::Not},
            {"sizeof", TokenType::SizeOf},

            // reserved words
            {"package", TokenType::Reserved},
            {"expt", TokenType::Reserved},
            {"fun", TokenType::Reserved},
            {"var", TokenType::Reserved},
            {"let", TokenType::Reserved},
            {"class", TokenType::Reserved},
            {"struct", TokenType::Reserved},
            {"record", TokenType::Reserved},
            {"extends", TokenType::Reserved}, // as for extending but adding this just in case
            {"abstract", TokenType::Reserved},
            {"impl", TokenType::Reserved},
            {"virtual", TokenType::Reserved},
            {"override", TokenType::Reserved},
            {"interface", TokenType::Reserved},
            {"super", TokenType::Reserved},
            {"typeof", TokenType::Reserved},
            {"final", TokenType::Reserved},
            {"static", TokenType::Reserved},
            {"const", TokenType::Reserved},
            {"immut", TokenType::Reserved},
            {"mutable", TokenType::Reserved},
            {"immutable", TokenType::Reserved},
            {"atomic", TokenType::Reserved},
            // types
            {"bit", TokenType::Reserved},
            {"byte", TokenType::Reserved},
            // while and for loops use the loop keyword, but adding these just in case
            {"while", TokenType::Reserved},
            {"for", TokenType::Reserved},
            {"foreach", TokenType::Reserved},
            {"continue", TokenType::Reserved},
            {"repeat", TokenType::Reserved},
            {"until", TokenType::Reserved},
            {"unless", TokenType::Reserved},
            {"when", TokenType::Reserved},
            {"where", TokenType::Reserved},
            {"try", TokenType::Reserved},
            {"catch", TokenType::Reserved},
            {"raise", TokenType::Reserved},
            {"except", TokenType::Reserved},
            {"finally", TokenType::Reserved},
            // no public needed, members will be public by default unless marked private
            {"public", TokenType::Reserved},
            {"private", TokenType::Reserved},
            {"protect", TokenType::Reserved},
            {"pub", TokenType::Reserved},
            {"priv", TokenType::Reserved},
            {"prot", TokenType::Reserved},
            // module stuff
            {"import", TokenType::Reserved},
            {"export", TokenType::Reserved},
            {"module", TokenType::Reserved},
            {"library", TokenType::Reserved},
            {"package", TokenType::Reserved},
            {"lib", TokenType::Reserved},
            // operators
            {"and", TokenType::Reserved},
            {"or", TokenType::Reserved},
            // other
            {"void", TokenType::Reserved},
            {"this", TokenType::Reserved},
            {"of", TokenType::Reserved},
            {"esc", TokenType::Reserved},
            {"do", TokenType::Reserved},
            {"goto", TokenType::Reserved},
            {"enum", TokenType::Reserved},
            {"switch", TokenType::Reserved},
            {"case", TokenType::Reserved},
            {"defer", TokenType::Reserved},
            {"yield", TokenType::Reserved},
            {"impli", TokenType::Reserved}, // implicit type constructing
            {"expli", TokenType::Reserved},
            {"async", TokenType::Reserved},
            {"await", TokenType::Reserved},
            {"default", TokenType::Reserved},
            {"delete", TokenType::Reserved},
            {"is", TokenType::Reserved},
            {"from", TokenType::Reserved},
            {"get", TokenType::Reserved},
            {"set", TokenType::Reserved},
        };

        auto It = Keywords.find(Text);
        TokenType Type = (It != Keywords.end()) ? It->second : TokenType::Identifier;
        return {Token(Type, Text, Location)};
    }

    std::vector<Token> ReadStringLiteral()
    {
        std::vector<Token> Result;

        const char QuoteChar = Peek();
        Advance();

        size_t Start = Position;
        std::string Text;
        while (Peek() != QuoteChar)
        {
            if (Peek() == '\0')
                throw std::runtime_error("Unterminated string literal");

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
                case '0':
                    Text += '\0';
                    break;
                case '\n':
                    break;
                case '{':
                    Text += '{';
                    break;
                default:
                    throw std::runtime_error("Invalid escape character");
                }

                Advance();
                continue;
            }

            if (Peek() == '{')
            {
                Result.push_back(Token(TokenType::StringLiteral, Text, Location));
                Text.clear();
                Result.push_back(Token(TokenType::Plus, "+", Location));
                Result.push_back(Token(TokenType::LParen, "(", Location));
                Advance();
                size_t Count = 0;
                size_t BraceDepth = 1;
                while (BraceDepth > 0)
                {
                    if (IsAtEnd())
                    {
                        throw std::runtime_error("No closing '}' in interpoliated string");
                    }
                    
                    if (Peek() == '{')
                    {
                        BraceDepth++;
                        continue;
                    }
                    if (Peek() == '}')
                    {
                        BraceDepth--;
                        continue;
                    }
                    std::vector<Token> Tokens = ReadToken();
                    Count += Tokens.size();
                    for (auto &Tok : Tokens)
                        Result.push_back(Tok);
                }
                if (Count <= 0)
                    Result.push_back(Token(TokenType::StringLiteral, "", Location));
                Result.push_back(Token(TokenType::RParen, ")", Location));
                Result.push_back(Token(TokenType::Plus, "+", Location));

                Advance();
                continue;
            }

            Text += Peek();
            Advance();
        }

        Result.push_back(Token(TokenType::StringLiteral, Text, Location));
        Advance();

        if (Result.size() > 1)
        {
            Result.insert(Result.begin(), Token(TokenType::LParen, "(", Location));
            Result.push_back(Token(TokenType::RParen, ")", Location));
        }

        // for (auto &Tok : Result)
        // {
        //     if (Tok.Type == TokenType::StringLiteral)
        //         std::cout << '"' << Tok.Text << '"';
        //     else
        //         std::cout << Tok.Text;
        // }
        // std::cout << std::endl;
        
        return Result;
    }

    Token ReadNumber()
    {
        size_t Start = Position;

        while (std::isdigit(Peek()) || (Peek() == '\'' && std::isdigit(Peek(1))))
            Advance();

        if (Peek() == '.' && std::isdigit(Peek(1)))
        {
            Advance(); // consume '.'

            while (std::isdigit(Peek()))
                Advance();
        }
        
        std::string Text = Source.substr(Start, Position - Start);
        Text.erase(std::remove(Text.begin(), Text.end(), '\''), Text.end());

        return Token(TokenType::Number, Text, Location);
    }

    bool IsAtEnd() const
    {
        return Position >= Source.size();
    }
};

