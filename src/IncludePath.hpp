
#include "Common.hpp"


namespace IncludePath
{
    std::filesystem::path DirPath;

    std::filesystem::path GetPersistentPath(const std::string &FolderName = ".Furn_IncludePath")
    {
        const char *home = std::getenv("HOME");

        // On Windows, HOME is not always set, fallback to USERPROFILE
        if (!home)
            home = std::getenv("USERPROFILE");

        // Fallback to current directory if nothing is set
        std::filesystem::path base = home ? std::filesystem::path(home) : std::filesystem::current_path();

        return base / FolderName;
    }

    void Init(const std::string &FolderName = ".Furn_IncludePath")
    {
        DirPath = GetPersistentPath(FolderName);
        std::filesystem::create_directories(DirPath);
    }

    bool Write(const std::string &FileName, const std::string &contents)
    {
        if (DirPath.empty())
            Init();

        std::ofstream Out(DirPath / FileName, std::ios::out | std::ios::trunc);
        if (!Out.is_open())
            return false;

        Out << contents;
        return true;
    }

    std::filesystem::path Path()
    {
        if (DirPath.empty())
            Init();
        return DirPath;
    }
}
