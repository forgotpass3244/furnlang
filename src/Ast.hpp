#pragma once
#include "Token.hpp"
#include "Common.hpp"
#include "GlobalParseLoc.hpp"

MapId RandomMapId();

class Statement;
class Expression;

using StatementPtr = std::shared_ptr<Statement>;
using ExpressionPtr = std::shared_ptr<Expression>;

using rt_Int = long long;
using rt_Float = double;

enum class ValueType
{
    Unknown,
    Dynamic, // Not an actual data type
    Custom,

    Null,
    ExternalFunction,
    Namespace,
    Function,
    Int,
    Float,
    Bool,
    String,
    Short,
    Long,
    Double,
    Character,
};

struct TypeDescriptor
{
    ValueType Type = ValueType::Null;
    std::vector<TypeDescriptor> Subtypes;
    ExpressionPtr CustomTypeName;
    short Nullable = 0;
    short Constant = false;
    int PointerDepth = 0;
    ExpressionPtr ArraySize = 0;

    explicit TypeDescriptor()
        : Subtypes(std::vector<TypeDescriptor>()), CustomTypeName(nullptr) {}

    TypeDescriptor(ValueType type, std::vector<TypeDescriptor> subtypes = std::vector<TypeDescriptor>(), ExpressionPtr customtypename = nullptr, short nullable = 0, short constant = false, int ispointer = 0, ExpressionPtr arraysize = 0)
        : Type(std::move(type)), Subtypes(std::move(subtypes)), CustomTypeName(customtypename), Nullable(nullable), Constant(constant), PointerDepth(ispointer), ArraySize(arraysize) {}

    TypeDescriptor &AsConstant()
    {
        Constant = true;
        return *this;
    }

    TypeDescriptor &AsPointer()
    {
        PointerDepth++;
        return *this;
    }
};

bool operator!(TypeDescriptor TypeDesc)
{
    return TypeDesc.Type == ValueType::Unknown;
}

enum class OperationType
{
    Equality,
    Add,
    Subtract,
    Multiply,
    Divide,
    GreaterThan,
    LessThan,
    GreaterThanOrEqualTo,
    LessThanOrEqualTo,
    Or,
    And,
    ForceUnwrap,
    BitAnd,

    Negate,
};

// === Base Classes ===

class Statement
{
public:
    virtual ~Statement() = default;

    ScriptLocation Location;
    Statement() : Location(CurrentParseToken.Location) {}
};

class Expression
{
public:
    virtual ~Expression() = default;

    ScriptLocation Location;
    Expression() : Location(CurrentParseToken.Location) {}
};

class ValueExpression : public Expression
{
public:
    std::any Val;

    ValueExpression(std::any val)
        : Val(std::move(val)) {}
};

// === Expression Nodes ===

class MapExpression : public Expression
{
public:
    std::unordered_map<ExpressionPtr, ExpressionPtr> KV_Expressions;
    TypeDescriptor ValType;

    MapExpression(
        std::unordered_map<ExpressionPtr, ExpressionPtr> &kvexpr,
        TypeDescriptor valtype)
        : KV_Expressions(kvexpr), ValType(valtype) {}
};

class VariableExpression : public Expression
{
public:
    std::string Name;
    MapId Address;

    explicit VariableExpression(std::string name, MapId address) : Name(std::move(name)), Address(address) {}
};

class ClassCastExpression : public Expression
{
public:
    ExpressionPtr Expr;
    TypeDescriptor Type;
    bool Throws;

    explicit ClassCastExpression(ExpressionPtr expr, TypeDescriptor type, bool throws) : Expr(std::move(expr)), Type(std::move(type)), Throws(throws) {}
};

class ClassEqExpression : public Expression
{
public:
    ExpressionPtr Expr;
    TypeDescriptor Type;

    explicit ClassEqExpression(ExpressionPtr expr, TypeDescriptor type) : Expr(std::move(expr)), Type(std::move(type)) {}
};

struct CallExpression : Expression
{
    ExpressionPtr Callee;
    std::vector<ExpressionPtr> Arguments;

