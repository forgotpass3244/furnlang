
#include "Common.hpp"
#include "Ast.hpp"
#include "CompileFlags.hpp"

#define INC_REF_COUNT(Type)                     \
    if (Type.PointerDepth && CmplFlags::GarbageCollect)       \
    {                                           \
        Output << "    inc QWORD [rax - 16]\n"; \
    }
#define DEC_REF_COUNT(Type)                     \
    if (Type.PointerDepth && CmplFlags::GarbageCollect)       \
    {                                           \
        Output << "    dec QWORD [rax - 16]\n"; \
    }

uint64_t CurrentScope = 0;

class AsmGenerator
{
public:
    std::vector<CompileError> Errors;
    std::vector<std::string> AvailableIdentifiers;

private:
    inline void Throw(CompileError e)
    {
        e.Location = CurrentEval->Location;
        if (auto ExprStmt = std::dynamic_pointer_cast<ExpressionStatement>(CurrentEval))
            e.Location = ExprStmt->Expr->Location;
        Errors.push_back(e);
    }

    std::vector<StatementPtr> Ast;
    std::stringstream Output;
    std::unordered_map<MapId, std::string> PendingFunctionDefinitions;

    StatementPtr CurrentEval = nullptr;

    struct MemberInfo
    {
        TypeDescriptor Type;
        uint64_t Offset; // byte offset in the instance
    };

    struct Variable
    {
        int64_t StackLoc = 0;
        TypeDescriptor TypeDesc;
        std::shared_ptr<std::vector<std::shared_ptr<FunctionDefinition>>> Funcs = nullptr;
        std::shared_ptr<std::unordered_map<std::string, MapId>> Namespace = nullptr;
        std::shared_ptr<std::unordered_map<std::string, MemberInfo>> Class = nullptr;
        MapId Address = 0;
        uint64_t ScopeI = CurrentScope;
        std::string Name;
    };

    struct CmplSymbol
    {
        TypeDescriptor TypeDesc;
        std::shared_ptr<Variable> Var = nullptr;
        std::shared_ptr<std::vector<std::shared_ptr<FunctionDefinition>>> Funcs = nullptr;
        std::shared_ptr<std::unordered_map<std::string, MapId>> Namespace = nullptr;
        std::shared_ptr<std::unordered_map<std::string, MemberInfo>> Class = nullptr;
    };

    int64_t StackSize = 0;
    std::vector<Variable> Variables;
    
    size_t LabelCount = 0;
    std::vector<std::string> DataList;

public:
    explicit AsmGenerator(std::vector<StatementPtr> ast)
        : Ast(std::move(ast))
    {
    }

private:
    void OpenScope()
    {
        Output << "    ; scope begin\n";
        CurrentScope++;
    }

    void CloseScope()
    {
        const int64_t ScopeLoc = CurrentScope--;

        if (Variables.empty())
            return;

        for (int i = Variables.size() - 1; i >= 0; i--)
        {
            Variable &Var = Variables.at(i);
            const MapId Address = Var.Address;

            if (Var.Funcs)
                continue;
            if (Var.Class)
                continue;
            if (Var.Namespace)
                continue;

            // is variable

            if (Var.ScopeI < ScopeLoc)
                continue;

            const CmplSymbol &LocalSymbol = ResolveSymbol(std::make_shared<VariableExpression>("*Local", Address));

            GenerateExpression(std::make_shared<VariableExpression>("*Local", Address));

            DestroyObject(LocalSymbol);

            Variables.erase(Variables.begin() + i);
            // stays aligned since we are iterating backwards

            Pop("r9", SizeOfType(LocalSymbol.TypeDesc));
        }
        
        Output << "    ; scope closed and locals destroyed\n";
    }

    void GarbageCollectObject(const CmplSymbol &Symbol)
    {
        if (!CmplFlags::GarbageCollect)
            return;
            
        if (Symbol.TypeDesc.PointerDepth)
        {
            TypeDescriptor Subtype = Symbol.TypeDesc;
            Subtype.PointerDepth--;
            // mov rdi, rax ; addr
            // mov rsi, rcx ; length
            // mov rax, 11 ; munmap syscall number
            // syscall
            std::string SkipLabel = CreateLabel();
            Output << "    mov rbx, rax\n";
            Output << "    mov rax, [rbx - 16] ; refcount\n";
            Output << "    test rax, rax\n";
            Output << "    jnz " << SkipLabel << "\n";
            Output << "    mov rax, rbx\n";
            Output << "    sub rax, 16 ; to size\n";
            Output << "    mov rdi, rax ; addr\n";
            Output << "    mov rsi, [rax + 8] ; length\n";
            Output << "    imul rsi, " << SizeOfType(Subtype) << "\n";
            Output << "    add rsi, 16\n";
            Output << "    mov rax, 11 ; munmap syscall number\n";
            Output << "    syscall\n";
            Output << SkipLabel << ":\n";
        }
    }

    void DestroyObject(const CmplSymbol &Symbol)
    {
        DEC_REF_COUNT(Symbol.TypeDesc);
        GarbageCollectObject(Symbol);
    }

    bool CompileTypeMatch(const TypeDescriptor &ObjectType, const TypeDescriptor &ExpectedType, const unsigned short Looseness = 1000)
    {
#define _CompileTypeMatch_NumbersCheck                                                                                         \
    if (ExpectedType.Type == ValueType::Long)                                                                                  \
    {                                                                                                                          \
        return ObjectType.Type == ValueType::Short || ObjectType.Type == ValueType::Int || ObjectType.Type == ValueType::Long; \
    }                                                                                                                          \
    else if (ExpectedType.Type == ValueType::Int)                                                                              \
    {                                                                                                                          \
        return ObjectType.Type == ValueType::Short || ObjectType.Type == ValueType::Int;                                       \
    }                                                                                                                          \
    else if (ExpectedType.Type == ValueType::Short)                                                                            \
    {                                                                                                                          \
        return ObjectType.Type == ValueType::Short || ObjectType.Type == ValueType::Int;                                       \
    }

        if (ObjectType.PointerDepth != ExpectedType.PointerDepth)
            return false;

        if (ObjectType.PointerDepth)
        {
            if (ObjectType.Constant && !ExpectedType.Constant)
                return false;
        }

        if (ObjectType.Type == ValueType::Custom || ExpectedType.Type == ValueType::Custom)
        {
            if (ObjectType.Type != ExpectedType.Type)
                return false;

            if (ObjectType.Constant && !ExpectedType.Constant)
                return false;

            return (*ResolveSymbol(ObjectType.CustomTypeName).Class).at("*ClassId").Offset == (*ResolveSymbol(ExpectedType.CustomTypeName).Class).at("*ClassId").Offset;
        }

        if (Looseness <= 0)
        {
            if (ObjectType.Type != ExpectedType.Type)
                return false;

            if (ObjectType.Nullable != ExpectedType.Nullable)
                return false;

            if (ObjectType.Constant != ExpectedType.Constant)
                return false;

            return true;
        }
        else if (Looseness <= 1)
        {
            if (ObjectType.Type != ExpectedType.Type)
                return false;

            if (ObjectType.Nullable && !ExpectedType.Nullable)
                return false;

            return true;
        }
        else if (Looseness <= 2)
        {
            if (ObjectType.Type != ExpectedType.Type)
                return false;

            if (ObjectType.Nullable && !ExpectedType.Nullable)
                return false;

            _CompileTypeMatch_NumbersCheck;

            return true;
        }
        else if (Looseness <= 3)
        {
            if (ObjectType.Type != ExpectedType.Type)
                return false;

            if (ObjectType.Nullable && !ExpectedType.Nullable)
                return false;

            if (ExpectedType.Type == ValueType::Dynamic)
                return true;

            _CompileTypeMatch_NumbersCheck;

            return true;
        }
        else
        {
            if (ObjectType.Type == ValueType::Null && ExpectedType.Nullable)
                return true;

            if (ObjectType.Nullable && !ExpectedType.Nullable)
                return false;

            if (ExpectedType.Type == ValueType::Dynamic)
                return true;

            _CompileTypeMatch_NumbersCheck;

            return ObjectType.Type == ExpectedType.Type;
        }
    }

