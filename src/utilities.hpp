#ifndef UTILITIES_H
#define UTILITIES_H

#include "common.hpp"

// custom types
class undefined_t
{
};
const undefined_t undefined = undefined_t{};

class no_return_data_t
{
};
const no_return_data_t no_return_data = no_return_data_t{};

double init_time = std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();

string current_console_title = "";
const string DIR_FurnScript_ConfigAndSettings = "furnscript";
const string LocalStorageDirName = "local_storage";
string LocalDirectory;

template <typename T>
std::string get_memory_addr(const T &var)
{
    std::stringstream ss;
    ss << std::hex << std::uppercase << std::setfill('0') << std::setw(2 * sizeof(void *)) << reinterpret_cast<const void *>(&var);
    return ss.str();
}

const char *terminal = std::getenv("TERM_PROGRAM");
const bool is_integrated_term = terminal != nullptr && std::strcmp(terminal, "vscode") == 0;

void clear_console()
{
#if defined _WIN32
    system("cls");
#elif defined(__LINUX__) || defined(__gnu_linux__) || defined(__linux__)
    system("clear");
#elif defined(__APPLE__)
    system("clear");
#endif

    if (is_integrated_term)
    {
        // only for vscode
        cout << u8"\033[2J\033[1;1H";
        cout.flush();
    }
}

string trim(const string &str)
{
    const char *whitespace = " \t\n\r\f\v";
    size_t start = str.find_first_not_of(whitespace);
    if (start == string::npos)
        return "";

    size_t end = str.find_last_not_of(whitespace);
    return str.substr(start, end - start + 1);
}

bool string_starts_with(const string &str, const string &prefix)
{
    return str.size() >= prefix.size() && str.compare(0, prefix.size(), prefix) == 0;
}

std::ostream &operator<<(std::ostream &os, const std::variant<string, bool, int, float> &var)
{
    if (std::holds_alternative<string>(var))
    {
        os << std::get<string>(var);
    }
    else if (std::holds_alternative<bool>(var))
    {
        os << (std::get<bool>(var) ? "true" : "false");
    }
    else if (std::holds_alternative<int>(var))
    {
        os << std::get<int>(var);
    }
    else if (std::holds_alternative<float>(var))
    {
        os << std::get<float>(var);
    }
    else
    {
        throw std::runtime_error("Unsupported type in variant");
    }
    return os;
}

bool string_ends_with(string str, string ending)
{
    int len = ending.size();
    if (str.size() >= len && str.substr(str.size() - len) == ending)
    {
        return true;
    }
    return false;
}

string to_upper(const string &input)
{
    string result = input;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c)
                   { return std::toupper(c); });
    return result;
}

string to_lower(const string &input)
{
    string result = input;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c)
                   { return std::tolower(c); });
    return result;
}

vector<string> split_string(const string &str, char delimiter, size_t desired_outcome = 100)
{
    vector<string> tokens;
    std::stringstream ss(str);
    string token;

    if (str.find(delimiter) == string::npos)
    {
        tokens.push_back(str);
        for (size_t i = 1; i < desired_outcome; ++i)
            tokens.push_back("");
        return tokens;
    }

    // Split into at most desired_outcome tokens:
    //  - we only do desired_outcome-1 delimiter splits, then collect the rest as the final token
    while (tokens.size() + 1 < desired_outcome && std::getline(ss, token, delimiter))
    {
        tokens.push_back(token);
    }

    // Whatever remains (including delimiters) goes into the last token
    string rest;
    std::getline(ss, rest);
    if (!rest.empty() || tokens.empty()) // push even empty if we had no splits
        tokens.push_back(rest);

    return tokens;
}

