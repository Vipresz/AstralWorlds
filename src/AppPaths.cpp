#include "AppPaths.h"
#include <string>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace Game {

bool SetWorkingDirectoryToExecutable() {
#ifdef _WIN32
    char buffer[MAX_PATH];
    const DWORD length = GetModuleFileNameA(nullptr, buffer, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) {
        return false;
    }

    std::string exePath(buffer, length);
    const size_t sep = exePath.find_last_of("\\/");
    if (sep == std::string::npos) {
        return false;
    }

    return SetCurrentDirectoryA(exePath.substr(0, sep).c_str()) != 0;
#else
    return true;
#endif
}

} // namespace Game
