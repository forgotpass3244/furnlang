#pragma once
#include "Common.hpp"
#include "Interpreter.hpp"

enum class SignalType
{
    Return,
    Break,
    Continue,
    Signal,
};

// throw Signal(SignalType::Return, 1)
class Signal
{
public:
    SignalType Type;
    std::any Data;

    Signal(SignalType type, std::any data = nullptr) : Type(type), Data(data)
    {
    }
};