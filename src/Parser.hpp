#pragma once
#include "Ast.hpp"
#include "Common.hpp"

using rt_Int = long;
using rt_Float = double;

std::string TokenTypeString(TokenType Type)
{
    return std::string(magic_enum::enum_name(Type));
}

bool CheckTypeMatch(ValueType Expected, ValueType ValueType)
{
    if (
        (Expected == ValueType::Dynamic && ValueType == ValueType::Unknown)
        || (Expected == ValueType::Unknown && ValueType == ValueType::Dynamic)
    )
        return false;

    if (Expected == ValueType::Dynamic || ValueType == ValueType::Dynamic)
        return true;

    return ValueType == Expected;
}

std::string LocationStr(const ScriptLocation &Location)
{
    return "in file '" + Location.File + "'\n(Line " + std::to_string(Location.Line) + ')';
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
            const size_t OriginalPosition = Position;
            StatementPtr Stmt = ParseStatement();

            if (std::dynamic_pointer_cast<VarDeclaration>(Stmt) || std::dynamic_pointer_cast<UseStatement>(Stmt))
                Statements.push_back(Stmt);
            else if (Stmt)
            {
                Position = OriginalPosition;
                Throw("Expected variable declaration before main execution");
            }
        }

        return Statements;
    }

    std::vector<Token> &Tokens;

