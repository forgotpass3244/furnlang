#pragma once
#include "Ast.hpp"
#include "Common.hpp"

std::string TokenTypeString(TokenType Type)
{
    return std::string(magic_enum::enum_name(Type));
}

bool CheckTypeMatch(ValueType Expected, ValueType ValueType)
{
    return ValueType == Expected;
}

class Parser
{
public:
    explicit Parser(std::vector<Token> &tokens) : Tokens(tokens), Position(0) {}

    std::vector<StatementPtr> ParseProgram()
    {
        std::vector<StatementPtr> Statements;

        while (!IsAtEnd())
        {
            Statements.push_back(ParseStatement());
        }

        return Statements;
    }

    std::vector<Token> &Tokens;

private:
    size_t Position;

    const Token &Advance()
    {
        if (!IsAtEnd())
            Position++;
        return Previous();
    }

    const Token &Peek(const int Offset = 0) const { return Tokens[Position + Offset]; }
    const Token &Previous() const { return Tokens[Position - 1]; }
    void GoBack() { Position = -1; }
    bool IsAtEnd() const { return Peek().Type == TokenType::Eof; }

    bool Match(TokenType Type)
    {
        if (Check(Type))
        {
            Advance();
            return true;
        }
        return false;
    }

    bool Check(TokenType Type) const
    {
        return !IsAtEnd() && Peek().Type == Type;
    }

    const Token &PeekNext() const
    {
        if (Position + 1 < Tokens.size())
            return Tokens[Position + 1];
        static Token eofToken(TokenType::Eof, "", 0);
        return eofToken;
    }

    ValueType GetType(const bool Forced = true, const bool ShouldAdvance = true, const bool GetMapType = false)
    {
        const size_t OriginalPosition = Position;

        ValueType Type = ValueType::Unknown;
        ValueType MapValType = ValueType::Unknown;

        if (Match(TokenType::IntType))
            Type = ValueType::Int;
        else if (Match(TokenType::FloatType))
            Type = ValueType::Float;
        else if (Match(TokenType::BoolType))
            Type = ValueType::Bool;
        else if (Match(TokenType::StringType))
            Type = ValueType::String;
        else if (Match(TokenType::Function))
            Type = ValueType::Function;
        else if (Match(TokenType::LBrace))
        {
            Type = ValueType::Map;

            if (!Match(TokenType::DotDotDot))
                MapValType = GetType();

            Expect(TokenType::RBrace);
        }
        else if (Forced)
            Throw("Expected type name");

        if (!ShouldAdvance)
            Position = OriginalPosition;

        if (GetMapType)
            return MapValType;
        else
            return Type;
    }

    StatementPtr ParseStatement()
    {
        // Macros
        if (Match(TokenType::DollarSign))
        {
            std::string PreprocessType = Expect(TokenType::Identifier).Text;

            if (PreprocessType == "macro")
            {
                std::string MacroName = Expect(TokenType::Identifier).Text;
                Expect(TokenType::Equals);

                std::vector<Token> MacroTokens;
                while (!Match(TokenType::SemiColon))
                {
                    MacroTokens.push_back(Peek());
                    Advance();
                }

                size_t OriginalPosition = Position;
                while (!IsAtEnd())
                {
                    Token Current = Peek();
                    if (Check(TokenType::Identifier) && Peek().Text == MacroName)
                    {
                        Advance();
                        Tokens.insert(Tokens.begin() + Position, MacroTokens.begin(), MacroTokens.end());
                        Tokens.erase(Tokens.begin() + Position - 1);
                        Position += MacroTokens.size();
                        continue;
                    }
                    Advance();
                }

                Position = OriginalPosition;
                return std::shared_ptr<EmptyStatement>();
            }
            else
                Throw("Invalid preprocess type");

            for (const auto &token : Tokens)
                std::cout << "Token: '" << token.Text << "' Line: " << token.Line << "\n";
        }

        if (Check(TokenType::IntType))
            return ParseVarDeclaration();
        if (Check(TokenType::FloatType))
            return ParseVarDeclaration();
        if (Check(TokenType::BoolType))
            return ParseVarDeclaration();
        if (Check(TokenType::StringType))
            return ParseVarDeclaration();

        if (Check(TokenType::Function))
        {
            if (Peek(2).Type == TokenType::Equals)
                return ParseVarDeclaration();
            else
            {
                Advance();
                return ParseFunctionDefinition();
            }
        }
        if (Check(TokenType::LBrace))
            return ParseVarDeclaration();

        if (Match(TokenType::If))
            return ParseIfStatement();
        if (Match(TokenType::For))
            return ParseForStatement();

        if (Check(TokenType::Library) && PeekNext().Type == TokenType::Identifier)
        {
            Advance();
            return ParseLibraryStatement();
        }

        // If current token is Identifier and next is '=' then parse assignment
        if (Check(TokenType::Identifier) && PeekNext().Type == TokenType::Equals)
        {
            std::string varName = Expect(TokenType::Identifier, "Expected variable name").Text;
            Expect(TokenType::Equals, "Expected '=' after variable name");
            ExpressionPtr value = ParseExpression();
            return std::make_shared<AssignmentStatement>(varName, value);
        }

        // Otherwise parse as expression statement (like function call)
        ExpressionPtr expr = ParseExpression();
        return std::make_shared<ExpressionStatement>(expr);
    }

