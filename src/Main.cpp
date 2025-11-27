
// linux g++ -std=c++17 Main.cpp -o /mnt/d/FurnScript/Compiler/furn
// windows g++ -std=c++17 Main.cpp -o D:/FurnScript/Compiler/furn.exe


#include "Common.hpp"

std::filesystem::path FileName;
std::filesystem::path LocalDirectory;
std::vector<std::string> argv;
std::filesystem::path SystemIncludePath;

#include <filesystem>
#include <fstream>
#include <iostream>

#include <filesystem>
#include <fstream>
#include <memory>
#include <iostream>

#include "IncludePath.hpp"

#include "Lexer.hpp"
#include "Parser.hpp"
#include "AsmGen.hpp"

#include "GlobalParseLoc.hpp"

int Validate(Parser &Parse, std::vector<StatementPtr> &Ast)
{
    if (CmplFlags::ParseInfo)
    {
        for (CompileError &Error : Parse.Errors)
        {
            if (Error.Location.File != FileName)
                continue;
            std::cout << Error.ToString(true) << '\n';
        }
        for (std::string &Name : Parse.MacroNames)
        {
            std::cout << "(Macro): " << Name << '\n';
        }
        for (std::string &Name : Parse.ClassNames)
        {
            std::cout << "(Class): " << Name << '\n';
        }

        for (StatementPtr &Stmt : Ast)
        {
            if (auto UseStmt = std::dynamic_pointer_cast<UseStatement>(Stmt))
            {
                if (UseStmt->UseNamespace)
                {
                    if (auto Expr = std::dynamic_pointer_cast<VariableExpression>(UseStmt->Expr))
                        std::cout << "(UseLib): " << Expr->Name << '\n';
                }
                else
                {
                    if (auto Expr = std::dynamic_pointer_cast<MemberExpression>(UseStmt->Expr))
                        std::cout << "(Use): " << Expr->Member << '\n';
                }
            }
        }

        return !Parse.Errors.empty();
    }

    long ErrorCount = 0;
    for (CompileError &Error : Parse.Errors)
    {
        if (Error.Severity >= SyntaxError)
            ErrorCount++;
    }

    if (ErrorCount > 0)
    {
        CompileError Last;
        for (size_t i = 0; i < Parse.Errors.size(); i++)
        {
            CompileError Error = Parse.Errors[i];

            if (Error.Severity <= Hint)
                continue;

            CompileError Next;
            if (i + 1 < Parse.Errors.size())
                Next = Parse.Errors[i + 1];

            if (Error.Location.File != Last.Location.File)
                std::cerr << Error.ToString(false) << "\n\n";
            else if (Error.Message == Next.Message)
                std::cerr << Error.Location.ToString(false) << '\n';
            else
                std::cerr << Error.ToString(false, false) << "\n\n";

            Last = Error;
        }
        return 1;
    }
    else
    {
        for (CompileError &Error : Parse.Errors)
        {
            if (Error.Severity >= Warning)
                std::cerr << Error.ToString() << "\n\n";
        }
    }
    
    return 0;
}

void SetupParse(Parser &Parse)
{
    if (Parse.Check(TokenType::Package))
    {
        Parse.Tokens.erase(Parse.Tokens.begin()); // skip package keyword
        Parse.Tokens.erase(Parse.Tokens.begin()); // skip package name
    }

    std::vector<Token> ImplicitStatements = {};

    Parse.Tokens.insert(Parse.Tokens.begin(), ImplicitStatements.begin(), ImplicitStatements.end());
}

