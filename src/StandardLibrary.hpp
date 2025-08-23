#pragma once
#include "Interpreter.hpp"
#include "Common.hpp"

#define exfunc ExternalFunction
#define def = [](const std::vector<std::any> &Args) -> std::any
#define ret return nullptr;
#define forarg for (const auto &Arg : Args)

const char *Terminal = std::getenv("TERM_PROGRAM");
const bool IsIntegratedTerminal = Terminal != nullptr && std::string(Terminal) == "vscode";

Value CreateStdLib()
{
    exfunc Exit def
    {
        std::exit(std::any_cast<rt_Int>(Args[0]));
    };

    exfunc SystemExecute def
    {
        forarg
            std::system(ToString(Arg).c_str());
        ret
    };

    Library StdLib;
    StdLib["Exit"] = Exit;
    StdLib["Execute"] = SystemExecute;

    exfunc WriteLn def
    {
        forarg
            std::cout << ToString(Arg);
        std::cout << '\n';
        ret
    };

    exfunc Write def
    {
        forarg
            std::cout << ToString(Arg);
        ret
    };

    exfunc ReadLn def
    {
        std::string Input;
        std::getline(std::cin, Input);
        return Input;
    };

    exfunc ClearIO def
    {
        #if defined _WIN32
            system("cls");
        #elif defined(__LINUX__) || defined(__gnu_linux__) || defined(__linux__)
            system("clear");
        #elif defined(__APPLE__)
            system("clear");
        #endif

        if (IsIntegratedTerminal)
        {
            // only for vscode
            std::cout << u8"\033[2J\033[1;1H";
            std::cout.flush();
        }

        ret
    };

    Library Terminal;
    Terminal["WriteLn"] = WriteLn;
    Terminal["Write"] = Write;
    Terminal["ReadLn"] = ReadLn;
    Terminal["ClearIO"] = ClearIO;
    StdLib["Terminal"] = Terminal;

    
    exfunc StringNew def
    {
        std::stringstream String;
        forarg
        {
            String << ToString(Arg);
        }
        
        return String.str();
    };

    exfunc StringCase def
    {
        const short Operation = std::any_cast<rt_Int>(Args[0]);
        std::stringstream String;

        size_t i = 0;
        forarg
        {
            i++;
            if (i <= 1)
                continue;
            String << ToString(Arg);
        }

        std::string Result = String.str();
        
        switch (Operation)
        {
        case 0:
            std::transform(Result.begin(), Result.end(), Result.begin(),
                           [](unsigned char c)
                           { return std::tolower(c); });
            break;

        case 1:
            std::transform(Result.begin(), Result.end(), Result.begin(),
                           [](unsigned char c)
                           { return std::toupper(c); });
            break;
        }

        return Result;
    };

    exfunc StringTrim def
    {
        std::stringstream String;
        forarg
        {
            String << ToString(Arg);
        }

        std::string str = String.str();

        auto Start = std::find_if_not(str.begin(), str.end(), [](unsigned char c)
                                      { return std::isspace(c); });
        auto End = std::find_if_not(str.rbegin(), str.rend(), [](unsigned char c)
                                    { return std::isspace(c); })
                       .base();

        if (Start >= End)
            return "";
        return std::string(Start, End);
    };

    Library String;
    String["New"] = StringNew;
    String["Case"] = StringCase;
    String["Trim"] = StringTrim;
    StdLib["String"] = String;

    return Value(StdLib);
}