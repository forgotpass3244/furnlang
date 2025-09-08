#pragma once
#include "Ast.hpp"
#include "Common.hpp"
#include "Signal.hpp"

// forward declaration
struct MapReference;

using ExternalFunction = std::function<std::any(const std::vector<std::any> &)>;
using Library = std::unordered_map<std::string, std::any>;

using MapId = unsigned long long;
MapId RandomMapId();

class Function
{
public:
    std::vector<StatementPtr> Body;
    std::vector<VarDeclaration> Parameters;
    TypeDescriptor ReturnType;
    std::any Self;

    Function(std::vector<StatementPtr> body, std::vector<VarDeclaration> parameters = std::vector<VarDeclaration>(), TypeDescriptor returntype = TypeDescriptor(ValueType::Null), std::any self = nullptr)
        : Body(std::move(body)), Parameters(std::move(parameters)), ReturnType(std::move(returntype)), Self(self) {}
};

struct Class
{
    std::vector<VarDeclaration> Members;
    std::string ClassName;
    explicit Class(std::string classname, std::vector<VarDeclaration> members)
        : ClassName(classname), Members(std::move(members)) {}
    explicit Class() {}
    
    Class(const Class &) = default;
    Class &operator=(const Class&) = default;
    Class(Class &&) noexcept = default;
    Class &operator=(Class&&) noexcept = default;
    ~Class() = default;
};

class ClassObject
{
public:
    std::shared_ptr<MapReference> Members;
    std::string ClassName;

    explicit ClassObject(std::string classname = "?")
        : ClassName(classname), Members(std::make_shared<MapReference>(RandomMapId(), TypeDescriptor(ValueType::Dynamic)))
    {}

    ClassObject(const ClassObject &) = default;
    ClassObject &operator=(const ClassObject&) = default;
    ClassObject(ClassObject &&) noexcept = default;
    ClassObject &operator=(ClassObject&&) noexcept = default;
    ~ClassObject() = default;
};

struct Any
{
    std::any Val;
    explicit Any(std::any val = nullptr)
        : Val(val) {}
};

using Value = std::variant<std::nullptr_t, rt_Int, rt_Float, bool, std::string, Function, MapReference, ExternalFunction, Library, Class, ClassObject, Any>;

std::string ToString(const Value &Val);
bool IsEqual(const Value &A, const Value &B);
rt_Int ToInt(const Value &Val);
TypeDescriptor MakeTypeDescriptor(const Value &Value);

MapId RandomMapId()
{
    MapId x = 0;
    MapId y = ULLONG_MAX;
    static std::random_device Rd;
    static std::mt19937_64 gen(Rd());
    std::uniform_int_distribution<MapId> dist(x, y);
    return dist(gen);
}

