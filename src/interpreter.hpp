#pragma once
#include "common.hpp"
#include "utilities.hpp"
#include "http_requests.hpp"
#include "signals.hpp"
#include "json_parser.hpp"

class INTERPRETER;
#include "basic_objects.hpp"

class INTERPRETER
{

public:
    long method_calls = 0;

    vector<unordered_map<string, any>> g_variables;
    unordered_map<string, any> g_funcs;
    vector<string> g_basic_classes;
    vector<string> read_only_classes;
    unordered_map<string, unordered_map<string, any>> classes_defaults_public;
    unordered_map<string, unordered_map<string, any>> classes_defaults_private;

    int current_executing_line = 0; //-

    int current_scope = 0;

    any error_value = undefined;
    bool interpreter_silent_errors = 0;
    short interpreter_error_count = 0;
    string last_error_message;
    any last_error_value = undefined;
    bool currently_tracing_back = false;

    bool executing_main = true;
    const string executing_main_name = "__Main";
    string current_executing_file;
    const string class_public_keyword = "self";
    const string class_private_keyword = "here";

    unordered_map<string, int> my_labels;

    string defining_class_values;
    string is_running_method;
    bool member_default_publicity;
    bool method_default_publicity;
    bool class_public_methods;

public:
    std::optional<ReturnSignal> execute(string code, bool new_scope = true)
    {
        if (new_scope)
            scope_up();

        const bool first_exec = !variable_exists(executing_main_name);

        vector<string> final_lines = split_lines(code, first_exec);

        final_lines.insert(final_lines.begin(), "");
        final_lines.insert(final_lines.end(), "");

        any return_value = ReturnSignal(no_return_data);

        define_variable(executing_main_name, executing_main, true);

        /* for (const auto &[label, content] : my_labels)
        {
            cout << "Label: \"" << label << "\"\n";
            cout << "Content:\n"
                 << content << "\n";
            cout << "-------------------------\n";
        } */

        bool went_down = false;

        for (int line_num = 0; line_num < final_lines.size(); ++line_num)
        {
            const string line = trim(final_lines[line_num]);

            if (line.empty())
                continue;
            else if (line == "return")
            {
                if (new_scope)
                    scope_down();
                return any_cast<ReturnSignal>(ReturnSignal(undefined));
            }
            else if (line == "break")
            {
                if (new_scope)
                    scope_down();
                return ReturnSignal(BreakSignal(0));
            }
            else if (line == "continue")
            {
                if (new_scope)
                    scope_down();
                return ReturnSignal(ContinueSignal(0));
            }

            any r = prc_ln(line);
            if (r.type() == typeid(ReturnSignal))
            {
                return_value = any_cast<ReturnSignal>(r);
                any return_data = any_cast<ReturnSignal>(r).value;

                if (return_data.type() == typeid(BreakSignal) || return_data.type() == typeid(ContinueSignal))
                    return ReturnSignal(return_data);
                else if (return_data.type() == typeid(GoToSignal))
                {
                    // cout << "Goto received " << any_cast<GoToSignal>(return_data).label << endl;

                    const string label = any_cast<GoToSignal>(return_data).label;

                    if ((my_labels.find(label) == my_labels.end()) || !first_exec)
                        return any_cast<ReturnSignal>(ReturnSignal(return_data));

                    const int label_num = my_labels[label];

                    line_num = label_num + 1;

                    // cout << "#" << line_num << " LINE... " << final_lines[line_num];

                    continue;
                }
                else
                {
                    if (new_scope)
                        scope_down();
                    return any_cast<ReturnSignal>(ReturnSignal(return_data));
                }
            }

            if (currently_tracing_back)
            {
                std::cerr << "    " << truncate_code(line, 21) << '\n';
                return ReturnSignal(BreakSignal(0));
            }
        }

        if (new_scope)
            scope_down();
        if (!went_down)
            return any_cast<ReturnSignal>(return_value);
        else
            return any_cast<ReturnSignal>(ReturnSignal(nullptr));
    }

    string truncate_code(const string &line, const short max_length = 37)
    {
        string line_display;
        bool in_quotes = false;
        
        for (int i = 0; i <= line.length(); ++i)
        {
            const char c = line[i];
            if (c == '"')
                in_quotes = !in_quotes;
            if (c == '\n' || i > max_length)
            {
                line_display = trim(line_display);
                line_display += string_ends_with(line_display, "{") && !in_quotes ? " ... }" : " ...";
                break;
            }
            line_display += c;
        }

        if (!line_display.empty() && std::iscntrl(line_display.back()))
            line_display.pop_back();
        return line_display;
    }

private:
    // Ensure the vector has at least `current_scope + 1` scopes
    void ensure_scope_exists()
    {
        method_calls++;
        while (g_variables.size() <= current_scope)
            g_variables.emplace_back();
    }

    void scope_up()
    {
        method_calls++;
        current_scope++;
        ensure_scope_exists();
    }

    void scope_down()
    {
        method_calls++;
        if (current_scope > 0)
            current_scope--;
        // Optionally clear the scope if you want to free memory:
        g_variables.pop_back();
    }

    string dump_variable_data()
    {
        method_calls++;
        stringstream outp;
        for (size_t i = 0; i < g_variables.size(); i++)
        {
            outp << "\nScope " << i << ":\n"; // Optional: Show the scope
            for (const auto &var : g_variables[i])
            {
                const string &var_name = var.first;
                const any &var_value = var.second;

                outp << " " << var_name << " = ";

                outp << get_type_name(var_value) << " ... " << printable(var_value);

                outp << "\n";
            }
            outp << "\n";
        }
        return outp.str();
    }

    bool variable_exists_in_current_scope(const std::string &name)
    {
        method_calls++;
        return g_variables[current_scope].count(name) > 0;
    }

    bool variable_exists(const std::string &name)
    {
        method_calls++;

        // Check normal scope variables
        for (int i = current_scope; i >= 0; --i)
        {
            if (g_variables[i].count(name))
                return true;
        }

        // Check fallback inside public/private maps
        const std::vector<std::string> fallback_keys = {class_public_keyword, class_private_keyword};
        for (const auto &key : fallback_keys)
        {
            for (int i = current_scope; i >= 0; --i)
            {
                if (g_variables[i].count(key))
                {
                    try
                    {
                        const auto &map = any_cast<const std::unordered_map<std::string, any> &>(g_variables[i][key]);
                        if (map.count(name))
                            return true;
                    }
                    catch (const std::bad_any_cast &)
                    {
                        // Optionally log or ignore
                    }
                }
            }
        }

        return false;
    }

    any &get_variable(const string &name)
    {
        method_calls++;

        int max_scope = std::min(current_scope, static_cast<int>(g_variables.size()) - 1);
        for (int i = max_scope; i >= 0; --i)
        {
            auto it = g_variables[i].find(name);
            if (it != g_variables[i].end())
            {
                any &val = it->second;
                return val;
            }
        }

        // Fallback: check inside "self" or "here" as maps
        const vector<string> fallback_keys = {class_public_keyword, class_private_keyword};
        for (const auto &key : fallback_keys)
        {
            for (int i = max_scope; i >= 0; --i)
            {
                auto container_it = g_variables[i].find(key);
                if (container_it != g_variables[i].end())
                {
                    any &container = container_it->second;
                    if (container.type() == typeid(unordered_map<string, any>))
                    {
                        auto &inner = any_cast<unordered_map<string, any> &>(container);
                        auto it = inner.find(name);
                        if (it != inner.end())
                        {
                            any &val = it->second;
                            return val;
                        }
                    }
                }
            }
        }

        error_any("VARIABLE NOT FOUND", name);

        static any r_temp = undefined;
        return r_temp;
    }

    void set_variable(const std::string &name, const any &value)
    {
        method_calls++;

        for (int i = current_scope; i >= 0; --i)
        {
            if (g_variables[i].count(name))
            {
                any &val = g_variables[i][name];
                val = value;
                return;
            }
        }

        // Fallback to class context (public/private maps)
        for (int i = current_scope; i >= 0; --i)
        {
            for (const auto &key : {class_public_keyword, class_private_keyword})
            {
                if (g_variables[i].count(key))
                {
                    try
                    {
                        auto &map = any_cast<std::unordered_map<std::string, any> &>(g_variables[i][key]);
                        if (map.count(name))
                        {
                            any &val = map[name];
                            map[name] = value;
                            return;
                        }
                    }
                    catch (const std::bad_any_cast &)
                    {
                        // Not a map so we skip
                    }
                }
            }
        }

        error_any("VARIABLE MUST BE DECLARED FIRST", "declare with 'let " + name + "'");
    }

    void define_variable(const std::string &name, const any &value, bool global_scope = false, const bool is_public = false, const bool class_member = false)
    {
        method_calls++;
        const bool method = !defining_class_values.empty();
        ensure_scope_exists(); // Make sure current scope exists
        if (global_scope && !class_member)
            g_variables[0][name] = value;
        else
        {
            if (class_member)
            {
                if (method)
                {
                    if (is_public)
                    {
                        if (value.type() == typeid(basic_function))
                        {
                            if (class_public_methods)
                                classes_defaults_public[defining_class_values][name] = value;
                            else
                                error_any("CANNOT DEFINE A PUBLIC METHOD HERE", name);
                        }
                        else
                            classes_defaults_public[defining_class_values][name] = value;
                    }
                    else
                        classes_defaults_private[defining_class_values][name] = value;
                    // g_variables[current_scope][name] = value;
                }
                else
                    error_any("CLASS MEMBERS MUST BE DECLARED DURING INITILIZATION", name);
                /*else if (is_public)
                {
                    if (variable_exists(class_public_keyword))
                    {
                        auto &pub = any_cast<unordered_map<string, any> &>(g_variables[current_scope][class_public_keyword]);
                        pub[name] = value;
                    }
                }
                else if (!is_public)
                {
                    if (variable_exists(class_private_keyword))
                    {
                        auto &pub = any_cast<unordered_map<string, any> &>(g_variables[current_scope][class_private_keyword]);
                        pub[name] = value;
                    }
                }*/
            }
            else
                g_variables[current_scope][name] = value;
        }
    }

    void undefine_variable(const string &name)
    {
        method_calls++;
        for (int i = current_scope; i >= 0; --i)
        {
            if (g_variables[i].count(name))
            {
                g_variables[i].erase(name);
                return;
            }
        }
    }

private:
    // Split code into logical segments per original line number, preserving empty lines
    vector<string> split_lines(const string &code, const bool identify_labels)
    {
        method_calls++;
        if (identify_labels)
            my_labels.clear();

        bool in_quotes = false;
        bool in_ml_comment = false;
        int in_braces = 0, in_parens = 0, in_brackets = 0;

        auto is_label_declaration = [](const string &line) -> bool
        {
            return string_starts_with(trim(line), "@");
        };

        vector<string> temp_lines;
        string line_accum;

        // First pass: split at ; or \n
        for (size_t i = 0; i < code.size(); ++i)
        {
            char ch = code[i];
            char nxt = (i + 1 < code.size()) ? code[i + 1] : '\0';
            char prv = (i > 0) ? code[i - 1] : '\0';

            if (!in_quotes)
            {
                if (ch == '\\' && nxt == '\n')
                {
                    ++i;
                    continue;
                }

                if (!in_ml_comment && ch == ':' && nxt == '[')
                {
                    in_ml_comment = true;
                    ++i;
                    continue;
                }
                if (in_ml_comment && ch == ']' && nxt == ':')
                {
                    in_ml_comment = false;
                    ++i;
                    continue;
                }
                if (in_ml_comment)
                    continue;

                if (ch == ':' && nxt == ':')
                {
                    while (i < code.size() && !(code[i] == '\n' && code[i - 1] != '\\'))
                        ++i;
                    --i;
                    continue;
                }

                if (ch == '{')
                    in_braces++;
                if (ch == '}')
                    in_braces = max(0, in_braces - 1);
                if (ch == '(')
                    in_parens++;
                if (ch == ')')
                    in_parens = max(0, in_parens - 1);
                if (ch == '[')
                    in_brackets++;
                if (ch == ']')
                    in_brackets = max(0, in_brackets - 1);
            }

            if (ch == '"')
            {
                in_quotes = !in_quotes;
                line_accum.push_back(ch);
                continue;
            }

            bool can_split = !in_quotes && !in_ml_comment && in_braces == 0 && in_parens == 0 && in_brackets == 0;

            if (can_split && (ch == ';' || (ch == '\n' && !(prv == '\\'))))
            {
                string segment = trim(line_accum);
                if (!segment.empty())
                    temp_lines.push_back(segment);
                line_accum.clear();
            }
            else
            {
                line_accum.push_back(ch);
            }
        }

        if (!line_accum.empty())
        {
            string segment = trim(line_accum);
            if (!segment.empty())
                temp_lines.push_back(segment);
        }

        // Second pass: process labels
        vector<string> result;
        string current_label;
        int i;
        for (i = 0; i < temp_lines.size(); ++i)
        {
            const string &line = temp_lines[i];
            if (identify_labels && is_label_declaration(line))
            {
                current_label = trim(line.substr(1));
                if (!IsValidVariableName(current_label))
                    error_any("INVALID LABEL NAME", current_label);
                else if (my_labels.find(current_label) != my_labels.end())
                    error_any("DUPLICATE LABEL", current_label);
                my_labels[current_label] = i;
                result.push_back("");
                // cout << "LABEL " << current_label << " " << i << endl;
            }
            else
                result.push_back(line);
        }

        return result;
    }