    int64_t SizeOfType(const TypeDescriptor &Type)
    {
        if (Type.PointerDepth)
        {
            return 8;
        }

        if (Type.Type == ValueType::Custom && Type.CustomTypeName)
        {
            // return the size of the REFERENCE address
            return 8;
            // NOT the following
            // const CmplSymbol &TypeSymbol = ResolveSymbol(Type.CustomTypeName);
            // if (TypeSymbol.Class)
            // {
            //     return TypeSymbol.Class->at("*ClassSize").Offset;
            // }
        }

        // right now we only use rax
        // which is 64 bit, to avoid
        // overcomplicating the compiler
        // so we will just return 8
        return 8;

        // switch (Type.Type)
        // {
        // case ValueType::Bool:
        //     return 1;

        // case ValueType::Character:
        //     return 1;

        // case ValueType::Int:
        //     return 4;

        // case ValueType::Short:
        //     return 2;

        // case ValueType::Long:
        //     return 8;

        // case ValueType::Float:
        //     return 4;

        // case ValueType::Double:
        //     return 8;

        // default:
        //     break;
        // }

        return 8;
    }

    CmplSymbol GarbageCmplSymbol = CmplSymbol{.TypeDesc = ValueType::Unknown};

    CmplSymbol ResolveSymbol(const ExpressionPtr &Expr)
    {
        if (auto VarExpr = std::dynamic_pointer_cast<VariableExpression>(Expr))
        {
            if (VarExpr->Name.empty())
            {
                return GarbageCmplSymbol;
            }

            bool Found = false;
            for (auto &&Var : Variables)
            {
                if (Var.Address != VarExpr->Address)
                    continue;

                Found = true;
                break;
            }

            if (!Found)
            {
                // look for identifier in namespaces
                for (auto &&Var : Variables)
                {
                    if (!Var.Namespace)
                        continue;
                    if (!Var.Namespace->count(VarExpr->Name))
                        continue;

                    for (auto &&MemberVar : Variables)
                    {
                        if (MemberVar.Address != Var.Namespace->at(VarExpr->Name))
                            continue;

                        // only look for a function
                        if (MemberVar.Funcs)
                            return CmplSymbol{.TypeDesc = MemberVar.TypeDesc, .Funcs = MemberVar.Funcs};

                        break;
                    }
                }

                return GarbageCmplSymbol;
            }

            for (auto &&Var : Variables)
            {
                if (Var.Address != VarExpr->Address)
                    continue;

                if (Var.Funcs)
                    return CmplSymbol{.TypeDesc = Var.TypeDesc, .Funcs = Var.Funcs};
                else if (Var.Namespace)
                    return CmplSymbol{.TypeDesc = Var.TypeDesc, .Namespace = Var.Namespace};
                else if (Var.Class && Var.StackLoc == -1)
                    return CmplSymbol{.TypeDesc = Var.TypeDesc, .Class = Var.Class};
                else
                    return CmplSymbol{.TypeDesc = Var.TypeDesc, .Var = std::make_shared<Variable>(Var), .Class = Var.Class};
            }
        }
        else if (auto Assign = std::dynamic_pointer_cast<AssignmentExpression>(Expr))
        {
            return ResolveSymbol(Assign->Value);
        }
        else if (auto Literal = std::dynamic_pointer_cast<ValueExpression>(Expr))
        {
            if (Literal->Val.type() == typeid(rt_Int))
                return CmplSymbol{.TypeDesc = TypeDescriptor(ValueType::Int).AsConstant()};
            else if (Literal->Val.type() == typeid(rt_Float))
                return CmplSymbol{.TypeDesc = TypeDescriptor(ValueType::Float).AsConstant()};
            else if (Literal->Val.type() == typeid(bool))
                return CmplSymbol{.TypeDesc = TypeDescriptor(ValueType::Bool).AsConstant()};
            else if (Literal->Val.type() == typeid(std::nullptr_t))
                return CmplSymbol{.TypeDesc = TypeDescriptor(ValueType::Null, {}, nullptr, 2).AsConstant()};
            else if (Literal->Val.type() == typeid(char))
                return CmplSymbol{.TypeDesc = TypeDescriptor(ValueType::Character).AsConstant()};
            else if (Literal->Val.type() == typeid(std::string))
            {
                if (ToString(Literal->Val).length() == 1)
                    return CmplSymbol{.TypeDesc = TypeDescriptor(ValueType::Character).AsConstant()};
                else
                    return CmplSymbol{.TypeDesc = TypeDescriptor(ValueType::Character).AsPointer()};
            }

            return GarbageCmplSymbol;
        }
        else if (auto Func = std::dynamic_pointer_cast<FunctionDefinition>(Expr))
        {
            std::vector<TypeDescriptor> FuncSubtypes;
            FuncSubtypes.push_back(Func->ReturnType);
            for (auto &&Param : Func->Arguments)
            {
                FuncSubtypes.push_back(Param.Type);
            }

            auto Funcs = std::make_shared<std::vector<std::shared_ptr<FunctionDefinition>>>();
            Funcs->push_back(Func);

            return CmplSymbol{.TypeDesc = TypeDescriptor(ValueType::Function, FuncSubtypes), .Funcs = Funcs};
        }
        else if (auto Namespace = std::dynamic_pointer_cast<NamespaceDefinition>(Expr))
        {
            auto Members = std::make_shared<std::unordered_map<std::string, MapId>>();
            for (auto &&[Name, Address] : Namespace->Definition)
            {
                (*Members)[Name] = Address;
            }

            for (auto &&Member : Namespace->Statements)
            {
                if (auto &&Decl = std::dynamic_pointer_cast<VarDeclaration>(Member))
                {
                    if (Decl->Address == 1) // is main()
                        continue;
                }
                GenerateStatement(Member);
            }

            return CmplSymbol{.TypeDesc = ValueType::Namespace, .Namespace = Members};
        }
        else if (auto Access = std::dynamic_pointer_cast<MemberExpression>(Expr))
        {
            CmplSymbol ObjectSymbol = ResolveSymbol(Access->Object);

            if (ObjectSymbol.Namespace)
            {
                if (!ObjectSymbol.Namespace->count(Access->Member))
                {
                    return GarbageCmplSymbol;
                }
                for (auto &&Var : Variables)
                {
                    if (Var.Address != ObjectSymbol.Namespace->at(Access->Member))
                        continue;

                    if (Var.Funcs)
                        return CmplSymbol{.TypeDesc = Var.TypeDesc, .Funcs = Var.Funcs};
                    else if (Var.Namespace)
                        return CmplSymbol{.TypeDesc = Var.TypeDesc, .Namespace = Var.Namespace};
                    else
                        return CmplSymbol{.TypeDesc = Var.TypeDesc, .Var = std::make_shared<Variable>(Var)};
                }

                return GarbageCmplSymbol;
            }
            else if (ObjectSymbol.Class)
            {
                if (!ObjectSymbol.Class->count(Access->Member))
                {
                    return GarbageCmplSymbol;
                }
                TypeDescriptor MemberType = ObjectSymbol.Class->at(Access->Member).Type;
                if (ObjectSymbol.TypeDesc.Constant)
                {
                    // if the object is constant, all of its members should be too
                    MemberType.Constant = true;
                }

                std::shared_ptr<std::unordered_map<std::string, AsmGenerator::MemberInfo>> MemberClass = nullptr;
                if (MemberType.CustomTypeName)
                {
                    MemberClass = ResolveSymbol(MemberType.CustomTypeName).Class;
                }

                return CmplSymbol{.TypeDesc = MemberType, .Class = MemberClass};
            }

            return GarbageCmplSymbol;
        }
        else if (auto Index = std::dynamic_pointer_cast<IndexExpression>(Expr))
        {
            CmplSymbol ObjectSymbol = ResolveSymbol(Index->Object);
            ObjectSymbol.TypeDesc.PointerDepth -= 1;
            return ObjectSymbol;
        }
        else if (auto Class = std::dynamic_pointer_cast<ClassBlueprint>(Expr))
        {
            auto Members = std::make_shared<std::unordered_map<std::string, MemberInfo>>();
            (*Members)["*ClassId"] = MemberInfo{.Type = ValueType::Unknown, .Offset = Class->UniqueId};

            uint64_t Size = 0;
            for (auto &&MemberDecl : Class->Members)
            {
                (*Members)[MemberDecl.Name] = MemberInfo{.Type = MemberDecl.Type, .Offset = Size};
                Size += SizeOfType(MemberDecl.Type);
            }

            (*Members)["*ClassSize"] = MemberInfo{.Type = ValueType::Unknown, .Offset = Size};

            return CmplSymbol{.TypeDesc = TypeDescriptor(ValueType::Custom, {}, nullptr), .Class = Members};
        }
        else if (auto NewExpr = std::dynamic_pointer_cast<UseExpression>(Expr))
        {
            CmplSymbol Type = ResolveSymbol(NewExpr->Type.CustomTypeName);
            auto Members = Type.Class;

            return CmplSymbol{.TypeDesc = NewExpr->Type, .Class = Members};
        }
        else if (auto Cast = std::dynamic_pointer_cast<ClassCastExpression>(Expr))
        {
            std::shared_ptr<std::unordered_map<std::string, AsmGenerator::MemberInfo>> Members = nullptr;

            if (Cast->Type.CustomTypeName)
            {
                CmplSymbol Type = ResolveSymbol(Cast->Type.CustomTypeName);
                Members = Type.Class;
            }

            return CmplSymbol{.TypeDesc = Cast->Type, .Class = Members};
        }
        else if (auto SizeOfType = std::dynamic_pointer_cast<SizeOfTypeExpression>(Expr))
        {
            return CmplSymbol{.TypeDesc = ValueType::Int};
        }
        else if (auto SizeOf = std::dynamic_pointer_cast<SizeOfExpression>(Expr))
        {
            return CmplSymbol{.TypeDesc = ValueType::Int};
        }
        else if (auto Bin = std::dynamic_pointer_cast<BinaryExpression>(Expr))
        {
            CmplSymbol SymbolA = ResolveSymbol(Bin->A);
            CmplSymbol SymbolB = ResolveSymbol(Bin->B);

            switch (Bin->Operator)
            {
            case OperationType::Add:
            case OperationType::Subtract:
            case OperationType::Multiply:
            case OperationType::Divide:
                return CmplSymbol{.TypeDesc = (CompileTypeMatch(SymbolA.TypeDesc, ValueType::Float) || CompileTypeMatch(SymbolB.TypeDesc, ValueType::Float)) ? ValueType::Float : ValueType::Int};
            case OperationType::LessThan:
            case OperationType::GreaterThan:
            case OperationType::LessThanOrEqualTo:
            case OperationType::GreaterThanOrEqualTo:
                return CmplSymbol{.TypeDesc = ValueType::Bool};
            default:
                break;
            }

            return GarbageCmplSymbol;
        }

        else if (auto Un = std::dynamic_pointer_cast<UnaryExpression>(Expr))
        {
            CmplSymbol Symbol = ResolveSymbol(Un->Expr);

            switch (Un->Operator)
            {
            case OperationType::Add:
            case OperationType::Subtract:
                return CmplSymbol{.TypeDesc = ValueType::Int};
            case OperationType::ForceUnwrap:
            {
                Symbol.TypeDesc.Nullable = false;
                return Symbol;
            }

            default:
                break;
            }

            return GarbageCmplSymbol;
        }
        else if (auto Call = std::dynamic_pointer_cast<CallExpression>(Expr))
        {
            CmplSymbol Symbol = ResolveSymbol(Call->Callee);

            if (Symbol.Funcs)
            {
                auto Func = CalculateBestOverload(Symbol.Funcs, Call, false);
                if (!Func)
                    return GarbageCmplSymbol;

                if (Func->ReturnType.CustomTypeName)
                {
                    return CmplSymbol{.TypeDesc = Func->ReturnType, .Class = ResolveSymbol(Func->ReturnType.CustomTypeName).Class};
                }
                return CmplSymbol{.TypeDesc = Func->ReturnType};
            }

            return GarbageCmplSymbol;
        }
        else if (auto UnownedReference = std::dynamic_pointer_cast<UnownedReferenceExpression>(Expr))
        {
            return ResolveSymbol(UnownedReference->Expr);
        }

        // if (Expr)
        //     Throw(CompileError("resolve symbol: failed (not implemented)", Error));
        return GarbageCmplSymbol;
    }

