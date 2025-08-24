#pragma once
#include "Common.hpp"

enum class SignalType
{
    Return,
    Break,
    Continue,
};

// throw Signal(SignalType::Return, 1)
class Signal
{
public:
    SignalType Type;
    std::any Data;

    Signal(SignalType type, std::any data = nullptr) : Type(type), Data(data) {}
};