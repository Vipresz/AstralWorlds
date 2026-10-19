#pragma once

#include "core/network/NetTypes.h"

#include <cstdint>
#include <string>
#include <vector>

namespace Game {
namespace Character {

enum class EquipSlot {
    Body,
    Head,
    Chest,
    Legs,
    Feet,
    Hands,
    MainHand,
    OffHand,
    Back,
    Unknown
};

EquipSlot EquipSlotFromString(const std::string& value);
const char* EquipSlotToString(EquipSlot slot);

struct EquippedItem {
    std::string itemId;
    EquipSlot slot = EquipSlot::Unknown;
};

struct LoadoutInventoryEntry {
    std::string itemId;
    uint32_t quantity = 1;
};

struct CharacterLoadout {
    std::string bodyItemId = "body_male_default";
    std::vector<EquippedItem> equipment;
    std::vector<LoadoutInventoryEntry> inventory;
};

struct CharacterLoginContext {
    Engine::Net::UserId userId = Engine::Net::kInvalidUserId;
    std::string displayName;
    bool isOnlineLogin = false;
};

} // namespace Character
} // namespace Game