    void GenerateStatement(const StatementPtr &Stmt)
    {
        CurrentEval = Stmt;
        
        if (auto Instruction = std::dynamic_pointer_cast<AssemblyInstructions>(Stmt))
        {
            Output << "; inline assembly begin\n";

            bool Newline = true;
            for (size_t i = 0; i < Instruction->Instructions.size(); i++)
            {
                const Token &Tok = Instruction->Instructions.at(i);
                if (Tok.Type == TokenType::SemiColon)
                {
                    if (!Newline)
                    {
                        Output << "\n";
                        Newline = true;
                    }
                }
                else if (Instruction->Instructions.size() > (i + 1) && Instruction->Instructions.at(i + 1).Type == TokenType::Colon)
                {
                    Output << Tok.Text << "\n";
                    i++;
                    Newline = true;
                }
                else if (Instruction->Instructions.size() > (i + 1) && Instruction->Instructions.at(i).Type == TokenType::Dot)
                {
                    if (Instruction->Instructions.size() > (i + 2) && Instruction->Instructions.at(i + 2).Type == TokenType::Colon)
                    {
                        Output << "." << Instruction->Instructions.at(i + 1).Text << ":\n";
                        i++;
                        i++;
                        Newline = true;
                    }
                    else
                    {
                        Output << " ." << Instruction->Instructions.at(i + 1).Text;
                        i++;
                        Newline = false;
                    }
                }
                else if (Tok.Text == "section" || Tok.Text == "global" || Tok.Text == "extern" || Tok.Text == "segment")
                {
                    if (Tok.Text == "segment")
                        Output << "section";
                    else
                        Output << Tok.Text;
                    Newline = false;
                }
                else
                {
                    if (Newline)
                    {
                        Newline = false;
                        Output << "    " << Tok.Text;
                    }
                    else
                    {
                        Output << " " << Tok.Text;
                    }
                }
            }

            if (!Newline)
                Output << "\n";

            Output << "; inline assembly end\n";
        }
        else if (auto ExprStmt = std::dynamic_pointer_cast<ExpressionStatement>(Stmt))
        {
            GenerateExpression(ExprStmt->Expr);
            Output << "    ; discard result\n";
        }
        else if (auto Multi = std::dynamic_pointer_cast<MultiStatement>(Stmt))
        {
            for (auto &&Stmt : Multi->Statements)
            {
                GenerateStatement(Stmt);
            }
        }
        else if (auto Decl = std::dynamic_pointer_cast<VarDeclaration>(Stmt))
        {
            CmplSymbol Symbol = ResolveSymbol(Decl->Initializer);

            if (Symbol.Funcs)
            {
                for (size_t fi = 0; fi < Symbol.Funcs->size(); fi++)
                {
                    auto Func = std::dynamic_pointer_cast<FunctionDefinition>(Symbol.Funcs->at(fi));

                    if (Func->ReturnType.Type == ValueType::Unknown && !Func->Body.empty())
                    {
                        if (auto Return = std::dynamic_pointer_cast<ReturnStatement>(Func->Body.at(0)))
                        {
                            Func->ReturnType = ResolveSymbol(Return->Expr).TypeDesc;
                        }
                    }

                    bool Found = false;
                    for (auto &&Var : Variables)
                    {
                        if (Var.Address != Decl->Address)
                            continue;

                        Var.Funcs->push_back(Func);

                        Found = true;
                        break;
                    }

                    if (!Found)
                    {
                        Variable Var = Variable{.StackLoc = 0 /* stack location doesnt matter for functions */, .TypeDesc = Decl->Type, .Funcs = Symbol.Funcs, .Address = Decl->Address, .Name = Decl->Name};
                        Variables.push_back(Var);
                    }

                    const bool IsMain = Decl->Address == 1;
                    std::string FuncLabel = MangleFunctionSignature(*Func, Decl->Name);
                    std::stringstream SavedOutput;

                    if (IsMain)
                    {
                        FuncLabel = "_start";
                        if (CmplFlags::StrictMode && !Func->Global)
                        {
                            Throw(CompileError("the main function was not exported (add `export` keyword)", Warning));
                        }
                    }
                    else
                    {
                        SavedOutput << Output.str();
                        Output.str(""); // clear
                    }

                    if (Func->Global || IsMain)
                        Output << "global " << FuncLabel << "\n";
                    Output << FuncLabel << ": ; begin function\n";

                    int64_t PreviousStackSize = StackSize;
                    StackSize = IsMain ? 0 : 8; // first 8 is return address

                    int64_t i = -1;
                    if (Func->Arguments.size() <= 1)
                        i = 0;
                    for (int j = Func->Arguments.size() - 1; j >= 0; j--)
                    {
                        VarDeclaration &ParamDecl = Func->Arguments.at(j);

                        if (ParamDecl.Type.CustomTypeName)
                        {
                            const CmplSymbol &ParamTypeSymbol = ResolveSymbol(ParamDecl.Type.CustomTypeName);
                            Variables.push_back(Variable{.StackLoc = StackSize - 8, .TypeDesc = ParamDecl.Type, .Class = ParamTypeSymbol.Class, .Address = ParamDecl.Address, .Name = ParamDecl.Name});
                        }
                        else
                        {
                            Variables.push_back(Variable{.StackLoc = StackSize - 8, .TypeDesc = ParamDecl.Type, .Address = ParamDecl.Address, .Name = ParamDecl.Name});
                        }

                        Push(SizeOfType(ParamDecl.Type));
                        i++;
                    }

                    OpenScope(); // parameters destroyed by the caller

                    for (const StatementPtr &Stmt : Func->Body)
                    {
                        GenerateStatement(Stmt);
                    }

                    CloseScope();

                    for (int i = Func->Arguments.size() - 1; i >= 0; i--)
                    {
                        Variables.pop_back();
                    }

                    if (IsMain)
                    {
                        Output << "    ; fallback exit\n";
                        Output << "    mov rax, 60 ; sysexit\n";
                        Output << "    mov rdi, 0 ; exit code\n";
                        Output << "    syscall ; call exit\n";
                        Output << "    ret ; if exit somehow fails its better to segfault here than leak into other functions\n";
                        Output << "; end function " << FuncLabel << "\n";
                    }
                    else
                    {
                        // return null
                        GenerateStatement(std::make_shared<ReturnStatement>(std::make_shared<ValueExpression>(nullptr)));
                        Output << "; end function " << FuncLabel << "\n";
                        std::string FunctionOutput = Output.str();
                        Output.str("");
                        Output << SavedOutput.str();
                        PendingFunctionDefinitions[Func->UniqueId] = FunctionOutput;
                    }

                    if (!IsMain)
                        StackSize = PreviousStackSize;
                }
            }
            else if (Symbol.Namespace)
            {
                Variable Var = Variable{.StackLoc = -1 /* stack location doesnt matter for namespaces */, .TypeDesc = Decl->Type, .Namespace = Symbol.Namespace, .Address = Decl->Address, .Name = Decl->Name};
                Variables.push_back(Var);
            }
            else if (Symbol.Class && !Symbol.TypeDesc.CustomTypeName)
            {
                Variable Var = Variable{.StackLoc = -1 /* stack location doesnt matter for classes */, .TypeDesc = Decl->Type, .Class = Symbol.Class, .Address = Decl->Address, .Name = Decl->Name};
                Variables.push_back(Var);
            }
            else
            {
                CmplSymbol InitSymbol = ResolveSymbol(Decl->Initializer);
                
                if (Decl->Type.Type == ValueType::Unknown)
                {
                    TypeDescriptor OldType = Decl->Type;
                    Decl->Type = InitSymbol.TypeDesc;
                    Decl->Type.Constant = OldType.Constant;
                }
                else if (!CompileTypeMatch(Symbol.TypeDesc, Decl->Type))
                {
                    Throw(CompileError("initializer type mismatch", Error));
                }

                std::shared_ptr<std::unordered_map<std::string, MemberInfo>> ClassMembers = nullptr;

                if (Decl->Type.CustomTypeName)
                {
                    CmplSymbol TypeSymbol = ResolveSymbol(Decl->Type.CustomTypeName);
                    ClassMembers = TypeSymbol.Class;
                }

                Variable NewVariable = Variable{.StackLoc = StackSize, .TypeDesc = Decl->Type, .Class = ClassMembers, .Address = Decl->Address, .Name = Decl->Name};

                ExpressionPtr InitExpr = Decl->Initializer;

                Variables.push_back(NewVariable);
                GenerateExpression(InitExpr);

                INC_REF_COUNT(Decl->Type);

                Output << "    sub rsp, " << SizeOfType(Decl->Type) << "\n";
                Output << "    mov [rsp], rax\n";
                Push(SizeOfType(Decl->Type));
            }
        }
        else if (auto Using = std::dynamic_pointer_cast<UseStatement>(Stmt))
        {
            CmplSymbol Object = ResolveSymbol(Using->Expr);

            Variable Var = Variable{.StackLoc = Object.Var ? Object.Var->StackLoc : -1, .TypeDesc = Object.TypeDesc, .Funcs = Object.Funcs, .Namespace = Object.Namespace, .Address = Using->Address};
            Variables.push_back(Var);
        }
        else if (auto If = std::dynamic_pointer_cast<IfStatement>(Stmt))
        {
            std::string EndLabel = CreateLabel();
            Output << "    ; begin if " << EndLabel << "\n";

            size_t i = 0;
            for (const auto &Branch : If->Then)
            {
                std::string Label = CreateLabel();

                Output << "    ; condition\n";
                GenerateExpression(If->Conditions.at(i));
                Output << "    test rax, rax\n";
                Output << "    jz " << Label << "\n";

                OpenScope();

                for (const StatementPtr &Stmt : Branch)
                {
                    GenerateStatement(Stmt);
                }

                CloseScope();

                Output << "    jmp " << EndLabel << "\n";

                Output << Label << ":\n";

                i++;
            }

            Output << EndLabel << ": ; end if\n";
        }
        else if (auto While = std::dynamic_pointer_cast<WhileStatement>(Stmt))
        {
            std::string BeginLabel = CreateLabel();
            std::string EndLabel = CreateLabel();
            Output << "    ; begin while " << EndLabel << "\n";
            Output << BeginLabel << ":\n";

            Output << "    ; condition\n";
            GenerateExpression(While->Condition);
            Output << "    test rax, rax\n";
            Output << "    jz " << EndLabel << "\n";

            for (const StatementPtr &Stmt : While->Body)
            {
                GenerateStatement(Stmt);
            }
            Output << "    jmp " << BeginLabel << "\n";

            Output << EndLabel << ": ; end while\n";
        }
        else if (auto Return = std::dynamic_pointer_cast<ReturnStatement>(Stmt))
        {
            GenerateExpression(Return->Expr);
            Output << "    ret\n";
        }
        else if (Stmt)
        {
            Throw(CompileError("GenerateStatement(): unhandled statement " + std::string(typeid(*Stmt).name()) + " (not implemented)", Error));
        }
    }