private:
    size_t Position;
    std::vector<std::string> UserDefinedTypes;

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
        static Token eofToken(TokenType::Eof, "");
        return eofToken;
    }

    TypeDescriptor GetType(const bool ShouldAdvance = true)
    {
        const size_t OriginalPosition = Position;

        ValueType Type = ValueType::Unknown;
        std::vector<TypeDescriptor> Subtypes;

        if (Match(TokenType::Null))
            Type = ValueType::Null;
        else if (Match(TokenType::IntType))
            Type = ValueType::Int;
        else if (Match(TokenType::FloatType))
            Type = ValueType::Float;
        else if (Match(TokenType::BoolType))
            Type = ValueType::Bool;
        else if (Match(TokenType::StringType))
            Type = ValueType::String;
        else if (Match(TokenType::Function))
            Type = ValueType::Function;
        else if (Match(TokenType::Caret))
        {
            Expect(TokenType::Function);
            Type = ValueType::ExternalFunction;
        }
        else if (Match(TokenType::Library))
            Type = ValueType::Library;
        else if (Match(TokenType::LBrace))
        {
            Type = ValueType::Map;
            Subtypes.push_back(GetType());

            Expect(TokenType::RBrace);
        }
        else
        {
            Position = OriginalPosition;
            Throw("Expected type name");
        }

        if (!ShouldAdvance)
            Position = OriginalPosition;

        return TypeDescriptor(Type, Subtypes);
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
            }
            else if (PreprocessType == "include")
            {
                std::string FileName = Expect(TokenType::StringLiteral).Text;
                std::ifstream File(LocalDirectory / FileName);

                if (!File.is_open())
                    throw std::runtime_error("Failed to open: " + FileName + '\n');

                std::string Content((std::istreambuf_iterator<char>(File)),
                                 std::istreambuf_iterator<char>());

                Lexer Lex(Content);
                Lex.Location.File = (LocalDirectory / FileName).string();
                std::vector<Token> IncludedTokens = Lex.Tokenize();

                Tokens.insert(Tokens.begin() + Position, IncludedTokens.begin(), IncludedTokens.end() - 1);
            }
            else
                Throw("Invalid preprocess type");

            return nullptr;
        }

        if (Match(TokenType::Var))
            return ParseVarDeclaration();

        if (Match(TokenType::Function))
            return ParseFunctionDefinition();
        if (Match(TokenType::Library))
            return ParseLibraryStatement();
        if (Match(TokenType::Object))
            return ParseClassDefinition();

        if (Match(TokenType::If))
            return ParseIfStatement();
        if (Match(TokenType::For))
            return ParseForStatement();

        if (Match(TokenType::RArrowThick))
            return std::make_shared<ReturnStatement>(ParseExpression());

        if (Match(TokenType::Use))
            return ParseUseStatement();

        // If current token is Identifier and next is '=' then parse assignment
        if (Check(TokenType::Identifier) && PeekNext().Type == TokenType::Equals)
        {
            std::string VarName = Expect(TokenType::Identifier).Text;
            Expect(TokenType::Equals);
            ExpressionPtr Val = ParseExpression();
            return std::make_shared<AssignmentStatement>(VarName, Val);
        }

        // userdefinedtype Hello = Hello("Hi")
        if (Check(TokenType::Identifier) && PeekNext().Type == TokenType::Identifier)
            ParseVarDeclaration();

        // Otherwise parse as expression statement (like function call)
        ExpressionPtr expr = ParseExpression();
        return std::make_shared<ExpressionStatement>(expr);
    }

    StatementPtr ParseVarDeclaration()
    {
        TypeDescriptor Type = GetType();
        std::string Name = Expect(TokenType::Identifier).Text;

        if (Match(TokenType::Equals))
        {
            ExpressionPtr Init = ParseExpression();
            return std::make_shared<VarDeclaration>(Init, Name, Type);
        }
        else
            return std::make_shared<VarDeclaration>(std::make_shared<ValueExpression>(nullptr), Name, Type);
    }

    StatementPtr ParseFunctionDefinition()
    {
        std::string Name = Expect(TokenType::Identifier).Text;

        Expect(TokenType::LParen);

        std::vector<VarDeclaration> Args;
        if (!Check(TokenType::RParen))
        {
            while (true)
            {
                StatementPtr Stmt = ParseVarDeclaration();

                if (auto Decl = std::dynamic_pointer_cast<VarDeclaration>(Stmt))
                    Args.push_back(*Decl);

                if (!Match(TokenType::Comma))
                    break;
            }
        }

        Expect(TokenType::RParen);

        TypeDescriptor ReturnType = TypeDescriptor(ValueType::Null);

        if (!Check(TokenType::RArrowThick))
        {
            if (!Check(TokenType::Do))
                ReturnType = GetType();
            Expect(TokenType::Do);
        }

        std::vector<StatementPtr> Body;

        if (!Check(TokenType::RArrowThick))
        {
            while (!Check(TokenType::End))
                Body.push_back(ParseStatement());
            Expect(TokenType::End);
        }
        else
            Body.push_back(ParseStatement());

        return std::make_shared<VarDeclaration>(std::make_shared<FunctionDefinition>(Body, Args), Name, TypeDescriptor(ValueType::Function));
    }

    StatementPtr ParseLibraryStatement()
    {
        std::string Name = Expect(TokenType::Identifier).Text;

        std::vector<StatementPtr> Definition;
        while (!Check(TokenType::End))
        {
            StatementPtr Stmt;
            if (Match(TokenType::Function))
                Stmt = ParseFunctionDefinition();
            else if (Match(TokenType::Library))
                Stmt = ParseLibraryStatement();
            else
                Stmt = ParseVarDeclaration();
            Definition.push_back(Stmt);
        }
        Expect(TokenType::End);
        return std::make_shared<VarDeclaration>(std::make_shared<LibraryDefinition>(Definition), Name, TypeDescriptor(ValueType::Library));
    }

    StatementPtr ParseClassDefinition()
    {
        Expect(TokenType::LParen);

        std::vector<VarDeclaration> Members;
        if (!Check(TokenType::RParen))
        {
            while (true)
            {
                const size_t OriginalPosition = Position;
                StatementPtr Stmt = ParseStatement();

                if (auto Decl = std::dynamic_pointer_cast<VarDeclaration>(Stmt))
                    Members.push_back(*Decl);
                else
                {
                    Position = OriginalPosition;
                    Throw("Expected class member");
                }

                if (!Match(TokenType::Comma))
                    break;
            }
        }

        Expect(TokenType::RParen);

        Expect(TokenType::RArrowThick);
        std::string Name = Expect(TokenType::Identifier).Text;
        UserDefinedTypes.push_back(Name);

        return nullptr;
    }
    
    StatementPtr ParseIfStatement()
    {
        std::vector<ExpressionPtr> Conditions;
        Conditions.push_back(ParseExpression());
        Expect(TokenType::Do);

        std::vector<std::vector<StatementPtr>> Then;
        Then.emplace_back();

        size_t i = 0;
        while (!Check(TokenType::End))
        {
            if (Match(TokenType::Else))
            {
                ++i;
                if (Match(TokenType::Do))
                    Conditions.push_back(std::make_shared<ValueExpression>(true));
                else
                {
                    Conditions.push_back(ParseExpression());
                    Expect(TokenType::Do);
                }
                Then.emplace_back();
            }
            Then[i].push_back(ParseStatement());
        }

        Expect(TokenType::End);
        return std::make_shared<IfStatement>(Conditions, Then);
    }

    StatementPtr ParseForStatement()
    {
        size_t OriginalPosition = Position;

        try
        {
            // for int Key: string Item in Iter do ... esc

            TypeDescriptor KeyType = GetType();
            std::string KeyName = Expect(TokenType::Identifier).Text;
            Expect(TokenType::Colon);

            TypeDescriptor ValType = GetType();
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
        catch(const std::runtime_error& e)
        {
            // for condition do ... esc (while loop)

            Position = OriginalPosition;
            
            ExpressionPtr Condition = ParseExpression();
            Expect(TokenType::Do);

            std::vector<StatementPtr> Body;
            while (!Check(TokenType::End))
                Body.push_back(ParseStatement());

            Expect(TokenType::End);
            return std::make_shared<WhileStatement>(Body, Condition);
        }
    }

    StatementPtr ParseUseStatement()
    {
        const bool UseLibrary = Match(TokenType::Library);
        ExpressionPtr Expr = ParseExpression();

        return std::make_shared<UseStatement>(Expr, UseLibrary);
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
                Expr = std::make_shared<IndexExpression>(Expr, std::make_shared<ValueExpression>(Ident.Text));
            }
            else if (Match(TokenType::DoubleColon))
            {
                Token Ident = Expect(TokenType::Identifier);
                Expr = std::make_shared<AccessExpression>(Expr, Ident.Text);
            }
            else
                break;
        }

        if (Match(TokenType::Plus))
        {
            ExpressionPtr B = ParsePrimary();
            return std::make_shared<BinaryExpression>(OperationType::Add, Expr, B);
        }
        else if (Match(TokenType::Minus))
        {
            ExpressionPtr B = ParsePrimary();
            return std::make_shared<BinaryExpression>(OperationType::Subtract, Expr, B);
        }
        else if (Match(TokenType::Star))
        {
            ExpressionPtr B = ParsePrimary();
            return std::make_shared<BinaryExpression>(OperationType::Multiply, Expr, B);
        }
        else if (Match(TokenType::Slash))
        {
            ExpressionPtr B = ParsePrimary();
            return std::make_shared<BinaryExpression>(OperationType::Divide, Expr, B);
        }
        else if (Match(TokenType::DoubleEquals))
        {
            ExpressionPtr B = ParsePrimary();
            return std::make_shared<BinaryExpression>(OperationType::Equality, Expr, B);
        }
        else if (Match(TokenType::ExclamationEquals))
        {
            ExpressionPtr B = ParsePrimary();
            return std::make_shared<BinaryExpression>(OperationType::Inequality, Expr, B);
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

        // Map
        if (Match(TokenType::LBrace))
        {
            Expect(TokenType::LParen);
            TypeDescriptor ValType = GetType();
            Expect(TokenType::RParen);

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
        {
            if (Previous().Text.find('.') != std::string::npos)
                return std::make_shared<ValueExpression>(rt_Float(std::stod(Previous().Text)));
            else
                return std::make_shared<ValueExpression>(rt_Int(std::stol(Previous().Text)));
        }
        if (Match(TokenType::StringLiteral))
            return std::make_shared<ValueExpression>(Previous().Text);
        if (Match(TokenType::Null))
            return std::make_shared<ValueExpression>(nullptr);
        if (Match(TokenType::True))
            return std::make_shared<ValueExpression>(true);
        if (Match(TokenType::False))
            return std::make_shared<ValueExpression>(false);

        throw std::runtime_error("Expected expression " + LocationStr(Peek().Location));
    }

    const Token &Expect(TokenType Type, const std::string &Message = "")
    {
        if (Match(Type))
            return Previous();
        throw std::runtime_error(LocationStr(Peek().Location) + ": Expected " + TokenTypeString(Type) + ", instead got " + TokenTypeString(Peek().Type));
    }

    void Throw(const std::string &Message)
    {
        throw std::runtime_error(LocationStr(Peek().Location) + ": " + Message);
    }
};