using Map = std::unordered_map<MapId, std::pair<Value, Value>>;
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

    void Unstore()
    {
        StoredMaps.erase(Id);
    }

    MapReference Clone()
    {
        MapReference _Map(RandomMapId(), ValType);

        // Clone the map
        StoredMaps[_Map.Id] = GetLiteralMap();
        return _Map;
    }

    Value Get(Value Key)
    {
        auto OuterIt = StoredMaps.find(Id);
        if (OuterIt == StoredMaps.end())
            return nullptr; // Null map reference

        auto &InnerMap = OuterIt->second;
        for (auto &entry : InnerMap)
        {
            if (IsEqual(entry.second.first, Key)) // match on "logical key"
                return entry.second.second; // return "value"
        }
        return nullptr; // Key not found
    }

    void Set(Value Key, Value Val)
    {
        if (CheckTypeMatch(ValueType::Null, MakeTypeDescriptor(Val).Type))
        {
            // Remove entry if key exists
            auto &InnerMap = StoredMaps[Id];
            for (auto it = InnerMap.begin(); it != InnerMap.end(); ++it)
            {
                if (IsEqual(it->second.first, Key))
                {
                    InnerMap.erase(it);
                    return;
                }
            }
            return;
        }
        if (ValType != MakeTypeDescriptor(Val) &&
            !(CheckTypeMatch(ValueType::Dynamic, ValType.Type)))
            throw std::runtime_error("Type mismatch for map value");

        auto &InnerMap = StoredMaps[Id];

        // Update if the key exists
        for (auto &entry : InnerMap)
        {
            if (IsEqual(entry.second.first, Key))
            {
                entry.second.second = Val;
                return;
            }
        }

        // Insert new entry with random ID
        InnerMap.emplace(RandomMapId(), std::make_pair(Key, Val));
    }

    size_t Length()
    {
        auto OuterIt = StoredMaps.find(Id);
        if (OuterIt == StoredMaps.end())
            return 0;

        return OuterIt->second.size();
    }

    size_t HighestIndex()
    {
        Map &_Map = GetLiteralMap();

        rt_Int MaxIndex = 0;
        for (auto &entry : _Map)
        {
            // entry.second.first is the "logical key"
            rt_Int KeyInt = ToInt(entry.second.first);
            if (KeyInt > MaxIndex)
                MaxIndex = KeyInt;
        }

        return MaxIndex;
    }

};

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
        return TypeDescriptor(ValueType::Function, {std::get<Function>(Value).ReturnType});
    else if (std::holds_alternative<ExternalFunction>(Value))
        Type = ValueType::ExternalFunction;
    else if (std::holds_alternative<Library>(Value))
        Type = ValueType::Library;
    else if (std::holds_alternative<Class>(Value))
        Type = ValueType::Unknown;
    else if (std::holds_alternative<ClassObject>(Value))
        return TypeDescriptor(ValueType::Custom, {}, std::get<ClassObject>(Value).ClassName);

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
    if (auto p = std::any_cast<Any>(&a))
        return *p;
    else if (auto p = std::any_cast<std::nullptr_t>(&a))
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
        return std::string(*p);
    else if (auto p = std::any_cast<char>(&a))
        return std::string(1, *p);
    else if (auto p = std::any_cast<Function>(&a))
        return *p;
    else if (auto p = std::any_cast<ExternalFunction>(&a))
        return *p;
    else if (auto p = std::any_cast<Library>(&a))
        return *p;
    else if (auto p = std::any_cast<MapReference>(&a))
        return *p;
    else if (auto p = std::any_cast<Class>(&a))
        return *p;
    else if (auto p = std::any_cast<ClassObject>(&a))
        return *p;

    throw std::runtime_error("Type held is not supported");
}

rt_Int ToInt(const Value &Val)
{
    if (std::holds_alternative<Any>(Val))
        return ToInt(AnyToValue(std::get<Any>(Val).Val));
    else if (std::holds_alternative<rt_Int>(Val))
        return std::get<rt_Int>(Val);
    else if (std::holds_alternative<rt_Int>(Val))
        return static_cast<rt_Int>(std::get<rt_Float>(Val));
    else if (std::holds_alternative<bool>(Val))
        return std::get<bool>(Val) ? 1 : 0;
    else if (std::holds_alternative<std::string>(Val))
    {
        std::string String = std::get<std::string>(Val);
        try
        {
            return rt_Int(std::stol(String));
        }
        catch (...)
        {
            return 0;
        }
    }

    return 0;
}

rt_Int ToInt(const std::any &Val)
{
    return ToInt(AnyToValue(Val));
}

rt_Float ToFloat(const Value &Val)
{
    if (std::holds_alternative<Any>(Val))
        return ToFloat(AnyToValue(std::get<Any>(Val).Val));
    else if (std::holds_alternative<rt_Int>(Val))
        return std::get<rt_Int>(Val);
    else if (std::holds_alternative<rt_Float>(Val))
        return std::get<rt_Float>(Val);
    else if (std::holds_alternative<bool>(Val))
        return std::get<bool>(Val) ? 1 : 0;
    else if (std::holds_alternative<std::string>(Val))
    {
        std::string String = std::get<std::string>(Val);
        try
        {
            return rt_Float(std::stod(String));
        }
        catch (...)
        {
            return 0;
        }
    }

    return 0;
}