    void GenerateExpression(const ExpressionPtr &Expr)
    {
        static auto ExprStmt = std::make_shared<ExpressionStatement>(nullptr);
        ExprStmt->Expr = Expr;
        CurrentEval = ExprStmt;

        if (auto Literal = std::dynamic_pointer_cast<ValueExpression>(Expr))
        {
            if (Literal->Val.type() == typeid(std::string) || Literal->Val.type() == typeid(char))
            {
                std::vector<int> StringBytes;
                size_t i = 0;
                for (unsigned char c : ToString(Literal->Val))
                {
                    StringBytes.push_back(int(c));
                    i++;
                }

                if (Literal->Val.type() == typeid(char))
                    Output << "    mov rax, " << StringBytes.at(0) << " ; char\n";
                else
                {
                    // Output << CreateData("db " + EscapedString + ", 0 ; string") << "\n";

                    Output << "    ; allocate string on the heap (char[])\n";
                    Output << "    ; multiple registers will be clobbered\n";
                    Output << "    mov rsi, " << StringBytes.size() * 8 + 16 << " ; size\n";
                    Output << "    mov rax, 9       ; mmap\n";
                    Output << "    mov rdi, 0       ; addr\n";
                    Output << "    mov rdx, 3       ; PROT_READ|PROT_WRITE\n";
                    Output << "    mov r10, 34      ; MAP_PRIVATE|MAP_ANONYMOUS\n";
                    Output << "    mov r8, -1       ; fd\n";
                    Output << "    mov r9, 0        ; offset\n";
                    Output << "    syscall\n";
                    Output << "    mov QWORD [rax + 0], 0 ; init refcount\n";
                    Output << "    mov QWORD [rax + 8], " << StringBytes.size() << " ; store string size\n";
                    Output << "    add rax, 16 ; above string size\n";

                    for (size_t i = 0; i < StringBytes.size(); i++)
                    {
                        const int &Byte = StringBytes.at(i);
                        Output << "    mov QWORD [rax + " << i * 8 << "], " << Byte << "\n";
                    }

                    Output << "    ; no null terminator needed since string size is always known at runtime\n";
                }
            }
            else
            {
                if (Literal->Val.type() == typeid(std::nullptr_t))
                    Output<< "    mov rax, " << 0 << " ; null\n";
                else if (Literal->Val.type() == typeid(bool))
                    Output << "    mov rax, " << int(std::any_cast<bool>(Literal->Val)) << " ; bool\n";
                else
                    Output << "    mov rax, " << ToString(Literal->Val) << " ; int\n";
            }
        }
        else if (auto VarExpr = std::dynamic_pointer_cast<VariableExpression>(Expr))
        {
            if (VarExpr->Name.empty())
            {
                Throw(CompileError("Awaiting identifier...", SyntaxError));
                for (Variable &Var : Variables)
                {
                    if (Var.Name == "main")
                        continue; // skip main

                    if (Var.Funcs)
                        AvailableIdentifiers.push_back("(Function): " + Var.Name);
                    else if (Var.Namespace)
                        AvailableIdentifiers.push_back("(Namespace): " + Var.Name);
                    else
                        AvailableIdentifiers.push_back("(Name): " + Var.Name);
                }
                return;
            }

            CmplSymbol Symbol = ResolveSymbol(VarExpr);

            if (Symbol.Funcs)
            {
                // Throw(CompileError(VarExpr->Name + " is a function, not a variable", Error));
                GenerateExpression(std::make_shared<CallExpression>(CallExpression(VarExpr, {})));
                return;
            }
            else if (!Symbol.Var)
            {
                Throw(CompileError(VarExpr->Name + " is not a valid variable", Error));
                return;
            }

            if (StackSize <= Symbol.Var->StackLoc)
                Throw(CompileError("invalid stack access (underflow)", Fatal));

            Output << "    mov rax, [rsp + " << int64_t(StackSize) - int64_t(Symbol.Var->StackLoc) - SizeOfType(Symbol.Var->TypeDesc) << "] ; load from stack\n";
        }
        else if (auto Access = std::dynamic_pointer_cast<MemberExpression>(Expr))
        {
            CmplSymbol ObjectSymbol = ResolveSymbol(Access->Object);
            CmplSymbol Symbol = ResolveSymbol(Access);

            if (ObjectSymbol.TypeDesc.Nullable)
            {
                Throw(CompileError("object was not unwrapped in member access expression (add object!.member)", Error));
            }

            if (ObjectSymbol.Namespace)
            {
                if (!ObjectSymbol.Namespace->count(Access->Member))
                {
                    Throw(CompileError(Access->Member + " is not a member of the namespace, was it exported?", Error));
                    return;
                }
                if (Symbol.Var)
                {
                    Output << "    mov rax, [rsp + " << int64_t(StackSize) - int64_t(Symbol.Var->StackLoc) - SizeOfType(Symbol.Var->TypeDesc) << "] ; load namespace member statically from stack\n";
                }
            }
            else if (ObjectSymbol.Class)
            {
                if (!ObjectSymbol.Class->count(Access->Member))
                {
                    Throw(CompileError(Access->Member + " is not a member of the object, is it public?", Error));
                    return;
                }
                GenerateExpression(Access->Object);
                Output << "    mov rax, [rax + " << ObjectSymbol.Class->at(Access->Member).Offset << "] ; get object member\n";
            }
            else
            {
                Throw(CompileError("not a valid namespace to access", Error));
                return;
            }

            if (Symbol.Funcs)
            {
                GenerateExpression(std::make_shared<CallExpression>(CallExpression(Access, {})));
            }
        }
        else if (auto Index = std::dynamic_pointer_cast<IndexExpression>(Expr))
        {
            CmplSymbol ObjectSymbol = ResolveSymbol(Index->Object);
            const CmplSymbol &Symbol = ResolveSymbol(Index);

            const CmplSymbol &IndexSymbol = ResolveSymbol(Index->Index);
            if (!CompileTypeMatch(IndexSymbol.TypeDesc, ValueType::Long))
            {
                Throw(CompileError("[] operator offset expects a number", Error));
            }

            if (ObjectSymbol.TypeDesc.Nullable)
            {
                Throw(CompileError("pointer was not unwrapped in index expression (add pointer![0])", Error));
            }

            if (!ObjectSymbol.TypeDesc.PointerDepth)
            {
                Throw(CompileError("[] operator expects a pointer type", Error));
            }

            GenerateExpression(Index->Object);
            Output << "    mov r8, rax ; save heap pointer\n";
            GenerateExpression(Index->Index);
            if (CmplFlags::BoundsChecking)
            {
                std::string OutOfBoundsErrorLabel = CreateLabel();
                static std::string OutOfBoundsErrorMessageData1 = CreateData("db 0x1B, \"[1;101mERROR: index [\"");
                static std::string OutOfBoundsErrorMessageData2 = CreateData("db \"] is out of bounds size\", 0x1B, \"[0m\", 10");

                Output << "    ; bounds checking\n";
                Output << "    mov r9, rax\n";
                Output << "    mov rcx, [r8 - 8]\n";
                Output << "    dec rcx\n";
                Output << "    cmp rcx, r9\n";
                Output << "    mov rax, 0\n";
                Output << "    mov rcx, 1\n";
                Output << "    cmovb rax, rcx\n";
                Output << "    test rax, rax\n";
                Output << "    jz " << OutOfBoundsErrorLabel << "\n";
                // ; write(1, msg, len)
                // mov     rax, 1        ; syscall: write
                // mov     rdi, 1        ; fd = stdout
                // mov     rsi, msg      ; buf
                // mov     rdx, len      ; count
                // syscall
                std::string DgLabel = CreateLabel();
                std::string WrLabel = CreateLabel();
                Output << "    mov rax, 1\n";
                Output << "    mov rdi, 1\n";
                Output << "    mov rsi, " << OutOfBoundsErrorMessageData1 << "\n";
                Output << "    mov rdx, 22\n";
                Output << "    syscall\n";
                Output << "    mov rbx, r9\n";
                Output << "    mov rdi, _numbuf + 20\n";
                Output << "    mov byte [rdi], 0\n";
                Output << "    mov rcx, 0\n";
                Output << "    cmp rbx, 0\n";
                Output << "    jge " << DgLabel << "\n";
                Output << "    neg rbx\n";
                Output << "    mov rcx, 1\n";
                Output << DgLabel << ":\n";
                Output << "    xor rdx, rdx\n";
                Output << "    mov rax, rbx\n";
                Output << "    mov rsi, 10\n";
                Output << "    div rsi\n";
                Output << "    add rdx, 48\n";
                Output << "    dec rdi\n";
                Output << "    mov byte [rdi], dl\n";
                Output << "    mov rbx, rax\n";
                Output << "    test rbx, rbx\n";
                Output << "    jnz " << DgLabel << "\n";
                Output << "    cmp rcx, 0\n";
                Output << "    je " << WrLabel << "\n";
                Output << "    dec rdi\n";
                Output << "    mov byte [rdi], 45\n";
                Output << WrLabel << ":\n";
                Output << "    mov rax, 1\n";
                Output << "    mov rsi, rdi\n";
                Output << "    mov rdx, _numbuf + 20\n";
                Output << "    sub rdx, rdi\n";
                Output << "    mov rdi, 1\n";
                Output << "    syscall\n";
                Output << "    mov rax, 1\n";
                Output << "    mov rdi, 1\n";
                Output << "    mov rsi, " << OutOfBoundsErrorMessageData2 << "\n";
                Output << "    mov rdx, 29\n";
                Output << "    syscall\n";
                Output << "    mov rax, 60\n";
                Output << "    mov rdi, 1\n";
                Output << "    syscall\n";
                //     f33460_print_NullInt: ; begin function
                //     ; scope begin
                //     mov rax, [rsp + 8] ; load from stack
                //     ; discard result
                // ; inline assembly begin
                //     mov rbx , rax
                //     mov rdi , _numbuf + 20
                //     mov byte [ rdi ] , 0
                // ; inline assembly end
                // ; inline assembly begin
                //     mov rcx , 0
                //     cmp rbx , 0
                //     jge .Dg
                //     neg rbx
                //     mov rcx , 1
                // ; inline assembly end
                // ; inline assembly begin
                // .Dg:
                //     xor rdx , rdx
                //     mov rax , rbx
                //     mov rsi , 10
                //     div rsi
                //     add rdx , 48
                //     dec rdi
                //     mov byte [ rdi ] , dl
                //     mov rbx , rax
                //     test rbx , rbx
                //     jnz .Dg
                // ; inline assembly end
                // ; inline assembly begin
                //     cmp rcx , 0
                //     je .Wr
                //     dec rdi
                //     mov byte [ rdi ] , 45
                // ; inline assembly end
                // ; inline assembly begin
                // .Wr:
                //     mov rax , 1
                //     mov rsi , rdi
                //     mov rdx , _numbuf + 20
                //     sub rdx , rdi
                //     mov rdi , 1
                //     syscall
                // ; inline assembly end
                //     ; scope closed and locals destroyed
                //     mov rax, 0 ; null
                //     ret
                // ; end function f33460_print_NullInt
                Output << OutOfBoundsErrorLabel << ":\n";
                Output << "    mov rax, r9\n";
            }
            ObjectSymbol.TypeDesc.PointerDepth = false;
            Output << "    imul rax, " << SizeOfType(ObjectSymbol.TypeDesc) << "\n";
            Output << "    mov rax, [r8 + rax] ; load index\n";
        }
        else if (auto Assign = std::dynamic_pointer_cast<AssignmentExpression>(Expr))
        {
            CmplSymbol NameSymbol = ResolveSymbol(Assign->Name);
            CmplSymbol ValSymbol = ResolveSymbol(Assign->Value);

            if (NameSymbol.TypeDesc.Constant)
            {
                Throw(CompileError("immutable, cannot reassign", Error));
                return;
            }

            if (auto AccessExpr = std::dynamic_pointer_cast<MemberExpression>(Assign->Name))
            {
                CmplSymbol ObjectSymbol = ResolveSymbol(AccessExpr->Object);
                if (!ObjectSymbol.Class || !ObjectSymbol.Class->count(AccessExpr->Member))
                {
                    // ResolveSymbol() should throw
                    return;
                }

                GenerateExpression(AccessExpr->Object);
                Output << "    mov r8, rax ; save object pointer\n";
                GenerateExpression(Assign->Value);
                Output << "    mov QWORD [r8 + " << ObjectSymbol.Class->at(AccessExpr->Member).Offset << "], rax ; reassign object member\n";
            }
            else if (auto IndexExpr = std::dynamic_pointer_cast<IndexExpression>(Assign->Name))
            {
                CmplSymbol ObjectSymbol = ResolveSymbol(IndexExpr->Object);
                GenerateExpression(IndexExpr->Object);
                Output << "    mov r8, rax ; save heap pointer\n";
                GenerateExpression(IndexExpr->Index);
                ObjectSymbol.TypeDesc.PointerDepth = false;
                Output << "    imul rax, " << SizeOfType(ObjectSymbol.TypeDesc) << "\n";
                Output << "    mov r9, rax ; save offset\n";
                GenerateExpression(Assign->Value);
                Output << "    mov QWORD [r8 + r9], rax ; reassign pointer offset\n";
            }
            else if (auto UnExpr = std::dynamic_pointer_cast<UnaryExpression>(Assign->Name))
            {
                switch (UnExpr->Operator)
                {

                default:
                    Throw(CompileError("invalid unary assignment", Fatal));
                    return;
                }
            }
            else if (!NameSymbol.Var)
            {
                Throw(CompileError("invalid variable to assign to (if you forced unwrapped change it to `x = x! + 1`)", Fatal));
                return;
            }
            else
            {
                Output << "    mov rax, QWORD [rsp + " << StackSize - NameSymbol.Var->StackLoc - SizeOfType(NameSymbol.Var->TypeDesc) << "]; load old value to decrement refcount\n";
                Output << "    mov r8, rax\n";
                DEC_REF_COUNT(NameSymbol.Var->TypeDesc);
                GenerateExpression(Assign->Value);
                Output << "    mov r9, rax\n";
                INC_REF_COUNT(NameSymbol.Var->TypeDesc);
                Output << "    mov rax, r8\n";
                GarbageCollectObject(NameSymbol);
                Output << "    mov rax, r9\n";
                Output << "    mov QWORD [rsp + " << StackSize - NameSymbol.Var->StackLoc - SizeOfType(NameSymbol.Var->TypeDesc) << "], rax ; reassign stack\n";
                Output << "    ; result is already in rax\n";
            }

            if (!CompileTypeMatch(ValSymbol.TypeDesc, NameSymbol.TypeDesc))
            {
                Throw(CompileError("assignment type mismatch", Error));
            }
        }
        else if (auto Call = std::dynamic_pointer_cast<CallExpression>(Expr))
        {
            CmplSymbol Symbol = ResolveSymbol(Call->Callee);

            if (Symbol.Funcs)
            {
                auto Func = CalculateBestOverload(Symbol.Funcs, Call, true);
                if (!Func)
                    return;

                for (int i = 0; i < Call->Arguments.size(); i++)
                {
                    ExpressionPtr Arg = Call->Arguments.at(i);
                    CmplSymbol ArgSymbol = ResolveSymbol(Arg);

                    GenerateExpression(Arg);
                    Push("rax", SizeOfType(ArgSymbol.TypeDesc));
                }

                Output << "    call " << MangleFunctionSignature(*Func) << "\n";
                Output << "    mov r9, rax ; save return data\n";

                Output << "    ; cleanup arguments\n";
                for (size_t i = 0; i < Call->Arguments.size(); i++)
                {
                    ExpressionPtr Arg = Call->Arguments.at(i);
                    CmplSymbol ArgSymbol = ResolveSymbol(Arg);
                    
                    Pop("rax", SizeOfType(ArgSymbol.TypeDesc));
                    GarbageCollectObject(ArgSymbol);
                }

                Output << "    mov rax, r9\n";
                Output << "    ; return data is in rax\n";

                return;
            }
            else
            {
                Throw(CompileError("not a valid function to call", Error));
                return;
            }

            if (Symbol.Funcs->size() == 1)
            {
                if (Call->Arguments.size() != Symbol.Funcs->at(0)->Arguments.size())
                    Throw(CompileError("function expects " + std::to_string(Symbol.Funcs->at(0)->Arguments.size()) + " arguments but " + std::to_string(Call->Arguments.size()) + ((Call->Arguments.size() == 1) ? " was provided" : " were provided"), Error));
                else
                    Throw(CompileError("arguments and function parameter types mismatch", Error));
            }
            else
            {
                Throw(CompileError("no overload of the function matches", Error));
            }
        }
        else if (auto NewExpr = std::dynamic_pointer_cast<UseExpression>(Expr))
        {
            CmplSymbol Symbol = ResolveSymbol(NewExpr);

            if (NewExpr->Type.PointerDepth)
            {
                const CmplSymbol &AllocSize = ResolveSymbol(NewExpr->Arguments.at(0));
                if (!CompileTypeMatch(AllocSize.TypeDesc, ValueType::Long))
                {
                    Throw(CompileError("new[] operator size expects a number", Error));
                }

                TypeDescriptor ElementType = NewExpr->Type;
                ElementType.PointerDepth = false;
                Output << "    ; allocate memory space for an array\n";
                GenerateExpression(NewExpr->Arguments.at(0));
                Output << "    mov rbx, rax ; save array size\n";
                Output << "    imul rax, " << SizeOfType(ElementType) << "\n";
                Output << "    add rax, 16 ; space for the array size to be stored\n";
                Output << "    mov rsi, rax ; size\n";
                Output << "    mov rax, 9       ; mmap\n";
                Output << "    mov rdi, 0       ; addr\n";
                Output << "    mov rdx, 3       ; PROT_READ|PROT_WRITE\n";
                Output << "    mov r10, 34      ; MAP_PRIVATE|MAP_ANONYMOUS\n";
                Output << "    mov r8, -1       ; fd\n";
                Output << "    mov r9, 0        ; offset\n";
                Output << "    syscall\n";
                Output << "    mov QWORD [rax + 0], 0 ; store reference count\n";
                Output << "    mov QWORD [rax + 8], rbx ; store array size\n";
                Output << "    add rax, 16 ; above array size\n";
            }
            else
            {
                Output << "    ; allocate an object\n";
                Output << "    mov rax, 9       ; mmap\n";
                Output << "    mov rdi, 0       ; addr\n";
                Output << "    mov rsi, " << Symbol.Class->at("*ClassSize").Offset << " ; size in bytes\n";
                Output << "    mov rdx, 3       ; PROT_READ|PROT_WRITE\n";
                Output << "    mov r10, 34      ; MAP_PRIVATE|MAP_ANONYMOUS\n";
                Output << "    mov r8, -1       ; fd\n";
                Output << "    mov r9, 0        ; offset\n";
                Output << "    syscall\n";
            }
        }
        else if (auto _SizeOfType = std::dynamic_pointer_cast<SizeOfTypeExpression>(Expr))
        {
            Output << "    mov rax, " << SizeOfType(_SizeOfType->Type) << " ; size of type\n";
        }
        else if (auto SizeOf = std::dynamic_pointer_cast<SizeOfExpression>(Expr))
        {
            CmplSymbol ObjectSymbol = ResolveSymbol(SizeOf->Expr);

            if (ObjectSymbol.TypeDesc.Nullable)
            {
                Throw(CompileError("pointer was not unwrapped in index expression (add sizeof(pointer!))", Error));
            }

            if (!ObjectSymbol.TypeDesc.PointerDepth)
            {
                Throw(CompileError("sizeof() operator expects a pointer type", Error));
            }

            GenerateExpression(SizeOf->Expr);
            Output << "    mov rax, [rax - 8] ; load array size for the sizeof() op\n";
        }
        else if (auto Cast = std::dynamic_pointer_cast<ClassCastExpression>(Expr))
        {
            GenerateExpression(Cast->Expr);
        }
        else if (auto Bin = std::dynamic_pointer_cast<BinaryExpression>(Expr))
        {
            const CmplSymbol &SymbolA = ResolveSymbol(Bin->A);
            const CmplSymbol &SymbolB = ResolveSymbol(Bin->B);

            Output << "    ; operand a\n";
            GenerateExpression(Bin->A);
            Output << "    mov rcx, rax\n";
            Output << "    ; operand b\n";
            GenerateExpression(Bin->B);

            if (SymbolA.TypeDesc.Nullable || SymbolB.TypeDesc.Nullable)
            {
                Throw(CompileError("an operand of the binary expression is nullable", Error));
            }

            switch (Bin->Operator)
            {
            case OperationType::Add:
                Output << "    add rcx, rax\n";
                Output << "    mov rax, rcx ; binary op result in rax\n";
                break;
            case OperationType::Subtract:
                Output << "    sub rcx, rax\n";
                Output << "    mov rax, rcx ; binary op result in rax\n";
                break;
            case OperationType::Multiply:
                Output << "    imul rcx, rax\n";
                Output << "    mov rax, rcx ; binary op result in rax\n";
                break;
            case OperationType::GreaterThan:
                Output << "    cmp rcx, rax\n";
                Output << "    mov rax, 0\n";
                Output << "    mov rcx, 1\n";
                Output << "    cmovg rax, rcx\n";
                break;
            case OperationType::LessThan:
                Output << "    cmp rcx, rax\n";
                Output << "    mov rax, 0\n";
                Output << "    mov rcx, 1\n";
                Output << "    cmovl rax, rcx\n";
                break;
            case OperationType::GreaterThanOrEqualTo:
                Output << "    cmp rcx, rax\n";
                Output << "    mov rax, 0\n";
                Output << "    mov rcx, 1\n";
                Output << "    cmovge rax, rcx\n";
                break;
            case OperationType::LessThanOrEqualTo:
                Output << "    cmp rcx, rax\n";
                Output << "    mov rax, 0\n";
                Output << "    mov rcx, 1\n";
                Output << "    cmovle rax, rcx\n";
                break;

            default:
                Throw(CompileError("TODO: binary op " + std::string(magic_enum::enum_name(Bin->Operator)) + " is not implemented", Error));
                break;
            }
        }
        else if (auto Un = std::dynamic_pointer_cast<UnaryExpression>(Expr))
        {
            CmplSymbol Symbol = ResolveSymbol(Un->Expr);

            GenerateExpression(Un->Expr);

            switch (Un->Operator)
            {
            case OperationType::Subtract:
                Output << "    mov rcx, 0\n";
                Output << "    sub rcx, rax\n";
                Output << "    mov rax, rcx\n";
                break;
            case OperationType::ForceUnwrap:
                if (!Symbol.TypeDesc.Nullable)
                    Throw(CompileError("force unwrap operator expects a nullable symbol", Error));
                break; // force unwrap does not change assembly output

            default:
                Throw(CompileError("TODO: unary operator '" + std::string(magic_enum::enum_name(Un->Operator)) + '\'', Error));
            }
        }
        else if (auto UnownedReference = std::dynamic_pointer_cast<UnownedReferenceExpression>(Expr))
        {
            Throw(CompileError("the unowned reference &operator cannot be used here", Error));
        }
    }

