#pragma once

#include <cctype>
#include <fstream>
#include <sstream>
#include <string>

namespace Game {
namespace Character {

inline std::string ReadTextFile(const char* path) {
    std::ifstream file(path);
    if (!file) {
        return {};
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

inline const char* SkipJsonWhitespace(const char* cursor) {
    while (cursor && *cursor && std::isspace(static_cast<unsigned char>(*cursor))) {
        ++cursor;
    }
    return cursor;
}

inline bool JsonTryGetString(const std::string& json, const char* key, std::string& out) {
    const std::string needle = std::string("\"") + key + "\"";
    const size_t keyPos = json.find(needle);
    if (keyPos == std::string::npos) {
        return false;
    }

    const size_t colonPos = json.find(':', keyPos + needle.size());
    if (colonPos == std::string::npos) {
        return false;
    }

    const size_t quoteStart = json.find('"', colonPos + 1);
    if (quoteStart == std::string::npos) {
        return false;
    }

    const size_t quoteEnd = json.find('"', quoteStart + 1);
    if (quoteEnd == std::string::npos || quoteEnd <= quoteStart + 1) {
        return false;
    }

    out.assign(json, quoteStart + 1, quoteEnd - quoteStart - 1);
    return true;
}

inline std::string JsonExtractObject(const std::string& json, const char* objectKey) {
    const std::string needle = std::string("\"") + objectKey + "\"";
    const size_t keyPos = json.find(needle);
    if (keyPos == std::string::npos) {
        return {};
    }

    const size_t braceStart = json.find('{', keyPos);
    if (braceStart == std::string::npos) {
        return {};
    }

    int depth = 0;
    for (size_t i = braceStart; i < json.size(); ++i) {
        if (json[i] == '{') {
            ++depth;
        } else if (json[i] == '}') {
            --depth;
            if (depth == 0) {
                return json.substr(braceStart, i - braceStart + 1);
            }
        }
    }

    return {};
}

inline std::string JsonExtractArray(const std::string& json, const char* arrayKey) {
    const std::string needle = std::string("\"") + arrayKey + "\"";
    const size_t keyPos = json.find(needle);
    if (keyPos == std::string::npos) {
        return {};
    }

    const size_t bracketStart = json.find('[', keyPos);
    if (bracketStart == std::string::npos) {
        return {};
    }

    int depth = 0;
    for (size_t i = bracketStart; i < json.size(); ++i) {
        if (json[i] == '[') {
            ++depth;
        } else if (json[i] == ']') {
            --depth;
            if (depth == 0) {
                return json.substr(bracketStart, i - bracketStart + 1);
            }
        }
    }

    return {};
}

} // namespace Character
} // namespace Game
