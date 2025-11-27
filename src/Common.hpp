#pragma once

#include <vector>
#include <string>
#include <cctype>
#include <map>
#include <unordered_map>
#include <variant>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stack>
#include <memory>
#include <functional>
#include <random>
#include <any>
#include <algorithm>
#include <utility>
#include <thread>
#include <mutex>
#include <atomic>
#include <shared_mutex>
#include <climits>
#include "MagicEnum.hpp"

// old interpreter stuff
// #define ClassTD(_Class, Nullable) TypeDescriptor(ValueType::Custom, {}, std::make_shared<ValueExpression>(_Class), Nullable)
// #define THROWSTDERROR(ExceptionClass, Message, Data) throw Signal(SignalType::Signal, ValueToAny(Interp->EvaluateExpression(std::make_shared<UseExpression>(UseExpression(ClassTD(ExceptionClass, false), {std::make_shared<ValueExpression>(Message)}, {VarDeclaration(std::make_shared<ValueExpression>(Data), "data")})))))
// #define VoidReferenceType TypeDescriptor(ValueType::Custom, {}, std::make_shared<ValueExpression>(rt_Int(1)), false, false)
// #define StdStringType TypeDescriptor(ValueType::Custom, {}, std::make_shared<MemberExpression>(std::make_shared<VariableExpression>("std", 3), "String"))
// #define exfunc ExternalFunction
// #define def = [](const std::vector<std::any> &Args) -> std::any
// #define _def (const std::vector<std::any> &Args)->std::any
// #define ThisObject std::shared_ptr<ClassObject> This = std::make_shared<ClassObject>(std::get<ClassObject>(Interp->LoadAddress(2)))
// #define ret return nullptr;
// #define forarg for (const std::any &Arg : Args)

using MapId = unsigned long long;
MapId RandomMapId()
{
    MapId x = 2;
    MapId y = ULLONG_MAX;
    static std::random_device Rd;
    static std::mt19937_64 gen(Rd());
    std::uniform_int_distribution<MapId> dist(x, y);
    return dist(gen);
}

using rt_Int = long long;
using rt_Float = double;

std::string ToString(const std::any &Val)
{
    if (Val.type() == typeid(std::nullptr_t))
        return "null";
    else if (Val.type() == typeid(rt_Int))
        return std::to_string(std::any_cast<rt_Int>(Val));
    else if (Val.type() == typeid(rt_Float))
    {
        std::string Number = std::to_string(std::any_cast<rt_Float>(Val));

        while (!Number.empty() && Number.back() == '0')
        {
            Number.pop_back();
        }

        if (!Number.empty() && Number.back() == '.')
        {
            Number += '0';
        }

        return Number;
    }
    else if (Val.type() == typeid(bool))
        return std::any_cast<bool>(Val) ? "true" : "false";
    else if (Val.type() == typeid(std::string))
        return std::any_cast<std::string>(Val);
    else if (Val.type() == typeid(char))
        return std::string(1, std::any_cast<char>(Val));

    return "<unknown>";
}
