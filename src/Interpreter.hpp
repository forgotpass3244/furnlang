#pragma once
#include "Ast.hpp"
#include "Common.hpp"
#include "Signal.hpp"

// forward declaration
struct MapReference;

using ExternalFunction = std::function<std::any(const std::vector<std::any> &)>;
using Library = std::unordered_map<std::string, std::any>;

class Function
{
public:
    std::vector<StatementPtr> Body;
    std::vector<VarDeclaration> Arguments;

    Function(std::vector<StatementPtr> body, std::vector<VarDeclaration> arguments = std::vector<VarDeclaration>())
        : Body(std::move(body)), Arguments(std::move(arguments)) {}
};

using Value = std::variant<std::nullptr_t, rt_Int, rt_Float, bool, std::string, Function, MapReference, ExternalFunction, Library>;

std::string ToString(const Value &val);
TypeDescriptor MakeTypeDescriptor(const Value &Value);

using Map = std::unordered_map<std::string, Value>;
using MapId = unsigned long long;
std::unordered_map<MapId, Map> StoredMaps;

struct MapReference
{
    MapId Id;
    TypeDescriptor ValType;

    MapReference(const MapReference &) = default;
    MapReference &operator=(const MapReference&) = default;
    MapReference(MapReference &&) noexcept = default;
    MapReference &operator=(MapReference&&) noexcept = default;
    ~MapReference() = default;

    explicit MapReference(MapId id, TypeDescriptor valtype)
        : Id(id), ValType(valtype) {}

    /* TODO: (DONE) Make the Get and Set methods
    set/get the key/values instead of
    the entire map */
    
    bool operator==(const MapReference &Other) const
    {
        return Id == Other.Id;
    }

    Map &GetLiteralMap()
    {
        return StoredMaps[Id];
    }

    Value Get(Value Key)
    {
        auto OuterIt = StoredMaps.find(Id);
        if (OuterIt == StoredMaps.end())
            return Value(nullptr); // Null map reference

        auto &InnerMap = OuterIt->second;
        auto InnerIt = InnerMap.find(ToString(Key));
        if (InnerIt == InnerMap.end())
            return Value(nullptr); // Key doesn't exist

        return InnerIt->second;
    }

    void Set(Value Key, Value Val)
    {
        if (CheckTypeMatch(ValueType::Unknown, ValType.Type))
            throw std::runtime_error("Type for map value is unknown");
        if (ValType != MakeTypeDescriptor(Val))
            throw std::runtime_error("Type mismatch for map value");

        StoredMaps[Id][ToString(Key)] = Val;
    }

    size_t Length() const
    {
        auto OuterIt = StoredMaps.find(Id);
        if (OuterIt == StoredMaps.end())
            return 0;

        return OuterIt->second.size();
    }
};

MapId RandomMapId(MapId x = 0, MapId y = ULLONG_MAX)
{
    static std::random_device Rd;
    static std::mt19937_64 gen(Rd());
    std::uniform_int_distribution<MapId> dist(x, y);
    return dist(gen);
}

TypeDescriptor MakeTypeDescriptor(const Value &Value)
{
    ValueType Type = ValueType::Null;

    if (std::holds_alternative<std::nullptr_t>(Value))
        Type = ValueType::Null;
    else if (std::holds_alternative<rt_Int>(Value))
        Type = ValueType::Int;
    else if (std::holds_alternative<rt_Float>(Value))
        Type = ValueType::Float;
    else if (std::holds_alternative<bool>(Value))
        Type = ValueType::Bool;
    else if (std::holds_alternative<std::string>(Value))
        Type = ValueType::String;
    else if (std::holds_alternative<MapReference>(Value))
        return TypeDescriptor(ValueType::Map, {std::get<MapReference>(Value).ValType});
    else if (std::holds_alternative<Function>(Value))
        Type = ValueType::Function;
    else if (std::holds_alternative<ExternalFunction>(Value))
        Type = ValueType::ExternalFunction;
    else if (std::holds_alternative<Library>(Value))
        Type = ValueType::Library;

    return TypeDescriptor(Type);
}

std::any ValueToAny(const Value &v)
{
    if (std::holds_alternative<MapReference>(v))
        return std::any(std::get<MapReference>(v));

    return std::visit([](auto &&Arg) -> std::any
                      { return std::any(Arg); }, v);
}

