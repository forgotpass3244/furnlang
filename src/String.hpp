#pragma once
#include "Parser.hpp"
#include "Interpreter.hpp"
#include "Common.hpp"

#define def_str = [&String](const std::vector<std::any> &Args) -> std::any

exfunc Trim def_str
{
    auto Start = std::find_if_not(String.begin(), String.end(), [](unsigned char c)
                                  { return std::isspace(c); });
    auto End = std::find_if_not(String.rbegin(), String.rend(), [](unsigned char c)
                                { return std::isspace(c); })
                   .base();

    if (Start >= End)
        return "";
    return std::string(Start, End);
};

exfunc Case def_str
{
    const short Operation = std::any_cast<rt_Int>(Args[0]);

    switch (Operation)
    {
    case 0:
        std::transform(String.begin(), String.end(), String.begin(),
                       [](unsigned char c)
                       { return std::tolower(c); });
        break;

    case 1:
        std::transform(String.begin(), String.end(), String.begin(),
                       [](unsigned char c)
                       { return std::toupper(c); });
        break;

    case -1:
        std::transform(String.begin(), String.end(), String.begin(),
                       [](unsigned char c)
                       {
                           if (std::islower(c))
                               return std::toupper(c);
                           if (std::isupper(c))
                               return std::tolower(c);
                           return static_cast<int>(c);
                       });
        break;
    }

    return String;
};

exfunc At def_str
{
    const short AtPos = std::any_cast<rt_Int>(Args[0]);

    if (AtPos > String.length() || AtPos <= 0)
        return nullptr;

    return String[AtPos - 1];
};

exfunc Find def_str
{
    const std::string Needle = std::any_cast<std::string>(Args[0]);

    size_t Pos = String.find(Needle);
    if (Pos != std::string::npos)
        return static_cast<rt_Int>(Pos + 2);

    return nullptr;
};

exfunc Reverse def_str
{
    std::reverse(String.begin(), String.end());

    return String;
};

Library StringLib;
StringLib["Trim"] = Trim;
StringLib["Case"] = Case;
StringLib["At"] = At;
StringLib["Find"] = Find;
StringLib["Length"] = rt_Int(String.length());
StringLib["Reverse"] = Reverse;
