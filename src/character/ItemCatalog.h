#pragma once

#include "character/CharacterTypes.h"

#include <string>
#include <unordered_map>

namespace Game {
namespace Character {

struct ItemDefinition {
    std::string id;
    EquipSlot slot = EquipSlot::Unknown;
    std::string modelPath;
    std::string displayName;
};

// Maps item IDs to model paths and equipment slots.
class ItemCatalog {
public:
    bool Load(const char* path = "character/items.json");

    const ItemDefinition* Find(const std::string& itemId) const;
    const ItemDefinition* DefaultBody() const;

private:
    std::unordered_map<std::string, ItemDefinition> m_items;
    std::string m_defaultBodyId = "body_male_default";
};

} // namespace Character
} // namespace Game
