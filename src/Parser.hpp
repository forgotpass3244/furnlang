#pragma once
#include "Ast.hpp"
#include "Common.hpp"

#define ret return nullptr;

using rt_Int = long long;
using rt_Float = double;

std::string TokenTypeString(TokenType Type)
{
    return std::string(magic_enum::enum_name(Type));
}

bool CheckTypeMatch(ValueType Expected, ValueType ValueType)
{
    if (
        (Expected == ValueType::Dynamic && ValueType == ValueType::Unknown) || (Expected == ValueType::Unknown && ValueType == ValueType::Dynamic))
        return false;

    if (Expected == ValueType::Dynamic || ValueType == ValueType::Dynamic)
        return true;

    return ValueType == Expected;
}

std::string LocationStr(const ScriptLocation &Location)
{
    return "in file '" + Location.File + "'\n(Line " + std::to_string(Location.Line) + ", Col " + std::to_string(Location.Column) + ')';
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

            if (!Stmt)
                continue;

            if (std::dynamic_pointer_cast<VarDeclaration>(Stmt) || std::dynamic_pointer_cast<UseStatement>(Stmt))
                Statements.push_back(Stmt);
            else if (Stmt)
            {
                Throw("Expected variable declaration before main execution", false);
            }
        }

        return Statements;
    }

    bool IsAtEnd() const { return Peek().Type == TokenType::Eof; }

    std::vector<Token> &Tokens;
    std::vector<std::string> Errors;
    std::vector<std::string> MacroNames;

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

    TypeDescriptor ParseType()
    {
        ValueType Type = ValueType::Unknown;
        std::string CustomTypeName;
        std::vector<TypeDescriptor> Subtypes;

        if (Match(TokenType::AnyType))
            Type = ValueType::Any;
        else if (Match(TokenType::Null))
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
            Expect(TokenType::RBrace);
            Type = ValueType::Map;
        }
        else if (Match(TokenType::Identifier))
        {
            Type = ValueType::Custom;
            CustomTypeName = Previous().Text;
        }
        else
        {
            Throw("Expected type name");
            return TypeDescriptor();
        }

        if (Match(TokenType::Of))
        {
            // {} of int
            // {} of <int>
            // {} of <int, float>

            if (Match(TokenType::LAngle))
            {
                do
                {
                    Subtypes.push_back(ParseType());
                } while (Match(TokenType::Comma));

                Expect(TokenType::RAngle);
            }
            else
                Subtypes.push_back(ParseType());
        }
        else if (CheckTypeMatch(ValueType::Function, Type))
            Subtypes.push_back(TypeDescriptor(ValueType::Null));
        else if (CheckTypeMatch(ValueType::Map, Type))
            Expect(TokenType::Of);

        return TypeDescriptor(Type, Subtypes, CustomTypeName);
    }

    StatementPtr ParseStatement()
    {
        // Macros
        if (Match(TokenType::Hash))
        {
            std::string PreprocessType = Expect(TokenType::Identifier).Text;

            if (PreprocessType == "macro")
            {
                std::string MacroName = Expect(TokenType::Identifier).Text;
                MacroNames.push_back(MacroName);

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
                    Throw("Failed to open: " + FileName + '\n');

                std::string Content((std::istreambuf_iterator<char>(File)),
                                    std::istreambuf_iterator<char>());

                Lexer Lex(Content);
                Lex.Location.File = (LocalDirectory / FileName).string();
                std::vector<Token> IncludedTokens = Lex.Tokenize();

                Tokens.insert(Tokens.begin() + Position, IncludedTokens.begin(), IncludedTokens.end() - 1);
            }
            else
            {
                Throw("Invalid preprocess type");
                ret
            }

            ret
        }

        if (Match(TokenType::Var))
            return ParseVarDeclaration();

        if (Match(TokenType::Function))
            return ParseFunctionDefinition();
        if (Match(TokenType::Library))
            return ParseLibraryStatement();
        if (Match(TokenType::Class))
            return ParseClassDefinition();

        if (Match(TokenType::If))
            return ParseIfStatement();
        if (Match(TokenType::For))
            return ParseForStatement();
        if (Match(TokenType::Do))
        {
            Expect(TokenType::Colon);
            return ParseReceiverStatement();
        }

        if (Match(TokenType::RArrowThick))
            return std::make_shared<ReturnStatement>(ParseExpression());
        if (Match(TokenType::RArrowWavy))
            return std::make_shared<SignalStatement>(ParseExpression());
        if (Match(TokenType::Break))
            return std::make_shared<BreakStatement>();

        if (Match(TokenType::Use))
            return ParseUseStatement();

        // Otherwise parse as expression statement (like function call)
        ExpressionPtr expr = ParseExpression();
        return std::make_shared<ExpressionStatement>(expr);
    }

    StatementPtr ParseVarDeclaration()
    {
        TypeDescriptor Type = ParseType();
        std::string Name = Expect(TokenType::Identifier).Text;

        if (Match(TokenType::Equals))
        {
            ExpressionPtr Init = ParseExpression();
            return std::make_shared<VarDeclaration>(Init, Name, Type);
        }
        else if (Match(TokenType::LParen))
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
            ExpressionPtr Expr = std::make_shared<UseExpression>(Type, Args);
            
            return std::make_shared<VarDeclaration>(Expr, Name, Type);
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

        if (!Check(TokenType::RArrowThick) && !Check(TokenType::Colon))
            ReturnType = ParseType();

        std::vector<StatementPtr> Body;

        if (!Check(TokenType::RArrowThick))
        {
            Expect(TokenType::Colon);

            while (!Match(TokenType::End))
                Body.push_back(ParseStatement());
        }
        else
            Body.push_back(ParseStatement());

        return std::make_shared<VarDeclaration>(std::make_shared<FunctionDefinition>(Body, Args, ReturnType), Name, TypeDescriptor(ValueType::Function, {ReturnType}));
    }

    StatementPtr ParseLibraryStatement()
    {
        std::string Name = Expect(TokenType::Identifier).Text;
        Expect(TokenType::Colon);

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
        std::string Name = Expect(TokenType::Identifier).Text;
        Expect(TokenType::Colon);

        std::vector<VarDeclaration> Members;
        while (!Match(TokenType::End))
        {
            StatementPtr Stmt;
            if (Match(TokenType::Function))
                Stmt = ParseFunctionDefinition();
            else
                Stmt = ParseVarDeclaration();

            if (auto Decl = std::dynamic_pointer_cast<VarDeclaration>(Stmt))
                Members.push_back(*Decl);
        }

        return std::make_shared<VarDeclaration>(std::make_shared<ClassBlueprint>(Name, Members), Name, TypeDescriptor(ValueType::Unknown));
    }

    StatementPtr ParseReceiverStatement()
    {
        std::vector<std::pair<TypeDescriptor, std::string>> ReceiveTypes; // Type and name
        std::vector<std::vector<StatementPtr>> With;
        With.emplace_back();

        size_t i = 0;
        while (!Match(TokenType::End))
        {
            if (Match(TokenType::With))
            {
                ++i;
                TypeDescriptor Type = ParseType();
                ReceiveTypes.push_back(std::pair<TypeDescriptor, std::string>(Type, Expect(TokenType::Identifier).Text));
                Expect(TokenType::Colon);
                With.emplace_back();
            }
            With[i].push_back(ParseStatement());
        }

        return std::make_shared<ReceiverStatement>(ReceiveTypes, With);
    }

    StatementPtr ParseIfStatement()
    {
        std::vector<ExpressionPtr> Conditions;
        Conditions.push_back(ParseExpression());
        Expect(TokenType::Colon);

        std::vector<std::vector<StatementPtr>> Then;
        Then.emplace_back();

        size_t i = 0;
        while (!Check(TokenType::End))
        {
            if (Match(TokenType::Else))
            {
                ++i;
                if (Match(TokenType::Colon))
                    Conditions.push_back(std::make_shared<ValueExpression>(true));
                else
                {
                    Conditions.push_back(ParseExpression());
                    Expect(TokenType::Colon);
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
        // for condition: ... esc
        // for 5, int i: ... esc
        // for Iter, int k, string v: ... esc
        // now its easy to tell the difference
        // between a while and for statement
        
        ExpressionPtr Expr = ParseExpression();
        if (!Match(TokenType::Comma))
        {
            Expect(TokenType::Colon);

            std::vector<StatementPtr> Body;
            while (!Check(TokenType::End))
                Body.push_back(ParseStatement());

            Expect(TokenType::End);

            return std::make_shared<WhileStatement>(Body, Expr);
        }
        else
        {
            TypeDescriptor KeyType = ParseType();
            std::string KeyName = Expect(TokenType::Identifier).Text;

            if (!Match(TokenType::Comma))
            {
                Expect(TokenType::Colon);

                std::vector<StatementPtr> Body;
                while (!Check(TokenType::End))
                    Body.push_back(ParseStatement());

                Expect(TokenType::End);
                return std::make_shared<ForStatement>(Body, Expr, KeyName, KeyType, "", TypeDescriptor(ValueType::Null));
            }
            else
            {
                TypeDescriptor ValType = ParseType();
                std::string ValName = Expect(TokenType::Identifier).Text;

                Expect(TokenType::Colon);

                std::vector<StatementPtr> Body;
                while (!Check(TokenType::End))
                    Body.push_back(ParseStatement());

                Expect(TokenType::End);
                return std::make_shared<ForStatement>(Body, Expr, KeyName, KeyType, ValName, ValType);
            }
        }
    }

    StatementPtr ParseUseStatement()
    {
        const bool UseLibrary = Match(TokenType::Library);
        ExpressionPtr Expr = ParseExpression();

        return std::make_shared<UseStatement>(Expr, UseLibrary);
    }

    int GetPrecedence(TokenType type)
    {
        switch (type)
        {
            case TokenType::Star:
            case TokenType::Slash:
                return 6;   // highest

            case TokenType::Plus:
            case TokenType::Minus:
                return 5;

            case TokenType::LAngle:
            case TokenType::RAngle:
                return 4;

            case TokenType::DoubleEquals:
            case TokenType::ExclamationEquals:
                return 3;

            case TokenType::Ampersand:
                return 2;

            case TokenType::Pipe:
                return 1;   // lowest

            case TokenType::Equals:
                return 0;  // assignment (often right-associative)

            default:
                return -99; // not a binary operator
        }
    }

    OperationType MapOperator(Token Tok)
    {
        switch (Tok.Type)
        {
            case TokenType::Plus: return OperationType::Add;
            case TokenType::Minus: return OperationType::Subtract;
            case TokenType::Star: return OperationType::Multiply;
            case TokenType::Slash: return OperationType::Divide;
            case TokenType::RAngle: return OperationType::GreaterThan;
            case TokenType::LAngle: return OperationType::LessThan;
            case TokenType::DoubleEquals: return OperationType::Equality;
            case TokenType::ExclamationEquals: return OperationType::Inequality;
            case TokenType::Pipe: return OperationType::Or;
            case TokenType::Ampersand: return OperationType::And;
            default: throw std::runtime_error("Unknown operator");
        }
    }    

    ExpressionPtr ParseExpression(int MinPrecedence = 0)
    {
        ExpressionPtr Lhs = ParsePrimary();

        while (true)
        {
            TokenType OpType = Peek().Type;
            int Precedence = GetPrecedence(OpType);
    
            if (Precedence < MinPrecedence)
                break; // Operator has lower precedence so stop climbing
    
            Token Op = Advance(); // Consume operator
    
            // For right-associative operators like '=', don't +1
            int NextMinPrecedence = (OpType == TokenType::Equals) ? Precedence : Precedence + 1;
    
            ExpressionPtr Rhs = ParseExpression(NextMinPrecedence);

            if (Op.Type == TokenType::Equals)
            {
                Lhs = std::make_shared<AssignmentExpression>(Lhs, Rhs);
                continue;
            }
    
            Lhs = std::make_shared<BinaryExpression>(MapOperator(Op), Lhs, Rhs);
        }

        return Lhs;
    }

    ExpressionPtr ParsePrimary()
    {
        ExpressionPtr Expr = ParseSecondary();
        
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
                Expr = std::make_shared<MemberExpression>(Expr, Ident.Text);
            }
            else if (Match(TokenType::DoubleColon))
            {
                Token Ident = Expect(TokenType::Identifier);
                Expr = std::make_shared<AccessExpression>(Expr, Ident.Text);
            }
            else
                break;
        }
        
        return Expr;
    }

    ExpressionPtr ParseSecondary()
    {
        if (Match(TokenType::LParen))
        {
            ExpressionPtr Expr = ParseExpression();
            Expect(TokenType::RParen);
            return Expr;
        }

        if (Match(TokenType::Function))
        {
            // function literal
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

            if (!Check(TokenType::RArrowThick) && !Check(TokenType::Colon))
                ReturnType = ParseType();

            std::vector<StatementPtr> Body;

            if (!Check(TokenType::RArrowThick))
            {
                Expect(TokenType::Colon);

                while (!Match(TokenType::End))
                    Body.push_back(ParseStatement());
            }
            else
                Body.push_back(ParseStatement());

            return std::make_shared<FunctionDefinition>(Body, Args, ReturnType);
        }

        if (Match(TokenType::Use))
        {
            TypeDescriptor Type = ParseType();
            Expect(TokenType::LParen);

            std::vector<ExpressionPtr> Args;
            if (!Check(TokenType::RParen))
            {
                do
                {
                    Args.push_back(ParseExpression());
                } while (Match(TokenType::Comma));
            }

            Expect(TokenType::RParen);
            ExpressionPtr Expr = std::make_shared<UseExpression>(Type, Args);
            return Expr;
        }

        // Map
        if (Match(TokenType::LBrace))
        {
            Expect(TokenType::LAngle);
            TypeDescriptor ValType = ParseType();
            Expect(TokenType::RAngle);

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

        Throw("Expected expression ");
        ret
    }

    const Token &Expect(TokenType Type, const std::string &Message = "")
    {
        if (Match(Type))
            return Previous();
        Throw("Expected " + TokenTypeString(Type) + ", instead got '" + Peek().Text + "' (" + TokenTypeString(Peek().Type) + ')');
        return Previous();
    }

    void Panic()
    {
        while (!IsAtEnd())
        {
            if (Previous().Type == TokenType::SemiColon ||
                Previous().Type == TokenType::End || Check(TokenType::Eof))
                return;

            switch (Peek().Type)
            {
            case TokenType::Identifier:
            case TokenType::Var:
            case TokenType::Function:
            case TokenType::If:
            case TokenType::For:
            case TokenType::Use:
            case TokenType::Library:
                return; // start of a new statement
            }

            Advance();
        }
    }

    void Throw(const std::string &Message, const bool panic = true)
    {
        Errors.push_back(LocationStr(Peek().Location) + ": " + Message);
        // std::cout << LocationStr(Peek().Location) + ": " + Message << std::endl;
        if (panic)
        {
            Advance();
            Panic();
        }
    }
};
