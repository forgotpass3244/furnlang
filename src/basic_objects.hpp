#include "common.hpp"
#include "interpreter.hpp"

struct basic_class
{
    const string name;
    any self;
    any here;

    basic_class(INTERPRETER &parent, const string &name_value)
        : name(name_value)
    {
        self = unordered_map<string, any>();
        here = unordered_map<string, any>();
    }
};

struct basic_function
{
    string name;
    string code;
    vector<string> param_names;
    vector<any> param_defaults;
    vector<string> param_types;
    vector<int> param_end_mod;

    basic_function(INTERPRETER &parent, const string _name, const string _code, const vector<string> _pnames, const vector<any> _pdefaults, const vector<string> _ptypes, const vector<int> _pendmods)
        : name(_name)
    {
        code = _code;
        param_names = _pnames;
        param_defaults = _pdefaults;
        param_types = _ptypes;
        param_end_mod = _pendmods;
    }
};

class silent_error : public std::exception
{
private:
    string message;
    any value;

public:
    silent_error(const string &msg, any val)
        : message(msg), value(val) {}

    any get_value() const
    {
        return value;
    }

    const char *what() const noexcept override
    {
        return message.c_str();
    }
};