// Overload: splits by a string delimiter (allows string literals like "++")
vector<string> split_string(const string &str, const string &delimiter, size_t max_splits = 100)
{
    vector<string> tokens;
    size_t start = 0;
    size_t splits = 0;
    size_t delim_len = delimiter.length();

    while (splits < max_splits)
    {
        size_t pos = str.find(delimiter, start);
        if (pos == string::npos)
            break;
        tokens.push_back(str.substr(start, pos - start));
        start = pos + delim_len;
        splits++;
    }

    // Append the remainder
    tokens.push_back(str.substr(start));
    return tokens;
}

// Optional: overload for C-string input
vector<string> split_string(const char *cstr, char delimiter, size_t max_splits = 100)
{
    return split_string(string(cstr), delimiter, max_splits);
}

vector<string> split_string(const char *cstr, const char *delim, size_t max_splits = 100)
{
    return split_string(string(cstr), string(delim), max_splits);
}

bool find_outside_quotes(const string &data, const string &to_find)
{
    bool inside_quotes = false;
    size_t n = data.length();

    for (size_t i = 0; i < n; ++i)
    {
        char current_char = data[i];

        if (current_char == '"')
        {
            inside_quotes = !inside_quotes;
        }

        if (!inside_quotes && data.substr(i, to_find.length()) == to_find)
        {
            return true;
        }
    }

    return false;
}

bool find_outside(const string &data, const string &to_find)
{
    bool inside_quotes = false;
    int paren_depth = 0;
    int brace_depth = 0;
    size_t n = data.length();

    for (size_t i = 0; i < n; ++i)
    {
        char current_char = data[i];

        // Toggle quote state (ignore escaped quotes if needed)
        if (current_char == '"' && (i == 0 || data[i - 1] != '\\'))
        {
            inside_quotes = !inside_quotes;
        }

        // Only update parens/braces if not in quotes
        if (!inside_quotes)
        {
            if (current_char == '(')
                ++paren_depth;
            else if (current_char == ')')
                --paren_depth;
            else if (current_char == '{')
                ++brace_depth;
            else if (current_char == '}')
                --brace_depth;
        }

        // If not in quote, and not inside any parens/braces, check for match
        if (!inside_quotes && paren_depth == 0 && brace_depth == 0)
        {
            if (data.substr(i, to_find.length()) == to_find)
            {
                return true;
            }
        }
    }

    return false;
}

namespace fs = std::filesystem;

string get_home_directory()
{
    char *home = getenv("USERPROFILE"); // Windows
    return home ? string(home) : "";
}

string get_storage_path(const string &filename, const string &format, const string StorageDir)
{
    string home = get_home_directory();
    fs::path folder = fs::path(home) / (DIR_FurnScript_ConfigAndSettings + "/" + StorageDir);
    if (!fs::exists(folder))
    {
        fs::create_directories(folder);
    }

    // We will only handle "txt" files now
    string ext = (format == "txt") ? ".txt" : "." + format;
    return (folder / (filename + ext)).string();
}

void save_data(const string &data, const string &filename = "data", const string &format = "txt", const string StorageDir = LocalStorageDirName)
{
    string file_path = get_storage_path(filename, format, StorageDir);

    std::ofstream file(file_path);
    if (file.is_open())
    {
        file << data;
        file.close();
    }
    else
    {
        // For now just do nothing
    }
}

string load_data(const string &filename = "data", const string &format = "txt")
{
    string file_path = get_storage_path(filename, format, LocalStorageDirName);

    std::ifstream file(file_path);
    if (file.is_open())
    {
        string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        return content;
    }

    // Return empty string if file is not found
    return nullptr;
}

void delete_data(const string &filename = "data", const string &format = "txt")
{
    string file_path = get_storage_path(filename, format, LocalStorageDirName);
    if (fs::exists(file_path))
    {
        fs::remove(file_path);
    }
    else
    {
        // nothing right now
    }
}

void ConsoleTitle(const string &title)
{
    current_console_title = title;
    SetConsoleTitle(current_console_title.c_str());
}

void force_key_press(WORD key)
{
    // Prepare the input structure
    INPUT input = {0};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = key; // Virtual-key code

    // Press the key
    SendInput(1, &input, sizeof(INPUT));

    // Release the key
    input.ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(1, &input, sizeof(INPUT));
}

