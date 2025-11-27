#include "Ast.hpp"
#define ADDRESS_NULL 0

enum VariableType
{
    Var,
    Parameter,
    Member,
    Template,
};

struct Symbol
{
    TypeDescriptor TypeDesc;
    VariableType VarType;
    MapId Address = 0;

    explicit Symbol(TypeDescriptor typedesc = ValueType::Unknown, VariableType vartype = Var, MapId address = 0)
        : TypeDesc(typedesc), VarType(vartype), Address(address) {}
};