    void Push(const std::string &Register, const int64_t Size)
    {
        Output << "    push " << Register << "\n";
        StackSize += Size;
    }

    void Pop(const std::string &Register, const int64_t Size)
    {
        Output << "    pop " << Register << "\n";
        StackSize -= Size;
        if (StackSize < 0)
        {
            Throw(CompileError("stack underflow", Fatal));
        }
    }

    void Push(const int64_t Size)
    {
        StackSize += Size;
    }

    void Pop(const int64_t Size)
    {
        StackSize -= Size;
        if (StackSize < 0)
        {
            Throw(CompileError("stack underflow", Fatal));
        }
    }

    std::string CreateLabel()
    {
        return "l" + std::to_string(LabelCount++);
    }

    std::string CreateData(const std::string &Input)
    {
        std::string DataName = "d" + std::to_string(DataList.size());
        DataList.push_back(DataName + ' ' + Input);
        return DataName;
    }

public:
    std::string GenerateProgram()
    {
        Output << "\nsection .bss\n    _numbuf resb 21\nsection .text\n";

        for (const StatementPtr &Stmt : Ast)
        {
            GenerateStatement(Stmt);
        }

        for (auto &&[FuncId, FuncBody] : PendingFunctionDefinitions)
        {
            if (FunctionSignatureCache.at(FuncId).second > 0)
                Output << FuncBody;
        }

        Output << "section .data\n";
        Output << "StringBase:\n";

        for (auto &&Data : DataList)
        {
            Output << "    " << Data << "\n";
        }

        if (CurrentScope > 0)
        {
            Throw(CompileError("scope stack could not be closed", Fatal));
        }

        if (StackSize > 300)
        {
            Throw(CompileError("final stack overflown", Error));
        }

        bool Found = false;
        for (auto &&Var : Variables)
        {
            if (Var.Address != 1)
                continue;

            Found = true;
            break;
        }

        if (!Found)
        {
            Throw(CompileError("main() function could not be found", Warning));
        }

        std::string Result = Output.str();

        size_t CharPos = 0;
        while ((CharPos = Result.find("    ", CharPos)) != std::string::npos)
        {
            Result.replace(CharPos, 4, "\t");
        }

        return Result;
    }