    std::pair<std::string, std::string> split_func_call(const std::string &text)
    {
        method_calls++;
        int paren_depth = 0;
        size_t last_close = std::string::npos;
        size_t match_open = std::string::npos;

        // Scan backwards to find last top-level ')'
        for (int i = static_cast<int>(text.size()) - 1; i >= 0; --i)
        {
            char c = text[i];
            if (c == ')')
            {
                if (paren_depth == 0)
                    last_close = i;
                ++paren_depth;
            }
            else if (c == '(')
            {
                --paren_depth;
                if (paren_depth == 0 && last_close != std::string::npos)
                {
                    match_open = i;
                    break;
                }
            }
        }

        if (match_open == std::string::npos || last_close == std::string::npos)
            return {"", ""};

        std::string func = text.substr(0, match_open);
        std::string args = text.substr(match_open + 1, last_close - match_open - 1);

        // Trim both
        auto trim = [](std::string &s)
        {
            size_t start = s.find_first_not_of(" \t\n\r");
            size_t end = s.find_last_not_of(" \t\n\r");
            s = (start == std::string::npos || end == std::string::npos) ? "" : s.substr(start, end - start + 1);
        };

        trim(func);
        trim(args);

        return {func, args};
    }

    // returns {library, member} where member is either literal after '.' or the
    // result of evaluate(...) on the expression inside [ ... ]
    std::pair<std::string, std::string> split_category(const std::string &input)
    {
        method_calls++;
        bool in_quotes = false;
        int bracket_depth = 0;
        size_t last_dot = std::string::npos;
        size_t last_bracket_open = std::string::npos;
        size_t last_bracket_close = std::string::npos;

        for (size_t i = 0; i < input.size(); ++i)
        {
            char c = input[i];
            if (c == '"' && bracket_depth == 0)
            {
                in_quotes = !in_quotes;
            }
            else if (!in_quotes)
            {
                if (c == '.' && bracket_depth == 0)
                {
                    last_dot = i;
                }
                else if (c == '[')
                {
                    if (bracket_depth == 0)
                        last_bracket_open = i;
                    ++bracket_depth;
                }
                else if (c == ']')
                {
                    --bracket_depth;
                    if (bracket_depth == 0)
                        last_bracket_close = i;
                }
            }
        }

        // 1) bracket form: lib[expr]
        if (last_bracket_open != std::string::npos && last_bracket_close != std::string::npos && last_bracket_close == input.size() - 1)
        {
            std::string lib = input.substr(0, last_bracket_open);
            std::string inner = input.substr(
                last_bracket_open + 1,
                last_bracket_close - last_bracket_open - 1);
            // evaluate(inner) -> any -> stringify
            any a = evaluate(inner);
            std::string member = printable(a);
            // cout << lib << endl << member << endl;
            return {lib, member};
        }
        // 2) dot form: lib.member
        else if (last_dot != std::string::npos)
        {
            return {
                input.substr(0, last_dot),
                input.substr(last_dot + 1)};
        }
        // 3) no split
        else
        {
            return {input, ""};
        }
    }

    vector<string> split_arguments(const string &cdata)
    {
        method_calls++;
        string data = trim(cdata);
        vector<string> result;
        string current;
        bool in_quotes = false;
        int brace_depth = 0;
        int bracket_depth = 0;
        int paren_depth = 0;

        for (size_t i = 0; i < data.size(); ++i)
        {
            char c = data[i];

            if (c == '"')
            {
                if (!in_quotes)
                    in_quotes = true;
                else
                    in_quotes = false;
            }
            else if (!in_quotes)
            {
                if (c == '{')
                    ++brace_depth;
                else if (c == '}')
                    --brace_depth;
                else if (c == '[')
                    ++bracket_depth;
                else if (c == ']')
                    --bracket_depth;
                else if (c == '(')
                    ++paren_depth;
                else if (c == ')')
                    --paren_depth;

                if (c == ',' && bracket_depth == 0 && paren_depth == 0 && brace_depth == 0)
                {
                    result.push_back(current);
                    current.clear();
                    continue;
                }
            }

            current += c;
        }

        if (!current.empty())
            result.push_back(trim(current));

        return result;
    }

    bool is_truthy(const any &a)
    {
        method_calls++;
        if (a.type() == typeid(long))
        {
            long val = any_cast_int(a);
            return !(val == 0 || val == -1);
        }
        if (a.type() == typeid(double))
        {
            double val = any_cast_float(a);
            return !(val == 0.0f || val == -1.0f);
        }
        if (a.type() == typeid(bool))
            return any_cast_bool(a);
        if (a.type() == typeid(string))
            return !any_cast_string(a).empty();
        if (a.type() == typeid(nullptr))
            return false;
        if (a.type() == typeid(undefined_t))
            return false;
        if (a.type() == typeid(no_return_data_t))
            return false;
        if (a.type() == typeid(basic_class))
        {
            basic_class my_class = any_cast<basic_class>(a);
            unordered_map<string, any> here_dict = any_cast<unordered_map<string, any>>(my_class.here);
            auto it = here_dict.find("__Truthy");
            if (it != g_funcs.end())
            {
                scope_up();
                define_variable(class_public_keyword, my_class.self, false);
                define_variable(class_private_keyword, my_class.here, false);
                basic_function auto_run_func = any_cast<basic_function>(it->second);
                auto result_any = execute(auto_run_func.code)->value;
                scope_down();
                return any_cast_bool(result_any);
            }
            return true;
        }
        error_any("UNSUPPORTED IS_TRUTHY OPERAND", a); // handle unknown types
        return false;
    };

    void apply_operator(std::vector<any> &values, std::vector<string> &ops)
    {
        method_calls++;
        any r = values.back();
        values.pop_back();

        string op = ops.back();
        ops.pop_back();

        auto meta_method = [this, op, r](any bclass, const string meta_name, any argument = undefined, any argument2 = undefined) -> any
        {
            basic_class my_class = any_cast<basic_class>(bclass);

            unordered_map<string, any> here_dict;
            if (my_class.name == "__Operators")
                here_dict = any_cast<unordered_map<string, any>>(my_class.self);
            else
                here_dict = any_cast<unordered_map<string, any>>(my_class.here);

            auto it = here_dict.find(meta_name);
            if (it != here_dict.end())
            {
                this->scope_up();
                this->define_variable(class_public_keyword, my_class.self, false);
                this->define_variable(class_private_keyword, my_class.here, false);
                basic_function auto_run_func = any_cast<basic_function>(it->second);
                any result_any = this->exec_bfunc(auto_run_func, vector<any>({argument, argument2}), false);
                this->scope_down();
                return result_any;
            }
            else
                error_any("NO META METHOD", "op " + op + " '" + meta_name + '\'');
            return no_return_data;
        };

        any l = values.empty() ? undefined : values.back();
        if (!values.empty())
            values.pop_back();

        if (variable_exists("__Operators"))
        {
            basic_class my_ops = any_cast<basic_class>(get_variable("__Operators"));
            unordered_map<string, any> op_names = any_cast<unordered_map<string, any>>(my_ops.here);
            unordered_map<string, any> op_funcs = any_cast<unordered_map<string, any>>(my_ops.self);

            for (auto [nm, oprr] : op_names)
            {
                const string opr = any_cast<string>(oprr);
                if (op == opr)
                {
                    if (l.type() == typeid(basic_class))
                        values.push_back(meta_method(l, "__" + nm, r));
                    else
                        values.push_back(meta_method(my_ops, nm, l, r));
                    return;
                }
            }
        }

        // Unary
        if (op == "!")
        {
            if (r.type() == typeid(basic_class))
                values.push_back(meta_method(r, "__Not"));
            else
                values.push_back(!is_truthy(r));
            return;
        }
        else if (op == "#")
        {
            if (r.type() == typeid(string))
                values.push_back(static_cast<long>(any_cast_string(r).size()));
            else if (r.type() == typeid(basic_class))
                values.push_back(meta_method(r, "__Hash"));
            return;
        }
        else if (op == "u-")
        {
            if (r.type() == typeid(long))
                values.push_back(-any_cast<long>(r));
            else if (r.type() == typeid(double))
                values.push_back(-any_cast<double>(r));
            else if (r.type() == typeid(basic_class))
                values.push_back(meta_method(r, "__uMinus"));
            return;
        }

        // any .. any => string
        if (op == "..")
        {
            values.push_back(printable(l) + printable(r));
            return;
        }
        // string + string
        else if (op == "+" &&
                 l.type() == typeid(string) &&
                 r.type() == typeid(string))
        {
            values.push_back(any_cast_string(l) + any_cast_string(r));
            return;
        }
        // string - int
        else if (op == "-" &&
                 l.type() == typeid(string) &&
                 r.type() == typeid(long))
        {
            string str = any_cast_string(l);
            int amount = any_cast_int(r);
            if (amount < str.size())
            {
                values.push_back(str.substr(0, str.size() - amount));
            }
            else
            {
                values.push_back(string(""));
            }
            return;
        }
        // string * int
        else if (op == "*" &&
                 l.type() == typeid(string) &&
                 r.type() == typeid(long))
        {
            string str = any_cast_string(l);
            int amount = any_cast_int(r);
            string result;
            for (int i = 0; i < amount; ++i)
                result += str; // Append the string to the result
            values.push_back(result);
            return;
        }
        // string / int
        else if (op == "/" &&
                 l.type() == typeid(string) &&
                 r.type() == typeid(long))
        {
            string str = any_cast_string(l);
            int amount = any_cast_int(r);

            vector<string> result;
            if (amount == 0)
            {
                string("");
                return;
            }

            size_t len = str.size();
            size_t chunk = len / amount; // base size of each part
            size_t rem = len % amount;   // leftover characters

            size_t offset = 0;
            for (size_t i = 0; i < amount; ++i)
            {
                size_t this_size = chunk + (i + 1 == amount ? rem : 0);
                result.push_back(str.substr(offset, this_size));
                offset += this_size;
            }
            values.push_back(result[0]);
            return;
        }
        // string == string
        else if (op == "==" &&
                 l.type() == typeid(string) &&
                 r.type() == typeid(string))
        {
            values.push_back(any_cast_string(l) == any_cast_string(r));
            return;
        }

        auto get_float = [&](const any &a) -> double
        {
            if (a.type() == typeid(long))
                return (double)any_cast_int(a);
            if (a.type() == typeid(double))
                return any_cast_float(a);
            error_any("EXPECTED NUMERIC VALUE", "(" + printable(a) + ") " + printable(l) + op + printable(r));
            return 0;
        };

        if (op == "...")
        {
            vector<any> vec;

            int a = any_cast_float(get_float(l));
            int b = any_cast_float(get_float(r));

            if (b < a)
                error_any("B MUST BE GREATER THAN A", b);
            for (int j = a; j <= b; ++j)
                vec.push_back(j);
            values.push_back(vec);

            return;
        }

        // Comparison
        // General comparison
        if (op == "==" || op == "!=")
        {
            if (l.type() != r.type() && (l.type() != typeid(basic_class)))
                values.push_back(op == "!="); // different types are never equal
            else if (l.type() == typeid(long))
                values.push_back((any_cast<long>(l) == any_cast<long>(r)) == (op == "=="));
            else if (l.type() == typeid(double))
                values.push_back((any_cast<double>(l) == any_cast<double>(r)) == (op == "=="));
            else if (l.type() == typeid(string))
                values.push_back((any_cast<string>(l) == any_cast<string>(r)) == (op == "=="));
            else if (l.type() == typeid(bool))
                values.push_back((any_cast<bool>(l) == any_cast<bool>(r)) == (op == "=="));
            else if (l.type() == typeid(basic_class))
            {
                const basic_class &l_class = any_cast<basic_class>(l);

                unordered_map<string, any> here_dict = any_cast<unordered_map<string, any>>(l_class.here);

                auto it = here_dict.find("__Eq");
                if (it != here_dict.end())
                    values.push_back(any_cast_bool(meta_method(l_class, "__Eq", r)) == (op == "=="));
                else
                {
                    const auto &r_class = any_cast<basic_class>(r);

                    if (l_class.self.type() == typeid(unordered_map<string, any>) &&
                        r_class.self.type() == typeid(unordered_map<string, any>))
                    {
                        const auto &l_map = any_cast<const unordered_map<string, any> &>(l_class.self);
                        const auto &r_map = any_cast<const unordered_map<string, any> &>(r_class.self);

                        bool are_equal = compare_unordered_map_string_any(l_class.self, r_class.self); // use function

                        bool result = (are_equal == (op == "=="));

                        values.push_back(result);
                    }
                    else
                    {
                        values.push_back(false);
                    }
                }
            }
            else
                // fallback: compare type_info names (optional safety net)
                values.push_back((l.type() == r.type()) == (op == "==")); // assume equal for matching types
            return;
        }
        else if (op == ">")
        {
            values.push_back(get_float(l) > get_float(r));
        }
        else if (op == "<")
        {
            values.push_back(get_float(l) < get_float(r));
        }
        else if (op == ">=")
        {
            values.push_back(get_float(l) >= get_float(r));
        }
        else if (op == "<=")
        {
            values.push_back(get_float(l) <= get_float(r));
        }
        else if (op == "||")
        {
            values.push_back(is_truthy(l) ? l : r);
        }
        else if (op == "&&")
        {
            values.push_back(is_truthy(l) ? r : l);
        }
        else if (op == "?")
        {
            values.push_back(is_truthy(l) ? r : undefined);
        }
        else if (op == ":")
        {
            values.push_back(l.type() != typeid(undefined_t) ? l : r);
        }
        else
        {
            double res = 0;

            // Arithmetic
            if (l.type() == typeid(basic_class))
            {
                string meta_name;

                if (op == "+")
                    meta_name = "__Add";
                else if (op == "-")
                    meta_name = "__Subtract";
                else if (op == "*")
                    meta_name = "__Multiply";
                else if (op == "/")
                    meta_name = "__Divide";
                else if (op == "%")
                    meta_name = "__Mod";
                else if (op == "^")
                    meta_name = "__Pow";

                res = any_cast_float(meta_method(l, meta_name, r), false);
            }
            else
            {
                double ld = get_float(l), rd = get_float(r);
                if (op == "+")
                    res = ld + rd;
                else if (op == "-")
                    res = ld - rd;
                else if (op == "*")
                    res = ld * rd;
                else if (op == "/")
                    res = (rd == 0 ? 0 : ld / rd);
                else if (op == "%")
                    if (rd == 0)
                        res = 0;
                    else
                        res = std::fmod(ld, rd);
                else if (op == "^")
                    res = std::pow(ld, rd);
                else
                    error_any("UNKNOWN OPERATOR", op);
            }

            // Return int if result is whole and inputs were ints
            if (l.type() != typeid(double) && r.type() != typeid(double) && std::floor(res) == res)
                values.push_back((long)res);
            else
                values.push_back(res);
        }
    }

