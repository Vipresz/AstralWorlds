#pragma once

#include "character/GameItemCatalog.h"
#include "core/ecs/components/PlayerItemOwnershipComponent.h"
#include "core/network/InventoryAction.h"
#include "systems/inventory/InventoryComponent.h"
#include "systems/inventory/InventoryOwnershipSync.h"

#include "Logger.h"

namespace Game {
namespace Inventory {

inline bool ApplyUseAtSlot(
    Engine::Systems::Inventory::InventoryComponent& inventory,
    Engine::ECS::PlayerItemOwnershipComponent& ownership,
    const Game::Character::GameItemCatalog& catalog,
    size_t slotIndex,
    Engine::Net::UserId ownerUserId) {
    if (slotIndex >= inventory.slots.size() || inventory.slots[slotIndex].isEmpty) {
        return false;
    }

    const Engine::Systems::Inventory::Item& item = inventory.slots[slotIndex].item;
    if (!catalog.CanUseItem(item.id)) {
        return false;
    }

    if (!inventory.RemoveQuantityFromSlot(slotIndex, 1)) {
        return false;
    }

    const std::string* itemId = catalog.FindItemIdByNumericId(item.id);
    if (!itemId) {
        return false;
    }

    LOG_INFO_FMT("Inventory", "Used item '%s' from bag slot %zu", itemId->c_str(), slotIndex);

    BuildOwnershipFromInventory(ownership, inventory, catalog, ownerUserId);
    return true;
}

inline bool ApplyDropAtSlot(
    Engine::Systems::Inventory::InventoryComponent& inventory,
    Engine::ECS::PlayerItemOwnershipComponent& ownership,
    const Game::Character::GameItemCatalog& catalog,
    size_t slotIndex,
    Engine::Net::UserId ownerUserId) {
    if (slotIndex >= inventory.slots.size() || inventory.slots[slotIndex].isEmpty) {
        return false;
    }

    const Engine::Systems::Inventory::Item& item = inventory.slots[slotIndex].item;
    const std::string* itemId = catalog.FindItemIdByNumericId(item.id);
    const char* itemLabel = itemId ? itemId->c_str() : "unknown";

    if (!inventory.ClearSlot(slotIndex)) {
        return false;
    }

    LOG_INFO_FMT(
        "Inventory",
        "Dropped item '%s' (qty %u) from bag slot %zu",
        itemLabel,
        item.quantity,
        slotIndex);

    BuildOwnershipFromInventory(ownership, inventory, catalog, ownerUserId);
    return true;
}

inline bool ApplyCombinedUseAtSlots(
    Engine::Systems::Inventory::InventoryComponent& inventory,
    Engine::ECS::PlayerItemOwnershipComponent& ownership,
    const Game::Character::GameItemCatalog& catalog,
    size_t primarySlotIndex,
    size_t secondarySlotIndex,
    Engine::Net::UserId ownerUserId) {
    if (primarySlotIndex >= inventory.slots.size() ||
        secondarySlotIndex >= inventory.slots.size() ||
        inventory.slots[primarySlotIndex].isEmpty ||
        inventory.slots[secondarySlotIndex].isEmpty) {
        return false;
    }
    if (primarySlotIndex == secondarySlotIndex) {
        return false;
    }
    if (!catalog.CanUseItem(inventory.slots[primarySlotIndex].item.id) ||
        !catalog.CanUseItem(inventory.slots[secondarySlotIndex].item.id)) {
        return false;
    }

    // Stub: any two usable items combine successfully for now.
    LOG_INFO_FMT(
        "Inventory",
        "Combined use succeeded for bag slots %zu and %zu (consuming both)",
        primarySlotIndex,
        secondarySlotIndex);

    if (!ApplyUseAtSlot(inventory, ownership, catalog, primarySlotIndex, ownerUserId)) {
        return false;
    }
    if (!ApplyUseAtSlot(inventory, ownership, catalog, secondarySlotIndex, ownerUserId)) {
        return false;
    }
    return true;
}

} // namespace Inventory
} // namespace Game
