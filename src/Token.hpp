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
    Of,
    With,
    Var,
    Use,
    Library,
    Class,
    Break,

    // Types
    IntType,
    FloatType,
    BoolType,
    StringType,
    AnyType,

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
    Hash,
    SemiColon,
    QuestionMark,
    Plus,
    Minus,
    Star,
    Slash,
    Caret,
    RArrowThick,
    RArrowThin,
    RArrowWavy,
    Tilde,
    Pipe,
    Ampersand,

    // Brackets
    LParen,
    RParen,
    LBrace,
    RBrace,
    LBracket,
    RBracket,
    LAngle,
    RAngle,

    // Misc
    Eof,
};

struct ScriptLocation
{
    std::string File;
    int Line = 1;
    int Column = 1;

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
