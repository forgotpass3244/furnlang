// g++ -std=c++17 -o D:/FurnScript/FurnScript/src/furnscript.exe D:/FurnScript/FurnScript/src/main.cpp -lws2_32
// g++ -std=c++17 -o newEXE fileCPP

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>

// including headers
#include "common.hpp"
#include "interpreter.hpp"
#include "utilities.hpp"

namespace fs = std::filesystem;

// ------------ main ------------
// ---------- function ----------
int main(int argc, char *argv[])
{
    save_data("", ".init", "txt");

    ConsoleTitle("Console");

    string MainScript;
    string LocalFile;

    if (argc >= 2)
    {
        std::tie(MainScript, LocalFile) = get_file(argv[1]);
    }
    else
    {
        std::tie(MainScript, LocalFile) = get_file();
    }

    if (!LocalFile.empty())
    {
        LocalDirectory = fs::path(LocalFile).parent_path().string();
    }
    else
    {
        LocalDirectory = "";
    }

    ConsoleTitle(LocalFile);

    // setting this to false significantly improves the speed of cout but breaks the output kinda
    std::ios::sync_with_stdio(true);
    cin.tie(nullptr);

    INTERPRETER interpreter;
    interpreter.current_executing_file = LocalFile;

    try
    {
        interpreter.execute(MainScript, true);
    }
    catch (const std::exception &e)
    {
        if (interpreter.currently_tracing_back)
            std::cerr << "\nTraceback could not be completed\n";
        else
            std::cerr << "\nAn unknown error has occurred. (terminating process)\nwhat:\n" + string(e.what());
    }

    if (interpreter.currently_tracing_back)
        std::cerr << "\nTraceback End\n";

    if (is_integrated_term)
        cout << "\nProcess finished with an error count of '"
             << interpreter.interpreter_error_count << "'\n";

    return interpreter.interpreter_error_count;
}