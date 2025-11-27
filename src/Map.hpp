#pragma once
#include "Parser.hpp"
#include "Interpreter.hpp"
#include "Common.hpp"

#define def_map = [&_Map](const std::vector<std::any> &Args) -> std::any

Library MapLib;
MapLib["Size"] = rt_Int(_Map.Length());
MapLib["End"] = rt_Int(_Map.HighestIndex() + 1);
