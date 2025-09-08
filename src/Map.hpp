#pragma once
#include "Parser.hpp"
#include "Interpreter.hpp"
#include "Common.hpp"

#define def_map = [&_Map](const std::vector<std::any> &Args) -> std::any

exfunc PushBack def_map
{
    if (_Map.Length() <= 0)
        _Map.Set(rt_Int(0), AnyToValue(Args[0]));
    else
        _Map.Set(rt_Int(_Map.HighestIndex() + 1), AnyToValue(Args[0]));
    ret
};

exfunc HighestIndex def_map
{
    return rt_Int(_Map.HighestIndex());
};

Library MapLib;
MapLib["Size"] = rt_Int(_Map.Length());
MapLib["PushBack"] = PushBack;
MapLib["Top"] = HighestIndex;
