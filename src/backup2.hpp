#pragma once
#include "Ast.hpp"
#include "Common.hpp"

// forward declaration
struct MapReference;

using ExternalFunction = std::function<std::any(const std::vector<std::any> &)>;
using Library = std::unordered_map<std::string, std::any>;

using Value = std::variant<std::nullptr_t, int, double, bool, std::string, FunctionDefinition, MapReference, ExternalFunction, Library>;

std::any ValueToAny(const Value &v)
{
    return std::visit([](auto &&Arg) -> std::any
                      { return std::any(Arg); }, v);
}

ValueType GetValueType(const Value &Value)
{
    ValueType Type;

    if (std::holds_alternative<std::nullptr_t>(Value))
        Type = ValueType::Null;
    else if (std::holds_alternative<int>(Value))
        Type = ValueType::Int;
    else if (std::holds_alternative<double>(Value))
        Type = ValueType::Float;
    else if (std::holds_alternative<bool>(Value))
        Type = ValueType::Bool;
    else if (std::holds_alternative<std::string>(Value))
        Type = ValueType::String;
    else if (std::holds_alternative<MapReference>(Value))
        Type = ValueType::Map;
    else if (std::holds_alternative<FunctionDefinition>(Value))
        Type = ValueType::Function;
    else if (std::holds_alternative<ExternalFunction>(Value))
        Type = ValueType::ExternalFunction;
    else if (std::holds_alternative<Library>(Value))
        Type = ValueType::Library;

    return Type;
}

std::string ToString(const Value &val);

using Map = std::unordered_map<std::string, Value>;
using MapId = unsigned long long;
std::unordered_map<MapId, Map> StoredMaps;

struct MapReference
{
    MapId Id;
    ValueType ValType = ValueType::Unknown;

    MapReference(const MapReference &) = default;
    MapReference &operator=(const MapReference &) = default;

    explicit MapReference(MapId id, ValueType valtype)
        : Id(id), ValType(valtype) {}

    /* TODO: (DONE) Make the Get and Set methods
    set/get the key/values instead of
    the entire map */

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
        if (CheckTypeMatch(ValueType::Unknown, ValType))
            throw std::runtime_error("Type for map value is unknown");
        if (!CheckTypeMatch(ValType, GetValueType(Val)))
            throw std::runtime_error("Type mismatch for map value");

        StoredMaps[Id][ToString(Key)] = Val;
    }

    bool operator==(const MapReference &Other) const
    {
        return Id == Other.Id;
    }
};

MapId RandomMapId(MapId x = 0, MapId y = ULLONG_MAX)
{
    static std::random_device Rd;
    static std::mt19937_64 gen(Rd());
    std::uniform_int_distribution<MapId> dist(x, y);
    return dist(gen);
}

Value AnyToValue(const std::any &a)
{
    if (auto p = std::any_cast<std::nullptr_t>(&a))
        return *p;
    else if (auto p = std::any_cast<int>(&a))
        return *p;
    else if (auto p = std::any_cast<double>(&a))
        return *p;
    else if (auto p = std::any_cast<bool>(&a))
        return *p;
    else if (auto p = std::any_cast<std::string>(&a))
        return *p;
    else if (auto p = std::any_cast<FunctionDefinition>(&a))
        return *p;
    else if (auto p = std::any_cast<ExternalFunction>(&a))
        return *p;
    else if (auto p = std::any_cast<Library>(&a))
        return *p;

    throw std::runtime_error("Type held is not supported");
}

