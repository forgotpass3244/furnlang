#pragma once
#include "Ast.hpp"
#include "Common.hpp"
#include "FurnBytecodeBuilder/FurnBytecodeBuilder.hpp"

class Compiler
{
    std::vector<CompileError> Errors;
    FurnBytecodeBuilder fvm;

    void Throw(std::string Message, SeverityLevel Level = Error)
    {
        Errors.push_back(CompileError(Message, Level));
    }

public:
    void Compile(const std::vector<StatementPtr> &Program)
    {
        for (const StatementPtr &Stmt : Program)
            cStatement(Stmt);

        if (!Errors.empty())
        {
            std::cout << "BUILD FAILED WITH ERROR MESSAGES:\n";
            for (const CompileError &e : Errors)
                std::cout << e.Message << std::endl;
        }
        else
        {
            std::cout << "BUILD FINISHED SUCCESSFULLY\n";
        }

        Disassemble(fvm.C.Data);

        fvm.Write("Program.furn");
    }

    void cStatement(const StatementPtr &Stmt)
    {
        if (auto Expr = std::dynamic_pointer_cast<ExpressionStatement>(Stmt))
        {
            LoadExpr(Expr->Expr);
        }
        else if (auto Decl = std::dynamic_pointer_cast<VarDeclaration>(Stmt))
        {
            fvm.DeclareVar(Decl->Address);

            if (Decl->Initializer)
            {
                LoadExpr(Decl->Initializer);
                fvm.StoreVar(Decl->Address); // Use 64-bit StoreVar
            }
        }
        else if (auto Use = std::dynamic_pointer_cast<UseStatement>(Stmt))
        {
            // Reserved for future use
        }
        else
        {
            Throw("statement not supported");
        }
    }

    void LoadExpr(ExpressionPtr &Expr)
    {
        if (auto ConstExpr = std::dynamic_pointer_cast<ValueExpression>(Expr))
        {
            if (ConstExpr->Val.type() != typeid(std::nullptr_t))
                fvm.C.U2(LoadConst(Expr));
            else
                fvm.C.U2(CONST_NULL);
        }
        else if (auto Assign = std::dynamic_pointer_cast<AssignmentExpression>(Expr))
        {
            if (auto Var = std::dynamic_pointer_cast<VariableExpression>(Assign->Name))
            {
                LoadExpr(Assign->Value);
                fvm.StoreVar(Var->Address);
            }
            else
            {
                Throw("assignment target must be a variable");
            }
        }
        else
        {
            LoadToStack(Expr);
        }
    }

    uint16_t LoadConst(ExpressionPtr &Expr)
    {
        if (auto ConstExpr = std::dynamic_pointer_cast<ValueExpression>(Expr))
        {
            if (ConstExpr->Val.type() == typeid(std::string))
                return fvm.AddString(std::any_cast<std::string>(ConstExpr->Val));
            else if (ConstExpr->Val.type() == typeid(rt_Int))
                return fvm.AddInt(std::any_cast<rt_Int>(ConstExpr->Val));
        }

        Throw("failed to load the constant");
        return 0;
    }

    void LoadToStack(ExpressionPtr &Expr)
    {
        if (auto VarExpr = std::dynamic_pointer_cast<VariableExpression>(Expr))
        {
            if (fvm.Locals.count(VarExpr->Address))
            {
                fvm.LoadVar(VarExpr->Address); // Use 64-bit LoadVar

                if (fvm.Locals.count(VarExpr->Address) && !fvm.Locals.at(VarExpr->Address).IsInitialized)
                    Throw("access to uninitialized memory at slot " + std::to_string(VarExpr->Address));
            }
            else
            {
                Throw("variable not declared: " + std::to_string(VarExpr->Address));
            }
        }
        else
        {
            Throw("failed to load the expression");
        }
    }
};
