// g++ -std=c++17 -o D:\FurnScript\Interpreter\FurnInterpreter.exe D:\FurnScript\Interpreter\src\Main.cpp

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

    try
    {
        Lexer Lex(Content);
        Lex.Location.File = std::filesystem::absolute(FileName).string();
        std::vector<Token> Tokens = Lex.Tokenize();

        Parser Parse(Tokens);
        std::vector<StatementPtr> Ast = Parse.ParseProgram();

        system("cls");
        // for (const auto &Token : Parse.Tokens)
        //     std::cout << "Token: '" << Token.Text << "' Line: " << Token.Line << "\n";

        Interpreter Interp;

        Interp.GlobalScope["Std"] = Variable(CreateStdLib(), TypeDescriptor(ValueType::ExternalFunction));

        Interp.Execute(Ast);
    }
    catch (const std::exception &Ex)
    {
        std::cerr << "Error: " << Ex.what() << '\n';
        return 1;
    }

    return 0;
}