std::string ToString(const Value &val)
{
    const ValueType Type = GetValueType(val);

    if (CheckTypeMatch(ValueType::Null, Type))
        return "null";
    else if (CheckTypeMatch(ValueType::Int, Type))
        return std::to_string(std::get<int>(val));
    else if (CheckTypeMatch(ValueType::Float, Type))
        return std::to_string(std::get<double>(val));
    else if (CheckTypeMatch(ValueType::Bool, Type))
        return std::get<bool>(val) ? "true" : "false";
    else if (CheckTypeMatch(ValueType::String, Type))
        return std::get<std::string>(val);
    else if (CheckTypeMatch(ValueType::Map, Type))
        return "Map<" + std::to_string(std::get<MapReference>(val).Id) + ">";
    else if (CheckTypeMatch(ValueType::Function, Type))
        return "Function";
    else if (CheckTypeMatch(ValueType::ExternalFunction, Type))
        return "ExternalFunction";

    return "<unknown>";
}

std::string ToString(const std::any &val)
{
    return ToString(AnyToValue(val));
}

struct Variable
{
    Value ValueData;
    ValueType Type;

    Variable() : ValueData(nullptr), Type(ValueType::Int) {};
    Variable(Value valuedata, ValueType type) : ValueData(valuedata), Type(type) {};
};

class Interpreter
{
public:
    void Execute(const std::vector<StatementPtr> &Program)
    {
        for (const auto &Statement : Program)
        {
            // std::cout << Statement << std::endl;
            ExecuteStatement(Statement);
        }
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
        {
            ExecuteVarDeclaration(*VarDecl);
        }
        else if (auto assign = std::dynamic_pointer_cast<AssignmentStatement>(Statement))
        {
            ExecuteAssignment(*assign);
        }
        else if (auto If = std::dynamic_pointer_cast<IfStatement>(Statement))
        {
            ExecuteIf(*If);
        }
        else if (auto For = std::dynamic_pointer_cast<ForStatement>(Statement))
        {
            ExecuteFor(*For);
        }
        else if (auto ExprStmt = std::dynamic_pointer_cast<ExpressionStatement>(Statement))
        {
            EvaluateExpression(ExprStmt->Expr); // execute expression (like function call)
        }
        else
        {
            // throw std::runtime_error("Unknown statement type.");
        }
    }

    void ExecuteVarDeclaration(const VarDeclaration &Decl)
    {
        Value ValueResult = EvaluateExpression(Decl.Initializer);

        if (!CheckTypeMatch(ValueType::Null, GetValueType(ValueResult)))
        {
            ValueType Type = GetValueType(ValueResult);
            if (!CheckTypeMatch(Decl.Type, Type))
                throw std::runtime_error("Type mismatch");
        }

        Variable Var{ValueResult, Decl.Type};

        if (LocalScopes.empty())
        {
            GlobalScope[Decl.Name] = Var;
        }
        else
        {
            CurrentLocalScope()[Decl.Name] = Var;
        }
    }

