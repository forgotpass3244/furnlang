// g++ -std=c++17 -o D:\FurnScript\Interpreter\FurnInterpreter.exe D:\FurnScript\Interpreter\src\Main.cpp -static -static-libgcc -static-libstdc++
// D:/FurnScript/Interpreter/FurnInterpreter.exe D:/FurnScript/Interpreter/scripts/HelloWorld.fn

#include "Common.hpp"

std::filesystem::path LocalDirectory;

std::vector<std::string> argv;

#include "Lexer.hpp"
#include "Parser.hpp"
#include "Interpreter.hpp"
#include "StandardLibrary.hpp"

int main(int argc, char *_argv[])
{
    argv = std::vector<std::string>(_argv, _argv + argc);

    std::ios::sync_with_stdio(false);

    std::string FileName;
    if (argc > 1)
        FileName = argv[1];
    else
    {
        std::cout << "No file attached\nRun path: ";
        std::getline(std::cin, FileName);
    }

    std::ifstream File(FileName);
    if (!File.is_open())
    {
        std::cerr << "Failed to open: " << FileName << '\n';
        return 1;
    }

    LocalDirectory = std::filesystem::path(FileName).parent_path();
    std::string Content((std::istreambuf_iterator<char>(File)),
                     std::istreambuf_iterator<char>());

    if (argv[2] == "Info")
    {
        Lexer Lex(Content);
        Lex.Location.File = std::filesystem::absolute(FileName).string();
        std::vector<Token> Tokens = Lex.Tokenize();

        Parser Parse = Parser(Tokens);
        std::vector<StatementPtr> Ast = Parse.ParseProgram();

        for (std::string &Error : Parse.Errors)
        {
            std::cout << "Error " + Error + '\n';
        }
        for (std::string &Name : Parse.MacroNames)
        {
            std::cout << "(Macro): " + Name + '\n';
        }

        for (StatementPtr &Stmt : Ast)
        {
            if (auto UseStmt = std::dynamic_pointer_cast<UseStatement>(Stmt))
            {
                if (UseStmt->UseLibrary)
                {
                    if (auto Expr = std::dynamic_pointer_cast<VariableExpression>(UseStmt->Expr))
                        std::cout << "(UseLib): " + Expr->Name + '\n';
                    if (auto Expr = std::dynamic_pointer_cast<AccessExpression>(UseStmt->Expr))
                        std::cout << "(UseLib): " + Expr->ToAccess + '\n';
                }
                else
                {
                    if (auto Expr = std::dynamic_pointer_cast<AccessExpression>(UseStmt->Expr))
                        std::cout << "(Use): " + Expr->ToAccess + '\n';
                }
            }
        }

        return 0;
    }

    Lexer Lex(Content);
    Lex.Location.File = std::filesystem::absolute(FileName).string();
    std::vector<Token> Tokens = Lex.Tokenize();

    Parser Parse(Tokens);
    std::vector<StatementPtr> Ast = Parse.ParseProgram();
    
    if (!Parse.Errors.empty())
    {
        for (auto &&Error : Parse.Errors)
        {
            std::cout << "Error " << Error << '\n';
        }
        return 1;
    }

    // for (const auto &Token : Parse.Tokens)
    //     std::cout << "Token: '" << Token.Text << "' Line: " << Token.Line << "\n";

    Interp.GlobalScope["Std"] = Variable(CreateStdLib(), TypeDescriptor(ValueType::Library));

    try
    {
        Interp.Execute(Ast);
    }
    catch (const std::runtime_error &ExecutionError)
    {
        std::cerr << "Error during runtime:\n"
                  << ExecutionError.what() << '\n';
        return 1;
    }
    catch (const Signal &Signal)
    {
        std::cerr << "Unreceived signal:\n"
                  << ToString(Signal.Data) << '\n';
        return 1;
    }

    return 0;
}