Value AnyToValue(const std::any &a)
{
    if (auto p = std::any_cast<std::nullptr_t>(&a))
        return *p;
    else if (auto p = std::any_cast<rt_Int>(&a))
        return *p;
    else if (auto p = std::any_cast<rt_Float>(&a))
        return *p;
    else if (auto p = std::any_cast<bool>(&a))
        return *p;
    else if (auto p = std::any_cast<std::string>(&a))
        return *p;
    else if (auto p = std::any_cast<const char*>(&a))
        return *p;
    else if (auto p = std::any_cast<Function>(&a))
        return *p;
    else if (auto p = std::any_cast<ExternalFunction>(&a))
        return *p;
    else if (auto p = std::any_cast<Library>(&a))
        return *p;
    else if (auto p = std::any_cast<MapReference>(&a))
        return *p;

    throw std::runtime_error("Type held is not supported");
}

std::string ToString(const Value &Val)
{
    const TypeDescriptor TypeDesc = MakeTypeDescriptor(Val);
    const ValueType Type = TypeDesc.Type;

    if (CheckTypeMatch(ValueType::Null, Type))
        return "null";
    else if (CheckTypeMatch(ValueType::Int, Type))
        return std::to_string(std::get<rt_Int>(Val));
    else if (CheckTypeMatch(ValueType::Float, Type))
        return std::to_string(std::get<rt_Float>(Val));
    else if (CheckTypeMatch(ValueType::Bool, Type))
        return std::get<bool>(Val) ? "true" : "false";
    else if (CheckTypeMatch(ValueType::String, Type))
        return std::get<std::string>(Val);
    else if (CheckTypeMatch(ValueType::Map, Type))
        return  "{...} Map " + std::to_string(std::get<MapReference>(Val).Id);
    else if (CheckTypeMatch(ValueType::Function, Type))
        return "Function";
    else if (CheckTypeMatch(ValueType::ExternalFunction, Type))
        return "ExternalFunction";
    else if (CheckTypeMatch(ValueType::Library, Type))
        return "Library";

    return "<unknown>";
}

std::string ToString(const std::any &Val)
{
    return ToString(AnyToValue(Val));
}

bool IsTruthy(const Value &Val)
{
    const ValueType Type = MakeTypeDescriptor(Val).Type;

    if (CheckTypeMatch(ValueType::Null, Type))
        return false;
    else if (CheckTypeMatch(ValueType::Bool, Type))
        return std::get<bool>(Val);
    
    // Anything else is considered truthy
    return true;
}

struct Variable
{
    Value ValueData;
    TypeDescriptor Type;

    Variable() : ValueData(nullptr), Type(TypeDescriptor(ValueType::Int)) {};
    Variable(Value valuedata, TypeDescriptor type) : ValueData(valuedata), Type(type) {};
};

class Interpreter
{
public:
    void Execute(const std::vector<StatementPtr> &Program)
    {
        for (const auto &Stmt : Program)
            ExecuteStatement(Stmt);

        MapReference ProgramArgv(RandomMapId(), TypeDescriptor(ValueType::String));
        for (const auto &s : argv)
            ProgramArgv.Set(rt_Int(ProgramArgv.Length()), s);

        ExpressionPtr ArgvExpr = std::make_shared<ValueExpression>(std::any(ProgramArgv));

        Function MainFunc = std::get<Function>(LookupVariable("Main"));
        ExecuteCall(CallExpression(
            std::make_shared<ValueExpression>(MainFunc),
            (MainFunc.Arguments.size() > 0) ? std::vector<ExpressionPtr>({ArgvExpr}) : std::vector<ExpressionPtr>()
        ));
    }

    std::unordered_map<std::string, Variable> GlobalScope;

private:
    std::vector<std::unordered_map<std::string, Variable>> LocalScopes;

    void PushLocalScope()
    {
        LocalScopes.emplace_back(); // Adds a new empty scope at the end
    }

    void PopLocalScope()
    {
        if (!LocalScopes.empty())
            LocalScopes.pop_back(); // Removes the most recent scope
        else
            throw std::runtime_error("No local scope to pop.");
    }

    std::unordered_map<std::string, Variable> &CurrentLocalScope()
    {
        if (LocalScopes.empty())
            throw std::runtime_error("No current local scope.");
        return LocalScopes.back(); // Returns the top/most recent scope
    }

