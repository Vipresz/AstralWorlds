#include "AppPaths.h"
#include <string>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace Game {

namespace {

std::string GetExecutableDirectoryImpl() {
#ifdef _WIN32
    char buffer[MAX_PATH];
    const DWORD length = GetModuleFileNameA(nullptr, buffer, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) {
        return {};
    }

    std::string exePath(buffer, length);
    const size_t sep = exePath.find_last_of("\\/");
    if (sep == std::string::npos) {
        return {};
    }

    return exePath.substr(0, sep);
#else
    return {};
#endif
}

} // namespace

bool SetWorkingDirectoryToExecutable() {
#ifdef _WIN32
    const std::string exeDir = GetExecutableDirectoryImpl();
    if (exeDir.empty()) {
        return false;
    }
    return SetCurrentDirectoryA(exeDir.c_str()) != 0;
#else
    return true;
#endif
}

std::string GetExecutableDirectory() {
    return GetExecutableDirectoryImpl();
}

std::string ResolveDataPath(const char* relativePath) {
    if (!relativePath || relativePath[0] == '\0') {
        return {};
    }

#ifdef _WIN32
    const char first = relativePath[0];
    if (((first >= 'A' && first <= 'Z') || (first >= 'a' && first <= 'z')) &&
        relativePath[1] == ':') {
        return relativePath;
    }
    if (relativePath[0] == '\\' && relativePath[1] == '\\') {
        return relativePath;
    }
#endif
    if (relativePath[0] == '/') {
        return relativePath;
    }

    const std::string exeDir = GetExecutableDirectoryImpl();
    if (exeDir.empty()) {
        return relativePath;
    }

    if (exeDir.back() == '/' || exeDir.back() == '\\') {
        return exeDir + relativePath;
    }
    return exeDir + "/" + relativePath;
}

} // namespace Game
