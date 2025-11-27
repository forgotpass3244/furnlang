#pragma once

class Parser;

#include "Ast.hpp"
#include "Common.hpp"
#include "Symbol.hpp"
#include "Error.hpp"
#include "GlobalParseLoc.hpp"

#define ret return nullptr;

std::string TokenTypeString(TokenType Type)
{
    return std::string(magic_enum::enum_name(Type));
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
            if (Check(TokenType::SemiColon))
                MatchTerminator(); // ignore leading semicolons

            if (IsAtEnd())
                break;
                
            StatementPtr Stmt = ParseStatement();

            if (!Stmt)
                continue;

            if (std::dynamic_pointer_cast<AssemblyInstructions>(Stmt) || std::dynamic_pointer_cast<VarDeclaration>(Stmt) || std::dynamic_pointer_cast<UseStatement>(Stmt))
                Statements.push_back(Stmt);
            else if (Stmt)
            {
                Statements.push_back(Stmt);
                Throw("Expected a declaration before main execution", false);
            }
        }

        return Statements;
    }

    bool IsAtEnd() const { return Peek().Type == TokenType::Eof; }

    std::vector<Token> &Tokens;
    std::vector<CompileError> Errors;
    std::vector<std::string> MacroNames;
    std::vector<std::string> ClassNames;
    std::unordered_map<std::string, MapId> ImportCache;

    bool REPL = false;

    size_t Position = 0;

public:
    const Token &Advance()
    {
        if (!IsAtEnd())
            Position++;
            
        CurrentParseToken = Previous();

        // while (!IsAtEnd() && Peek().Type == TokenType::Marker_Cursor)
        //     Position++;

        return Previous();
    }

    const Token EofToken = Token(TokenType::Eof, "", ScriptLocation("", -1));

    const Token &Peek(const int Offset = 0) const
    {
        if (Position + Offset >= Tokens.size())
            return EofToken;
        return Tokens[Position + Offset];
    }
    const Token &Previous() const {
        size_t Pos = Position - 1;
        if (Pos < 0)
            return Tokens[0];
        return Tokens[Pos];
    }

    bool Match(TokenType Type)
    {
        if (Check(Type))
        {
            Advance();
            return true;
        }
        return false;
    }

    bool Check(TokenType Type)
    {
        if (Type == TokenType::RBrace && IsAtEnd())
        {
            Throw("Unexpected EOF", false);
            return true;
        }
        if (IsAtEnd())
            return false;
        if (Peek().Type == TokenType::Reserved && Type == TokenType::Identifier)
            return true;
        return Peek().Type == Type;
    }

    const Token &PeekNext() const
    {
        if (Position + 1 < Tokens.size())
            return Tokens[Position + 1];
        static Token eofToken(TokenType::Eof, "");
        return eofToken;
    }

public:
    std::vector<std::unordered_map<std::string, Symbol>> LocalScopes;

    std::unordered_map<std::string, Symbol> &CurrentLocalScope()
    {
        if (LocalScopes.empty())
            PushLocalScope();
        return LocalScopes.back(); // Returns the top/most recent scope
    }

    MapId AddressCount = 2;
    MapId NewAddress()
    {
        return ++AddressCount;
    }
    const Symbol GarbageSymbol;
    Symbol NewSymbol(TypeDescriptor TypeDesc = ValueType::Unknown, VariableType VarType = Var)
    {
        Symbol NewSymbol(TypeDesc, VarType);
        NewSymbol.Address = NewAddress();
        return NewSymbol;
    }