    std::unordered_map<MapId, std::pair<std::string, size_t> /* <signature, refcount> */> FunctionSignatureCache;

    /*
     * function signature generator
     * with mangled names that are
     * easier to understand when debugging
     */
    std::string MangleFunctionSignature(const FunctionDefinition &Func, std::string OptionalFuncName = "function")
    {
        if (FunctionSignatureCache.count(Func.UniqueId))
        {
            FunctionSignatureCache.at(Func.UniqueId).second++;
            return FunctionSignatureCache.at(Func.UniqueId).first;
        }

        std::replace(OptionalFuncName.begin(), OptionalFuncName.end(), '-', '_');

        std::string Result = "f" + (std::to_string(Func.UniqueId)).substr(0, 5) + "_" + OptionalFuncName + "_" + std::string(magic_enum::enum_name(Func.ReturnType.Type));

        if (Func.ReturnType.Nullable && Func.ReturnType.Type != ValueType::Null)
            Result += "N";
        if (!Func.ReturnType.Constant)
            Result += "M";
        for (size_t i = 0; i < Func.ReturnType.PointerDepth; i++)
            Result += "P";

        for (auto &&Param : Func.Arguments)
        {
            Result += std::string(magic_enum::enum_name(Param.Type.Type));

            if (Param.Type.Nullable && Func.ReturnType.Type != ValueType::Null)
                Result += "N";
            if (!Param.Type.Constant)
                Result += "M";
            for (size_t i = 0; i < Func.ReturnType.PointerDepth; i++)
                Result += "P";
        }

        FunctionSignatureCache[Func.UniqueId] = std::make_pair(Result, 0);

        return Result;
    }