    int precedence(const string &op)
    {
        method_calls++;

        if (variable_exists("__Operators"))
        {
            basic_class my_ops = any_cast<basic_class>(get_variable("__Operators"));
            unordered_map<string, any> op_names = any_cast<unordered_map<string, any>>(my_ops.here);
            unordered_map<string, any> op_funcs = any_cast<unordered_map<string, any>>(my_ops.self);

            for (auto [nm, oprr] : op_names)
            {
                const string opr = any_cast<string>(oprr);
                if (op == opr)
                    return any_cast_int(GetIndex(any_cast<basic_function>(op_funcs[nm]).param_defaults, 2, -1));
            }
        }

        if (op == "!" || op == "#" || op == "u-")
            return 7; // Unary ops - highest

        if (op == "...")
            return 6; // Range creation (1 ... 100)

        if (op == "^")
            return 5; // Exponentiation

        if (op == "*" || op == "/" || op == "%")
            return 4; // Multiplicative

        if (op == "+" || op == "-")
            return 3; // Additive

        if (op == "..")
            return 2; // String concatenation

        if (op == "<" || op == ">" || op == "<=" || op == ">=" ||
            op == "==" || op == "!=" || op == "&&" || op == "||")
            return 1; // Comparisons & logical ops

        if (op == "?" || op == ":")
            return 0; // Ternary - lowest

        return -1; // Unknown operator
    }

    vector<string> tokenize(const string &expr)
    {
        method_calls++;
        vector<string> tokens;
        string cur;
        bool inStr = false;
        string singles = "+-*/<>!#%?:";
        vector<string> multis = {"==", "!=", "<=", ">=", "||", "&&", "...", ".."};

        if (variable_exists("__Operators"))
        {
            basic_class my_ops = any_cast<basic_class>(get_variable("__Operators"));
            unordered_map<string, any> op_names = any_cast<unordered_map<string, any>>(my_ops.here);

            for (auto [nm, oprr] : op_names)
            {
                const string opr = any_cast<string>(oprr);
                if (opr.length() == 1)
                    singles += opr;
                else
                    multis.push_back(opr);
            }
        }

        for (size_t i = 0; i < expr.size(); ++i)
        {
            char c = expr[i];

            // ——— Inside a string literal ———
            if (inStr)
            {
                cur += c;
                if (c == '"')
                {
                    // we just closed the string; now see if there's a .MethodCall(...) to attach
                    inStr = false;
                    size_t j = i + 1;
                    while (j < expr.size() && expr[j] == '.')
                    {
                        cur += '.';
                        ++j;
                        while (j < expr.size() && (isalnum(expr[j]) || expr[j] == '_'))
                            cur += expr[j++];
                        if (j < expr.size() && expr[j] == '(')
                        {
                            int depth = 1;
                            cur += '(';
                            ++j;
                            while (j < expr.size() && depth > 0)
                            {
                                cur += expr[j];
                                if (expr[j] == '(')
                                    ++depth;
                                else if (expr[j] == ')')
                                    --depth;
                                ++j;
                            }
                        }
                    }
                    tokens.push_back(cur);
                    cur.clear();
                    i = j - 1;
                }
                continue;
            }

            // ——— NEW: Handle $ before " ———
            if (c == '$' && i + 1 < expr.size() && expr[i + 1] == '"')
            {
                if (!cur.empty())
                {
                    tokens.push_back(cur);
                    cur.clear();
                }
                cur = "$\"";
                inStr = true;
                ++i; // skip the '"' we just consumed manually
                continue;
            }

            // ——— Starting a normal string literal ———
            if (c == '"')
            {
                if (!cur.empty())
                {
                    tokens.push_back(cur);
                    cur.clear();
                }
                cur = "\"";
                inStr = true;
                continue;
            }

            // ——— Handle '(' for either sub‐expr or function call ———
            if (c == '(')
            {
                if (!cur.empty() && isalpha(cur.back()))
                {
                    string fn = cur + "(";
                    cur.clear();
                    int depth = 1;
                    ++i;
                    while (i < expr.size() && depth > 0)
                    {
                        fn += expr[i];
                        if (expr[i] == '(')
                            ++depth;
                        else if (expr[i] == ')')
                            --depth;
                        ++i;
                    }
                    --i;
                    tokens.push_back(fn);
                }
                else
                {
                    if (!cur.empty())
                    {
                        tokens.push_back(cur);
                        cur.clear();
                    }
                    tokens.push_back("(");
                }
                continue;
            }

            // ——— Closing parenthesis ———
            if (c == ')')
            {
                if (!cur.empty())
                {
                    tokens.push_back(cur);
                    cur.clear();
                }
                tokens.push_back(")");
                continue;
            }

            // ——— Multi‐char operators ———
            bool matched = false;
            for (auto &op : multis)
            {
                if (expr.compare(i, op.size(), op) == 0)
                {
                    if (!cur.empty())
                    {
                        tokens.push_back(cur);
                        cur.clear();
                    }
                    tokens.push_back(op);
                    i += op.size() - 1;
                    matched = true;
                    break;
                }
            }
            if (matched)
                continue;

            // ——— Whitespace ———
            if (isspace(c))
            {
                if (!cur.empty())
                {
                    tokens.push_back(cur);
                    cur.clear();
                }
            }
            // ——— Single‐char operator ———
            else if (strchr(singles.c_str(), c))
            {
                if (!cur.empty())
                {
                    tokens.push_back(cur);
                    cur.clear();
                }
                tokens.emplace_back(1, c);
            }
            // ——— Otherwise accumulate identifier/number ———
            else
            {
                cur += c;
            }
        }

        if (!cur.empty())
            tokens.push_back(cur);
        return tokens;
    }

    vector<string> sort_by_length(const vector<string> &input)
    {
        vector<string> sorted = input; // Copy to avoid modifying original

        std::sort(sorted.begin(), sorted.end(), [](const string &a, const string &b)
                  {
                      if (a.length() == b.length())
                          return a < b;               // Alphabetical order for same length
                      return a.length() > b.length(); // Descending length
                  });

        return sorted;
    }

    any evaluate_math(const string &expression)
    {
        method_calls++;

        vector<string> def_ops = {
            "!",
            "#",
            "+",
            "-",
            "*",
            "/",
            "%",
            "^",
            "==",
            "!=",
            "<",
            ">",
            "<=",
            ">=",
            "||",
            ":",
            "&&",
            "?",
            "...",
            "..",
        };

        if (variable_exists("__Operators"))
        {
            basic_class my_ops = any_cast<basic_class>(get_variable("__Operators"));
            unordered_map<string, any> op_names = any_cast<unordered_map<string, any>>(my_ops.here);

            for (auto [nm, oprr] : op_names)
            {
                const string opr = any_cast<string>(oprr);
                def_ops.push_back(opr);
            }
        }

        auto tokens = tokenize(expression);
        vector<any> values;
        vector<string> ops;

        for (auto &tok : tokens)
        {
            // cout << tok << '\n';
            if (tok == "(")
            {
                ops.push_back(tok);
            }
            else if (tok == ")")
            {
                while (!ops.empty() && ops.back() != "(")
                {
                    apply_operator(values, ops);
                }
                if (!ops.empty())
                    ops.pop_back();
            }
            else if (std::find(def_ops.begin(), def_ops.end(), tok) != def_ops.end())
            {
                bool isUnary = (values.empty() || (!ops.empty() && ops.back() == "("));

                string actual_op = tok;
                if (tok == "-" && isUnary)
                    actual_op = "u-"; // unary minus

                while (!ops.empty() && precedence(ops.back()) >= precedence(actual_op))
                {
                    apply_operator(values, ops);
                }
                ops.push_back(actual_op);
            }
            else if (tok.empty())
            {
                values.push_back(0);
            }
            else
                values.push_back(evaluate(tok));
        }

        while (!ops.empty())
        {
            apply_operator(values, ops);
        }

        if (values.empty())
            throw std::runtime_error("Invalid expression");
        return values.back();
    }

    std::pair<vector<string>, vector<string>> extract_conditions_and_contents(const string &code, char front = '{', char back = '}')
    {
        method_calls++;
        vector<string> conditions;
        vector<string> contents;
        string current_condition = "";
        string current_content = "";
        vector<char> stack;
        bool inside_string = false;
        int stack_parentheses = 0;

        size_t i = 0;
        while (i < code.size())
        {
            char current_char = code[i];

            // Toggle inside_string flag when encountering quotes (ignoring escaped quotes)
            if (current_char == '"')
                inside_string = !inside_string;

            if (!inside_string)
            { // Only process braces if not inside a string
                if (current_char == front && stack_parentheses == 0)
                {
                    if (!stack.empty())
                        current_content += current_char; // Keep nested opening brace
                    stack.push_back(front);
                    if (stack.size() == 1)
                    { // First opening brace -> store condition
                        conditions.push_back(trim(current_condition));
                        current_condition.clear(); // Reset condition collector
                    }
                }
                else if (current_char == back && stack_parentheses == 0)
                {
                    stack.pop_back();
                    if (!stack.empty())
                        current_content += current_char; // Keep nested closing brace
                    if (stack.empty())
                    { // Last closing brace -> store content
                        contents.push_back(current_content);
                        current_content.clear(); // Reset content collector
                    }
                }
                else if (stack.empty())
                { // Outside braces -> collecting condition
                    current_condition += current_char;

                    if (current_char == '(')
                        stack_parentheses++;
                    else if (current_char == ')')
                        stack_parentheses--;
                }
                else
                { // Inside braces -> collecting content
                    current_content += current_char;
                }
            }
            else
            {
                if (!stack.empty())
                {
                    current_content += current_char; // Inside quotes, keep collecting normally
                }
                else
                {
                    current_condition += current_char;
                }
            }

            i++;
        }

        return {conditions, contents};
    }

    string get_type_name(const any data)
    {
        method_calls++;
        if (data.type() == typeid(silent_error))
            return "error";
        else if (data.type() == typeid(string))
            return "string";
        else if (data.type() == typeid(int))
            return "int";
        else if (data.type() == typeid(double))
            return "float";
        else if (data.type() == typeid(bool))
            return "bool";
        else if (data.type() == typeid(nullptr) || data.type() == typeid(undefined_t))
            return "void";
        else if (data.type() == typeid(vector<any>))
            return "array";
        else if (data.type() == typeid(unordered_map<string, any>))
            return "dict";
        else if (data.type() == typeid(basic_function))
            return "function";
        else if (data.type() == typeid(basic_class))
        {
            basic_class my_class = any_cast<basic_class>(data);
            return my_class.name;
        }
        return "unknown";
    }