    void ExecuteStatement(const StatementPtr &Statement)
    {
        if (auto VarDecl = std::dynamic_pointer_cast<VarDeclaration>(Statement))
            ExecuteVarDeclaration(*VarDecl);
        else if (auto assign = std::dynamic_pointer_cast<AssignmentStatement>(Statement))
            ExecuteAssignment(*assign);
        else if (auto If = std::dynamic_pointer_cast<IfStatement>(Statement))
            ExecuteIf(*If);
        else if (auto While = std::dynamic_pointer_cast<WhileStatement>(Statement))
            ExecuteWhile(*While);
        else if (auto For = std::dynamic_pointer_cast<ForStatement>(Statement))
            ExecuteFor(*For);
        else if (auto ExprStmt = std::dynamic_pointer_cast<ExpressionStatement>(Statement))
            EvaluateExpression(ExprStmt->Expr); // execute expression (like function call)
        else if (auto UseStmt = std::dynamic_pointer_cast<UseStatement>(Statement))
        {
            if (UseStmt->UseLibrary)
            {
                Library Lib = std::get<Library>(EvaluateExpression(UseStmt->Expr));
                for (auto &[Key, _Val] : Lib)
                {
                    Value Val = AnyToValue(_Val);
                    Variable Var(Val, MakeTypeDescriptor(Val));

                    if (LocalScopes.empty())
                        GlobalScope[Key] = Var;
                    else
                        CurrentLocalScope()[Key] = Var;
                }
            }
            else
            {
                if (auto AccessExpr = std::dynamic_pointer_cast<AccessExpression>(UseStmt->Expr))
                {
                    const std::string Name = AccessExpr->ToAccess;

                    Value Val = EvaluateExpression(AccessExpr);
                    Variable Var(Val, MakeTypeDescriptor(Val));

                    if (LocalScopes.empty())
                        GlobalScope[Name] = Var;
                    else
                        CurrentLocalScope()[Name] = Var;
                }
            }
        }
        else if (auto Return = std::dynamic_pointer_cast<ReturnStatement>(Statement))
            throw Signal(SignalType::Return, ValueToAny(EvaluateExpression(Return->Expr)));
        else if (Statement)
            throw std::runtime_error("Unknown statement type.");
    }

    void ExecuteVarDeclaration(const VarDeclaration &Decl)
    {
        Value ValueResult = EvaluateExpression(Decl.Initializer);

        if (!CheckTypeMatch(ValueType::Null, MakeTypeDescriptor(ValueResult).Type))
        {
            TypeDescriptor Type = MakeTypeDescriptor(ValueResult);
            if (Type != Decl.Type)
                throw std::runtime_error("Type mismatch");
        }

        Variable Var{ValueResult, Decl.Type};

        if (LocalScopes.empty())
            GlobalScope[Decl.Name] = Var;
        else
            CurrentLocalScope()[Decl.Name] = Var;
    }

    void ExecuteAssignment(const AssignmentStatement &Assign)
    {
        Value Val = EvaluateExpression(Assign.Value);
        TypeDescriptor Type = MakeTypeDescriptor(Val);

        // Walk through local scopes from innermost (back) to outermost (front)
        for (auto It = LocalScopes.rbegin(); It != LocalScopes.rend(); ++It)
        {
            if (It->count(Assign.VariableName))
            {
                Variable &Var = (*It)[Assign.VariableName];

                if (!CheckTypeMatch(ValueType::Null, Type.Type))
                {
                    if (Type != Var.Type)
                        throw std::runtime_error("Type mismatch");
                }

                Var.ValueData = Val;
                return;
            }
        }

        if (GlobalScope.count(Assign.VariableName))
        {
            Variable &Var = GlobalScope[Assign.VariableName];

            if (!CheckTypeMatch(ValueType::Null, Type.Type))
            {
                if (Type != Var.Type)
                    throw std::runtime_error("Type mismatch");
            }

            Var.ValueData = Val;
            return;
        }

        throw std::runtime_error("Assignment to undeclared variable: " + Assign.VariableName);
    }