    StatementPtr ParseVarDeclaration()
    {
        ValueType Type;
        ValueType MapValType = ValueType::Unknown;

        size_t OriginalPosition = Position;
        Type = GetType(true, false);
        MapValType = GetType(true, true, true);

        std::string Name = Expect(TokenType::Identifier, "Expected variable name").Text;
        if (Match(TokenType::Equals))
        {
            ExpressionPtr Init = ParseExpression();

            // Handle implicit map value type
            if (auto Map = std::dynamic_pointer_cast<MapExpression>(Init))
            {
                if (Map->ValType == ValueType::Unknown)
                    Map->ValType = MapValType;
            }

            return std::make_shared<VarDeclaration>(Init, Name, Type);
        }
        else
            return std::make_shared<VarDeclaration>(std::make_shared<NullExpression>(), Name, Type);
    }

    StatementPtr ParseFunctionDefinition()
    {
        std::string Name = Expect(TokenType::Identifier, "Expected function name").Text;

        // Accept optional ()
        if (Match(TokenType::LParen))
        {
            Expect(TokenType::RParen, "Expected ')' after '(' in function declaration");
        }

        std::vector<StatementPtr> Body;
        while (!Check(TokenType::End))
        {
            Body.push_back(ParseStatement());
        }
        Expect(TokenType::End, "Expected 'end'");
        return std::make_shared<VarDeclaration>(std::make_shared<FunctionDefinition>(Body), Name, ValueType::Function);
    }

    StatementPtr ParseLibraryStatement()
    {
        std::string Name = Expect(TokenType::Identifier).Text;

        std::vector<StatementPtr> Definition;
        while (!Check(TokenType::End))
        {
            StatementPtr Stmt = ParseStatement();
            if (auto Decl = std::dynamic_pointer_cast<VarDeclaration>(Stmt))
                Definition.push_back(Stmt);
            else
                Throw("Library definition failed");
        }
        Expect(TokenType::End);
        return std::make_shared<VarDeclaration>(std::make_shared<LibraryDefinition>(Definition), Name, ValueType::Library);
    }

    StatementPtr ParseIfStatement()
    {
        ExpressionPtr Condition = ParseExpression();
        Expect(TokenType::Do);

        std::vector<StatementPtr> Then;
        while (!Check(TokenType::End))
        {
            Then.push_back(ParseStatement());
        }

        Expect(TokenType::End);
        return std::make_shared<IfStatement>(Condition, Then);
    }