    CallExpression(ExpressionPtr callee, std::vector<ExpressionPtr> args)
        : Callee(callee), Arguments(args) {}
};

struct IndexExpression : Expression
{
    ExpressionPtr Object;
    ExpressionPtr Index;

    IndexExpression(ExpressionPtr object, ExpressionPtr index)
        : Object(object), Index(index) {}
};

struct MemberExpression : Expression
{
    ExpressionPtr Object;
    std::string Member;
    const bool Throws;

    MemberExpression(ExpressionPtr object, std::string member, bool throws = true)
        : Object(object), Member(member), Throws(throws) {}
};

// === Statement Nodes ===

class EmptyStatement : public Statement
{
};

class VarDeclaration : public Statement
{
public:
    TypeDescriptor Type;
    std::string Name;
    MapId Address;
    ExpressionPtr Initializer;

    VarDeclaration(ExpressionPtr init, std::string name, MapId address = 0, TypeDescriptor type = ValueType::Unknown)
        : Initializer(std::move(init)), Name(name), Address(std::move(address)), Type(type) {}
};

class MemberDeclaration : public Statement
{
public:
    TypeDescriptor Type;
    std::string Name;
    MapId Address;
    ExpressionPtr Initializer;
    bool ConstantSelfReference;

    MemberDeclaration() {}

    MemberDeclaration(ExpressionPtr init, std::string name, MapId address, TypeDescriptor type, bool constantselfreference = false)
        : Initializer(std::move(init)), Name(name), Address(std::move(address)), Type(type), ConstantSelfReference(constantselfreference) {}
};

class AssignmentExpression : public Expression
{
public:
    ExpressionPtr Name;
    ExpressionPtr Value;

    AssignmentExpression(ExpressionPtr name, ExpressionPtr value)
        : Name(std::move(name)), Value(std::move(value)) {}
};

class FunctionDefinition : public Expression
{
public:
    std::vector<StatementPtr> Body;
    std::vector<VarDeclaration> Arguments;
    TypeDescriptor ReturnType;
    bool Global = false;
    MapId UniqueId;

    FunctionDefinition(std::vector<StatementPtr> body, std::vector<VarDeclaration> arguments = std::vector<VarDeclaration>(), TypeDescriptor returntype = TypeDescriptor(ValueType::Null))
        : Body(std::move(body)), Arguments(std::move(arguments)), ReturnType(std::move(returntype)), UniqueId(RandomMapId()) {}
};

class AssemblyInstructions : public Statement
{
public:
    std::vector<Token> Instructions;

    AssemblyInstructions(std::vector<Token> instructions)
        : Instructions(std::move(instructions)) {}
};

class ClassBlueprint : public Expression
{
public:
    std::string ClassName; // for init method
    std::vector<MemberDeclaration> Members;
    std::vector<ExpressionPtr> InheritsFrom;
    std::vector<MapId> Templates;
    bool IsImplicit = false;
    MapId UniqueId;

    ClassBlueprint(std::string classname, std::vector<MemberDeclaration> members, std::vector<ExpressionPtr> inheritsfrom, ExpressionPtr polymorphic = nullptr, std::vector<MapId> templates = std::vector<MapId>(), bool isimplicit = false)
        : ClassName(classname), Members(std::move(members)), InheritsFrom(inheritsfrom), Templates(std::move(templates)), IsImplicit(isimplicit), UniqueId(RandomMapId()) {}
};

class ReceiverStatement : public Statement
{
public:
    std::vector<std::pair<TypeDescriptor, MapId>> ReceiveTypes;
    std::vector<std::vector<StatementPtr>> With;

    ReceiverStatement(std::vector<std::pair<TypeDescriptor, MapId>> receivetypes, std::vector<std::vector<StatementPtr>> with)
        : ReceiveTypes(std::move(receivetypes)), With(std::move(with)) {}
};

class IfStatement : public Statement
{
public:
    std::vector<ExpressionPtr> Conditions;
    std::vector<std::vector<StatementPtr>> Then;