rt_Float ToFloat(const std::any &Val)
{
    return ToFloat(AnyToValue(Val));
}

bool ToBool(const Value &Val)
{
    if (std::holds_alternative<std::nullptr_t>(Val))
        return false;
    else if (std::holds_alternative<bool>(Val))
        return std::get<bool>(Val);
    
    // Anything else is considered truthy
    return true;
}

bool IsEqual(const Value &A, const Value &B)
{
    if (MakeTypeDescriptor(A) != MakeTypeDescriptor(B))
        return false;
    if (CheckTypeMatch(ValueType::Map, MakeTypeDescriptor(A).Type))
        return std::get<MapReference>(A).Id == std::get<MapReference>(B).Id;
    return ToString(A) == ToString(B);
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

        Value MainVariable = LookupVariable("Main");
        if (MakeTypeDescriptor(MainVariable) != TypeDescriptor(ValueType::Function, {TypeDescriptor(ValueType::Null)}))
            throw std::runtime_error("Main variable must be a function with a null return type");
            
        Function MainFunc = std::get<Function>(MainVariable);
        ExecuteCall(CallExpression(
            std::make_shared<ValueExpression>(MainFunc),
            (MainFunc.Parameters.size() > 0) ? std::vector<ExpressionPtr>({ArgvExpr}) : std::vector<ExpressionPtr>()
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
        else if (auto Rec = std::dynamic_pointer_cast<ReceiverStatement>(Statement))
            ExecuteReceiver(*Rec);
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

            return;
        }
        else if (auto Return = std::dynamic_pointer_cast<ReturnStatement>(Statement))
            throw Signal(SignalType::Return, ValueToAny(EvaluateExpression(Return->Expr)));
        else if (auto _Signal = std::dynamic_pointer_cast<SignalStatement>(Statement))
            throw Signal(SignalType::Signal, ValueToAny(EvaluateExpression(_Signal->Expr)));
        else if (auto Break = std::dynamic_pointer_cast<BreakStatement>(Statement))
            throw Signal(SignalType::Break);

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

    Value ExecuteAssignment(const AssignmentExpression &Assign)
    {
        Value Val = EvaluateExpression(Assign.Value);
        TypeDescriptor Type = MakeTypeDescriptor(Val);

        std::string VariableName;
        if (auto Var = std::dynamic_pointer_cast<VariableExpression>(Assign.Name))
            VariableName = Var->Name;

        if (!VariableName.empty())
        {
            // Walk through local scopes from innermost (back) to outermost (front)
            for (auto It = LocalScopes.rbegin(); It != LocalScopes.rend(); ++It)
            {
                if (It->count(VariableName))
                {
                    Variable &Var = (*It)[VariableName];

                    if (!CheckTypeMatch(ValueType::Null, Type.Type))
                    {
                        if (Type != Var.Type)
                            throw std::runtime_error("Type mismatch");
                    }

                    Var.ValueData = Val;
                    return Val;
                }
            }

            if (GlobalScope.count(VariableName))
            {
                Variable &Var = GlobalScope[VariableName];

                if (!CheckTypeMatch(ValueType::Null, Type.Type))
                {
                    if (Type != Var.Type)
                        throw std::runtime_error("Type mismatch");
                }

                Var.ValueData = Val;
                return Val;
            }
        }
        else if (auto Index = std::dynamic_pointer_cast<IndexExpression>(Assign.Name))
        {
            MapReference Map = std::get<MapReference>(EvaluateExpression(Index->Object));
            Map.Set(EvaluateExpression(Index->Index), Val);
            return Val;
        }
        else if (auto Member = std::dynamic_pointer_cast<MemberExpression>(Assign.Name))
        {
            ClassObject Object = std::get<ClassObject>(EvaluateExpression(Member->Object));
            if (MakeTypeDescriptor(Val) != MakeTypeDescriptor(Object.Members->Get(Member->Member)))
                throw std::runtime_error("Type mismatch when assigning object member");
            Object.Members->Set(Member->Member, Val);
            return Val;
        }

        throw std::runtime_error("Assignment failed");
    }

public:
    Value ExecuteCall(const CallExpression &Expr)
    {
        Value ValueResult = EvaluateExpression(Expr.Callee);

        if (std::holds_alternative<Function>(ValueResult))
        {
            Function Func = std::get<Function>(ValueResult);

            // Handle arguments

            if (Expr.Arguments.size() != Func.Parameters.size())
                throw std::runtime_error("Parameter to argument number mismatch");

            std::unordered_map<std::string, Variable> Arguments;

            size_t i = 0;
            for (const auto &Decl : Func.Parameters)
            {
                const ExpressionPtr &ArgumentExpr = Expr.Arguments[i];
                Value ArgumentResult = EvaluateExpression(ArgumentExpr);

                if (MakeTypeDescriptor(ArgumentResult) != Decl.Type && !CheckTypeMatch(ValueType::Null, MakeTypeDescriptor(ArgumentResult).Type))
                    throw std::runtime_error("Type mismatch for function parameter");

                Arguments[Decl.Name] = Variable(ArgumentResult, Decl.Type);
                i++;
            }

            std::vector<std::unordered_map<std::string, Variable>> OldScopes = LocalScopes;
            LocalScopes.clear();
            PushLocalScope();
            
            CurrentLocalScope()["this"] = Variable(AnyToValue(Func.Self), MakeTypeDescriptor(AnyToValue(Func.Self)));
            for (const auto &Arg : Arguments)
                CurrentLocalScope()[Arg.first] = Arg.second;

            try
            {
                for (const auto &Statement : Func.Body)
                    ExecuteStatement(Statement);
            }
            catch (Signal &Signal)
            {
                LocalScopes = OldScopes;

                if (Signal.Type == SignalType::Signal)
                    throw Signal;

                if (Signal.Type != SignalType::Return)
                    throw std::runtime_error("Invalid signal received when executing function");

                Value ReturnVal = AnyToValue(Signal.Data);
                if (MakeTypeDescriptor(ReturnVal) != Func.ReturnType)
                    throw std::runtime_error("Return value does not match function return type");
                return ReturnVal;
            }

            LocalScopes = OldScopes;
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
            throw std::runtime_error("Unsupported call type.");
    }

private:
    Value ExecuteUseExpr(const UseExpression &Expr)
    {
        TypeDescriptor Type = Expr.Type;
        ValueType MainType = Type.Type;

        Value Val = nullptr;
        if (Expr.Arguments.size() > 0)
            Val = EvaluateExpression(Expr.Arguments[0]);

        if (CheckTypeMatch(ValueType::Any, MainType))
            return Any(ValueToAny(Val));
        else if (CheckTypeMatch(ValueType::Int, MainType))
            return ToInt(Val);
        else if (CheckTypeMatch(ValueType::Float, MainType))
            return ToFloat(Val);
        else if (CheckTypeMatch(ValueType::Bool, MainType))
            return ToBool(Val);
        else if (CheckTypeMatch(ValueType::String, MainType))
        {
            std::string str;
            for (const auto &Arg : Expr.Arguments)
                str += ToString(EvaluateExpression(Arg));

            return str;
        }
        else if (CheckTypeMatch(ValueType::Map, MainType))
            return MapReference(RandomMapId(), Type.Subtypes[0]);
        else if (CheckTypeMatch(ValueType::Custom, MainType))
        {
            Value ValueResult = LookupVariable(Type.CustomTypeName);
            if (std::holds_alternative<Class>(ValueResult))
            {
                Class Blueprint = std::get<Class>(ValueResult);
                ClassObject Object(Blueprint.ClassName);
                for (VarDeclaration Decl : Blueprint.Members)
                {
                    Value Val = EvaluateExpression(Decl.Initializer);
                    if (std::holds_alternative<Function>(Val))
                    {
                        Function Func = std::get<Function>(Val);
                        Func.Self = Object;
                        Val = Func;
                    }
                    Object.Members->Set(Decl.Name, Val);
                }

                Value InitMethod = Object.Members->Get("__Init");
                if (!CheckTypeMatch(ValueType::Null, MakeTypeDescriptor(InitMethod).Type))
                    ExecuteCall(CallExpression(std::make_shared<ValueExpression>(ValueToAny(InitMethod)), Expr.Arguments));

                return Object;
            }
            else
                throw std::runtime_error("Invalid class blueprint");
        }
        else
            throw std::runtime_error("Unsupported type for use expression");
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
            throw std::runtime_error("Unsupported index type");
    }

    Value ExecuteMember(const MemberExpression &Expr)
    {
        Value ValueResult = EvaluateExpression(Expr.Object);

        if (std::holds_alternative<ClassObject>(ValueResult))
        {
            ClassObject Object = std::get<ClassObject>(ValueResult);
            Value Val = Object.Members->Get(Expr.Member);
            if (CheckTypeMatch(ValueType::Null, MakeTypeDescriptor(Val).Type))
                throw std::runtime_error("Member '" + Expr.Member + "' in class " + Object.ClassName + " is null");
            return Val;
        }
        else if (std::holds_alternative<std::string>(ValueResult))
        {
            std::string String = std::get<std::string>(ValueResult);
#include "String.hpp"
            return AnyToValue(StringLib[Expr.Member]);
        }
        else if (std::holds_alternative<MapReference>(ValueResult))
        {
            MapReference _Map = std::get<MapReference>(ValueResult);
#include "Map.hpp"
            return AnyToValue(MapLib[Expr.Member]);
        }
        else
            throw std::runtime_error("Object does not have any accessable members");
    }

    std::unordered_map<std::string, const Library *> LibraryCache;
    std::unordered_map<std::string, const std::any *> LibraryMemberCache;

    Value ExecuteAccess(const AccessExpression &Expr)
    {
        Value ValueResult = EvaluateExpression(Expr.Object);

        if (!std::holds_alternative<Library>(ValueResult))
            throw std::runtime_error("Unsupported access type");

        const Library &Lib = std::get<Library>(ValueResult);

        // Cache the library pointer
        LibraryCache[Expr.ToAccess] = &Lib;

        // Get the value reference
        auto it = Lib.find(Expr.ToAccess);
        if (it == Lib.end())
            throw std::runtime_error("Member not found in library");

        const std::any *CachedVal = &it->second;
        LibraryMemberCache[Expr.ToAccess] = CachedVal;

        return AnyToValue(*CachedVal);
    }

    void ExecuteReceiver(const ReceiverStatement &Rec)
    {
        try
        {
            PushLocalScope();
            for (const auto &Statement : Rec.With[0])
                ExecuteStatement(Statement);
            PopLocalScope();
        }
        catch (Signal& Signal)
        {
            PopLocalScope();

            if (Signal.Type == SignalType::Return)
                throw Signal;

            TypeDescriptor ValType = MakeTypeDescriptor(AnyToValue(Signal.Data));
            bool TypeMatched = false;

            size_t i = 1;
            for (auto &[Type, Name] : Rec.ReceiveTypes)
            {
                if (Type == ValType)
                {
                    TypeMatched = true;
                    PushLocalScope();
                    CurrentLocalScope()[Name] = Variable(AnyToValue(Signal.Data), Type);
                    for (const auto &Statement : Rec.With[i])
                        ExecuteStatement(Statement);
                    PopLocalScope();
                }
                
                i++;
            }

            // Push the signal up if
            // no types match
            if (!TypeMatched)
                throw Signal;
        }
        
    }

    void ExecuteIf(const IfStatement &If)
    {
        for (size_t i = 0; i < If.Conditions.size(); ++i)
        {
            Value ConditionResult = EvaluateExpression(If.Conditions[i]);

            bool ConditionIsTruthy = ToBool(ConditionResult);
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
        while (ToBool(EvaluateExpression(While.Condition)))
        {
            try
            {
                PushLocalScope();
                for (const auto &Statement : While.Body)
                    ExecuteStatement(Statement);
                PopLocalScope();
            }
            catch (Signal &Signal)
            {
                PopLocalScope();
                if (Signal.Type == SignalType::Break)
                    break;
                else
                    throw Signal;
            }
        }
    }

    void ExecuteFor(const ForStatement &For)
    {
        Value IterValue = EvaluateExpression(For.Iter);
        Map *IterMap = nullptr;
        MapReference Map(RandomMapId(), TypeDescriptor(ValueType::Null));
        rt_Int Range = 0;

        if (std::holds_alternative<MapReference>(IterValue))
        {
            Map.Unstore();
            Map = std::get<MapReference>(IterValue);
            IterMap = &Map.GetLiteralMap();
        }
        else if (std::holds_alternative<rt_Int>(IterValue))
        {
            Map.Unstore();
            Range = std::get<rt_Int>(IterValue);
        }
        else
            throw std::runtime_error("For loop value is not iterable.");

        if (IterMap && CheckTypeMatch(ValueType::Int, For.KeyType.Type))
        {
            // Collect logical key/value pairs
            std::vector<std::pair<Value, Value>> Entries;
            Entries.reserve(IterMap->size());

            for (auto &entry : *IterMap)
            {
                Entries.emplace_back(entry.second.first, entry.second.second);
            }

            // Sort by integer value of logical key
            std::sort(Entries.begin(), Entries.end(),
                      [](const auto &A, const auto &B)
                      {
                          return ToInt(A.first) < ToInt(B.first);
                      });

            TypeDescriptor Type = TypeDescriptor(ValueType::Int);

            for (auto &[Key, Val] : Entries)
            {
                rt_Int KeyInt = ToInt(Key);

                PushLocalScope();
                auto &Scope = CurrentLocalScope();
                
                Scope[For.KeyName] = Variable(KeyInt, Type);
                Scope[For.ValName] = Variable(Val, For.KeyType);

                for (const auto &Stmt : For.Body)
                    ExecuteStatement(Stmt);

                PopLocalScope();
            }
        }
        else
        {
            if (Map.ValType != For.ValType)
                throw std::runtime_error("Type mismatch in for loop");
                
            if (IterMap)
            {
                for (auto &[Id, KV] : *IterMap)
                {
                    PushLocalScope();
                    auto &Scope = CurrentLocalScope();

                    Scope[For.KeyName] = Variable(KV.first, For.KeyType);
                    Scope[For.ValName] = Variable(KV.second, For.ValType);

                    for (const auto &Stmt : For.Body)
                        ExecuteStatement(Stmt);

                    PopLocalScope();
                }
            }
            else if (Range > 0)
            {
                TypeDescriptor Type = TypeDescriptor(ValueType::Int);
                if (For.KeyType != Type)
                    throw std::runtime_error("Type mismatch in for range (extected int type)");

                rt_Int i = 0;
                while (i < Range)
                {
                    i += 1;

                    PushLocalScope();
                    auto &Scope = CurrentLocalScope();

                    Scope[For.KeyName] = Variable(i, Type);

                    for (const auto &Stmt : For.Body)
                        ExecuteStatement(Stmt);

                    PopLocalScope();
                }
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
        else if (auto Use = std::dynamic_pointer_cast<UseExpression>(Expr))
            return ExecuteUseExpr(*Use);
        else if (auto Index = std::dynamic_pointer_cast<IndexExpression>(Expr))
            return ExecuteIndex(*Index);
        else if (auto Member = std::dynamic_pointer_cast<MemberExpression>(Expr))
            return ExecuteMember(*Member);
        else if (auto Assign = std::dynamic_pointer_cast<AssignmentExpression>(Expr))
            return ExecuteAssignment(*Assign);
        else if (auto Access = std::dynamic_pointer_cast<AccessExpression>(Expr))
        {
            return ExecuteAccess(*Access);
        }
        else if (auto Func = std::dynamic_pointer_cast<FunctionDefinition>(Expr))
        {
            return Function(Func->Body, Func->Arguments, Func->ReturnType);
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
        else if (auto _Class = std::dynamic_pointer_cast<ClassBlueprint>(Expr))
        {
            Class Blueprint;
            Blueprint.ClassName = _Class->ClassName;
            Blueprint.Members = _Class->Members;
            return Blueprint;
        }
        else if (auto Bin = std::dynamic_pointer_cast<BinaryExpression>(Expr))
        {
            ExpressionPtr A = Bin->A;
            ExpressionPtr B = Bin->B;

            std::function ToNumber = [this, &A, &B](const rt_Float &Val) -> Value
            {
                bool xIsInt = CheckTypeMatch(ValueType::Int, MakeTypeDescriptor(EvaluateExpression(A)).Type);
                bool yIsInt = CheckTypeMatch(ValueType::Int, MakeTypeDescriptor(EvaluateExpression(B)).Type);

                if (xIsInt && yIsInt)
                    return rt_Int(Val);

                return Val;
            };

            switch (Bin->Operator)
            {
            case OperationType::Equality:
                return IsEqual(EvaluateExpression(A), EvaluateExpression(B));
            case OperationType::Inequality:
                return !IsEqual(EvaluateExpression(A), EvaluateExpression(B));

            case OperationType::Or:
            {
                Value _A = EvaluateExpression(A);
                return ToBool(_A) ? _A : EvaluateExpression(B);
            }
            case OperationType::And:
            {
                Value _A = EvaluateExpression(A);
                return ToBool(_A) ? EvaluateExpression(B) : _A;
            }

            default: {
                    rt_Float X = ToFloat(EvaluateExpression(A));
                    rt_Float Y = ToFloat(EvaluateExpression(B));

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
                    case OperationType::GreaterThan:
                        return X > Y;
                    case OperationType::LessThan:
                        return X < Y;
                    default:
                        throw std::runtime_error("Invalid binary expression");
                    }
                }
            }
        }
    
        else if (Expr)
            throw std::runtime_error("Unknown expression type");
        return nullptr;
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

Interpreter Interp;

std::string ToString(const Value &Val)
{
    if (std::holds_alternative<Any>(Val))
        return ToString(AnyToValue(std::get<Any>(Val).Val));
    else if (std::holds_alternative<std::nullptr_t>(Val))
        return "null";
    else if (std::holds_alternative<rt_Int>(Val))
        return std::to_string(std::get<rt_Int>(Val));
    else if (std::holds_alternative<rt_Float>(Val))
        return std::to_string(std::get<rt_Float>(Val));
    else if (std::holds_alternative<bool>(Val))
        return std::get<bool>(Val) ? "true" : "false";
    else if (std::holds_alternative<std::string>(Val))
        return std::get<std::string>(Val);
    else if (std::holds_alternative<MapReference>(Val))
    {
        MapReference Map = std::get<MapReference>(Val);

        std::string Result = "Map " + std::to_string(Map.Id) + "\n{\n";
        bool IsEmpty = true;
        for (auto &[Id, KV] : StoredMaps[Map.Id])
        {
            IsEmpty = false;
            std::string KeyStr;
            if (std::holds_alternative<std::string>(KV.first))
                KeyStr = '\'' + ToString(KV.first) + '\'';
            else
                KeyStr = ToString(KV.first);
            Result += "    [" + KeyStr + "]: " + ToString(KV.second) + ",\n";
        }

        if (IsEmpty)
            return Result.substr(0, Result.length() - 1) + "}";

        return Result.substr(0, Result.length() - 2) + "\n}";
    }
    else if (std::holds_alternative<Function>(Val))
        return "function";
    else if (std::holds_alternative<ClassObject>(Val))
    {
        ClassObject Object = std::get<ClassObject>(Val);
        Value Method = Object.Members->Get("__ToString");
        if (!CheckTypeMatch(ValueType::Null, MakeTypeDescriptor(Method).Type))
            return ToString(Interp.ExecuteCall(CallExpression(std::make_shared<ValueExpression>(ValueToAny(Method)), {})));
    }

    return "<unknown>";
}

std::string ToString(const std::any &Val)
{
    return ToString(AnyToValue(Val));
}