    void ExecuteAssignment(const AssignmentStatement &Assign)
    {
        Value Val = EvaluateExpression(Assign.Value);
        ValueType Type = GetValueType(Val);

        // Walk through local scopes from innermost (back) to outermost (front)
        for (auto It = LocalScopes.rbegin(); It != LocalScopes.rend(); ++It)
        {
            if (It->count(Assign.VariableName))
            {
                Variable &Var = (*It)[Assign.VariableName];

                if (!CheckTypeMatch(ValueType::Null, Type))
                {
                    if (!CheckTypeMatch(Var.Type, Type))
                        throw std::runtime_error("Type mismatch");
                }

                Var.ValueData = Val;
                return;
            }
        }

        if (GlobalScope.count(Assign.VariableName))
        {
            Variable &Var = GlobalScope[Assign.VariableName];

            if (!CheckTypeMatch(ValueType::Null, Type))
            {
                if (!CheckTypeMatch(Var.Type, Type))
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

        if (std::holds_alternative<FunctionDefinition>(ValueResult))
        {
            FunctionDefinition Func = std::get<FunctionDefinition>(ValueResult);

            PushLocalScope();

            Value This = AnyToValue(Func.This);
            CurrentLocalScope()["lib"] = Variable(This, GetValueType(This));

            for (const auto &Statement : Func.Body)
            {
                ExecuteStatement(Statement);
            }
            PopLocalScope();

            return nullptr;
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
        {
            std::cerr << "ExecuteCall: Unexpected call type: variant holds type index = " << ValueResult.index() << std::endl;
            throw std::runtime_error("Unsupported call type.");
        }
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
        Value ConditionResult = EvaluateExpression(If.Condition);
        bool IsTruthy;
        if (std::holds_alternative<std::nullptr_t>(ConditionResult))
            IsTruthy = false;
        else if (std::holds_alternative<int>(ConditionResult))
            IsTruthy = true;
        else if (std::holds_alternative<bool>(ConditionResult))
            IsTruthy = std::get<bool>(ConditionResult);
        else
            throw std::runtime_error("Invalid condition.");

        if (IsTruthy)
        {
            PushLocalScope();
            for (const auto &Statement : If.ThenBranch)
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

        if (CheckTypeMatch(ValueType::Int, For.KeyType))
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
                int KeyInt;
                try
                {
                    KeyInt = std::stoi(KeyStr);
                }
                catch (...)
                {
                    KeyInt = 0;
                }
                PushLocalScope();
                CurrentLocalScope()[For.KeyName] = Variable(Value(KeyInt), ValueType::Int);
                CurrentLocalScope()[For.ValName] = Variable(Val, GetValueType(Val));
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

                if (!CheckTypeMatch(For.KeyType, GetValueType(Value(Key))))
                    throw std::runtime_error("Iterator key type must match.");

                CurrentLocalScope()[For.KeyName] = Variable(Value(Key), For.KeyType);
                CurrentLocalScope()[For.ValName] = Variable(Val, GetValueType(Val));

                for (const auto &Stmt : For.Body)
                    ExecuteStatement(Stmt);
                PopLocalScope();
            }
        }
    }

    Value EvaluateExpression(const ExpressionPtr &Expr)
    {
        if (std::dynamic_pointer_cast<NullExpression>(Expr))
            return nullptr;
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
        else if (auto Num = std::dynamic_pointer_cast<NumberExpression>(Expr))
        {
            return Num->Value;
        }
        else if (auto Str = std::dynamic_pointer_cast<StringExpression>(Expr))
        {
            return Str->Value;
        }
        else if (auto Bool = std::dynamic_pointer_cast<BooleanExpression>(Expr))
        {
            return Bool->Value;
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
            return Func->Body;
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

            for (auto &[Key, _Val] : NewLibrary)
            {
                Value Val = AnyToValue(_Val);
                if (CheckTypeMatch(ValueType::Function, GetValueType(Val)))
                {
                    FunctionDefinition Raw = std::get<FunctionDefinition>(Val);
                    if (Raw.This.type() == typeid(nullptr))
                    {
                        Raw.This = NewLibrary;
                        NewLibrary[Key] = Raw;
                    }
                }
            }

            return NewLibrary;
        }
        else if (auto Bin = std::dynamic_pointer_cast<BinaryExpression>(Expr))
        {
            Value LeftVal = EvaluateExpression(Bin->Left);
            Value RightVal = EvaluateExpression(Bin->Right);
            if (std::holds_alternative<int>(LeftVal) && std::holds_alternative<int>(RightVal))
            {
                int L = std::get<int>(LeftVal);
                int R = std::get<int>(RightVal);
                if (Bin->Operator == "==")
                {
                    return L == R;
                }
            }
            throw std::runtime_error("Invalid binary expression.");
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
        {
            return GlobalScope.at(Name).ValueData;
        }

        const std::string LibName = "lib";
        for (auto it = LocalScopes.rbegin(); it != LocalScopes.rend(); ++it)
        {
            if (it->count(LibName))
            {
                Value _MyLibrary = it->at(LibName).ValueData;
                Library MyLibrary = std::get<Library>(_MyLibrary);
                return AnyToValue(MyLibrary[Name]);
            }
        }

        throw std::runtime_error("Undefined variable: " + Name);
    }
};