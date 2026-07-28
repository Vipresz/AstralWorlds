#pragma once

#include "character/CharacterTypes.h"

#include <glm/glm.hpp>
#include <unordered_map>

namespace Game {
namespace Character {

struct AttachmentRule {
    glm::vec3 localOffset{0.0f};
    glm::vec3 localRotationDegrees{0.0f};
    glm::vec3 localScale{1.0f};
    bool inheritParentYaw = true;
    bool inheritParentScale = true;
};

// Defines how equipped items are positioned relative to the character root.
class EquipmentRules {
public:
    bool Load(const char* path = "character/equipment_rules.json");

    const AttachmentRule& Get(EquipSlot slot) const;

private:
    void SetDefaults();
    std::unordered_map<EquipSlot, AttachmentRule> m_rules;
};

} // namespace Character
} // namespace Game
