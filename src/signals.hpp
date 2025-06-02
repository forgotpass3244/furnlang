#include "common.hpp"
#include "interpreter.hpp"

struct ReturnSignal
{
    any value;
    ReturnSignal(any val) : value(val) {}
};

struct BreakSignal
{
    any value;
    BreakSignal(any val) : value(val) {}
};

struct ContinueSignal
{
    any value;
    ContinueSignal(any val) : value(val) {}
};

struct GoToSignal
{
    string label;
    GoToSignal(string _label) : label(_label) {}
};