    void error_any(const string msg, const any value)
    {
        method_calls++;
        interpreter_error_count++;
        last_error_message = msg;
        last_error_value = value;
        if (currently_tracing_back)
            return;
        const string what = truncate_code(printable(value));
        const string firstln_msg = truncate_code(msg) + ": " + "(" + get_type_name(value) + ") " + what;
        if (interpreter_silent_errors)
            throw silent_error(msg, value);
        std::cerr << '\n'
                  << "Uncaught Error '"
                  << current_executing_file
                  << "'\n"
                  << firstln_msg
                  << '\n';
        short i = 0;
        for (auto &&letter : firstln_msg)
        {
            i++;
            // EXPECTED STRING VALUE: (int) 100
            //                              ^^^
            if (i > firstln_msg.size() - what.size())
            {
                std::cerr << "^";
            }
            else
            {
                std::cerr << " ";
            }
        }
        std::cerr << "\nTraceback Begin\n\n";

        currently_tracing_back = true;
    }

    template <typename T>
    vector<any> to_any_vector(const vector<T> &input)
    {
        method_calls++;
        vector<any> output;
        output.reserve(input.size());
        for (const auto &item : input)
        {
            output.emplace_back(item);
        }
        return output;
    }

    any any_cast_void(const any &value, const bool strict = true)
    {
        method_calls++;
        if (value.type() == typeid(nullptr))
            return nullptr;
        else if (value.type() == typeid(undefined_t))
            return undefined;
        else
            error_any("EXPECTED VOID VALUE", value);
        return undefined;
    }

    bool any_cast_bool(const any &value, const bool strict = true)
    {
        method_calls++;

        if (!strict)
            return is_truthy(value);

        try
        {
            return any_cast<bool>(value);
        }
        catch (const std::bad_any_cast &)
        {
            if (strict)
                error_any("EXPECTED BOOL VALUE", value);
            return true;
        }
    }

    string any_cast_string(const any &value, const bool strict = true)
    {
        method_calls++;

        if (!strict)
            return printable(value);

        try
        {
            return any_cast<string>(value);
        }
        catch (const std::bad_any_cast &)
        {
            if (strict)
                error_any("EXPECTED STRING VALUE", value);
            return "";
        }
    }

    long any_cast_int(const any &value, const bool strict = true)
    {
        method_calls++;
        if (!strict)
        {
            if (value.type() == typeid(nullptr) || value.type() == typeid(undefined_t))
            {
                return 0;
            }
            else if (value.type() == typeid(bool))
            {
                return any_cast<bool>(value) ? 1 : -1;
            }
            else if (value.type() == typeid(double))
            {
                return static_cast<long>(any_cast<double>(value)); // truncate
            }
        }
        else
        {
            if (value.type() == typeid(double))
            {
                double f = any_cast<double>(value);
                if (f == static_cast<long>(f))
                    return static_cast<long>(f);
            }
        }

        try
        {
            return any_cast<long>(value);
        }
        catch (const std::bad_any_cast &)
        {
            if (strict)
                error_any("EXPECTED INTEGER VALUE", value);
            return 0;
        }
    }

    double any_cast_float(const any &value, const bool strict = true)
    {
        method_calls++;
        if (!strict)
        {
            if (value.type() == typeid(nullptr) || value.type() == typeid(undefined_t))
            {
                return 0.0f;
            }
            else if (value.type() == typeid(bool))
            {
                return any_cast<bool>(value) ? 1.0f : -1.0f;
            }
            else if (value.type() == typeid(long))
            {
                return static_cast<double>(any_cast<long>(value));
            }
        }

        try
        {
            return any_cast<double>(value);
        }
        catch (const std::bad_any_cast &)
        {
            if (strict)
                error_any("EXPECTED FLOAT VALUE", value);
            return 0.0f;
        }
    }

    basic_function any_cast_bfunc(const any &value, const bool strict = true)
    {
        method_calls++;
        try
        {
            return any_cast<basic_function>(value);
        }
        catch (const std::bad_any_cast &)
        {
            if (strict)
                error_any("EXPECTED FUNCTION VALUE", value);
            return basic_function(*this, "_UNNAMED", "", vector<string>(), vector<any>(), vector<string>(), vector<int>());
        }
    }

    string printable(const any &data, const bool pretty_print = false, const int indent = 0)
    {
        method_calls++;

        // safeguard
        if (currently_tracing_back)
            return "";

        auto make_indent = [](int count)
        {
            return string(count, ' ');
        };

        if (data.type() == typeid(silent_error))
        {
            const silent_error err = any_cast<silent_error>(data);
            return truncate_code(err.what()) + string(": ") + "(" + get_type_name(err.get_value()) + ") " + truncate_code(printable(err.get_value()));
        }
        else if (data.type() == typeid(string))
        {
            static auto replace_char_string = [](const string &str, char target, const string &replacement) -> string
            {
                string result;
                result.reserve(str.size());

                for (char c : str)
                {
                    if (c == target)
                        result += replacement;
                    else
                        result += c;
                }

                return result;
            };

            string str = any_cast_string(data);
            return str;
            if (indent == 0)
                return str;
            else
                return replace_char_string(str, '"', "\\'");
        }
        else if (data.type() == typeid(long))
            return std::to_string(any_cast<long>(data));
        else if (data.type() == typeid(double))
        {
            double value = any_cast_float(data);
            string str = std::to_string(value);
            // trim trailing zeros
            str.erase(str.find_last_not_of('0') + 1, string::npos);
            if (!str.empty() && str.back() == '.')
                str.append("0");
            return str;
        }
        else if (data.type() == typeid(bool))
            return any_cast_bool(data) ? "true" : "false";
        else if (data.type() == typeid(nullptr))
            return "nil";
        else if (data.type() == typeid(undefined))
            return "undefined";
        else if (data.type() == typeid(vector<any>))
        {
            const auto &vec = any_cast<const vector<any> &>(data);
            const bool compact = pretty_print && (vec.size() <= 1);

            string out = "[";
            if (pretty_print && !compact && !vec.empty())
                out += '\n';
            for (size_t i = 0; i < vec.size(); ++i)
            {
                if (pretty_print && !compact)
                    out += make_indent(indent + 4);

                const bool is_str = get_type_name(vec[i]) == "string";
                if (is_str)
                    out += '"';
                out += printable(vec[i], pretty_print, indent + 4);
                if (is_str)
                    out += '"';

                if (i + 1 < vec.size())
                    out += ", "; // Always space after commas

                if (pretty_print && !compact)
                    out += '\n';
            }
            if (pretty_print && !compact && !vec.empty())
                out += make_indent(indent);
            out += "]";
            return out;
        }
        else if (data.type() == typeid(unordered_map<string, any>))
        {
            const auto &dict = any_cast<const unordered_map<string, any> &>(data);
            const bool compact = pretty_print && (dict.size() <= 1);

            string out = "$[";
            if (pretty_print && !compact && !dict.empty())
                out += '\n';

            size_t count = 0;
            for (const auto &[key, val] : dict)
            {
                if (pretty_print && !compact)
                    out += make_indent(indent + 4);

                out += key + ": ";

                const bool is_str = get_type_name(val) == "string";
                if (is_str)
                    out += '"';
                out += printable(val, pretty_print, indent + 4);
                if (is_str)
                    out += '"';

                if (++count < dict.size())
                    out += ", ";

                if (pretty_print && !compact)
                    out += '\n';
            }

            if (pretty_print && !compact && !dict.empty())
                out += make_indent(indent);
            out += "]";
            return out;
        }
        else if (data.type() == typeid(basic_function))
        {
            basic_function my_func = any_cast_bfunc(data);
            const string pself = "function " + my_func.name + '<' + get_memory_addr(my_func) + '>';
            return pself;
        }
        else if (data.type() == typeid(basic_class))
        {
            basic_class my_class = any_cast<basic_class>(data);
            const string pself = "object " + my_class.name + '<' + get_memory_addr(my_class) + '>';
            unordered_map<string, any> here_dict = any_cast<unordered_map<string, any>>(my_class.here);
            auto it = here_dict.find("__Print");
            if (it != g_funcs.end())
            {
                scope_up();
                define_variable(class_public_keyword, my_class.self, false);
                define_variable(class_private_keyword, my_class.here, false);
                basic_function auto_run_func = any_cast<basic_function>(it->second);
                any result_any = exec_bfunc(auto_run_func, vector<any>(), false);
                scope_down();
                return any_cast_string(result_any);
            }
            return pself;
        }

        return "<unknown>";
    }

    any &get_ptr(any &val)
    {
        return val;
    }

    any evaluate(const string &cdata)
    {
        method_calls++;

        string data = cdata;

        auto quotes_split = split_string(data, '"', 3);
        bool interpolate = !data.empty() && data[0] == '$';

        if (quotes_split.size() == 2 &&
            ((interpolate && data[1] == '"') || (!interpolate && data.front() == '"' && data.back() == '"')))
        {
            string raw = quotes_split[1];
            string unescaped;
            for (size_t i = 0; i < raw.size(); ++i)
            {
                if (raw[i] == '\\' && i + 1 < raw.size())
                {
                    char next = raw[i + 1];
                    switch (next)
                    {
                    case '\\':
                        unescaped += '\\';
                        break;
                    case '\'':
                        unescaped += '"';
                        break;
                    case 'n':
                        unescaped += '\n';
                        break;
                    case 't':
                        unescaped += '\t';
                        break;
                    case 'r':
                        unescaped += '\r';
                        break;
                    case 'b':
                        unescaped += '\b';
                        break;
                    case 'e':
                        unescaped += '\x1B';
                        break;
                    case 'f':
                        unescaped += '\f';
                        break;
                    case 'a':
                        unescaped += '\a';
                        break;
                    case 'v':
                        unescaped += '\v';
                        break;
                    case '0':
                        unescaped += '\0';
                        break;
                    case '$':
                        unescaped += '$';
                        break; // escape $ symbol
                    default:
                        unescaped += next;
                        error_any("INVALID ESCAPE SEQUENCE", string(1, '\\') + next);
                        break;
                    }
                    ++i;
                }
                else if (interpolate && raw[i] == '$' && i + 1 < raw.size() && raw[i + 1] == '{')
                {
                    size_t end = raw.find('}', i + 2);
                    if (end != string::npos)
                    {
                        string varname = raw.substr(i + 2, end - i - 2);
                        string value = printable(evaluate(varname)); // You define this
                        unescaped += value;
                        i = end;
                    }
                    else
                    {
                        unescaped += raw[i];
                    }
                }
                else
                {
                    unescaped += raw[i];
                }
            }
            return unescaped;
        }

        if (variable_exists(data))
        {
            const string name = data;
            any &var = get_variable(name);
            return var;
        }
        else if (data == "true" || data == "yes")
            return true;
        else if (data == "false" || data == "no")
            return false;
        else if (data == "nil")
            return nullptr;
        else if (data == "undef")
            return undefined;

        if (find_outside(data, "["))
        {
            const auto cond_and_cont = extract_conditions_and_contents(data, '[', ']');
            const string cond = cond_and_cont.first[0];
            const string cont = trim(cond_and_cont.second[0]);
            if (cond.empty())
            {
                const auto raw_array = split_arguments(cont);

                vector<any> evaluated_array;
                evaluated_array.reserve(raw_array.size());

                for (auto &&item : raw_array)
                {
                    evaluated_array.push_back(evaluate(trim(item)));
                }
                for (auto &&i : evaluated_array)
                {
                    // cout << printable(i);
                }
                return evaluated_array;
            }
            else if (cond == "$")
            {
                const auto raw_dict = split_arguments(cont);

                unordered_map<string, any> evaluated_dict;
                // evaluated_dict.reserve(raw_dict.size());

                for (auto &&item : raw_dict)
                {
                    const vector<string> split_key_value = split_string(item, ':', 2);
                    // cout << split_key_value[0] << endl << split_key_value[1] << endl;
                    const string key = trim(split_key_value[0]);
                    const any val = evaluate(trim(split_key_value[1]));
                    evaluated_dict[key] = val;
                }
                return evaluated_dict;
            }
        }

        try
        {
            string cleaned;
            for (char c : data)
            {
                if (c != '\'') // remove single quotes
                    cleaned += c;
            }

            size_t pos;
            long int_value = std::stoi(cleaned, &pos);
            if (pos == cleaned.size())
                return int_value;
        }
        catch (...)
        {
        }

        try
        {
            size_t pos;
            double float_value = std::stof(data, &pos);
            if (pos == data.size())
                return float_value;
        }
        catch (...)
        {
        }

        if (!data.empty())
        {
            string ops = "!#+-*/^<>?:%";
            vector<string> comp_ops = {
                "==",
                "!=",
                "<=",
                ">=",
                "||",
                "&&",
                "...",
                "..",
            };

            if (variable_exists("__Operators"))
            {
                basic_class my_ops = any_cast<basic_class>(get_variable("__Operators"));
                unordered_map<string, any> op_names = any_cast<unordered_map<string, any>>(my_ops.here);

                for (auto [nm, oprr] : op_names)
                {
                    const string opr = any_cast<string>(oprr);
                    if (opr.length() == 1)
                        ops += opr;
                    else
                        comp_ops.push_back(opr);
                }
            }

            auto has_single_op = [&](const string &s)
            {
                bool inside_string = false;
                int brace_depth = 0;
                int brack_depth = 0;
                int paren_depth = 0;

                for (size_t i = 0; i < s.size(); ++i)
                {
                    char c = s[i];

                    // Handle string literals
                    if (c == '"')
                    {
                        inside_string = !inside_string;
                        continue;
                    }

                    if (inside_string)
                        continue;

                    // Handle braces
                    if (c == '{')
                    {
                        brace_depth++;
                        continue;
                    }
                    if (c == '}')
                    {
                        if (brace_depth > 0)
                            brace_depth--;
                        continue;
                    }

                    if (brace_depth > 0)
                        continue;

                    if (c == '[')
                    {
                        brack_depth++;
                        continue;
                    }
                    if (c == ']')
                    {
                        if (brack_depth > 0)
                            brack_depth--;
                        continue;
                    }

                    if (brack_depth > 0)
                        continue;

                    // Handle parentheses
                    if (c == '(')
                    {
                        paren_depth++;
                        continue;
                    }
                    if (c == ')')
                    {
                        if (paren_depth > 0)
                            paren_depth--;
                        continue;
                    }

                    // We only care about operators outside of any () or {}
                    if (paren_depth == 0)
                    {
                        // Multi-char comparison operators (e.g., ==, !=, <=, >=)
                        for (const auto &cop : comp_ops)
                        {
                            if (i + cop.size() <= s.size() && s.compare(i, cop.size(), cop) == 0)
                                return true;
                        }

                        // Single character operators (e.g., + - * / % etc.)
                        if (ops.find(c) != string::npos)
                            return true;
                    }
                }

                return false;
            };

            if (has_single_op(data))
                return evaluate_math(cdata);
        }

        return prc_ln(data);
    }