    Value ExecuteCall(const CallExpression &Expr)
    {
        Value ValueResult = EvaluateExpression(Expr.Callee);

        if (std::holds_alternative<Function>(ValueResult))
        {
            Function Func = std::get<Function>(ValueResult);

            PushLocalScope();
            
            // Handle arguments

            if (Expr.Arguments.size() != Func.Arguments.size())
                throw std::runtime_error("Parameter to argument number mismatch");

            size_t i = 0;
            for (const auto &Decl : Func.Arguments)
            {
                const ExpressionPtr &ArgumentExpr = Expr.Arguments[i];
                Value ArgumentResult = EvaluateExpression(ArgumentExpr);

                if (MakeTypeDescriptor(ArgumentResult) != Decl.Type)
                    throw std::runtime_error("Type mismatch for function argument");

                CurrentLocalScope()[Decl.Name] = Variable(ArgumentResult, Decl.Type);
                i++;
            }

            try
            {
                for (const auto &Statement : Func.Body)
                    ExecuteStatement(Statement);
                throw Signal(SignalType::Return);
            }
            catch (Signal &Signal)
            {
                PopLocalScope();
                if (Signal.Type != SignalType::Return)
                    throw std::runtime_error("Invalid signal found when executing function");
                return AnyToValue(Signal.Data);
            }
        }
        else if (std::holds_alternative<ExternalFunction>(ValueResult))
        {
            ExternalFunction ExternalFunc = std::get<ExternalFunction>(ValueResult);

            std::vector<std::any> Arguments;
            for (auto &&ArgExpr : Expr.Arguments)
            {
                Arguments.push_back(ValueToAny(EvaluateExpression(ArgExpr)));
            }

            return AnyToValue(ExternalFunc(Arguments));
        }
        else
            throw std::runtime_error("Unsupported call type.");
    }

    Value ExecuteIndex(const IndexExpression &Expr)
    {
        Value ValueResult = EvaluateExpression(Expr.Object);
        Value IndexResult = EvaluateExpression(Expr.Index);

        if (std::holds_alternative<MapReference>(ValueResult))
        {
            MapReference Map = std::get<MapReference>(ValueResult);
            return Map.Get(IndexResult);
        }
        else
            throw std::runtime_error("Unsupported index type.");
    }

    Value ExecuteAccess(const AccessExpression &Expr)
    {
        Value ValueResult = EvaluateExpression(Expr.Object);

        if (std::holds_alternative<Library>(ValueResult))
        {
            Library Lib = std::get<Library>(ValueResult);
            return AnyToValue(Lib[Expr.ToAccess]);
        }
        else
            throw std::runtime_error("Unsupported access type.");
    }

    void ExecuteIf(const IfStatement &If)
    {
        for (size_t i = 0; i < If.Conditions.size(); ++i)
        {
            Value ConditionResult = EvaluateExpression(If.Conditions[i]);

            bool ConditionIsTruthy = IsTruthy(ConditionResult);
            if (ConditionIsTruthy)
            {
                PushLocalScope();
                for (const auto &Statement : If.Then[i])
                    ExecuteStatement(Statement);
                PopLocalScope();
                break; // Stop after the first truthy block
            }
        }
    }

    void ExecuteWhile(const WhileStatement &While)
    {
        while (IsTruthy(EvaluateExpression(While.Condition)))
        {
            PushLocalScope();
            for (const auto &Statement : While.Body)
                ExecuteStatement(Statement);
            PopLocalScope();
        }
    }

    void ExecuteFor(const ForStatement &For)
    {
        Value IterValue = EvaluateExpression(For.Iter);
        Map *IterMap = nullptr;

        if (std::holds_alternative<MapReference>(IterValue))
            IterMap = &std::get<MapReference>(IterValue).GetLiteralMap();
        else
            throw std::runtime_error("For loop value is not iterable.");

        if (CheckTypeMatch(ValueType::Int, For.KeyType.Type))
        {
            std::vector<std::pair<std::string, Value>> Entries(IterMap->begin(), IterMap->end());
            std::sort(Entries.begin(), Entries.end(), [](const auto &A, const auto &B)
                      {
            auto toInt = [](const std::string &s) {
                try { return std::stoi(s); } catch (...) { return 0; }
            };
            return toInt(A.first) < toInt(B.first); });

            for (auto &[KeyStr, Val] : Entries)
            {
                rt_Int KeyInt;
                try
                {
                    KeyInt = std::stoi(KeyStr);
                }
                catch (...)
                {
                    KeyInt = 0;
                }
                PushLocalScope();
                CurrentLocalScope()[For.KeyName] = Variable(Value(KeyInt), TypeDescriptor(ValueType::Int));
                CurrentLocalScope()[For.ValName] = Variable(Val, MakeTypeDescriptor(Val));
                for (const auto &Stmt : For.Body)
                    ExecuteStatement(Stmt);
                PopLocalScope();
            }
        }
        else
        {
            for (auto &[Key, Val] : *IterMap)
            {
                PushLocalScope();

                if (MakeTypeDescriptor(Value(Key)) != For.KeyType)
                    throw std::runtime_error("Iterator key type must match.");

                CurrentLocalScope()[For.KeyName] = Variable(Value(Key), For.KeyType);
                CurrentLocalScope()[For.ValName] = Variable(Val, MakeTypeDescriptor(Val));

                for (const auto &Stmt : For.Body)
                    ExecuteStatement(Stmt);
                PopLocalScope();
            }
        }
    }