int randint(int min, int max)
{
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrib(min, max);
    return distrib(gen);
}

void wait(float time)
{
    std::this_thread::sleep_for(std::chrono::duration<float>(time));
}

void print(const string &text, bool newline = true)
{
    newline ? cout << text << endl : cout << text;
}

// ---------- File Handling ----------
string DefaultScript = "";

std::pair<string, string> get_file(const string &fp = "")
{
    string content = "";
    string file_path = fp;

    if (file_path.empty())
    {
        cout << "No file attached" << endl;
        cout << "Run path: ";
        std::getline(cin, file_path);

        // Clean up user input
        file_path.erase(std::remove(file_path.begin(), file_path.end(), '"'), file_path.end());
        file_path.erase(std::remove(file_path.begin(), file_path.end(), '\''), file_path.end());
        file_path.erase(std::remove(file_path.begin(), file_path.end(), '&'), file_path.end());

        file_path = fs::path(file_path).string();
    }

    fs::path search_path = fs::path(file_path);
    string base_name = search_path.stem().string(); // e.g., "myscript"
    fs::path directory = search_path.parent_path();
    if (directory.empty())
        directory = fs::current_path();

    // Try to find the first file starting with base_name
    for (const auto &entry : fs::directory_iterator(directory))
    {
        if (entry.is_regular_file())
        {
            string fname = entry.path().filename().string();
            if (fname.rfind(base_name, 0) == 0) // starts with base_name
            {
                file_path = entry.path().string();
                break;
            }
        }
    }

    ifstream file(file_path);
    if (!file.is_open())
    {
        cout << "File not found matching: " << base_name << endl;
        wait(1.0f);
        std::exit(1);
    }

    content.assign((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();

    return {content, file_path};
}

// Set text to clipboard
void SetClipboard(const string &text)
{
    if (OpenClipboard(nullptr))
    {
        EmptyClipboard();

        HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE, text.size() + 1);
        if (hGlobal)
        {
            char *pGlobal = static_cast<char *>(GlobalLock(hGlobal));
            if (pGlobal)
            {
                memcpy(pGlobal, text.c_str(), text.size() + 1);
                GlobalUnlock(hGlobal);
                SetClipboardData(CF_TEXT, hGlobal);
            }
        }

        CloseClipboard();
    }
}

// Get text from clipboard
string GetClipboard()
{
    string result = "";

    if (OpenClipboard(nullptr))
    {
        HANDLE hData = GetClipboardData(CF_TEXT);
        if (hData)
        {
            char *pszText = static_cast<char *>(GlobalLock(hData));
            if (pszText)
            {
                result = pszText;
                GlobalUnlock(hData);
            }
        }
        CloseClipboard();
    }

    return result;
}

bool compare_unordered_map_string_any(const any &a, const any &b)
{
    if (a.type() != b.type())
        return false;

    if (a.type() == typeid(int))
        return any_cast<int>(a) == any_cast<int>(b);
    if (a.type() == typeid(double))
        return any_cast<double>(a) == any_cast<double>(b);
    if (a.type() == typeid(string))
        return any_cast<string>(a) == any_cast<string>(b);
    if (a.type() == typeid(bool))
        return any_cast<bool>(a) == any_cast<bool>(b);

    if (a.type() == typeid(unordered_map<string, any>))
    {
        const auto &map_a = any_cast<const unordered_map<string, any> &>(a);
        const auto &map_b = any_cast<const unordered_map<string, any> &>(b);

        if (map_a.size() != map_b.size())
            return false;

        for (const auto &[key, val_a] : map_a)
        {
            auto it = map_b.find(key);
            if (it == map_b.end())
                return false;
            if (!compare_unordered_map_string_any(val_a, it->second))
                return false;
        }

        return true;
    }

    // fallback: unsupported type
    return false;
}

#endif // UTILITIES_H