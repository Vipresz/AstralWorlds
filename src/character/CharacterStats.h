#pragma once

#include "character/JsonUtil.h"
#include "Logger.h"

#include <cstdlib>
#include <string>

namespace Game {
namespace Character {

struct CharacterStats {
    static constexpr const char* kManifestPath = "character/character_stats.json";

    // Defaults = previous 1.4 walk / 3.5 run, then walk x2 and run x1.5.
    float walkSpeed = 2.8f;
    float runSpeed = 5.25f;
};

inline bool JsonTryGetFloat(const std::string& json, const char* key, float& out) {
    const std::string needle = std::string("\"") + key + "\"";
    const size_t keyPos = json.find(needle);
    if (keyPos == std::string::npos) {
        return false;
    }

    const size_t colonPos = json.find(':', keyPos + needle.size());
    if (colonPos == std::string::npos) {
        return false;
    }

    const char* cursor = SkipJsonWhitespace(json.c_str() + colonPos + 1);
    if (!cursor || !*cursor) {
        return false;
    }

    char* end = nullptr;
    const float value = std::strtof(cursor, &end);
    if (end == cursor) {
        return false;
    }

    out = value;
    return true;
}

inline bool LoadCharacterStats(CharacterStats& out, const char* path = CharacterStats::kManifestPath) {
    const char* manifestPath = path ? path : CharacterStats::kManifestPath;
    const std::string json = ReadTextFile(manifestPath);
    if (json.empty()) {
        LOG_WARNING_FMT("CharacterStats",
                        "Stats manifest not found at %s — using defaults (walk=%.2f run=%.2f)",
                        manifestPath,
                        out.walkSpeed,
                        out.runSpeed);
        return false;
    }

    const std::string locomotionJson = JsonExtractObject(json, "locomotion");
    const std::string& source = locomotionJson.empty() ? json : locomotionJson;
    JsonTryGetFloat(source, "walk_speed", out.walkSpeed);
    JsonTryGetFloat(source, "run_speed", out.runSpeed);

    if (out.walkSpeed < 0.01f) {
        out.walkSpeed = 0.01f;
    }
    if (out.runSpeed < out.walkSpeed) {
        out.runSpeed = out.walkSpeed;
    }

    LOG_INFO_FMT("CharacterStats",
                 "Loaded %s (walk=%.2f run=%.2f)",
                 manifestPath,
                 out.walkSpeed,
                 out.runSpeed);
    return true;
}

} // namespace Character
} // namespace Game
