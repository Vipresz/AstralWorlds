#pragma once

#include "AppPaths.h"

#include <fstream>
#include <string>

namespace Game {
namespace Tests {

inline bool SetWorkingDirectoryToTestRuntime() {
    return SetWorkingDirectoryToExecutable();
}

inline bool FileExists(const std::string& path) {
    std::ifstream file(path);
    return file.good();
}

inline std::string RequireAsset(const char* relativePath) {
    if (FileExists(relativePath)) {
        return relativePath;
    }
#ifdef GAME_SOURCE_DIR
    const std::string fromSource = std::string(GAME_SOURCE_DIR) + "/" + relativePath;
    if (FileExists(fromSource)) {
        return fromSource;
    }
#endif
    return relativePath;
}

} // namespace Tests
} // namespace Game
