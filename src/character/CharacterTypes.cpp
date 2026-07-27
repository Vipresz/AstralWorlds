#include "character/CharacterTypes.h"

#include <algorithm>
#include <cctype>

namespace Game {
namespace Character {

namespace {

std::string ToLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

} // namespace

EquipSlot EquipSlotFromString(const std::string& value) {
    const std::string key = ToLower(value);
    if (key == "body") return EquipSlot::Body;
    if (key == "head") return EquipSlot::Head;
    if (key == "chest") return EquipSlot::Chest;
    if (key == "legs") return EquipSlot::Legs;
    if (key == "feet") return EquipSlot::Feet;
    if (key == "hands") return EquipSlot::Hands;
    if (key == "mainhand" || key == "main_hand") return EquipSlot::MainHand;
    if (key == "offhand" || key == "off_hand") return EquipSlot::OffHand;
    if (key == "back") return EquipSlot::Back;
    return EquipSlot::Unknown;
}

const char* EquipSlotToString(EquipSlot slot) {
    switch (slot) {
        case EquipSlot::Body: return "body";
        case EquipSlot::Head: return "head";
        case EquipSlot::Chest: return "chest";
        case EquipSlot::Legs: return "legs";
        case EquipSlot::Feet: return "feet";
        case EquipSlot::Hands: return "hands";
        case EquipSlot::MainHand: return "mainhand";
        case EquipSlot::OffHand: return "offhand";
        case EquipSlot::Back: return "back";
        default: return "unknown";
    }
}

} // namespace Character
} // namespace Game