int main(int argc, const char *_argv[])
{
    argv = std::vector<std::string>(_argv, _argv + argc);

    for (size_t c = 2; c < argv.size(); c++)
    {
        const std::string arg = argv.at(c);

        if (arg == "-parseinfo")
            CmplFlags::ParseInfo = true;
        else if (arg == "-compileinfo")
            CmplFlags::CompileInfo = true;
        else if (arg == "-strict")
            CmplFlags::StrictMode = true;
        else if (arg == "-out")
            CmplFlags::OutputFlag = argv.at(++c);
        else if (arg == "-r")
            CmplFlags::RunAfterComp = true;
        else if (arg == "-q")
            CmplFlags::QuietComp = true;
        else if (arg == "-lwgcc")
            CmplFlags::LinkWithGcc = true;
        else if (arg == "--release")
        {
            CmplFlags::BoundsChecking = false;
        }
        else if (arg == "-nogarbagecollect")
            CmplFlags::GarbageCollect = false;
        else if (arg == "-cursor")
        {
            // throw std::runtime_error(argv.at(++c));
            CmplFlags::CursorPosition = std::stoi(argv.at(++c));
        }
        else
            std::cout << "unrecognized flag '" << arg << '\'' << std::endl;
    }

    IncludePath::Init();
    SystemIncludePath = IncludePath::DirPath;

    // Printing optimizations
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    if (argc > 1)
        FileName = argv[1];
    else
    {
        std::cout << "usage:\nfurn <file> [ flags... ]\n";
        return 0;
        // std::cout << "No file attached\nCompile file: ";
        // std::cout.flush();
        // std::string _FileName;
        // std::getline(std::cin, _FileName);
        // FileName = _FileName;
    }

    std::ifstream File(FileName, std::ios::binary);
    if (!File.is_open())
    {
        std::cerr << "Failed to open: " << FileName << '\n';
        return 1;
    }

    LocalDirectory = std::filesystem::current_path(); // std::filesystem::path(FileName).parent_path();
    std::string Content((std::istreambuf_iterator<char>(File)),
                     std::istreambuf_iterator<char>());

    File.close();

    Lexer Lex(Content);
    Lex.Location.File = FileName;
    std::vector<Token> Tokens = Lex.Tokenize();

    Parser Parse(Tokens);
    SetupParse(Parse);
    std::vector<StatementPtr> Ast = Parse.ParseProgram();

    const int Validation = Validate(Parse, Ast);
    if (Validation == 0 && (!CmplFlags::ParseInfo || CmplFlags::CompileInfo))
    {
        std::stringstream CompConsoleOut;

        std::filesystem::path OutputFile = CmplFlags::OutputFlag.empty() ? LocalDirectory / FileName.stem().concat(".asm") : LocalDirectory / CmplFlags::OutputFlag.stem().concat(".asm");
        
        CompConsoleOut << "compiling..." << std::endl;

        AsmGenerator Gen(Ast);
        std::string Result = Gen.GenerateProgram();

        for (CompileError &Error : Gen.Errors)
        {
            const std::filesystem::path &File = Error.Location.File;
            size_t Line = Error.Location.Line;
            size_t Column = Error.Location.Column;

            if (CmplFlags::CompileInfo)
            {
                std::cout << Error.ToString(false, true, true) << "\n";
            }
            else
            {
                std::cerr << Error.ToString(false, true, true) << "\n\n";
            }

            if (!CmplFlags::CompileInfo && Line > 0)
            {
                std::ifstream In(File);

                if (In)
                {
                    std::string Text;

                    for (size_t i = 1; i <= Line && std::getline(In, Text); ++i)
                    {
                        if (i == Line)
                        {
                            std::cerr << Text << '\n';
                            if (Column > 0)
                            {
                                std::cerr << std::string(Column - 2, ' ') << "\x1b[1;97m^\x1b[0m\n";
                                std::cerr << std::string(Column - 2, ' ') << "\x1b[96mnote: here\x1b[0m\n\n";
                            }
                        }
                    }

                    In.close();
                }
            }
        }
        std::cout.flush();
        std::cerr.flush();
        if (CmplFlags::CompileInfo)
        {
            for (auto &Name : Gen.AvailableIdentifiers)
            {
                std::cout << Name << std::endl;
            }

            return 0;
        }
        if (!Gen.Errors.empty())
        {
            return 1;
        }

        std::ofstream f(OutputFile);
        f << Result;
        f.close(); // gcc -nostdlib -no-pie hello.o -lc -o hello
        CompConsoleOut << ".asm in " << OutputFile.stem().concat(".asm") << std::endl;
        system((("cd \"" + LocalDirectory.string() + "\"") + "; nasm -felf64 " + OutputFile.stem().concat(".asm").string()).c_str());
        CompConsoleOut << ".o in " << OutputFile.stem().concat(".o") << std::endl;
        if (CmplFlags::LinkWithGcc)
            system((("cd \"" + LocalDirectory.string() + "\"") + "; gcc -nostdlib -no-pie " + OutputFile.stem().concat(".o").string() + " -lc -o " + OutputFile.stem().string()).c_str());
        else
            system((("cd \"" + LocalDirectory.string() + "\"") + "; ld " + OutputFile.stem().concat(".o").string() + " -o " + OutputFile.stem().string()).c_str());
        CompConsoleOut << "executable in " << OutputFile.stem() << std::endl;

        if (!CmplFlags::QuietComp)
            std::cout << CompConsoleOut.str();

        if (CmplFlags::RunAfterComp)
            system((("cd \"" + LocalDirectory.string() + "\"") + "; ./" + OutputFile.stem().string()).c_str());
    }
    return 0;
}
