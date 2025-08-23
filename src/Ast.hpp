#pragma once
#include "Token.hpp"
#include "Common.hpp"

class Statement;
class Expression;

using StatementPtr = std::shared_ptr<Statement>;
using ExpressionPtr = std::shared_ptr<Expression>;

enum class ValueType
{
    Unknown,
    Dynamic, // Not an actual data type

    Null,
    ExternalFunction,
    Library,
    Function,
    Int,
    Float,
    Bool,
    String,
    Map
};

struct TypeDescriptor
{
    ValueType Type;
    std::vector<TypeDescriptor> Subtypes;
    std::string UdtName;

    explicit TypeDescriptor()
        : Type(ValueType::Null), Subtypes(std::vector<TypeDescriptor>()), UdtName("") {}

    explicit TypeDescriptor(ValueType type, std::vector<TypeDescriptor> subtypes = std::vector<TypeDescriptor>(), std::string udtname = "")
        : Type(std::move(type)), Subtypes(std::move(subtypes)), UdtName(std::move(udtname)) {}
};

// Equality operator
inline bool operator==(const TypeDescriptor &lhs, const TypeDescriptor &rhs)
{
    return lhs.Type == rhs.Type &&
           lhs.UdtName == rhs.UdtName &&
           lhs.Subtypes == rhs.Subtypes; // std::vector handles the recursive comparison
}

// Inequality operator
inline bool operator!=(const TypeDescriptor &lhs, const TypeDescriptor &rhs)
{
    return !(lhs == rhs);
}

enum class OperationType
{
    Equality,
    Inequality,
    Add,
    Subtract,
    Multiply,
    Divide,
};

// === Base Classes ===

class Statement
{
public:
    virtual ~Statement() = default;
};

class Expression
{
public:
    virtual ~Expression() = default;
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

    explicit MapExpression(
        std::unordered_map<ExpressionPtr, ExpressionPtr> &kvexpr,
        TypeDescriptor valtype)
        : KV_Expressions(kvexpr), ValType(valtype) {}
};

class VariableExpression : public Expression
{
public:
    std::string Name;

    explicit VariableExpression(std::string name) : Name(std::move(name)) {}
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

struct AccessExpression : Expression
{
    ExpressionPtr Object;
    std::string ToAccess;

    AccessExpression(ExpressionPtr object, std::string toaccess)
        : Object(object), ToAccess(toaccess) {}
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
    ExpressionPtr Initializer;

    VarDeclaration(ExpressionPtr init, std::string name, TypeDescriptor type)
        : Initializer(std::move(init)), Name(std::move(name)), Type(type) {}
};

class AssignmentStatement : public Statement
{
public:
    std::string VariableName;
    ExpressionPtr Value;

    AssignmentStatement(std::string name, ExpressionPtr value)
        : VariableName(std::move(name)), Value(std::move(value)) {}
};

class FunctionDefinition : public Expression
{
public:
    std::vector<StatementPtr> Body;
    std::vector<VarDeclaration> Arguments;

    FunctionDefinition(std::vector<StatementPtr> body, std::vector<VarDeclaration> arguments = std::vector<VarDeclaration>())
        : Body(std::move(body)), Arguments(std::move(arguments)) {}
};

class ClassBlueprint : public Statement
{
public:
    std::vector<VarDeclaration> Members;

    ClassBlueprint(std::vector<VarDeclaration> members)
        : Members(std::move(members)) {}
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
    std::string KeyName;
    std::string ValName;
    TypeDescriptor KeyType;

    ForStatement(std::vector<StatementPtr> body, ExpressionPtr iter, std::string keyname, std::string valname, TypeDescriptor keytype)
        : Body(std::move(body)), Iter(std::move(iter)), KeyName(keyname), ValName(valname), KeyType(keytype) {}
};

class ReturnStatement : public Statement
{
public:
    ExpressionPtr Expr;

    ReturnStatement(ExpressionPtr expr)
        : Expr(std::move(expr)) {}
};

class LibraryDefinition : public Expression
{
public:
    std::vector<StatementPtr> Definition;

    LibraryDefinition(std::vector<StatementPtr> definition)
        : Definition(std::move(definition)) {}
};

class UseStatement : public Statement
{
public:
    ExpressionPtr Expr;
    bool UseLibrary;

    UseStatement(ExpressionPtr expr, bool uselibrary)
        : Expr(std::move(expr)), UseLibrary(uselibrary) {}
};

// << Add this to support expression statements like function calls >>
class ExpressionStatement : public Statement
{
public:
    ExpressionPtr Expr;

    explicit ExpressionStatement(ExpressionPtr expr)
        : Expr(std::move(expr)) {}
};