    Value EvaluateExpression(const ExpressionPtr &Expr)
    {
        if (auto Val = std::dynamic_pointer_cast<ValueExpression>(Expr))
            return AnyToValue(Val->Val);
        else if (auto Var = std::dynamic_pointer_cast<VariableExpression>(Expr))
        {
            return LookupVariable(Var->Name);
        }
        else if (auto Map = std::dynamic_pointer_cast<MapExpression>(Expr))
        {
            MapReference NewMap(RandomMapId(), Map->ValType);

            for (auto &[k, v] : Map->KV_Expressions)
            {
                Value Key = EvaluateExpression(k);
                NewMap.Set(Key, EvaluateExpression(v));
            }

            return NewMap;
        }
        else if (auto Call = std::dynamic_pointer_cast<CallExpression>(Expr))
            return ExecuteCall(*Call);
        else if (auto Index = std::dynamic_pointer_cast<IndexExpression>(Expr))
        {
            return ExecuteIndex(*Index);
        }
        else if (auto Access = std::dynamic_pointer_cast<AccessExpression>(Expr))
        {
            return ExecuteAccess(*Access);
        }
        else if (auto Func = std::dynamic_pointer_cast<FunctionDefinition>(Expr))
        {
            return Function(Func->Body, Func->Arguments);
        }
        else if (auto Lib = std::dynamic_pointer_cast<LibraryDefinition>(Expr))
        {
            Library NewLibrary;

            for (const auto &Statement : Lib->Definition)
            {
                if (auto Decl = std::dynamic_pointer_cast<VarDeclaration>(Statement))
                {
                    Value Val = EvaluateExpression(Decl->Initializer);
                    NewLibrary[Decl->Name] = ValueToAny(Val);
                }
            }

            return NewLibrary;
        }
        else if (auto Bin = std::dynamic_pointer_cast<BinaryExpression>(Expr))
        {
            Value A = EvaluateExpression(Bin->A);
            Value B = EvaluateExpression(Bin->B);

            std::function ToFloat = [](const Value &Val) -> rt_Float
            {
                const ValueType &Type = MakeTypeDescriptor(Val).Type;

                if (CheckTypeMatch(ValueType::Int, Type))
                    return std::get<rt_Int>(Val);
                else if (CheckTypeMatch(ValueType::Float, Type))
                    return std::get<rt_Float>(Val);

                return 0;
            };

            rt_Float X = ToFloat(A);
            rt_Float Y = ToFloat(B);

            std::function ToNumber = [&A, &B](const rt_Float &Val) -> Value
            {
                bool xIsInt = CheckTypeMatch(ValueType::Int, MakeTypeDescriptor(A).Type);
                bool yIsInt = CheckTypeMatch(ValueType::Int, MakeTypeDescriptor(B).Type);

                if (xIsInt && yIsInt)
                    return int(Val);

                return Val;
            };

            switch (Bin->Operator)
            {
            case OperationType::Add:
                return ToNumber(X + Y);
            case OperationType::Subtract:
                return ToNumber(X - Y);
            case OperationType::Multiply:
                return ToNumber(X * Y);
            case OperationType::Divide:
                return ToNumber(X / Y);

            case OperationType::Equality:
                if (MakeTypeDescriptor(A) != MakeTypeDescriptor(B))
                    return false;
                return ToString(A) == ToString(B);
            case OperationType::Inequality:
                if (MakeTypeDescriptor(A) != MakeTypeDescriptor(B))
                    return true;
                return ToString(A) != ToString(B);
            default:
                throw std::runtime_error("Invalid binary expression.");
            }
        }
        
        throw std::runtime_error("Unknown expression type.");
    }

    Value LookupVariable(const std::string &Name)
    {
        // Search from the top (most recent scope) to the bottom
        for (auto it = LocalScopes.rbegin(); it != LocalScopes.rend(); ++it)
        {
            if (it->count(Name))
                return it->at(Name).ValueData;
        }

        // Check global scope
        if (GlobalScope.count(Name))
            return GlobalScope.at(Name).ValueData;

        throw std::runtime_error("Undefined variable: " + Name);
    }
};