private:
    void PushLocalScope()
    {
        LocalScopes.emplace_back();
    }

    void PopLocalScope()
    {
        if (!LocalScopes.empty())
        {
            LocalScopes.pop_back();
        }
    }

    Symbol LookupVariable(const std::string &Name, const bool Throws = true)
    {
        // Search from the top (most recent scope) to the bottom
        size_t i = LocalScopes.size();
        for (auto it = LocalScopes.rbegin(); it != LocalScopes.rend(); ++it)
        {
            i--;
            // if (i < MaxLookup - 1)
            //     break;
            if (it->count(Name))
                return it->at(Name);
        }

        // if (Throws)
        //     Throw("Undefined variable: " + Name, false, Error);
        return GarbageSymbol;
    }

    std::string ParseName()
    {
        if (Check(TokenType::Reserved))
        {
            Throw("Name is reserved. It is not recommended to use this name, as future updates may cause this to break.", false, Warning);
            return Advance().Text;
        }

        std::string Name = Expect(TokenType::Identifier).Text;
        return Name;
    }

    TypeDescriptor ParseType(const bool AllowModifiers = true)
    {
        ValueType Type = ValueType::Unknown;
        ExpressionPtr CustomTypeName;
        std::vector<TypeDescriptor> Subtypes;
        ExpressionPtr ArraySize = nullptr;

        if (Match(TokenType::IntType))
            Type = ValueType::Int;
        else if (Match(TokenType::FloatType))
            Type = ValueType::Float;
        else if (Match(TokenType::BoolType))
            Type = ValueType::Bool;
        else if (Match(TokenType::DoubleType))
            Type = ValueType::Double;
        else if (Match(TokenType::ShortType))
            Type = ValueType::Short;
        else if (Match(TokenType::LongType))
            Type = ValueType::Long;
        else if (Match(TokenType::CharacterType))
            Type = ValueType::Character;
        else if (Match(TokenType::Null)) // voidptr
        {
            Type = ValueType::Dynamic;

            if (Check(TokenType::LAngle) && PeekNext().Type != TokenType::RAngle)
                Throw("null type does not support generic types", false, SyntaxError);
        }
        else if (Check(TokenType::LParen))
        {
            Type = ValueType::Function;
        }
        else if (Match(TokenType::Caret))
        {
            Expect(TokenType::Function);
            Type = ValueType::ExternalFunction;
        }
        else if (Check(TokenType::Identifier))
        {
            Type = ValueType::Custom;
            CustomTypeName = ParsePrimary(false);
        }
        else
        {
            Throw("Expected type name", false);
            return TypeDescriptor(ValueType::Unknown);
        }

        if (Type == ValueType::Custom && Match(TokenType::LAngle) && PeekNext().Type != TokenType::RAngle)
        {
            // generic<int, float>

            do
            {
                Subtypes.push_back(ParseType(false));
            } while (Match(TokenType::Comma));

            Expect(TokenType::RAngle);
        }

        int PointerDepth = 0;
        while (Match(TokenType::LBracket))
        {
            if (!Check(TokenType::RBracket))
            {
                ArraySize = ParseExpression();
                Expect(TokenType::RBracket);
            }
            else
            {
                Expect(TokenType::RBracket);
            }
            PointerDepth++;
        }

        bool Nullable = AllowModifiers && Match(TokenType::QuestionMark);
        if (Nullable && Type != ValueType::Custom)
        {
            Throw("Primitive type cannot be nullable", false, SyntaxError);
        }

        if (Type == ValueType::Function)
        {
            // ((int[][], char[], MyClass?) :: ReturnType)

            Expect(TokenType::LParen);
            Expect(TokenType::LParen);

            if (!Match(TokenType::DotDotDot) && !Check(TokenType::RParen))
            {
                do
                {
                    Subtypes.push_back(ParseType());
                } while (Match(TokenType::Comma));
            }

            Expect(TokenType::RParen);
            Expect(TokenType::Return);
            Subtypes.insert(Subtypes.begin(), ParseType());
            Expect(TokenType::RParen);
        }

        bool IsConstant = false;
        if (AllowModifiers)
        {
            IsConstant = Match(TokenType::Immutable) || !Match(TokenType::Mutable);
        }

        return TypeDescriptor(Type, Subtypes, CustomTypeName, Nullable, IsConstant, PointerDepth, ArraySize);
    }

    unsigned short MatchTerminator()
    {
        // semicolon not required
        // if (!Check(TokenType::SemiColon))
        //     Throw("Expected a `;` to terminate the statement", false, SyntaxError);

        unsigned short i = 0;
        while (Match(TokenType::SemiColon))
            ++i;

        return i;
    }

    StatementPtr ParseStatement()
    {
        if (Check(TokenType::SemiColon))
        {
            MatchTerminator();
            return nullptr; // empty statement
        }

        else if (Check(TokenType::Package))
        {
            Throw("A package statement cannot be used here, it must be the very first statement", false, SyntaxError);
            Advance();
            Advance();
            MatchTerminator();
            return nullptr;
        }

        StatementPtr Stmt = nullptr;
        if (Match(TokenType::At) || Check(TokenType::Import))
        {
            return ParsePreprocessor();
        }

        else if (Check(TokenType::Identifier) && PeekNext().Type == TokenType::Colon)
        {
            Stmt = ParseVarDeclaration();
        }

        else if (Match(TokenType::Return))
        {
            if (LookupVariable("*CanReturn", false).TypeDesc.Type != ValueType::Dynamic)
                Throw("Return cannot be used outside of a function", false, SyntaxError, Previous());

            ExpressionPtr Expr = ParseExpression();
            MatchTerminator();
            if (!Check(TokenType::RBrace))
                Throw("Unreachable code detected", false, Info);
            return std::make_shared<ReturnStatement>(Expr);
        }

        else if (Match(TokenType::Raise))
        {
            Stmt = std::make_shared<SignalStatement>(ParseExpression());
        }

        else if (Match(TokenType::Break))
        {
            if (LookupVariable("*CanBreak", false).TypeDesc.Type != ValueType::Dynamic)
                Throw("Break can only be used in a loop", false, SyntaxError, Previous());

            Stmt = std::make_shared<BreakStatement>();
        }

        else if (Match(TokenType::In))
        {
            Stmt = ParseUseStatement();
        }

        if (Stmt)
        {
            MatchTerminator();
            return Stmt;
        }

        // all others dont need semicolons

        if (Match(TokenType::If))
            return ParseIfStatement();
        if (Match(TokenType::For))
            return ParseLoopStatement();
        if (Match(TokenType::While))
            return ParseWhileStatement();
        if (Check(TokenType::LBrace))
            return ParseReceiverStatement();

        const bool IsExport = Match(TokenType::Export);

        if (Match(TokenType::Function))
        {
            if (PeekNext().Type == TokenType::Equals || PeekNext().Type == TokenType::Colon)
            {
                Throw("A 'defn' statement defines functions, not variables", false, SyntaxError, Previous());
            }

            std::shared_ptr<VarDeclaration> Decl = std::dynamic_pointer_cast<VarDeclaration>(ParseFunctionDefinition());
            std::shared_ptr<FunctionDefinition> Func = std::dynamic_pointer_cast<FunctionDefinition>(Decl->Initializer);
            Func->Global = IsExport;
            *Decl->Initializer = *Func;
            return Decl;
        }

        else if (Match(TokenType::Class))
        {
            return ParseClassDefinition();
        }

        else if (Check(TokenType::Identifier) && PeekNext().Type == TokenType::Colon)
        {
            Stmt = ParseVarDeclaration();
        }

        else if (Check(TokenType::Import))
        {
            return ParsePreprocessor();
        }

        if (Stmt)
        {
            MatchTerminator();
            return Stmt;
        }

        if (IsExport)
        {
            Throw("Expected a declaration to export", false, SyntaxError, Previous());
        }

        // Otherwise parse as expression statement (like function call)
        ExpressionPtr Expr = ParseExpression();
        MatchTerminator();
        return std::make_shared<ExpressionStatement>(Expr);
    }

    StatementPtr ParsePreprocessor()
    {
        if (Match(TokenType::Import))
        {
            const Token &ImportToken = Previous();
            std::filesystem::path ImportDirectory = std::filesystem::path(Previous().Location.File).parent_path();

            const bool ImportPackage = Match(TokenType::Package);
            std::string ImportName = ParseName();
            std::string AsNamespace;

            if (Match(TokenType::As))
                AsNamespace = ParseName();
            else
                AsNamespace = ImportName;

            bool Found = false;

            std::vector<std::filesystem::directory_entry> Entries;

            std::function<void(const std::filesystem::path &)> CollectEntries;
            CollectEntries = [&](const std::filesystem::path &Dir)
            {
                try
                {
                    std::filesystem::recursive_directory_iterator it(
                        Dir, std::filesystem::directory_options::skip_permission_denied);
                    std::filesystem::recursive_directory_iterator end;

                    for (; it != end; ++it)
                    {
                        const auto &Entry = *it;

                        if (Entry.is_regular_file())
                        {
                            std::string Name = Entry.path().filename().string();
                            if (Name == ".Include")
                            {
                                it.disable_recursion_pending();

                                std::ifstream File(Entry.path(), std::ios::in);
                                if (!File.is_open())
                                    continue;

                                std::string Content((std::istreambuf_iterator<char>(File)),
                                                    std::istreambuf_iterator<char>());
                                File.close();

                                CollectEntries(Content);

                                IncludePath::DirPath = Content;

                                continue;
                            }
                        }

                        // skip directories like '.lintleaf' or '.vscode'
                        if (Entry.is_directory())
                        {
                            std::string Name = Entry.path().filename().string();
                            if (!Name.empty() && Name[0] == '.')
                            {
                                it.disable_recursion_pending();
                                continue;
                            }
                        }

                        if (Entry.is_regular_file())
                        {
                            Entries.push_back(Entry);
                        }
                    }
                }
                catch (const std::exception &e)
                {
                    // std::cout << e.what() << std::endl;
                }
            };

            CollectEntries(ImportDirectory);

            std::vector<Token> IncludedTokens;

            if (!ImportPackage)
            {
                // scan the import directory if not package
                
                for (auto &Entry : Entries)
                {
                    if (!Entry.is_regular_file())
                        continue;

                    auto Stem = Entry.path().stem().string();
                    if (Stem == ImportName)
                    {
                        std::ifstream File(Entry.path(), std::ios::in);
                        if (!File.is_open())
                            continue;

                        std::string Content((std::istreambuf_iterator<char>(File)),
                                            std::istreambuf_iterator<char>());
                        File.close();

                        Lexer Lex(Content);
                        Lex.Location.File = Entry.path().string();
                        IncludedTokens = Lex.Tokenize();
                        if (IncludedTokens.front().Type == TokenType::Package)
                        {
                            IncludedTokens.erase(IncludedTokens.begin()); // skip package keyword
                            if (IncludedTokens.front().Type == TokenType::Identifier || IncludedTokens.front().Type == TokenType::Reserved)
                            {
                                IncludedTokens.erase(IncludedTokens.begin()); // skip package name
                            }

                            Throw("Imported file is a package, please add 'import package' instead", false, SyntaxError, ImportToken);
                        }
                        Found = true;
                        break;
                    }
                }
            }
            else
            {
                // look for packages

                CollectEntries(IncludePath::DirPath);

                for (auto &Entry : Entries)
                {
                    std::ifstream File(Entry.path(), std::ios::in);
                    if (!File.is_open())
                        continue;

                    std::string _Content;
                    char Buf[512];
                    File.read(Buf, sizeof(Buf));
                    _Content.assign(Buf, File.gcount());

                    Lexer Lex(_Content);
                    Lex.Location.File = Entry.path().string();

                    try
                    {
                        auto A = Lex.ReadToken();
                        if (A.empty())
                            continue;

                        Token BeginningToken = A.front();
                        if (BeginningToken.Type != TokenType::Package)
                            continue;

                        auto B = Lex.ReadToken();
                        if (B.empty())
                            continue;

                        Token NameToken = B.front();
                        if (NameToken.Type != TokenType::Identifier && NameToken.Type != TokenType::Reserved)
                            continue;

                        if (NameToken.Text != ImportName)
                        {
                            continue;
                        }
                    }
                    catch (const std::runtime_error& e)
                    {
                        // lexer error (most likely a raw binary file)
                        continue;
                    }
                    

                    File.clear();
                    File.seekg(0, std::ios::beg);
                    std::string Content((std::istreambuf_iterator<char>(File)),
                                        std::istreambuf_iterator<char>());
                    File.close();
                    Lex.Source = Content;
                    IncludedTokens = Lex.Tokenize();
                    Found = true;
                    break;
                }
            }

            if (!Found)
            {
                Throw((ImportPackage ? "Package not found: " : "File not found: ") + ImportName, false, Error, Previous());
                CurrentLocalScope()[AsNamespace] = NewSymbol(ValueType::Namespace);
                return nullptr;
            }

            if (ImportCache.count((ImportPackage ? "" : "./") + ImportName))
            {
                CurrentLocalScope()[AsNamespace] = NewSymbol(ValueType::Namespace);
                return std::make_shared<VarDeclaration>(std::make_shared<VariableExpression>(AsNamespace, ImportCache[(ImportPackage ? "" : "./") + ImportName]), ImportName, AddressCount, ValueType::Namespace);
            }

            IncludedTokens.insert(IncludedTokens.begin(), {Token(TokenType::Identifier, AsNamespace),
                                                           Token(TokenType::LBrace, "@{")});
            IncludedTokens.push_back(Token(TokenType::RBrace, "@}"));

            IncludedTokens.erase(std::remove_if(IncludedTokens.begin(), IncludedTokens.end(),
                                                [](Token Tok)
                                                { return Tok.Type == TokenType::Eof; }), // filter out eof
                                 IncludedTokens.end());

            if (Position > Tokens.size())
                Position = Tokens.size();
            Tokens.insert(Tokens.begin() + Position, IncludedTokens.begin(), IncludedTokens.end());

            return ParseNamespaceStatement((ImportPackage ? "" : "./") + ImportName);
        }

        std::string PreprocessType = Expect(TokenType::Identifier).Text;

        if (PreprocessType == "Define")
        {
            std::string MacroName = ParseName();
            MacroNames.push_back(MacroName);

            Expect(TokenType::SemiColon);

            std::vector<Token> MacroTokens;
            while (true)
            {
                if (Check(TokenType::At) && PeekNext().Text == "End")
                {
                    Advance();
                    Advance();
                    break;
                }
                MacroTokens.push_back(Peek());
                Advance();
            }

            size_t OriginalPosition = Position;
            while (!IsAtEnd())
            {
                Token Current = Peek();
                if (Check(TokenType::Identifier) && Peek().Text == MacroName)
                {
                    for (Token &Tok : MacroTokens)
                        Tok.Location = Peek().Location;
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
        else if (PreprocessType == "Asmbl")
        {
            StatementPtr Stmt = nullptr;
            Expect(TokenType::LBrace);
            
            std::vector<Token> Instructions;
            while (!Match(TokenType::RBrace))
            {
                if (Check(TokenType::StringLiteral))
                {
                    Throw("String literal is not allowed here", false, SyntaxError);
                    Advance();
                    continue;
                }
                
                Instructions.push_back(Advance());
            }
            
            return std::make_shared<AssemblyInstructions>(Instructions);
        }
        else
        {
            Throw("Invalid preprocess type");
        }

        return nullptr;
    }

    StatementPtr ParseVarDeclaration(const bool IsMember = false)
    {
        std::string Name = ParseName();
        Expect(TokenType::Colon);

        if (Check(TokenType::Equals) || (Check(TokenType::Mutable) && PeekNext().Type == TokenType::Equals) || (Check(TokenType::Immutable) && PeekNext().Type == TokenType::Equals))
        {
            const bool IsConstant = Match(TokenType::Immutable) || !Match(TokenType::Mutable);
            Expect(TokenType::Equals);
            // Implicit type assignment (x: = new Object() or x: immut = new Object())

            ExpressionPtr Init = ParseExpression();
            // if (!std::dynamic_pointer_cast<UseExpression>(Init) || std::dynamic_pointer_cast<UseExpression>(Init)->Type.Type != ValueType::Custom)
            //     Throw("Type inference is not allowed here, for explicitness", false, SyntaxError, Previous());

            TypeDescriptor Type = ValueType::Unknown;
            Type.Constant = IsConstant;

            CurrentLocalScope()[Name] = NewSymbol(Type);
            return std::make_shared<VarDeclaration>(Init, Name, AddressCount, Type);
        }
        
        TypeDescriptor Type = ParseType();

        if (CurrentLocalScope().count(Name))
        {
            Throw("Multiple declaration", false, Warning);
        }

        if (Match(TokenType::Equals))
        {
            ExpressionPtr Init = ParseExpression();
            CurrentLocalScope()[Name] = NewSymbol(Type);
            return std::make_shared<VarDeclaration>(Init, Name, AddressCount, Type);
        }
        else if (Type.Constant && !IsMember)
        {
            Throw("Initializer required in immutable variable declaration", false, Error, Previous());

            ExpressionPtr Expr = std::make_shared<UseExpression>(Type, std::vector<ExpressionPtr>());
            CurrentLocalScope()[Name] = NewSymbol(Type);
            return std::make_shared<VarDeclaration>(Expr, Name, AddressCount, Type);
        }
        else if (!Type.Nullable)
        {
            if (!IsMember)
            {
                Throw("|Append:?| No initializer in non-nullable variable declaration", false, Error, Previous());
                
                ExpressionPtr Expr = std::make_shared<UseExpression>(Type, std::vector<ExpressionPtr>());
                CurrentLocalScope()[Name] = NewSymbol(Type);
                return std::make_shared<VarDeclaration>(Expr, Name, AddressCount, Type);
            }
            else
            {
                Throw("A non-nullable member must be initialized in the constructor", false, Hint, Previous());

                ExpressionPtr Expr = std::make_shared<ValueExpression>(nullptr);
                CurrentLocalScope()[Name] = NewSymbol(Type);
                return std::make_shared<VarDeclaration>(Expr, Name, AddressCount, Type);
            }
        }
        else
        {
            CurrentLocalScope()[Name] = NewSymbol(Type);
            return std::make_shared<VarDeclaration>(nullptr, Name, AddressCount, Type);
        }
    }

    StatementPtr ParseFunctionDefinition()
    {
        std::string Name = ParseName();

        PushLocalScope();
        CurrentLocalScope()["*CanReturn"] = Symbol(ValueType::Dynamic);

        std::vector<VarDeclaration> Params;

        if (Check(TokenType::LParen))
        {
            Expect(TokenType::LParen);

            if (!Check(TokenType::RParen))
            {
                do
                {
                    std::string ParamName = ParseName();
                    if (CurrentLocalScope().find(Name) != CurrentLocalScope().end())
                        Throw("Function parameter was already declared", false, Warning);

                    Expect(TokenType::Colon);
                    TypeDescriptor ParamType = ParseType();

                    CurrentLocalScope()[ParamName] = NewSymbol(ParamType, Parameter);
                    Params.push_back(VarDeclaration(nullptr, ParamName, AddressCount, ParamType));
                    if (LookupVariable(ParamName, false).VarType == Member)
                        Throw("Function parameter shadows a class member", false, Info);

                } while (Match(TokenType::Comma));
            }

            Expect(TokenType::RParen);
        }

        TypeDescriptor ReturnType = TypeDescriptor(ValueType::Unknown);
        if (Match(TokenType::Return))
        {
            const bool CheckMut = Check(TokenType::Mutable) || Check(TokenType::Immutable);
            ReturnType = ParseType();
            if (CheckMut && ReturnType.Type != ValueType::Custom)
            {
                Throw("Expected reference type", false, SyntaxError, Previous());
            }
            if (!CheckMut)
                ReturnType.Constant = true;
        }

        MapId FunctionAddress = Name == "main" ? 1 : NewAddress();
        if ((CurrentLocalScope().count(Name) || CurrentLocalScope()[Name].VarType != Parameter) && LookupVariable(Name, false).TypeDesc.Type != ValueType::Custom)
        {
            CurrentLocalScope()[Name] = Symbol(TypeDescriptor(ValueType::Function, {ReturnType}), Var, FunctionAddress);
        }
        if (LocalScopes.size() > 1 && LocalScopes.at(LocalScopes.size() - 2).count(Name))
        {
            FunctionAddress = LocalScopes.at(LocalScopes.size() - 2).at(Name).Address;
        }

        std::vector<StatementPtr> Body;

        if (!Match(TokenType::RArrowThick))
        {
            if (ReturnType.Type == ValueType::Unknown)
                ReturnType = TypeDescriptor(ValueType::Null).AsConstant();

            Expect(TokenType::LBrace);

            while (!Match(TokenType::RBrace))
                Body.push_back(ParseStatement());
        }
        else
        {
            ExpressionPtr Expr = ParseExpression();
            // if (ReturnType.Type == ValueType::Unknown)
            //     ReturnType = GuessTypeOf(Expr);
            // if (ReturnType.Type == ValueType::Unknown)
            //     Throw("Could not deduce implicit return type, please mark explicitly", false, SyntaxError, Previous());
            Body.push_back(std::make_shared<ReturnStatement>(Expr));
        }

        PopLocalScope();

        std::vector<TypeDescriptor> FuncSubtypes;
        FuncSubtypes.push_back(ReturnType);
        for (auto &&Param : Params)
        {
            FuncSubtypes.push_back(Param.Type);
        }

        CurrentLocalScope()[Name] = Symbol(TypeDescriptor(ValueType::Function, {ReturnType}), Var, FunctionAddress);

        return std::make_shared<VarDeclaration>(std::make_shared<FunctionDefinition>(Body, Params, ReturnType), Name, FunctionAddress, TypeDescriptor(ValueType::Function, FuncSubtypes, nullptr, false, true));
    }

    StatementPtr ParseNamespaceStatement(const std::string AddToCache)
    {
        std::string Name = ParseName();
        CurrentLocalScope()[Name] = NewSymbol(ValueType::Namespace);
        const MapId NamespaceAddress = AddressCount;

        if (!AddToCache.empty())
            ImportCache[AddToCache] = AddressCount;

        Expect(TokenType::LBrace);

        PushLocalScope();

        std::unordered_map<std::string, MapId> Definition;
        std::vector<StatementPtr> Statements;
        while (!Match(TokenType::RBrace))
        {
            const bool IsExport = Match(TokenType::Export);

            StatementPtr Stmt = ParseStatement();
            Statements.push_back(Stmt);

            if (auto Decl = std::dynamic_pointer_cast<VarDeclaration>(Stmt))
            {
                if (IsExport)
                {
                    std::string Name = Decl->Name;
                    MapId Address = Decl->Address;
                    Definition[Name] = Address;
                }
            }
            else if (Stmt && !std::dynamic_pointer_cast<AssemblyInstructions>(Stmt) && !std::dynamic_pointer_cast<UseStatement>(Stmt))
            {
                Statements.push_back(Stmt);
                Throw("Expected a declaration", false);
            }
        }

        PopLocalScope();

        return std::make_shared<VarDeclaration>(std::make_shared<NamespaceDefinition>(Definition, Statements), Name, NamespaceAddress, TypeDescriptor(ValueType::Namespace, {}, nullptr, false, true));
    }

    StatementPtr ParseClassDefinition()
    {
        const bool IsImplicit = Match(TokenType::QuestionMark);

        std::string Name = ParseName();
        Symbol ClassSymbol = NewSymbol(ValueType::Custom);
        CurrentLocalScope()[Name] = ClassSymbol;
        ClassNames.push_back(Name);

        std::vector<MapId> Templates;
        if (Match(TokenType::LBracket))
        {
            do
            {
                std::string TemplateName = ParseName();
                CurrentLocalScope()[TemplateName] = NewSymbol(ValueType::Unknown, Template);
                Templates.push_back(AddressCount);
            } while (Match(TokenType::Comma));
            Expect(TokenType::RBracket);
        }

        std::vector<ExpressionPtr> InheritsFrom;
        if (Match(TokenType::As))
        {
            do
            {
                InheritsFrom.push_back(ParsePrimary(false));
            } while (Match(TokenType::Comma));
        }

        // if (!Match(TokenType::LBrace))
        //     return std::make_shared<VarDeclaration>(std::make_shared<ValueExpression>(nullptr), Name, ClassSymbol.Address, TypeDescriptor(ValueType::Unknown, {}, nullptr, true, false));

        Match(TokenType::LBrace);

        PushLocalScope();
        CurrentLocalScope()["*This"] = Symbol(ValueType::Custom, Var, 2);

        std::vector<MemberDeclaration> Members;
        while (!Match(TokenType::RBrace))
        {
            PushLocalScope();

            bool ConstantSelfReference = true;
            bool IsPrivate = !Match(TokenType::Dot);

            StatementPtr Stmt;
            if (!IsPrivate && Peek().Text == Name)
            {
                ConstantSelfReference = false;
                IsPrivate = true;
                Stmt = ParseFunctionDefinition();
            }
            else if (Match(TokenType::Function))
            {
                if (Match(TokenType::Immutable))
                    ConstantSelfReference = true;
                else if (Match(TokenType::Mutable))
                    ConstantSelfReference = false;

                if (Peek().Text == Name)
                    Throw("Expected method name, not a constructor", false);
                Stmt = ParseFunctionDefinition();
            }
            else
            {
                std::string Name = ParseName();
                Expect(TokenType::Colon);
                TypeDescriptor Type = ParseType();
                CurrentLocalScope()[Name] = NewSymbol(Type, Member);
                Stmt = std::make_shared<VarDeclaration>(nullptr, Name, AddressCount, Type);
            }

            MatchTerminator();

            PopLocalScope();

            if (auto Decl = std::dynamic_pointer_cast<VarDeclaration>(Stmt))
            {
                if (Decl->Name == Name && !std::dynamic_pointer_cast<FunctionDefinition>(Decl->Initializer))
                    Throw("Class name shadows class member", false, Info);
                else if (LookupVariable(Decl->Name, false).Address != 0 && LookupVariable(Decl->Name, false).VarType == Member && !std::dynamic_pointer_cast<FunctionDefinition>(Decl->Initializer))
                    Throw("Redefinition of a class member", false, Warning, Previous());
                else if (Decl->Name != Name)
                    CurrentLocalScope()[Decl->Name] = Symbol(Decl->Type, Member, Decl->Address);

                if (IsPrivate)
                    Decl->Name = '#' + Decl->Name;
                Members.push_back(MemberDeclaration(Decl->Initializer, Decl->Name, Decl->Address, Decl->Type, ConstantSelfReference));
            }
        }

        PopLocalScope();
        return std::make_shared<VarDeclaration>(std::make_shared<ClassBlueprint>(Name, Members, InheritsFrom, nullptr, Templates, IsImplicit), Name, ClassSymbol.Address, TypeDescriptor(ValueType::Unknown, {}, nullptr, false, true));
    }

    StatementPtr ParseReceiverStatement()
    {
        Expect(TokenType::LBrace);

        std::vector<std::pair<TypeDescriptor, MapId>> ReceiveTypes; // Type and address
        std::vector<std::vector<StatementPtr>> With;
        With.emplace_back();

        PushLocalScope();

        size_t i = 0;
        while (true)
        {
            if (Match(TokenType::RBrace))
            {
                PopLocalScope();

                if (Match(TokenType::With))
                {
                    PushLocalScope();
                    ++i;
                    std::string Name = ParseName();
                    Expect(TokenType::Colon);
                    TypeDescriptor Type = ParseType(false);
                    CurrentLocalScope()[Name] = NewSymbol(Type);
                    ReceiveTypes.push_back(std::pair<TypeDescriptor, MapId>(Type, AddressCount));
                    Expect(TokenType::LBrace);
                    With.emplace_back();
                }
                else
                    break;
            }
            if (!Check(TokenType::RBrace))
                With[i].push_back(ParseStatement());
        }

        return std::make_shared<ReceiverStatement>(ReceiveTypes, With);
    }

    StatementPtr ParseIfStatement()
    {
        std::vector<ExpressionPtr> Conditions;
        Expect(TokenType::LParen);
        Conditions.push_back(ParseExpression());
        Expect(TokenType::RParen);
        Expect(TokenType::LBrace);

        std::vector<std::vector<StatementPtr>> Then;
        Then.emplace_back();

        PushLocalScope();

        size_t i = 0;
        while (true)
        {
            if (Match(TokenType::RBrace))
            {
                PopLocalScope();

                if (Match(TokenType::ElseIf))
                {
                    PushLocalScope();
                    ++i;
                    Expect(TokenType::LParen);
                    Conditions.push_back(ParseExpression());
                    Expect(TokenType::RParen);
                    Expect(TokenType::LBrace);
                    Then.emplace_back();
                }
                else if (Match(TokenType::Else))
                {
                    PushLocalScope();
                    ++i;
                    Conditions.push_back(std::make_shared<ValueExpression>(true));
                    Expect(TokenType::LBrace);
                    Then.emplace_back();
                }
                else
                    break;
            }
            if (!Check(TokenType::RBrace))
                Then[i].push_back(ParseStatement());
        }

        return std::make_shared<IfStatement>(Conditions, Then);
    }

    StatementPtr ParseLoopStatement()
    {
        PushLocalScope();
        CurrentLocalScope()["*CanBreak"] = Symbol(ValueType::Dynamic);

        if (Check(TokenType::LParen) && PeekNext().Type == TokenType::Identifier && Peek(2).Type == TokenType::Colon)
        {
            Expect(TokenType::LParen);
            StatementPtr CountDecl = ParseVarDeclaration();

            Expect(TokenType::SemiColon);
            ExpressionPtr Condition = ParseExpression();
            Expect(TokenType::SemiColon);
            ExpressionPtr PostExpr = ParseExpression();

            Expect(TokenType::RParen);

            Expect(TokenType::LBrace);

            std::vector<StatementPtr> Body;
            while (!Match(TokenType::RBrace))
                Body.push_back(ParseStatement());

            Body.push_back(std::make_shared<ExpressionStatement>(PostExpr));
            
            PopLocalScope();
            return std::make_shared<MultiStatement>(MultiStatement({CountDecl, std::make_shared<WhileStatement>(Body, Condition)}));
        }

        if (Check(TokenType::Identifier) && (PeekNext().Type == TokenType::Colon || PeekNext().Type == TokenType::In || PeekNext().Type == TokenType::Comma))
        {
            std::string KeyName = ParseName();
            TypeDescriptor KeyType = ValueType::Unknown;
            if (Match(TokenType::Colon))
                KeyType = ParseType(false);
            CurrentLocalScope()[KeyName] = NewSymbol(KeyType);
            const MapId KeyId = AddressCount;

            if (!Match(TokenType::Comma))
            {
                Expect(TokenType::In);
                ExpressionPtr Iter = ParseExpression();

                Expect(TokenType::LBrace);

                std::vector<StatementPtr> Body;
                while (!Match(TokenType::RBrace))
                    Body.push_back(ParseStatement());

                PopLocalScope();
                return std::make_shared<ForStatement>(Body, Iter, 0, TypeDescriptor(ValueType::Unknown), KeyId, KeyType);
            }
            else
            {
                std::string ValName = ParseName();

                TypeDescriptor ValType = ValueType::Unknown;
                if (Match(TokenType::Colon))
                    ValType = ParseType();

                CurrentLocalScope()[ValName] = NewSymbol(ValType);
                const MapId ValId = AddressCount;

                Expect(TokenType::In);
                ExpressionPtr Iter = ParseExpression();

                Expect(TokenType::LBrace);

                std::vector<StatementPtr> Body;
                while (!Match(TokenType::RBrace))
                    Body.push_back(ParseStatement());

                PopLocalScope();
                return std::make_shared<ForStatement>(Body, Iter, KeyId, KeyType, ValId, ValType);
            }
        }
        else
        {
            ExpressionPtr Expr = ParseExpression();

            Expect(TokenType::LBrace);

            std::vector<StatementPtr> Body;
            while (!Match(TokenType::RBrace))
                Body.push_back(ParseStatement());

            PopLocalScope();
            return std::make_shared<ForStatement>(Body, Expr, 0, ValueType::Unknown, 0, ValueType::Unknown);
        }
    }

    StatementPtr ParseWhileStatement()
    {
        PushLocalScope();
        CurrentLocalScope()["*CanBreak"] = Symbol(ValueType::Dynamic);

        ExpressionPtr Expr;

        if (!Check(TokenType::LBrace))
        {
            Expr = ParseExpression();
            if (auto Val = std::dynamic_pointer_cast<ValueExpression>(Expr))
            {
                if (Val->Val.type() == typeid(bool) && std::any_cast<bool>(Val->Val))
                {
                    Throw("|RemoveSymbol| A condition 'true' is redundant", false, Hint, Previous());
                }
            }
        }
        else
        {
            Expr = std::make_shared<ValueExpression>(true);
        }

        Expect(TokenType::LBrace);

        std::vector<StatementPtr> Body;
        while (!Match(TokenType::RBrace))
            Body.push_back(ParseStatement());

        PopLocalScope();
        return std::make_shared<WhileStatement>(Body, Expr);
    }

    StatementPtr ParseUseStatement()
    {
        ExpressionPtr Expr = ParsePrimary(false);
        Expect(TokenType::Import);
        Expr = std::make_shared<MemberExpression>(Expr, Expect(TokenType::Identifier).Text);
        std::string Alias = Previous().Text;
        if (Match(TokenType::As))
        {
            Alias = Expect(TokenType::Identifier).Text;
        }
        CurrentLocalScope()[Alias] = NewSymbol(TypeDescriptor(ValueType::Unknown));

        return std::make_shared<UseStatement>(Expr, false, AddressCount);
    }

    int GetPrecedence(TokenType type)
    {
        switch (type)
        {
        case TokenType::Star:
        case TokenType::Slash:
            return 6;

        case TokenType::Plus:
        case TokenType::Minus:
            return 5;

        case TokenType::LAngle:
        case TokenType::RAngle:
        case TokenType::LAngleEqual:
        case TokenType::RAngleEqual:
            return 4;

        case TokenType::DoubleEquals:
            return 3;

        case TokenType::DoubleAmpersand:
            return 2;

        case TokenType::DoublePipe:
            return 1; // lowest

        case TokenType::Equals:
            return 0; // assignment (often right-associative)

        default:
            return -99; // not a binary operator
        }
    }

    OperationType MapOperator(TokenType Type)
    {
        switch (Type)
        {
        // binary
        case TokenType::Plus:
            return OperationType::Add;
        case TokenType::Minus:
            return OperationType::Subtract;
        case TokenType::Star:
            return OperationType::Multiply;
        case TokenType::Slash:
            return OperationType::Divide;
        case TokenType::RAngle:
            return OperationType::GreaterThan;
        case TokenType::RAngleEqual:
            return OperationType::GreaterThanOrEqualTo;
        case TokenType::LAngle:
            return OperationType::LessThan;
        case TokenType::LAngleEqual:
            return OperationType::LessThanOrEqualTo;
        case TokenType::DoubleEquals:
            return OperationType::Equality;
        case TokenType::DoublePipe:
            return OperationType::Or;
        case TokenType::DoubleAmpersand:
            return OperationType::And;
        case TokenType::Ampersand:
            return OperationType::BitAnd;
        case TokenType::Exclamation:
            return OperationType::ForceUnwrap;

        // unary
        case TokenType::Not:
            return OperationType::Negate;

        default:
            throw std::runtime_error("Unknown operator " + std::string(magic_enum::enum_name(Type)));
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

            if (OpType == TokenType::Equals)
            {
                Advance();
                Lhs = std::make_shared<AssignmentExpression>(Lhs, ParseExpression());
                continue;
            }
            else if (PeekNext().Type == TokenType::Equals)
            {
                Advance();
                Advance();
                Lhs = std::make_shared<AssignmentExpression>(Lhs, std::make_shared<BinaryExpression>(MapOperator(OpType), Lhs, ParseExpression()));
                break;
            }

            Advance(); // Consume operator

            // For right-associative operators like '=', don't +1
            int NextMinPrecedence = Precedence + 1;

            ExpressionPtr Rhs = ParseExpression(NextMinPrecedence);
            Lhs = std::make_shared<BinaryExpression>(MapOperator(OpType), Lhs, Rhs);
        }

        return Lhs;
    }

    ExpressionPtr ParsePrimary(const bool AllowComplex = true)
    {
        ExpressionPtr Expr = ParseSecondary();
        if (!Expr)
        {
            Throw("Expected expression");
            return std::make_shared<ValueExpression>(nullptr);
        }

        while (true)
        {
            if (Check(TokenType::LParen) && PeekNext().Type != TokenType::As && AllowComplex && !std::dynamic_pointer_cast<ValueExpression>(Expr))
            {
                auto CallExpr = std::make_shared<CallExpression>(nullptr, std::vector<ExpressionPtr>());
                Advance();

                std::vector<ExpressionPtr> &Args = CallExpr->Arguments;
                if (!Check(TokenType::RParen))
                {
                    do
                    {
                        Args.push_back(ParseExpression());
                    } while (Match(TokenType::Comma));
                }
                Expect(TokenType::RParen);
                CallExpr->Callee = Expr;
                Expr = CallExpr;

                if (Check(TokenType::LParen))
                    break;
                // break so code like this does not cause issues:
                // func()(Pet as? Dog).Pet?()
            }
            else if (Check(TokenType::LBracket) && AllowComplex)
            {
                Advance();
                ExpressionPtr IndexExpr = ParseExpression();
                Expect(TokenType::RBracket);
                Expr = std::make_shared<IndexExpression>(Expr, IndexExpr);
            }
            else if (Check(TokenType::Dot) || (Check(TokenType::QuestionMark) && PeekNext().Type == TokenType::Dot))
            {
                const bool Throws = !Match(TokenType::QuestionMark);
                Advance();

                Token Ident = Expect(TokenType::Identifier);
                Expr = std::make_shared<MemberExpression>(Expr, Ident.Text, Throws);
            }
            else if (Check(TokenType::LParen) && PeekNext().Type == TokenType::As && AllowComplex)
            {
                Expect(TokenType::LParen);
                Expect(TokenType::As);

                const bool Throws = !Match(TokenType::QuestionMark);
                TypeDescriptor Type = ParseType(false);
                // if (Type.Type != ValueType::Custom)
                //     Throw("Expected reference type", false, SyntaxError, Previous());
                Expr = std::make_shared<ClassCastExpression>(Expr, Type, Throws);
                Expect(TokenType::RParen);
                break;
            }
            else if (Check(TokenType::Exclamation) && AllowComplex)
            {
                Advance();
                Expr = std::make_shared<UnaryExpression>(OperationType::ForceUnwrap, Expr);
            }
            else
                break;
        }

        return Expr;
    }

    ExpressionPtr ParseSecondary()
    {
        if (Previous().IsCursor)
        {
            return std::make_shared<VariableExpression>("", 0);
        }
        
        if (Match(TokenType::At))
        {
            std::string PreprocessType = Expect(TokenType::Identifier).Text;

            if (PreprocessType == "SizeOf")
            {
                Expect(TokenType::LParen);
                const TypeDescriptor &Type = ParseType(false);
                Expect(TokenType::RParen);
                return std::make_shared<SizeOfTypeExpression>(Type);
            }
        }

        if (Match(TokenType::LParen))
        {
            if (!Check(TokenType::RParen) && PeekNext().Type != TokenType::Colon)
            {
                // (expr)
                ExpressionPtr Expr = ParseExpression();
                Expect(TokenType::RParen);
                return Expr;
            }
            else
            {
                // (x: int) => x * x # implicit return type!
                // () :: int => 10 * 10
                PushLocalScope();

                std::vector<VarDeclaration> Params;
                if (!Check(TokenType::RParen))
                {
                    do
                    {
                        std::string ParamName = ParseName();
                        Expect(TokenType::Colon);
                        TypeDescriptor ParamType = ParseType();
                        CurrentLocalScope()[ParamName] = NewSymbol(ParamType, Parameter);
                        Params.push_back(VarDeclaration(nullptr, ParamName, AddressCount, ParamType));
                    } while (Match(TokenType::Comma));
                }
                
                Expect(TokenType::RParen);

                TypeDescriptor ReturnType = TypeDescriptor(ValueType::Unknown);
                if (Match(TokenType::Return))
                {
                    const bool CheckMut = Check(TokenType::Mutable) || Check(TokenType::Immutable);
                    ReturnType = ParseType();
                    if (CheckMut && ReturnType.Type != ValueType::Custom)
                    {
                        Throw("Expected reference type", false, SyntaxError, Previous());
                    }
                    if (!CheckMut)
                        ReturnType.Constant = true;
                }

                std::vector<StatementPtr> Body;

                if (!Match(TokenType::RArrowThick))
                {
                    if (ReturnType.Type == ValueType::Unknown)
                        ReturnType = TypeDescriptor(ValueType::Null).AsConstant();

                    Expect(TokenType::LBrace);

                    while (!Match(TokenType::RBrace))
                        Body.push_back(ParseStatement());
                }
                else
                {
                    ExpressionPtr Expr = ParseExpression();
                    Body.push_back(std::make_shared<ReturnStatement>(Expr));
                }

                PopLocalScope();
                return std::make_shared<FunctionDefinition>(Body, Params, ReturnType);
            }
        }

        if (Check(TokenType::Not) || Check(TokenType::Plus) || Check(TokenType::Minus) || Check(TokenType::Star))
        {
            const OperationType Op = MapOperator(Peek().Type);
            ExpressionPtr Expr;
            if (Match(TokenType::Not))
                Expr = ParseExpression(3);
            else
            {
                Advance();
                Expr = ParsePrimary();
            }
            return std::make_shared<UnaryExpression>(Op, Expr);
        }

        if (Check(TokenType::PlusPlus) || Check(TokenType::MinusMinus))
        {
            const Token &Tok = Advance();
            ExpressionPtr AssignTo = ParsePrimary();
            return std::make_shared<AssignmentExpression>(AssignTo, std::make_shared<BinaryExpression>(Tok.Type == TokenType::PlusPlus ? OperationType::Add : OperationType::Subtract, AssignTo, std::make_shared<ValueExpression>(rt_Int(1))));
        }

        if (Match(TokenType::Ampersand))
        {
            ExpressionPtr Expr = ParsePrimary();
            return std::make_shared<UnownedReferenceExpression>(Expr);
        }

        if (Check(TokenType::BoolType) || Check(TokenType::IntType) || Check(TokenType::FloatType))
        {
            TypeDescriptor Type = ParseType();
            Expect(TokenType::LParen);
            ExpressionPtr Arg = ParseExpression();
            Expect(TokenType::RParen);

            return std::make_shared<UseExpression>(UseExpression(Type, {Arg}));
        }

        if (Match(TokenType::SizeOf))
        {
            Expect(TokenType::LParen);
            ExpressionPtr Expr = ParseExpression();
            Expect(TokenType::RParen);
            return std::make_shared<SizeOfExpression>(Expr);
        }

        if (Check(TokenType::New))
        {
            Advance();

            std::vector<ExpressionPtr> Args;

            TypeDescriptor Type = ParseType(false);
            if (Type.Type != ValueType::Custom && !Type.PointerDepth)
                Throw("Expected reference type", false, SyntaxError, Previous());

            if (Type.PointerDepth)
            {
                if (Type.ArraySize)
                    Args.push_back(Type.ArraySize);
                else
                    Throw("Expected an array allocation size", false, SyntaxError, Previous());
            }
            else
            {
                Expect(TokenType::LParen);

                if (!Check(TokenType::RParen))
                {
                    do
                    {
                        Args.push_back(ParseExpression());
                    } while (Match(TokenType::Comma));
                }

                Expect(TokenType::RParen);
            }

            if (Match(TokenType::With))
            {
                if (!Check(TokenType::Dot))
                    Expect(TokenType::Dot);

                PushLocalScope();

                std::vector<VarDeclaration> Members;
                while (Match(TokenType::Dot))
                {
                    if (!Match(TokenType::This))
                    {
                        std::string Name = ParseName();
                        Expect(TokenType::LParen);
                        Members.push_back(VarDeclaration(ParseExpression(), Name, 0, ValueType::Unknown));
                        Expect(TokenType::RParen);
                    }
                    else
                    {
                        CurrentLocalScope()["*This"] = Symbol(ValueType::Custom, Var, 2);
                        
                        Expect(TokenType::LBrace);
                        while (!Match(TokenType::RBrace))
                        {
                            PushLocalScope();
                            
                            if (!Match(TokenType::Dot))
                                Throw("Overriding private members in behavioral injections is not allowed", false, Error, Previous());

                            Expect(TokenType::Function);
                            StatementPtr Stmt = ParseFunctionDefinition();

                            if (auto Decl = std::dynamic_pointer_cast<VarDeclaration>(Stmt))
                            {
                                CurrentLocalScope()[Decl->Name] = Symbol(Decl->Type, Member, Decl->Address);
                                Members.push_back(*Decl);
                            }

                            PopLocalScope();
                        }
                        
                        break;
                    }
                }

                PopLocalScope();
                return std::make_shared<UseExpression>(UseExpression(Type, Args, Members));
            }

            return std::make_shared<UseExpression>(Type, Args);
        }

        if (Match(TokenType::Number))
        {
            if (Previous().Text.find('.') != std::string::npos)
                return std::make_shared<ValueExpression>(rt_Float(std::stod(Previous().Text)));
            else
                return std::make_shared<ValueExpression>(rt_Int(std::stol(Previous().Text)));
        }
        if (Match(TokenType::StringLiteral))
        {
            if (Previous().Text.length() == 1) // char literal
            {
                return std::make_shared<ValueExpression>(Previous().Text.at(0));
            }

            return std::make_shared<ValueExpression>(Previous().Text);
        }
        if (Match(TokenType::Null))
            return std::make_shared<ValueExpression>(nullptr);
        if (Match(TokenType::True))
            return std::make_shared<ValueExpression>(true);
        if (Match(TokenType::False))
            return std::make_shared<ValueExpression>(false);

        if (Check(TokenType::Identifier))
        {
            Symbol Decl = LookupVariable(Peek().Text, PeekNext().Type != TokenType::LParen);
            Advance();
            if (Decl.VarType == Member)
                return std::make_shared<MemberExpression>(std::make_shared<VariableExpression>("self", 2), Previous().Text);
            return std::make_shared<VariableExpression>(Previous().Text, Decl.Address);
        }
        if (Check(TokenType::This))
        {
            Symbol Decl = LookupVariable("*This");
            Advance();
            return std::make_shared<VariableExpression>("*This", Decl.Address);
        }
        
        return nullptr;
    }

    const Token &Expect(TokenType Type, const std::string &Message = "")
    {
        if (Match(Type))
            return Previous();
        Throw("Expected " + TokenTypeString(Type) + ", instead got '" + Peek().Text + "' (" + TokenTypeString(Peek().Type) + ')', true, SyntaxError, Peek());
        return Peek();
    }

    void Panic()
    {
        while (!IsAtEnd())
        {
            switch (Peek().Type)
            {
            case TokenType::Comma:
            case TokenType::Colon:
            case TokenType::Equals:
            case TokenType::RParen:
            case TokenType::RBracket:
            case TokenType::LBrace:
            case TokenType::RBrace:
            case TokenType::LAngle:
            case TokenType::RAngle:
            case TokenType::Return:
            case TokenType::If:
            case TokenType::For:
            case TokenType::In:
            case TokenType::New:
                return; // found a safe synchronized point
            default:
                break;
            }

            Advance();
        }
    }

    void Throw(const std::string &Message, const bool panic = true, const SeverityLevel &Severity = SyntaxError, const Token &Tok = Token(TokenType::Reserved, "", ScriptLocation("", -2)))
    {
        Errors.push_back(CompileError(Message, Severity, Tok.Location.Line == -2 ? Peek().Location : Tok.Location));
        if (panic)
        {
            Advance();
            Panic();
        }
    }
};