    any GetIndex(const any &list, int index, const any &default_value = any{})
    {
        method_calls++;
        if (list.type() == typeid(vector<any>))
        {
            const auto &vec = any_cast<const vector<any> &>(list);
            if (index >= 0 && index < static_cast<int>(vec.size()))
            {
                return vec[index];
            }
            return default_value;
        }
        return default_value;
    }
    any GetIndex(const any &list, int index, const char *default_value)
    {
        method_calls++;
        if (default_value == nullptr)
            return GetIndex(list, index, string(""));
        return GetIndex(list, index, string(default_value));
    }

    vector<string> is_decl_var(const string &line)
    {
        method_calls++;
        bool in_quote = false;
        int stack_parentheses = 0;
        int stack_braces = 0;

        static const vector<string> valid_ops = {
            "=",
            "+=",
            "-=",
            "*=",
            "/=",
            "^=",
            "%=",
            "<<",
            ">>",
        };

        for (size_t i = 0; i < line.size(); ++i)
        {
            char c = line[i];

            if (c == '"')
            {
                in_quote = !in_quote;
                continue;
            }

            if (!in_quote)
            {
                if (c == '{')
                {
                    stack_braces++;
                    continue;
                }
                else if (c == '}')
                {
                    stack_braces--;
                    continue;
                }

                if (c == '(')
                {
                    stack_parentheses++;
                    continue;
                }
                else if (c == ')')
                {
                    stack_parentheses--;
                    continue;
                }
            }

            if (in_quote || stack_braces != 0 || stack_parentheses != 0)
                continue;

            for (const string &op : valid_ops)
            {
                size_t len = op.size();
                if (i + len > line.size())
                    continue;
                if (line.compare(i, len, op) != 0)
                    continue;

                // Extra check for plain "=" to avoid "==", "!=", etc.
                if (op == "=")
                {
                    if ((i > 0 && (line[i - 1] == '=' || line[i - 1] == '!' || line[i - 1] == '<' || line[i - 1] == '>')) ||
                        (i + 1 < line.size() && line[i + 1] == '='))
                        continue;
                }

                // Check before operator
                bool before_ok = (i == 0);
                if (!before_ok)
                {
                    char b = line[i - 1];
                    before_ok = (std::isalpha(b) || b == ' ' || b == '_' || b == '-');
                }
                if (!before_ok)
                    continue;

                // Check after operator
                bool after_ok = (i + len >= line.size());
                if (!after_ok)
                {
                    char a = line[i + len];
                    after_ok = (std::isalnum(a) || a == ' ' || a == '_' || a == '-');
                }
                if (!after_ok)
                    continue;

                string lhs = trim(line.substr(0, i));
                string rhs = trim(line.substr(i + len));
                return {lhs, op, rhs};
            }
        }

        return {"", "", ""};
    }

    bool IsValidVariableName(const string &name)
    {
        method_calls++;
        if (name.empty())
            return false;

        // First character must be a letter or underscore
        if (!(std::isalpha(name[0]) || name[0] == '_'))
            return false;

        // Remaining characters can be alphanumeric or underscore
        for (size_t i = 1; i < name.size(); ++i)
        {
            char c = name[i];
            if (!(std::isalnum(c) || c == '_'))
            {
                return false;
            }
        }

        return true;
    }