    std::shared_ptr<FunctionDefinition> CalculateBestOverload(std::shared_ptr<std::vector<std::shared_ptr<FunctionDefinition>>> Funcs, std::shared_ptr<CallExpression> Call, const bool Throws = false)
    {
        std::shared_ptr<FunctionDefinition> Best = nullptr;

        // Try strictest → loosest
        for (int Looseness = 0; Looseness <= 4; Looseness++)
        {
            std::shared_ptr<FunctionDefinition> Match = nullptr;

            for (auto &Func : *Funcs)
            {
                // Arg count mismatch → skip early
                if (Func->Arguments.size() != Call->Arguments.size())
                    continue;

                bool AllParamsMatch = true;

                for (size_t i = 0; i < Func->Arguments.size(); ++i)
                {
                    auto CallType = ResolveSymbol(Call->Arguments[i]).TypeDesc;
                    auto ParamType = Func->Arguments[i].Type;

                    if (!CompileTypeMatch(CallType, ParamType, Looseness))
                    {
                        AllParamsMatch = false;
                        break;
                    }
                }

                if (!AllParamsMatch)
                    continue;

                // First match at this looseness
                if (!Match)
                {
                    Match = Func;
                }
                else
                {
                    // Second match → ambiguous
                    if (Throws)
                        Throw(CompileError("ambiguous call of overloaded function " + MangleFunctionSignature(*Match), Fatal));
                    return nullptr;
                }
            }

            // If we found a valid match at this looseness → return it
            if (Match)
                return Match;
        }

        // No overloads matched at any looseness
        if (Throws)
            Throw(CompileError("no overload of the function matches", Error));
        return nullptr;
    }
};
