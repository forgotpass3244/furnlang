#pragma once
#include "Common.hpp"

enum class TokenType
{
    // Keywords
    Function,
    If,
    Else,
    Do,
    End,
    For,
    In,
    Var,
    Use,
    Library,
    Object,

    // Types
    IntType,
    FloatType,
    BoolType,
    StringType,

    // Identifiers and literals
    Identifier,
    Number,
    StringLiteral,
    Null,
    True,
    False,

    // Operators and punctuation
    Colon,
    DoubleColon, // 'Accessor' Std::Terminal::WriteLn(x)
    Equals,
    DoubleEquals,
    ExclamationEquals,
    Exclamation,
    Dot,
    DotDotDot,
    Comma,
    DollarSign,
    SemiColon,
    QuestionMark,
    Plus,
    Minus,
    Star,
    Slash,
    Caret,
    RArrowThick,
    RArrowThin,

    // Brackets
    LParen,
    RParen,
    LBrace,
    RBrace,
    LBracket,
    RBracket,

    // Misc
    Eof,
};

struct ScriptLocation
{
    std::string File;
    int Line;

    ScriptLocation(std::string file, int line = 1)
        : File(std::move(file)), Line(line) {}

    ScriptLocation()
        : File(""), Line(1) {}
};

struct Token
{
    TokenType Type;
    std::string Text;
    ScriptLocation Location;

    Token(TokenType type, std::string text, ScriptLocation location = ScriptLocation("?", -1))
        : Type(type), Text(std::move(text)), Location(location) {}
};