    any prc_ln(const string &cline)
    {
        method_calls++;

        string line = trim(cline);

        any non_value = no_return_data;
        if (line.empty() || currently_tracing_back)
            return non_value;

        auto conditions_and_contents = extract_conditions_and_contents(line);
        auto conditions = conditions_and_contents.first;
        auto contents = conditions_and_contents.second;

        const vector<string> command_split = split_string(line + "  ", ' ', 2);
        const string command = command_split[0];
        const string command_data = trim(command_split[1]);

        if ((command == "glo" || command == "let" || command == "pub" || command == "pri") && IsValidVariableName(command_data))
        {
            bool is_global = command == "glo";
            bool is_public = command == "pub";
            string var_name = command_data; // Strip prefix

            if (!IsValidVariableName(var_name))
            {
                error_any("Invalid variable name", any(var_name));
                return non_value;
            }

            // Declare and initialize to nullptr
            define_variable(var_name, undefined, is_global, is_public, command == "pub" || command == "pri");

            return non_value;
        }
        else if (command == "return")
            return ReturnSignal(evaluate(command_data));
        else if (command == "fail")
        {
            const any val = evaluate(command_data);
            if (val.type() == typeid(silent_error))
            {
                const silent_error err = any_cast<silent_error>(val);
                error_any(err.what(), err.get_value());
            }
            else
                error_any("ERROR", val);
            return non_value;
        }
        else if (command == "exit")
        {
            const int exit_code = any_cast_int(evaluate(command_data));
            if (is_integrated_term)
                cout << "\nProcess exited with exit code " + std::to_string(exit_code) + ".\n";
            std::exit(exit_code);
            return exit_code;
        }
        else if (command == "goto")
        {
            bool condition = true;

            const vector<string> args = split_arguments(command_data);

            const string to = trim(args[0]);

            if (args.size() > 1)
                condition = is_truthy(evaluate(args[1]));

            if (condition)
                return ReturnSignal(GoToSignal(to));

            return non_value;
        }

        else if ((conditions.size() > 0 && contents.size() > 0) && (conditions.size() == contents.size()) && (is_decl_var(conditions[0])[1] != "=") && !conditions[0].empty() && string_ends_with(line, "}"))
        {
            auto ctrl_and_vals = split_func_call(conditions[0]);
            string controller = ctrl_and_vals.first;
            auto possible_code = contents[0];

            const string raw_tk = ctrl_and_vals.second;

            string ctrl_type;
            vector<string> ctrl_data;

            const vector<string> tks = split_string(conditions[0] + "  ", ' ', 2);
            ctrl_type = trim(tks[0]);
            const string &args = tks[1];
            ctrl_data = split_arguments(trim(tks[1]));

            auto prc_arguments = split_arguments(raw_tk);
            vector<string> arguments;

            for (string &arg : prc_arguments)
            {
                arguments.push_back(trim(arg));
            }

            if (ctrl_type == "class" || ctrl_type == "struct" || ctrl_type == "record" || ctrl_type == "module")
            {
                string nm = ctrl_data[0];
                string inherited_from;

                if (ctrl_type == "class")
                {
                    if (find_outside(nm, ":"))
                    {
                        const vector<string> split_nm = split_string(nm, ':', 2);
                        nm = trim(split_nm[0]);
                        inherited_from = trim(split_nm[1]);
                    }
                }

                if (!inherited_from.empty())
                {
                    classes_defaults_public[nm] = classes_defaults_public[inherited_from];
                    classes_defaults_private[nm] = classes_defaults_private[inherited_from];
                }

                g_basic_classes.push_back(nm);
                if (ctrl_type == "record" || ctrl_type == "module")
                    read_only_classes.push_back(nm);
                defining_class_values = nm;
                member_default_publicity = ctrl_type != "class";
                method_default_publicity = ctrl_type == "module";
                class_public_methods = ctrl_type == "class" || ctrl_type == "module";
                execute(possible_code, true);
                defining_class_values = "";

                if (ctrl_type == "module")
                {
                    basic_class new_class(*this, nm);
                    new_class.self = classes_defaults_public[nm];
                    new_class.here = classes_defaults_private[nm];

                    define_variable(nm, new_class, true);
                }
            }
            else if (conditions[0] == "attempt")
            {
                if (!(conditions.size() > 1))
                    error_any("ATTEMPTING TO SILENTLY SWALLOW AN ERROR", string("please add an 'except' block"));

                const vector<string> try_tks = split_string(conditions[1] + "  ", ' ', 2);
                const string try_type = trim(try_tks[0]);
                const string err_name = trim(try_tks[1]);

                if (try_type == "except") {
                    bool success = true;
                    const bool pre_silent = interpreter_silent_errors;

                    std::optional<ReturnSignal> ret_data;

                    try
                    {
                        interpreter_silent_errors = true;
                        ret_data = execute(possible_code);
                        interpreter_silent_errors = pre_silent;
                    }
                    catch (const silent_error& err)
                    {
                        success = false;
                        interpreter_silent_errors = pre_silent;
                        scope_up();
                        define_variable(err_name, err);
                        ret_data = execute(contents[1], false);
                        scope_down();
                    }

                    if (conditions.size() > 2)
                    {
                        if (conditions[2] == "success")
                        {
                            if (success)
                            {
                                const auto ret_val = ret_data->value;
                                if (ret_val.type() == typeid(no_return_data_t))
                                    ret_data = execute(contents[2]);
                            }

                            if (conditions.size() > 3)
                            {
                                if (conditions[3] == "finally")
                                    execute(contents[3]);
                                else
                                    error_any("EXPECTED KEYWORD 'finally'", conditions[3]);
                            }
                        }
                        else if (conditions[2] == "finally")
                        {
                            if (!ret_data.has_value())
                                ret_data = execute(contents[2]);
                            else
                                execute(contents[2]);
                        }
                        else
                            error_any("EXPECTED KEYWORD 'success' or 'finally'", conditions[2]);
                    }

                    if (ret_data.has_value())
                    {
                        const auto ret_val = ret_data->value;
                        if (ret_val.type() != typeid(no_return_data_t))
                            return ReturnSignal(ret_val);
                    }
                }
                else
                    error_any("EXPECTED KEYWORD 'except'", try_type);
            }
            else
            {
                if (ctrl_type == "while")
                {
                    bool cond = is_truthy(evaluate(ctrl_data[0]));
                    int i = -1;

                    if (!trim(possible_code).empty() || true)
                        while (cond)
                        {
                            i++;

                            const auto ret_data = execute(possible_code);

                            if (ret_data.has_value())
                            {
                                const auto ret_val = ret_data->value;
                                if (ret_val.type() == typeid(BreakSignal))
                                    break;
                                else if (ret_val.type() != typeid(no_return_data_t) && ret_val.type() != typeid(ContinueSignal))
                                    return ReturnSignal(ret_val);
                            }

                            cond = is_truthy(evaluate(ctrl_data[0]));
                        }
                }
                else if (ctrl_type == "foreach")
                {
                    const string item_var = ctrl_data[0];
                    any iterable = evaluate(ctrl_data[1]);
                    int i = -1;

                    // DEBUG cout << iterable.type().name();

                    vector<any> vec; // We'll fill this

                    if (iterable.type() == typeid(vector<any>))
                        vec = std::any_cast<const vector<any> &>(iterable);
                    else if (iterable.type() == typeid(unordered_map<string, any>))
                    {
                        const auto &map = std::any_cast<const unordered_map<string, any> &>(iterable);
                        for (const auto &kv : map)
                        {
                            vec.push_back(kv.second); // store only the values
                        }
                    }
                    else if (iterable.type() == typeid(string))
                    {
                        const string &s = std::any_cast<const string &>(iterable);
                        for (char ch : s)
                        {
                            vec.push_back(string(1, ch)); // make string from 1 char
                        }
                    }
                    else if (iterable.type() == typeid(long))
                    {
                        long n = std::any_cast<long>(iterable);
                        if (n < 0)
                            error_any("Cannot iterate a negative number.", iterable);
                        for (long j = 0; j < n; ++j)
                            vec.push_back(j);
                    }
                    else
                    {
                        error_any("Trying to iterate over non-iterable type.", iterable);
                        return non_value;
                    }

                    if (!trim(possible_code).empty())
                        for (const auto &item : vec)
                        {
                            i++;

                            scope_up();
                            define_variable(item_var, item); // here 'item' is already a single 'any'
                            const auto ret_data = execute(possible_code, false);
                            scope_down();

                            if (ret_data.has_value())
                            {
                                const auto ret_val = ret_data->value;
                                if (ret_val.type() == typeid(BreakSignal))
                                    break;
                                else if (ret_val.type() != typeid(no_return_data_t) && ret_val.type() != typeid(ContinueSignal))
                                    return ReturnSignal(ret_val);
                            }
                        }
                }
                else if (ctrl_type == "if")
                {
                    bool condition_matched = false;
                    for (size_t i = 0; i < conditions.size(); ++i)
                    {
                        auto cond_content = trim(conditions[i]);

                        string cond_type, cond_data;
                        if (string_starts_with(cond_content, "else"))
                        {
                            string after_else = trim(cond_content.substr(4)); // skip "else"
                            if (string_starts_with(after_else, "if "))
                            {
                                cond_type = "else if";
                                cond_data = trim(after_else.substr(3)); // skip "if "
                            }
                            else if (after_else.empty())
                            {
                                cond_type = "else";
                                cond_data = "";
                            }
                            else
                            {
                                error_any("Invalid else condition", cond_content);
                            }
                        }
                        else if (string_starts_with(cond_content, "if "))
                        {
                            cond_type = "if";
                            cond_data = trim(cond_content.substr(3)); // skip "if "
                        }
                        else
                        {
                            error_any("Invalid condition type", cond_content);
                        }

                        if (cond_type == "if" || cond_type == "else if")
                        {
                            if (!condition_matched)
                            {
                                bool cond = is_truthy(evaluate(cond_data));
                                if (cond)
                                {
                                    condition_matched = true;
                                    const auto ret_data = execute(contents[i]);
                                    if (ret_data.has_value())
                                    {
                                        const auto ret_val = ret_data->value;
                                        if (ret_val.type() != typeid(no_return_data_t))
                                            return ReturnSignal(ret_val);
                                    }
                                    break;
                                }
                            }
                        }
                        else if (cond_type == "else")
                        {
                            if (!condition_matched)
                            {
                                const auto ret_data = execute(contents[i]);
                                if (ret_data.has_value())
                                {
                                    const auto ret_val = ret_data->value;
                                    if (ret_val.type() != typeid(no_return_data_t))
                                        return ReturnSignal(ret_val);
                                }
                            }
                        }
                    }
                }
                else if (ctrl_type == "match")
                {
                    bool matched = false;
                    any matching = evaluate(trim(ctrl_data[0]));
                    for (size_t i = 1; i < conditions.size(); ++i)
                    {
                        any to_match = evaluate(trim(conditions[i]));
                        vector<any> toks = {matching, to_match};
                        vector<string> op = {"=="};
                        apply_operator(toks, op);

                        if (is_truthy(toks.back()))
                        {
                            const auto ret_data = execute(contents[i]);
                            if (ret_data.has_value())
                            {
                                const auto ret_val = ret_data->value;
                                if (ret_val.type() != typeid(no_return_data_t))
                                    return ReturnSignal(ret_val);
                            }
                            matched = true;
                            break;
                        }
                    }
                    if (!matched)
                    {
                        const auto ret_data = execute(possible_code);
                        if (ret_data.has_value())
                        {
                            const auto ret_val = ret_data->value;
                            if (ret_val.type() != typeid(no_return_data_t))
                                return ReturnSignal(ret_val);
                        }
                    }
                }
                else
                {
                    if (controller == "for")
                    {
                        scope_up();
                        prc_ln(arguments[0]);

                        if (!trim(possible_code).empty() || true)
                        {
                            while (is_truthy(evaluate(arguments[1])))
                            {
                                const auto ret_data = execute(possible_code, false);

                                if (ret_data.has_value())
                                {
                                    const auto ret_val = ret_data->value;
                                    if (ret_val.type() == typeid(BreakSignal))
                                        break;
                                    else if (ret_val.type() != typeid(no_return_data_t) && ret_val.type() != typeid(ContinueSignal))
                                        return ReturnSignal(ret_val);
                                }

                                prc_ln(arguments[2]);
                            }
                        }

                        scope_down();
                    }
                    else if (IsValidVariableName(controller) || ctrl_type == "pub" || ctrl_type == "pri" || ctrl_type == "glo" || ctrl_type == "let")
                    {
                        string func_name = controller;

                        vector<string> param_names;
                        vector<any> param_defaults;
                        vector<string> param_types;
                        vector<int> param_end_mod;

                        for (auto &&param : arguments)
                        {
                            string param_name = param;
                            any param_default = undefined;
                            string param_type = "";
                            int param_end_modif = 0;

                            if (find_outside(param_name, "="))
                            {
                                const vector<string> split_p = split_string(param, '=', 2);
                                param_name = trim(split_p[0]);
                                param_default = evaluate(trim(split_p[1]));
                            }
                            if (find_outside(param_name, ":"))
                            {
                                const vector<string> split_p2 = split_string(param_name, ':', 2);
                                // cout << trim(split_p2[0]) << endl;
                                // cout << trim(split_p2[1]) << endl;
                                param_name = trim(split_p2[1]);
                                param_type = trim(split_p2[0]);
                                if (string_starts_with(param_type, "?"))
                                {
                                    param_type = trim(param_type.substr(1));
                                    param_end_modif = 1;
                                }
                                else if (string_starts_with(param_type, "&"))
                                {
                                    param_type = trim(param_type.substr(1));
                                    param_end_modif = 2;
                                }
                            }

                            if (!IsValidVariableName(param_name))
                                error_any("INVALID PARAMETER NAME", param_name);
                            param_names.push_back(param_name);
                            param_defaults.push_back(param_default);
                            param_types.push_back(param_type);
                            param_end_mod.push_back(param_end_modif);
                        }

                        basic_function new_func(*this, func_name, possible_code, param_names, param_defaults, param_types, param_end_mod);

                        if (func_name == "fn")
                            return new_func;

                        const bool method = !defining_class_values.empty();
                        const bool is_public = ctrl_type == "pub";
                        const bool is_private = ctrl_type == "pri";
                        const bool is_global = ctrl_type == "glo";
                        const bool is_local = ctrl_type == "let";

                        const string func_name_error = string("INVALID ") + (method ? "METHOD" : "FUNCTION") + " NAME";

                        if (variable_exists(func_name) && !method && IsValidVariableName(func_name))
                            set_variable(func_name, new_func);
                        else if (is_public)
                        {
                            func_name = trim(func_name.substr(3));
                            new_func.name = func_name;
                            if (!IsValidVariableName(func_name))
                                error_any(func_name_error, func_name);

                            define_variable(func_name, new_func, false, true, true);
                        }
                        else if (is_private)
                        {
                            func_name = trim(func_name.substr(3));
                            new_func.name = func_name;
                            if (!IsValidVariableName(func_name))
                                error_any(func_name_error, func_name);
                            define_variable(func_name, new_func, false, false, true);
                        }
                        else if (is_global)
                        {
                            func_name = trim(func_name.substr(3));
                            new_func.name = func_name;
                            if (!IsValidVariableName(func_name))
                                error_any(func_name_error, func_name);
                            define_variable(func_name, new_func, true);
                        }
                        else if (is_local)
                        {
                            func_name = trim(func_name.substr(3));
                            new_func.name = func_name;
                            if (!IsValidVariableName(func_name))
                                error_any(func_name_error, func_name);
                            define_variable(func_name, new_func, false);
                        }
                        else
                        {
                            if (method)
                            {
                                /* make methods default to public
                                unless its in a struct */

                                new_func.name = func_name;

                                if (!IsValidVariableName(func_name))
                                    error_any(func_name_error, func_name);

                                define_variable(func_name, new_func, false, method_default_publicity, true);
                            }
                            else
                                error_any("FUNCTION MUST BE DECLARED FIRST", "declare with 'let " + func_name + "( ... ) { ... }'");
                        }
                    }
                }
            }
            return non_value;
        }
        else
        {
            auto var_data = is_decl_var(line);
            string var_name = var_data[0];
            string mut_type = var_data[1];
            string value_str = var_data[2];

            if (mut_type == "=")
            {
                bool is_declaration = false;
                string var_avaliability = "";

                bool is_public = false;
                bool class_member = false;

                if (string_starts_with(var_name, "glo "))
                {
                    is_declaration = true;
                    var_avaliability = "global";
                    var_name = trim(var_name.substr(3));
                }
                else if (string_starts_with(var_name, "let "))
                {
                    is_declaration = true;
                    var_avaliability = "local";
                    var_name = trim(var_name.substr(3));
                }
                else if (string_starts_with(var_name, "pub "))
                {
                    class_member = true;
                    is_public = true;
                    is_declaration = true;
                    var_avaliability = "local";
                    var_name = trim(var_name.substr(3));
                }
                else if (string_starts_with(var_name, "pri "))
                {
                    class_member = true;
                    is_declaration = true;
                    var_avaliability = "local";
                    var_name = trim(var_name.substr(3));
                }

                const bool typep = find_outside(var_name, ":");
                string ptype;
                if (typep)
                {
                    const vector<string> p = split_string(var_name, ':', 2);
                    ptype = trim(p[0]);
                    var_name = trim(p[1]);
                }

                if (!IsValidVariableName(var_name))
                {
                    if (!is_declaration && (find_outside(var_name, ".") || find_outside(var_name, "[")))
                    {
                        auto [library, prop] = split_category(var_name);

                        any value = evaluate(value_str);

                        if (library == "Task")
                        {
                            if (prop == "Title")
                            {
                                ConsoleTitle(printable(value));
                                return non_value;
                            }
                            else if (prop == "OutSync")
                            {
                                const bool should_sync = any_cast_bool(value);
                                std::ios::sync_with_stdio(should_sync);
                                return non_value;
                            }
                            else if (prop == "SilentErrors")
                            {
                                interpreter_silent_errors = any_cast_bool(value);
                                return non_value;
                            }
                        }
                        else if (library == "plat_Windows")
                        {
                            if (prop == "uClipboard")
                            {
                                const string &text = any_cast_string(value);
                                SetClipboard(text);
                                return non_value;
                            }
                        }
                        else
                        {
                            any object = evaluate(library);
                            string obj_type = get_type_name(object);

                            any original_obj = object;

                            // detect if the object is readonly like a record
                            auto it_read_only = std::find(read_only_classes.begin(), read_only_classes.end(), obj_type);
                            const bool read_only = it_read_only != read_only_classes.end();

                            auto it_class = std::find(g_basic_classes.begin(), g_basic_classes.end(), obj_type);
                            const bool is_class = it_class != g_basic_classes.end();
                            if (is_class)
                            {
                                basic_class my_class = any_cast<basic_class>(object);
                                object = my_class.self;
                                obj_type = get_type_name(my_class.self);
                            }

                            if (read_only)
                                error_any("ATTEMPTED TO EDIT A READ-ONLY OBJECT", original_obj);

                            if (obj_type == "array")
                            {
                                vector<any> &arr = any_cast<vector<any> &>(object);

                                const int idx = std::stoi(prop);
                                arr[idx - 1] = value;
                                if (variable_exists(library))
                                    set_variable(library, arr);
                                return arr;
                            }
                            else if (obj_type == "dict")
                            {
                                unordered_map<string, any> &dict = any_cast<unordered_map<string, any> &>(object);

                                const string idx = prop;
                                if (value.type() == typeid(undefined_t))
                                    dict.erase(idx);
                                else
                                    dict[idx] = value;
                                if (variable_exists(library))
                                {
                                    if (!is_class)
                                        set_variable(library, dict);
                                    else
                                    {
                                        basic_class my_class = any_cast<basic_class>(original_obj);
                                        my_class.self = dict;
                                        set_variable(library, my_class);
                                        return my_class;
                                    }
                                }
                                return dict;
                            }
                        }
                        error_any("UNKNOWN PROPERTY IN PROPERTY LIBRARY '" + library + "'", prop);
                        return non_value;
                    }
                    else
                    {
                        error_any("INVALID VARIABLE NAME", var_name);
                        return non_value;
                    }
                }

                // cout << var_name << " " << value_str;
                any value = evaluate(value_str);

                if (typep)
                {
                    const bool pstrict = false;

                    if (ptype == "void")
                        value = any_cast_void(value, pstrict);
                    else if (ptype == "bool")
                        value = any_cast_bool(value, pstrict);
                    else if (ptype == "string")
                        value = any_cast_string(value, pstrict);
                    else if (ptype == "int")
                        value = any_cast_int(value, pstrict);
                    else if (ptype == "float")
                        value = any_cast_float(value, pstrict);
                    else if (ptype == "array")
                        value = any_cast<vector<any>>(value);
                    else if (ptype == "dict")
                        value = any_cast<unordered_map<string, any>>(value);
                    else if (ptype == "fn")
                        value = any_cast_bfunc(value, pstrict);
                    else if (std::find(g_basic_classes.begin(), g_basic_classes.end(), ptype) != g_basic_classes.end())
                    {
                        const string IncorrectTypeError = "EXPECTED '" + ptype + "' TYPE";

                        try
                        {
                            value = any_cast<basic_class>(value);
                        }
                        catch (const std::bad_any_cast &e)
                        {
                            error_any(IncorrectTypeError, value);
                        }

                        if (any_cast<basic_class>(value).name != ptype)
                            error_any(IncorrectTypeError, value);
                    }
                    else
                        error_any("UNKNOWN TYPE PARAMETER", ptype);
                }

                if (var_avaliability == "global")
                {
                    define_variable(var_name, value, true, is_public, class_member);
                }
                else if (var_avaliability == "local")
                {
                    define_variable(var_name, value, false, is_public, class_member);
                }
                else if (!is_declaration)
                {
                    // Assignment to existing variable
                    if (!defining_class_values.empty() && !variable_exists(var_name))
                        define_variable(var_name, value, false, member_default_publicity, true);
                    else
                        set_variable(var_name, value);
                }

                return non_value;
            }

            auto is_int = [](const any &val) -> bool
            {
                return val.type() == typeid(int);
            };

            if ((mut_type == "+=" || mut_type == "-=" || mut_type == "*=" || mut_type == "/=" || mut_type == "^=" || mut_type == "%=") && false)
            {
                any old_any = evaluate(var_name);
                any rhs_any = evaluate(value_str);

                vector<any> mut_tokens = {old_any, rhs_any};

                vector<string> ops;
                if (mut_type == "+=")
                    ops = {"+"};
                else if (mut_type == "-=")
                    ops = {"-"};
                else if (mut_type == "*=")
                    ops = {"*"};
                else if (mut_type == "/=")
                    ops = {"/"};
                else if (mut_type == "^=")
                    ops = {"^"};
                else if (mut_type == "%=")
                    ops = {"%"};

                apply_operator(mut_tokens, ops);

                any new_val = mut_tokens.back();
                mut_tokens.pop_back(); // optional cleanup

                set_variable(var_name, new_val);
                return non_value;
            }
            else if (mut_type == "<<" || mut_type == ">>")
            {
                if (mut_type == "<<")
                {
                    any out_val = evaluate(value_str);
                    if (var_name == "io")
                    {
                        // FAST
                        cout << printable(out_val);

                        // FOR UNICODE8 ... allows emojis

                        // REALLY SLOW
                        // const string msg = printable(out_val).c_str();
                        // system(string("echo " + msg).c_str());
                    }
                    else if (var_name == "errors")
                    {
                        error_value = out_val;
                        return non_value;
                    }
                    else if (var_name == "garbage")
                    {
                        undefine_variable(value_str);
                        return non_value;
                    }
                    else if (var_name == "script")
                    {
                        scope_up();
                        const string module_file = LocalDirectory + "\\" + any_cast_string(out_val);
                        executing_main = false;
                        const string pre_file = current_executing_file;
                        current_executing_file = any_cast_string(out_val);
                        execute(get_file(module_file).first, false);
                        define_variable(executing_main_name, true, true);
                        scope_down();
                        executing_main = true;
                        current_executing_file = pre_file;
                        return non_value;
                    }
                    else
                    {
                        error_any("INVALID IO TYPE", var_name);
                    }
                    return non_value;
                }
                else if (mut_type == ">>")
                {
                    string in_val = value_str;
                    if (var_name == "io")
                    {
                        if (!IsValidVariableName(in_val))
                            error_any("INVALID INPUT POINTER", in_val);
                        string answer;
                        std::getline(cin, answer);
                        define_variable(in_val, answer);
                    }
                    else if (var_name == "errors")
                    {
                        error_any(printable(evaluate(in_val)), error_value);
                    }
                    else
                    {
                        error_any("INVALID IO TYPE", var_name);
                    }
                    return non_value;
                }
            }

            auto result = split_func_call(line);
            if (!(result.first.empty() && result.second.empty()) && string_ends_with(line, ")"))
            {
                string library;
                string func_call = result.first;
                const string libfunc_split = func_call;

                if (find_outside(libfunc_split, ".") || find_outside(libfunc_split, "["))
                {
                    auto pair = split_category(libfunc_split);
                    library = pair.first;
                    func_call = pair.second;
                }

                string raw_arguments = result.second;
                auto unprc_arguments = split_arguments(raw_arguments);
                vector<any> arguments;

                for (string &arg : unprc_arguments)
                {
                    arguments.push_back(evaluate(trim(arg)));
                }

                if (library.empty())
                {
                    auto it_class = std::find(g_basic_classes.begin(), g_basic_classes.end(), func_call);
                    if (it_class != g_basic_classes.end())
                    {
                        scope_up();
                        basic_class new_class(*this, func_call);
                        new_class.self = classes_defaults_public[func_call];
                        new_class.here = classes_defaults_private[func_call];

                        unordered_map<string, any> here_dict = any_cast<unordered_map<string, any>>(new_class.here);

                        auto it_constructor = here_dict.find("__New");

                        if (it_constructor != here_dict.end())
                        {
                            basic_function func = any_cast_bfunc(here_dict["__New"]);
                            scope_up();

                            define_variable(class_public_keyword, new_class.self, false);
                            define_variable(class_private_keyword, new_class.here, false);

                            exec_bfunc(func, arguments, false);

                            if (variable_exists(class_public_keyword) && variable_exists(class_private_keyword))
                            {
                                new_class.self = get_variable(class_public_keyword);
                                new_class.here = get_variable(class_private_keyword);
                            }
                            scope_down();
                        }
                        return new_class;
                    }
                    else
                    {
                        if (func_call == "error")
                            return silent_error(any_cast_string(GetIndex(arguments, 0, "")), GetIndex(arguments, 1, undefined));
                        else if (func_call == "StrToFunc")
                        {
                            const string code = any_cast_string(GetIndex(arguments, 0, ""));
                            return basic_function(*this, "funcstr", code, vector<string>(), vector<any>(), vector<string>(), vector<int>());
                        }
                        else if (func_call == "eval")
                        {
                            const string code = any_cast_string(GetIndex(arguments, 0, ""));
                            return evaluate(code);
                        }
                        else if (func_call == "sleep")
                        {
                            float time = any_cast_float(GetIndex(arguments, 0, 0.0f), false);
                            wait(time);
                            return true;
                        }
                        else if (func_call == "sys")
                        {
                            system(printable(GetIndex(arguments, 0, "")).c_str());
                            return non_value;
                        }
                        else if (func_call == "toPrint")
                            return printable(GetIndex(arguments, 0, 0));
                        else if (func_call == "TypeNameOf")
                            return get_type_name(GetIndex(arguments, 0, undefined));
                        else if (func_call == "TypeEq")
                            return get_type_name(GetIndex(arguments, 0, undefined)) == get_type_name(GetIndex(arguments, 1, undefined));
                        else if (func_call == "RandomInt")
                            return randint(any_cast_int(GetIndex(arguments, 0, 1)), any_cast_int(GetIndex(arguments, 1, 10)));
                        else if (func_call == "NewDetachedThread")
                        {
                            basic_function func = any_cast_bfunc(GetIndex(arguments, 0));

                            INTERPRETER new_interpreter;
                            new_interpreter.current_scope = current_scope;
                            new_interpreter.g_variables = g_variables;
                            new_interpreter.g_funcs = g_funcs;
                            new_interpreter.g_basic_classes = g_basic_classes;
                            std::thread thread([this, func]()
                                               { execute(func.code); });

                            thread.detach();
                            return non_value;
                        }
                        else if (func_call == "_DUMP_VAR")
                            return dump_variable_data();

                        any defined_func = evaluate(func_call);
                        if (defined_func.type() == typeid(basic_function))
                        {
                            basic_function func = any_cast_bfunc(defined_func);
                            return exec_bfunc(func, arguments);
                        }
                    }
                }
                else if (library == "Task")
                {
                    if (func_call == "MakeWebRequest")
                    {
                        try
                        {
                            // make web request
                            const xhttp::HttpResult response = xhttp::HttpClient::Get(any_cast_string(GetIndex(arguments, 0, "")), any_cast_string(GetIndex(arguments, 1, "")));
                            
                            // convert the HttpResult to a dictionary
                            unordered_map<string, any> result;

                            for (const auto &[key, value] : response)
                                result[key] = value;

                            return result;
                        }
                        catch(const std::runtime_error& err)
                        {
                            error_any("WEB REQUEST FAILED", string(err.what()));
                        }
                    }
                    else if (func_call == "ParseJSON")
                        return xjson::parse_json(any_cast_string(GetIndex(arguments, 0, "")));
                    else if (func_call == "Kill")
                    {
                        const int exit_code = any_cast_int(GetIndex(arguments, 0, 0));
                        std::exit(exit_code);
                        return exit_code;
                    }
                    else if (func_call == "TimeSinceYear")
                    {
                        // Set up Jan 1, 2025 at 00:00:00
                        std::tm tm_start = {};
                        tm_start.tm_year = any_cast_int(GetIndex(arguments, 0, 1970)) - 1900; // Years since 1900
                        tm_start.tm_mon = 0;                                               // January
                        tm_start.tm_mday = 1;                                              // Day 1

                        // Convert to time_t
                        std::time_t time_start = std::mktime(&tm_start);

                        // Convert to chrono::system_clock::time_point
                        std::chrono::system_clock::time_point jan1_yr =
                            std::chrono::system_clock::from_time_t(time_start);

                        // Get current time
                        std::chrono::system_clock::time_point now = std::chrono::system_clock::now();

                        // Compute duration in seconds as a double
                        const long seconds = std::chrono::duration_cast<std::chrono::duration<long>>(now - jan1_yr).count();

                        return seconds;
                    }
                    else if (func_call == "ClearIO")
                    {
                        clear_console();
                        return non_value;
                    }
                    else if (func_call == "Alert")
                    {
                        cout << "\a";
                        return non_value;
                    }
                }
                else if (library == "Math")
                {
                    const double o1 = any_cast_float(GetIndex(arguments, 0, 0), false);
                    const double o2 = any_cast_float(GetIndex(arguments, 1, 0), false);

                    if (func_call == "Add")
                        return o1 + o2;
                    else if (func_call == "Subtract")
                        return o1 - o2;
                    else if (func_call == "Multiply")
                        return o1 * o2;
                    else if (func_call == "Divide")
                        return o1 / o2;
                }
                else if (library == "LocalStorage")
                {
                    if (func_call == "Save")
                    {
                        save_data(any_cast_string(GetIndex(arguments, 1, "data")), any_cast_string(GetIndex(arguments, 0, "")));
                        return non_value;
                    }
                    else if (func_call == "Load")
                    {
                        const string data = load_data(any_cast_string(GetIndex(arguments, 0, "data")));
                        return data;
                    }
                    else if (func_call == "Delete")
                    {
                        delete_data(any_cast_string(GetIndex(arguments, 0, "data")));
                        return non_value;
                    }
                }
                else if (library == "plat_Windows")
                {
                    if (func_call == "Lock")
                    {
                        system("rundll32.exe user32.dll,LockWorkStation");
                        return non_value;
                    }
                    else if (func_call == "InvokeKeystroke")
                    {
                        force_key_press(any_cast_string(GetIndex(arguments, 0, "A"))[0]);
                        return non_value;
                    }
                }
                else if (library == "FileSystem")
                {
                    const string file_path = any_cast_string(GetIndex(arguments, 0, ""));

                    if (func_call == "GetLocalDir")
                        return LocalDirectory;
                    else if (func_call == "Read")
                    {
                        ifstream in_file(file_path);
                        if (!in_file)
                            return nullptr;

                        std::ostringstream buffer;
                        buffer << in_file.rdbuf();
                        return buffer.str();
                    }
                    else if (func_call == "Write")
                    {
                        std::ofstream out_file(file_path, std::ios::trunc); // trunc = overwrite mode
                        if (!out_file)
                            return nullptr;

                        out_file << any_cast_string(GetIndex(arguments, 0, ""));
                    }
                    else if (func_call == "Append")
                    {
                        std::ofstream out_file(file_path, std::ios::app); // app = append mode
                        if (!out_file)
                            return nullptr;

                        out_file << any_cast_string(GetIndex(arguments, 0, ""));
                    }
                }
                else
                {
                    // error_any("UNKNOWN FUNCTION LIBRARY", library);
                    any temp = evaluate(library);
                    any &object = temp;
                    const string obj_type = get_type_name(object);

                    string obj_name = library;
                    if (find_outside(obj_name, "."))
                        obj_name = split_string(obj_name, '.')[0];

                    if (obj_type == "error")
                    {
                        const silent_error err = any_cast<silent_error>(object);

                        if (func_call == "Message")
                            return string(err.what());
                        else if (func_call == "Value")
                            return err.get_value();
                    }
                    else if (obj_type == "string")
                    {
                        const string str = any_cast_string(object);

                        if (func_call == "Length")
                            return static_cast<long>(str.size());
                        else if (func_call == "Reverse")
                        {
                            string reversed = str;
                            std::reverse(reversed.begin(), reversed.end());
                            return reversed;
                        }
                        else if (func_call == "Capitalize")
                            return to_upper(str.substr(0, 1)) + str.substr(1);
                        else if (func_call == "Split")
                        {
                            const string delimiter = any_cast_string(GetIndex(arguments, 0, ""));
                            const int max_items = any_cast_int(GetIndex(arguments, 1, 100));
                            return to_any_vector(split_string(str, delimiter, max_items));
                        }
                        else if (func_call == "Chars")
                        {
                            vector<string> chars;
                            for (char c : str)
                                chars.push_back(string(1, c));
                            return to_any_vector(chars);
                        }
                        else if (func_call == "CharAt")
                            return string(1, str[any_cast_int(GetIndex(arguments, 0, 0))]);
                        else if (func_call == "Trim")
                            return trim(str);
                        else if (func_call == "toUpper")
                            return to_upper(str);
                        else if (func_call == "isUpper")
                            return str == to_upper(str);
                        else if (func_call == "toLower")
                            return to_lower(str);
                        else if (func_call == "isLower")
                            return str == to_lower(str);
                        else if (func_call == "isEmpty")
                            return str.size() == 0;
                        else if (func_call == "SubChunk")
                            return str.substr(any_cast_int(GetIndex(arguments, 0, 0)), any_cast_int(GetIndex(arguments, 1, 0)));
                    }
                    else if (obj_type == "array")
                    {
                        vector<any> &arr = any_cast<vector<any> &>(object); // not const anymore!

                        if (func_call == "GetFromIndex")
                        {
                            const int idx = any_cast_int(GetIndex(arguments, 0, 1));
                            if (idx < 1 || static_cast<size_t>(idx) > arr.size())
                                return undefined;
                            return arr[idx - 1];
                        }
                        else if (func_call == "Size")
                        {
                            const long arr_size = static_cast<long>(arr.size());
                            return arr_size;
                        }
                        else if (func_call == "Contains")
                        {
                            for (any &item : arr)
                            {
                                any to_match = GetIndex(arguments, 0, 0);
                                vector<any> toks = {item, to_match};
                                vector<string> op = {"=="};
                                apply_operator(toks, op);
                                if (is_truthy(toks.back()))
                                    return true;
                            }
                            return false;
                        }
                        else if (func_call == "PrettyPrint")
                            return printable(arr, true);
                        else if (func_call == "PushBack")
                        {
                            arr.push_back(GetIndex(arguments, 0, 0));
                            return arr; // optional: could return nil or the new array
                        }
                        else if (func_call == "PopBack")
                        {
                            if (!arr.empty())
                                arr.pop_back();
                            return arr; // optional
                        }
                        else if (func_call == "PushFront")
                        {
                            // Push an item to the front of the array
                            any value_to_push = GetIndex(arguments, 0, 0);
                            arr.insert(arr.begin(), value_to_push);
                            return arr; // optional: could return nil or the new array
                        }
                        else if (func_call == "PopFront")
                        {
                            // Pop an item from the front of the array
                            if (!arr.empty())
                            {
                                any front_value = arr.front();
                                arr.erase(arr.begin());
                            }
                            return arr; // Return the array
                        }
                    }
                    else if (obj_type == "dict")
                    {
                        unordered_map<string, any> &dict = any_cast<unordered_map<string, any> &>(object);

                        if (func_call == "GetFromKey")
                        {
                            const string idx = printable(GetIndex(arguments, 0, 0));
                            const auto it_dict = dict.find(idx);
                            if (it_dict == dict.end())
                                return undefined;
                            return it_dict->second;
                        }
                        else if (func_call == "GetKeys")
                        {
                            vector<any> keys;
                            for (const auto &pair : dict)
                                keys.push_back(pair.first);
                            return keys;
                        }
                        else if (func_call == "GetValues")
                        {
                            vector<any> values;
                            for (const auto &pair : dict)
                                values.push_back(pair.second);
                            return values;
                        }
                        else if (func_call == "Size")
                        {
                            const long dict_size = static_cast<long>(dict.size());
                            return dict_size;
                        }
                        else if (func_call == "PrettyPrint")
                            return printable(dict, true);
                        else
                        {
                            basic_function func = any_cast_bfunc(dict[func_call]);
                            return exec_bfunc(func, arguments);
                        }
                    }
                    else
                    {
                        basic_class &my_class = any_cast<basic_class &>(object);
                        unordered_map<string, any> self_dict = any_cast<unordered_map<string, any>>(my_class.self);
                        unordered_map<string, any> here_dict = any_cast<unordered_map<string, any>>(my_class.here);

                        const string reset_is_method = is_running_method;
                        is_running_method = my_class.name;

                        basic_function func = any_cast_bfunc(
                            self_dict.find(func_call) == self_dict.end() && reset_is_method == is_running_method
                                ? here_dict[func_call]
                                : self_dict[func_call]);

                        string func_code = func.code;
                        scope_up();

                        define_variable(class_public_keyword, my_class.self, false);
                        define_variable(class_private_keyword, my_class.here, false);

                        any ret_val = exec_bfunc(func, arguments, false);

                        is_running_method = reset_is_method;

                        my_class.self = get_variable(class_public_keyword);
                        my_class.here = get_variable(class_private_keyword);

                        if (variable_exists(library))
                            set_variable(library, my_class);

                        if (ret_val.type() != typeid(no_return_data_t))
                        {
                            scope_down();
                            return ret_val;
                        }

                        scope_down();
                        return my_class;
                    }

                    return undefined;
                }
                if (library.empty())
                {
                    error_any("UNKNOWN FUNCTION", func_call);
                }
                else
                {
                    error_any("UNKNOWN FUNCTION IN FUNCTION LIBRARY '" + library + "'", func_call);
                }
            }
            else
            {
                if (find_outside(line, ".") || find_outside(line, "["))
                {
                    auto split_result = split_category(line);

                    const string library = split_result.first;
                    const string prop = split_result.second;

                    if (library == "Task")
                    {
                        if (prop == "Title")
                        {
                            return current_console_title;
                        }
                        else if (prop == "Runtime")
                        {
                            const double cur_time = std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();
                            const double elapsed_time = cur_time - init_time;
                            return elapsed_time;
                        }
                        else if (prop == "ErrorCount")
                            return long(interpreter_error_count);
                        else if (prop == "LastErrorMessage")
                            return last_error_message;
                        else if (prop == "LastErrorValue")
                            return last_error_value;
                    }
                    else if (library == "plat_Windows")
                    {
                        if (prop == "uClipboard")
                        {
                            return GetClipboard();
                        }
                    }
                    else if (library == "MACRO")
                    {
                        if (prop == "TOOLS")
                        {
                            return string("\n");
                        }

                        else if (prop == "NO_RETURN_DATA")
                        {
                            return no_return_data;
                        }

                        else if (prop == "TRUE_INT")
                        {
                            return 1;
                        }
                        else if (prop == "NIL_INT")
                        {
                            return 0;
                        }
                        else if (prop == "FALSE_INT")
                        {
                            return -1;
                        }
                        else if (prop == "UNDEF_INT")
                        {
                            return 0;
                        }

                        else if (prop == "TRUE_FLOAT")
                        {
                            return 1.0f;
                        }
                        else if (prop == "NIL_FLOAT")
                        {
                            return 0.0f;
                        }
                        else if (prop == "FALSE_FLOAT")
                        {
                            return -1.0f;
                        }
                        else if (prop == "UNDEF_FLOAT")
                        {
                            return 0.0f;
                        }

                        else if (prop == "EXIT_SUCCESS")
                        {
                            return 0;
                        }
                        else if (prop == "EXIT_FAILURE")
                        {
                            return 1;
                        }
                    }
                    else
                    {
                        any object = evaluate(library); // allow array indexing
                        string obj_type = get_type_name(object);

                        auto it_class = std::find(g_basic_classes.begin(), g_basic_classes.end(), obj_type);
                        if (it_class != g_basic_classes.end())
                        {
                            basic_class my_class = any_cast<basic_class>(object);
                            unordered_map<string, any> self_dict = any_cast<unordered_map<string, any>>(my_class.self);
                            unordered_map<string, any> here_dict = any_cast<unordered_map<string, any>>(my_class.here);

                            object =
                                self_dict.find(prop) == self_dict.end() && my_class.name == is_running_method
                                    ? here_dict
                                    : self_dict;

                            obj_type = get_type_name(object);
                        }

                        if (obj_type == "dict")
                        {
                            unordered_map<string, any> &dict = any_cast<unordered_map<string, any> &>(object);

                            const string idx = prop;
                            const auto it_dict = dict.find(idx);
                            if (it_dict == dict.end())
                                return undefined;
                            return it_dict->second;
                        }
                        else if (obj_type == "array")
                        {
                            vector<any> &arr = any_cast<vector<any> &>(object);

                            const int idx = std::stoi(prop);
                            // array indexing starts at one??????
                            return arr[idx - 1];
                        }
                    }
                    error_any("UNKNOWN VALUE IN VALUE LIBRARY '" + library + "'", prop);
                    return non_value;
                }
            }
        }

        error_any("COULD NOT RESOLVE", line);
        return non_value;
    }

