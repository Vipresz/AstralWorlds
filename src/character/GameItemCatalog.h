#pragma once

#include "systems/inventory/InventoryComponent.h"

#include <cstdint>
#include <string>
#include <unordered_map>

namespace Game {
namespace Character {

struct GameItemTooltipInfo {
    std::string name;
    std::string typeLabel;
    std::string description;
};

struct GameItemDefinition {
    std::string id;
    uint32_t numericId = 0;
    std::string displayName;
    std::string category;
    std::string description;
    std::string iconPath;
    Engine::Systems::Inventory::ItemType itemType =
        Engine::Systems::Inventory::ItemType::Resource;
    uint32_t stackSize = 64;
};

constexpr uint32_t kDefaultResourceStackCap = 20;
constexpr uint32_t kUniqueItemStackCap = 1;

// Inventory item definitions (icons, numeric ids, stack sizes).
class GameItemCatalog {
public:
    bool Load(const char* path = "character/game_items.json");

    static uint32_t DefaultStackCapForType(Engine::Systems::Inventory::ItemType type);

    const GameItemDefinition* Find(const std::string& itemId) const;
    const GameItemDefinition* FindByNumericId(uint32_t numericId) const;
    const std::string* FindItemIdByNumericId(uint32_t numericId) const;

    uint32_t GetStackCap(const std::string& itemId) const;
    uint32_t GetStackCap(uint32_t numericId) const;

    const std::unordered_map<std::string, GameItemDefinition>& Items() const { return m_items; }

    Engine::Systems::Inventory::Item CreateInventoryItem(uint32_t numericId, uint32_t quantity) const;
    Engine::Systems::Inventory::Item CreateInventoryItem(
        const std::string& itemId,
        uint32_t quantity) const;

    GameItemTooltipInfo BuildTooltipInfo(uint32_t numericId) const;
    GameItemTooltipInfo BuildTooltipInfo(const std::string& itemId) const;

    static std::string ItemTypeDisplayName(Engine::Systems::Inventory::ItemType type);
    static std::string ItemTypeDisplayName(const std::string& typeKey);

private:
    std::unordered_map<std::string, GameItemDefinition> m_items;
    std::unordered_map<uint32_t, std::string> m_numericToId;
};

} // namespace Character
} // namespace Game
