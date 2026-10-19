#include "ManifestTradeConfig.h"

#include "Logger.h"

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>

using EngineLogging::Logger;

namespace Game {
namespace UI {

namespace {

std::string ReadTextFile(const char* path) {
    std::ifstream file(path);
    if (!file) {
        return {};
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

const char* SkipJsonWhitespace(const char* cursor) {
    while (cursor && *cursor && std::isspace(static_cast<unsigned char>(*cursor))) {
        ++cursor;
    }
    return cursor;
}

bool JsonTryGetFloat(const std::string& json, const char* key, float& out) {
    const std::string needle = std::string("\"") + key + "\"";
    const size_t keyPos = json.find(needle);
    if (keyPos == std::string::npos) {
        return false;
    }

    const size_t colonPos = json.find(':', keyPos + needle.size());
    if (colonPos == std::string::npos) {
        return false;
    }

    const char* start = SkipJsonWhitespace(json.c_str() + colonPos + 1);
    char* end = nullptr;
    const float value = std::strtof(start, &end);
    if (end == start) {
        return false;
    }

    out = value;
    return true;
}

std::string JsonExtractObject(const std::string& json, const char* objectKey) {
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

} // namespace

bool LoadManifestTradeConfig(ManifestTradeConfig& out, const char* manifestPath) {
    const char* path = manifestPath ? manifestPath : kManifestTradeConfigPath;
    const std::string json = ReadTextFile(path);
    if (json.empty()) {
        LOG_WARNING_FMT("ManifestTradeConfig", "Manifest not found at %s — using defaults", path);
        return false;
    }

    const std::string tradeJson = JsonExtractObject(json, "trade");
    if (tradeJson.empty()) {
        return false;
    }

    float maxDistance = out.maxDistance;
    if (JsonTryGetFloat(tradeJson, "max_distance", maxDistance)) {
        out.maxDistance = std::max(0.0f, maxDistance);
    }
    return true;
}

} // namespace UI
} // namespace Game