    any exec_bfunc(basic_function &func, vector<any> arguments = vector<any>(), const bool scopes = true)
    {
        if (scopes)
            scope_up();

        vector<string> param_names = func.param_names;
        vector<any> param_defaults = func.param_defaults;
        vector<string> param_types = func.param_types;
        vector<int> param_end_mod = func.param_end_mod;

        int p_i = 0;
        for (string &pname : param_names)
        {
            const string ptype = param_types[p_i];
            const bool pstrict = param_end_mod[p_i] != 1;
            any pdefault = param_defaults[p_i];
            if (ptype.empty())
                define_variable(pname, GetIndex(arguments, p_i, pdefault), false);
            else
            {
                any arg;
                const bool nodf = pdefault.type() == typeid(undefined_t);

                if (ptype == "void")
                {
                    if (nodf)
                        pdefault = nullptr;
                    arg = any_cast_void(GetIndex(arguments, p_i, pdefault), pstrict);
                }
                else if (ptype == "bool")
                {
                    if (nodf)
                        pdefault = false;
                    arg = any_cast_bool(GetIndex(arguments, p_i, pdefault), pstrict);
                }
                else if (ptype == "string")
                {
                    if (nodf)
                        pdefault = string("");
                    arg = any_cast_string(GetIndex(arguments, p_i, pdefault), pstrict);
                }
                else if (ptype == "int")
                {
                    if (nodf)
                        pdefault = 0;
                    arg = any_cast_int(GetIndex(arguments, p_i, pdefault), pstrict);
                }
                else if (ptype == "float")
                {
                    if (nodf)
                        pdefault = 0.0f;
                    arg = any_cast_float(GetIndex(arguments, p_i, pdefault), pstrict);
                }
                else if (ptype == "array")
                {
                    if (nodf)
                        pdefault = vector<any>();
                    arg = any_cast<vector<any>>(GetIndex(arguments, p_i, pdefault));
                }
                else if (ptype == "dict")
                {
                    if (nodf)
                        pdefault = unordered_map<string, any>();
                    arg = any_cast<unordered_map<string, any>>(GetIndex(arguments, p_i, pdefault));
                }
                else if (ptype == "fn")
                {
                    basic_function func(*this, "_UNNAMED", "", vector<string>(), vector<any>(), vector<string>(), vector<int>());
                    if (nodf)
                        pdefault = func;
                    arg = any_cast_bfunc(GetIndex(arguments, p_i, pdefault), pstrict);
                }
                else if (std::find(g_basic_classes.begin(), g_basic_classes.end(), ptype) != g_basic_classes.end())
                {
                    if (nodf)
                        pdefault = undefined;
                    const string IncorrectTypeError = "EXPECTED '" + ptype + "' TYPE";

                    any rarg = GetIndex(arguments, p_i, pdefault);

                    try
                    {
                        arg = any_cast<basic_class>(rarg);
                    }
                    catch (const std::bad_any_cast &e)
                    {
                        error_any(IncorrectTypeError, rarg);
                    }

                    if (any_cast<basic_class>(arg).name != ptype)
                        error_any(IncorrectTypeError, arg);
                }
                else
                    error_any("UNKNOWN TYPE PARAMETER", ptype);

                define_variable(pname, arg, false);
            }

            p_i += 1;
        }

        auto maybe_ret = execute(any_cast_string(func.code), false);
        if (scopes)
            scope_down();
        if (maybe_ret.has_value())
            return maybe_ret->value;
        return no_return_data;
    }
};