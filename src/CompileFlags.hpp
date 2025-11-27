
#pragma once

namespace CmplFlags
{
    bool ParseInfo = false;
    bool CompileInfo = false;
    bool StrictMode = false;
    std::filesystem::path OutputFlag;
    bool RunAfterComp = false;
    bool QuietComp = false;
    bool LinkWithGcc = false;
    bool BoundsChecking = true;
    int CursorPosition = 0;
    bool GarbageCollect = true;
}
