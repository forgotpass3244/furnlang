#pragma once
#include "Common.hpp"

#define TT_NULL TokenType(-1)

enum class TokenType
{
    Reserved,

    // Keywords
    Function,
    If,
    Else,
    ElseIf,
    For,
    While,
    In,
    As,
    Of,
    With,
    New,
    Immutable,
    Mutable,
    Import,
    Package,
    Class,
    Break,
    Return,
    Raise,
    This,
    Export,

    // Types
    IntType,
    FloatType,
    BoolType,
    DoubleType,
    ShortType,
    LongType,
    CharacterType,

    // Identifiers and literals
    Identifier,
    Number,
    StringLiteral,
    Null,
    True,
    False,

    // Operators and punctuation
    SizeOf,
    Not,
    Colon,
    DoubleColon, // 'Accessor' Std::Terminal::WriteLn(x)
    ColonEquals,
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
    PlusPlus,
    MinusMinus,
    Star,
    Slash,
    Caret,
    At,
    RArrowThick,
    RArrowThin,
    RArrowWavy,
    Tilde,
    Pipe,
    DoublePipe,
    Ampersand,
    DoubleAmpersand,

    // Brackets
    LParen,
    RParen,
    LBrace,
    RBrace,
    LBracket,
    RBracket,
    LAngle,
    LAngleEqual,
    RAngle,
    RAngleEqual,

    // Misc
    Eof,
};

struct ScriptLocation
{
    std::filesystem::path File;
    size_t Line = 1;
    size_t Column = 1;

    ScriptLocation(std::string file, size_t line = 1)
        : File(std::move(file)), Line(line) {}

    ScriptLocation()
        : File(""), Line(1) {}

public:
    std::string ToString(const bool ShowFile = true)
    {
        if (!ShowFile)
            return "(Line " + std::to_string(Line) + ", Col " + std::to_string(Column) + ')';
        return "in file \x1b[36m'" + File.string() + "'\x1b[0m\n(Line " + std::to_string(Line) + ", Col " + std::to_string(Column) + ')';
    }
};

struct Token
{
    TokenType Type;
    std::string Text;
    ScriptLocation Location;
    bool IsCursor = false;

    Token(TokenType type, std::string text, ScriptLocation location = ScriptLocation("?", -1))
        : Type(type), Text(std::move(text)), Location(location) {}
};

bool operator!(Token Tok)
{
    return Tok.Type == TT_NULL;
}
