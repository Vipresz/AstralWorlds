#include "character/SkillCatalog.h"

#include "AppPaths.h"
#include "Logger.h"
#include "character/JsonUtil.h"
#include "character/SkillDefinition.h"

#include <cmath>

using EngineLogging::Logger;

namespace Game {
namespace Character {

namespace {

std::string JoinPath(const std::string& root, const std::string& file) {
    if (root.empty()) {
        return file;
    }
    if (root.back() == '/' || root.back() == '\\') {
        return root + file;
    }
    return root + "/" + file;
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

    size_t cursor = colonPos + 1;
    while (cursor < json.size() && std::isspace(static_cast<unsigned char>(json[cursor]))) {
        ++cursor;
    }
    if (cursor >= json.size()) {
        return false;
    }

    char* end = nullptr;
    const float value = std::strtof(json.c_str() + cursor, &end);
    if (end == json.c_str() + cursor) {
        return false;
    }
    out = value;
    return std::isfinite(value);
}

bool JsonTryGetBool(const std::string& json, const char* key, bool& out) {
    const std::string needle = std::string("\"") + key + "\"";
    const size_t keyPos = json.find(needle);
    if (keyPos == std::string::npos) {
        return false;
    }

    const size_t colonPos = json.find(':', keyPos + needle.size());
    if (colonPos == std::string::npos) {
        return false;
    }

    const std::string tail = json.substr(colonPos + 1, 12);
    if (tail.find("true") != std::string::npos) {
        out = true;
        return true;
    }
    if (tail.find("false") != std::string::npos) {
        out = false;
        return true;
    }
    return false;
}

std::vector<std::string> SplitTopLevelObjects(const std::string& arrayJson) {
    std::vector<std::string> objects;
    if (arrayJson.size() < 2) {
        return objects;
    }

    int depth = 0;
    size_t objectStart = std::string::npos;
    for (size_t i = 0; i < arrayJson.size(); ++i) {
        const char c = arrayJson[i];
        if (c == '{') {
            if (depth == 0) {
                objectStart = i;
            }
            ++depth;
        } else if (c == '}') {
            --depth;
            if (depth == 0 && objectStart != std::string::npos) {
                objects.push_back(arrayJson.substr(objectStart, i - objectStart + 1));
                objectStart = std::string::npos;
            }
        }
    }
    return objects;
}

} // namespace

bool SkillCatalog::LoadManifest(const std::string& manifestPath, SkillDefinition& out) {
    const std::string json = ReadTextFile(manifestPath.c_str());
    if (json.empty()) {
        LOG_WARNING_FMT("SkillCatalog", "Skill manifest missing or empty: %s", manifestPath.c_str());
        return false;
    }

    out = SkillDefinition{};
    JsonTryGetString(json, "id", out.id);
    JsonTryGetString(json, "name", out.name);
    JsonTryGetString(json, "animation", out.animationPath);
    JsonTryGetString(json, "icon_unpressed", out.iconUnpressed);
    JsonTryGetString(json, "icon_pressed", out.iconPressed);
    JsonTryGetString(json, "hotkey", out.hotkey);

    float duration = 1.0f;
    if (JsonTryGetFloat(json, "duration_seconds", duration) && duration > 0.01f) {
        out.durationSeconds = duration;
    }

    float cooldown = out.durationSeconds;
    if (JsonTryGetFloat(json, "cooldown_seconds", cooldown) && cooldown > 0.01f) {
        out.cooldownSeconds = cooldown;
    } else {
        out.cooldownSeconds = out.durationSeconds;
    }

    JsonTryGetBool(json, "full_body", out.fullBody);
    JsonTryGetFloat(json, "damage", out.damage);

    float damageDelay = 0.4f;
    if (JsonTryGetFloat(json, "damage_delay_seconds", damageDelay) && damageDelay >= 0.0f) {
        out.damageDelaySeconds = damageDelay;
    }

    std::string target;
    if (JsonTryGetString(json, "target", target)) {
        out.target = SkillTargetTypeFromString(target);
    }

    const std::string areaObject = JsonExtractObject(json, "area");
    if (!areaObject.empty()) {
        std::string shape;
        if (JsonTryGetString(areaObject, "shape", shape)) {
            out.area.shape = SkillAreaShapeFromString(shape);
        }
        JsonTryGetFloat(areaObject, "range", out.area.range);
        JsonTryGetFloat(areaObject, "angle_degrees", out.area.angleDegrees);
        JsonTryGetFloat(areaObject, "radius", out.area.radius);
        if (out.area.radius <= 0.01f) {
            JsonTryGetFloat(areaObject, "width", out.area.radius);
        }
    }

    if (out.id.empty() || out.animationPath.empty()) {
        LOG_WARNING_FMT("SkillCatalog", "Skill manifest incomplete: %s", manifestPath.c_str());
        return false;
    }

    return true;
}

bool SkillCatalog::Load(const char* catalogPath) {
    m_skills.clear();
    m_byId.clear();

    const std::string resolvedCatalog = Game::ResolveDataPath(catalogPath);
    const std::string catalogJson = ReadTextFile(resolvedCatalog.c_str());
    if (catalogJson.empty()) {
        LOG_WARNING_FMT("SkillCatalog", "Skill catalog missing: %s", resolvedCatalog.c_str());
        return false;
    }

    const std::string skillsArray = JsonExtractArray(catalogJson, "skills");
    const std::vector<std::string> entries = SplitTopLevelObjects(skillsArray);
    if (entries.empty()) {
        LOG_WARNING("SkillCatalog", "Skill catalog has no entries");
        return false;
    }

    const size_t slashPos = resolvedCatalog.find_last_of("/\\");
    const std::string catalogDir =
        slashPos == std::string::npos ? std::string() : resolvedCatalog.substr(0, slashPos + 1);

    for (const std::string& entryJson : entries) {
        std::string manifestRel;
        if (!JsonTryGetString(entryJson, "manifest", manifestRel)) {
            continue;
        }

        SkillDefinition skill;
        const std::string manifestPath = JoinPath(catalogDir, manifestRel);
        if (!LoadManifest(manifestPath, skill)) {
            continue;
        }

        m_byId[skill.id] = m_skills.size();
        m_skills.push_back(std::move(skill));
    }

    LOG_INFO_FMT("SkillCatalog", "Loaded %zu skill definition(s) from %s",
                 m_skills.size(),
                 resolvedCatalog.c_str());
    return !m_skills.empty();
}

const SkillDefinition* SkillCatalog::Find(const std::string& id) const {
    const auto it = m_byId.find(id);
    if (it == m_byId.end()) {
        return nullptr;
    }
    return &m_skills[it->second];
}

const SkillDefinition* SkillCatalog::FindByHotkey(const std::string& hotkey) const {
    if (hotkey.empty()) {
        return nullptr;
    }
    for (const SkillDefinition& skill : m_skills) {
        if (skill.hotkey == hotkey) {
            return &skill;
        }
    }
    return nullptr;
}

} // namespace Character
} // namespace Game
