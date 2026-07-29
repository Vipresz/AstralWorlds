#pragma once

#include <string>

namespace Game {

// Returns false if the executable directory could not be resolved.
bool SetWorkingDirectoryToExecutable();

// Absolute directory containing the running executable (no trailing slash).
std::string GetExecutableDirectory();

// Resolves a runtime data path relative to the executable directory.
std::string ResolveDataPath(const char* relativePath);

} // namespace Game