    IfStatement(std::vector<ExpressionPtr> conditions, std::vector<std::vector<StatementPtr>> then)
        : Conditions(std::move(conditions)), Then(std::move(then)) {}
};

class WhileStatement : public Statement
{
public:
    std::vector<StatementPtr> Body;
    ExpressionPtr Condition;

    WhileStatement(std::vector<StatementPtr> body, ExpressionPtr condition)
        : Body(std::move(body)), Condition(std::move(condition)) {}
};

class ForStatement : public Statement
{
public:
    ExpressionPtr Iter;
    std::vector<StatementPtr> Body;
    MapId KeyName;
    MapId ValName;
    TypeDescriptor KeyType;
    TypeDescriptor ValType;

    ForStatement(std::vector<StatementPtr> body, ExpressionPtr iter, MapId keyname, TypeDescriptor keytype, MapId valname, TypeDescriptor valtype)
        : Body(std::move(body)), Iter(std::move(iter)), KeyName(keyname), KeyType(keytype), ValName(valname), ValType(valtype) {}
};

class ReturnStatement : public Statement
{
public:
    ExpressionPtr Expr;

    ReturnStatement(ExpressionPtr expr)
        : Expr(std::move(expr)) {}
};

class SignalStatement : public Statement
{
public:
    ExpressionPtr Expr;

    SignalStatement(ExpressionPtr expr)
        : Expr(std::move(expr)) {}
};

class BreakStatement : public Statement
{
};

class MultiStatement : public Statement
{
public:
    std::vector<StatementPtr> Statements;

    MultiStatement(std::vector<StatementPtr> statements)
        : Statements(std::move(statements)) {}
};

class NamespaceDefinition : public Expression
{
public:
    std::unordered_map<std::string, MapId> Definition;
    std::vector<StatementPtr> Statements;

    NamespaceDefinition(std::unordered_map<std::string, MapId> definition, std::vector<StatementPtr> statements)
        : Definition(std::move(definition)), Statements(std::move(statements)) {}
};

class UseStatement : public Statement
{
public:
    ExpressionPtr Expr;
    bool UseNamespace;
    MapId Address;

    UseStatement(ExpressionPtr expr, bool usenamespace, MapId address)
        : Expr(std::move(expr)), UseNamespace(usenamespace), Address(address) {}
};

class UseExpression : public Expression
{
public:
    TypeDescriptor Type;
    std::vector<ExpressionPtr> Arguments;
    std::vector<VarDeclaration> InlineDefinition;

    UseExpression(TypeDescriptor type, std::vector<ExpressionPtr> args, std::vector<VarDeclaration> inlinedefinition = {})
        : Type(type), Arguments(args), InlineDefinition(inlinedefinition)
    {
    }
};

class ExpressionStatement : public Statement
{
public:
    ExpressionPtr Expr;

    explicit ExpressionStatement(ExpressionPtr expr)
        : Expr(std::move(expr)) {}
};

class BinaryExpression : public Expression
{
public:
    OperationType Operator;
    ExpressionPtr A;
    ExpressionPtr B;

    BinaryExpression(OperationType op, ExpressionPtr a, ExpressionPtr b)
        : Operator(std::move(op)), A(std::move(a)), B(std::move(b)) {}
};

class UnaryExpression : public Expression
{
public:
    OperationType Operator;
    ExpressionPtr Expr;

    UnaryExpression(OperationType op, ExpressionPtr expr)
        : Operator(std::move(op)), Expr(std::move(expr)) {}
};

class SizeOfTypeExpression : public Expression
{
public:
    TypeDescriptor Type;

    SizeOfTypeExpression(TypeDescriptor type)
        : Type(std::move(type)) {}
};

class SizeOfExpression : public Expression
{
public:
    ExpressionPtr Expr;

    SizeOfExpression(ExpressionPtr expr)
        : Expr(std::move(expr)) {}
};

class UnownedReferenceExpression : public Expression
{
public:
    ExpressionPtr Expr;

    UnownedReferenceExpression(ExpressionPtr expr)
        : Expr(std::move(expr)) {}
};