    StatementPtr ParseForStatement()
    {
        // for int Key: Item in Iter do ... esc

        ValueType KeyType = GetType(false);
        if (CheckTypeMatch(ValueType::Unknown, KeyType))
            KeyType = ValueType::Int;

        std::string KeyName = Expect(TokenType::Identifier).Text;
        Expect(TokenType::Colon);
        std::string ValName = Expect(TokenType::Identifier).Text;

        Expect(TokenType::In);

        ExpressionPtr Iter = ParseExpression();

        Expect(TokenType::Do);

        std::vector<StatementPtr> Body;
        while (!Check(TokenType::End))
            Body.push_back(ParseStatement());

        Expect(TokenType::End);
        return std::make_shared<ForStatement>(Body, Iter, KeyName, ValName, KeyType);
    }

    ExpressionPtr ParseExpression()
    {
        ExpressionPtr Expr = ParsePrimary();

        while (true)
        {
            if (Match(TokenType::LParen))
            {
                std::vector<ExpressionPtr> Args;
                if (!Check(TokenType::RParen))
                {
                    do
                    {
                        Args.push_back(ParseExpression());
                    } while (Match(TokenType::Comma));
                }
                Expect(TokenType::RParen);
                Expr = std::make_shared<CallExpression>(Expr, Args);
            }
            else if (Match(TokenType::LBracket))
            {
                ExpressionPtr IndexExpr = ParseExpression();
                Expect(TokenType::RBracket);
                Expr = std::make_shared<IndexExpression>(Expr, IndexExpr);
            }
            else if (Match(TokenType::Dot))
            {
                Token Ident = Expect(TokenType::Identifier);
                Expr = std::make_shared<IndexExpression>(Expr, std::make_shared<StringExpression>(Ident.Text));
            }
            else if (Match(TokenType::DoubleColon))
            {
                Token Ident = Expect(TokenType::Identifier);
                Expr = std::make_shared<AccessExpression>(Expr, Ident.Text);
            }
            else
                break;
        }

        if (Match(TokenType::DoubleEquals))
        {
            ExpressionPtr Right = ParsePrimary();
            return std::make_shared<BinaryExpression>("==", Expr, Right);
        }

        return Expr;
    }

    ExpressionPtr ParsePrimary()
    {
        if (Match(TokenType::LParen))
        {
            ExpressionPtr expr = ParseExpression();
            Expect(TokenType::RParen, "Expected ')' after expression");
            return expr;
        }

        if (Match(TokenType::Library))
            return std::make_shared<VariableExpression>("lib");

        // Map
        if (Match(TokenType::LBrace))
        {
            ValueType ValType = ValueType::Unknown;

            if (Match(TokenType::LParen))
            {
                ValType = GetType();
                Expect(TokenType::RParen);
            }

            std::unordered_map<ExpressionPtr, ExpressionPtr> KV_Expressions;

            while (!Match(TokenType::RBrace))
            {
                Expect(TokenType::LBracket);
                ExpressionPtr KeyExpr = ParseExpression();
                Expect(TokenType::RBracket);
                Expect(TokenType::Colon);
                ExpressionPtr ValExpr = ParseExpression();

                // Comma not required on the last key
                if (!Check(TokenType::RBrace))
                    Expect(TokenType::Comma);

                KV_Expressions[KeyExpr] = ValExpr;
            }

            return std::make_shared<MapExpression>(KV_Expressions, ValType);
        }

        if (Match(TokenType::Identifier))
            return std::make_shared<VariableExpression>(Previous().Text);
        if (Match(TokenType::Number))
            return std::make_shared<NumberExpression>(std::stoi(Previous().Text));
        if (Match(TokenType::String))
            return std::make_shared<StringExpression>(Previous().Text);
        if (Match(TokenType::Null))
            return std::make_shared<NullExpression>();
        if (Match(TokenType::True))
            return std::make_shared<BooleanExpression>(true);
        if (Match(TokenType::False))
            return std::make_shared<BooleanExpression>(false);

        throw std::runtime_error("Expected expression at line " + std::to_string(Peek().Line));
    }

    const Token &Expect(TokenType Type, const std::string &Message = "")
    {
        if (Match(Type))
            return Previous();
        throw std::runtime_error(std::to_string(Peek().Line) + ": Expected " + TokenTypeString(Type));
    }

    void Throw(const std::string &Message)
    {
        throw std::runtime_error(std::to_string(Peek().Line) + ": " + Message);
    }
};