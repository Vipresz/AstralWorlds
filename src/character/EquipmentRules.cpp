#include "character/EquipmentRules.h"

#include "Logger.h"
#include "character/JsonUtil.h"

#include <cstdlib>

using EngineLogging::Logger;

namespace Game {
namespace Character {

namespace {

bool JsonTryGetVec3(const std::string& json, const char* key, glm::vec3& out) {
    const std::string needle = std::string("\"") + key + "\"";
    const size_t keyPos = json.find(needle);
    if (keyPos == std::string::npos) {
        return false;
    }

    const size_t bracketStart = json.find('[', keyPos);
    const size_t bracketEnd = json.find(']', bracketStart);
    if (bracketStart == std::string::npos || bracketEnd == std::string::npos) {
        return false;
    }

    const std::string values = json.substr(bracketStart + 1, bracketEnd - bracketStart - 1);
    const size_t commaA = values.find(',');
    const size_t commaB = values.find(',', commaA == std::string::npos ? 0 : commaA + 1);
    if (commaA == std::string::npos || commaB == std::string::npos) {
        return false;
    }

    out.x = std::strtof(values.c_str(), nullptr);
    out.y = std::strtof(values.c_str() + commaA + 1, nullptr);
    out.z = std::strtof(values.c_str() + commaB + 1, nullptr);
    return true;
}

bool ParseRuleObject(const std::string& slotName, const std::string& objectJson, AttachmentRule& out) {
    JsonTryGetVec3(objectJson, "offset", out.localOffset);
    JsonTryGetVec3(objectJson, "rotation", out.localRotationDegrees);
    JsonTryGetVec3(objectJson, "scale", out.localScale);
    return EquipSlotFromString(slotName) != EquipSlot::Unknown;
}

} // namespace

void EquipmentRules::SetDefaults() {
    m_rules.clear();

    m_rules[EquipSlot::Head] = AttachmentRule{{0.0f, 1.65f, 0.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}};
    m_rules[EquipSlot::Chest] = AttachmentRule{{0.0f, 1.05f, 0.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}};
    m_rules[EquipSlot::Legs] = AttachmentRule{{0.0f, 0.55f, 0.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}};
    m_rules[EquipSlot::Feet] = AttachmentRule{{0.0f, 0.05f, 0.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}};
    m_rules[EquipSlot::Hands] = AttachmentRule{{0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}};
    m_rules[EquipSlot::MainHand] = AttachmentRule{{0.35f, 1.0f, 0.15f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}};
    m_rules[EquipSlot::OffHand] = AttachmentRule{{-0.35f, 1.0f, 0.15f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}};
    m_rules[EquipSlot::Back] = AttachmentRule{{0.0f, 1.15f, -0.18f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}};
}

bool EquipmentRules::Load(const char* path) {
    SetDefaults();

    const std::string json = ReadTextFile(path);
    if (json.empty()) {
        LOG_WARNING_FMT("EquipmentRules", "Rules file not found at %s — using built-in offsets", path);
        return false;
    }

    static const char* kSlotNames[] = {
        "head", "chest", "legs", "feet", "hands", "mainhand", "offhand", "back",
    };

    for (const char* slotName : kSlotNames) {
        const std::string objectJson = JsonExtractObject(json, slotName);
        if (objectJson.empty()) {
            continue;
        }

        const EquipSlot slot = EquipSlotFromString(slotName);
        AttachmentRule rule = Get(slot);
        if (ParseRuleObject(slotName, objectJson, rule)) {
            m_rules[slot] = rule;
        }
    }

    LOG_INFO_FMT("EquipmentRules", "Loaded attachment rules from %s", path);
    return true;
}

const AttachmentRule& EquipmentRules::Get(EquipSlot slot) const {
    static const AttachmentRule kIdentity{};
    const auto it = m_rules.find(slot);
    return it == m_rules.end() ? kIdentity : it->second;
}

} // namespace Character
} // namespace Game
