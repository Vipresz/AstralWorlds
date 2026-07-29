#pragma once

#include "Logger.h"
#include "character/CharacterTypes.h"
#include "character/GameItemCatalog.h"
#include "core/ecs/components/PlayerItemOwnershipComponent.h"
#include "core/network/ItemOwnershipReplication.h"
#include "core/network/NetTypes.h"
#include "systems/inventory/InventoryBagLog.h"

namespace Game {
namespace Inventory {

inline void ClearInventorySlots(Engine::Systems::Inventory::InventoryComponent& inventory) {
    for (auto& slot : inventory.slots) {
        slot.isEmpty = true;
    }
}

// Mirror authoritative slot layout into the local bag UI model.
inline void SyncInventoryFromOwnership(
    Engine::Systems::Inventory::InventoryComponent& inventory,
    const Engine::ECS::PlayerItemOwnershipComponent& ownership,
    const Game::Character::GameItemCatalog& catalog) {
    ClearInventorySlots(inventory);

    size_t placedStacks = 0;
    for (const Engine::ECS::OwnedItemStack& stack : ownership.ownedItems) {
        if (stack.quantity == 0 || stack.itemId.empty()) {
            continue;
        }
        if (stack.slotIndex >= inventory.slots.size()) {
            LOG_WARNING_FMT(
                "Inventory",
                "Skipping ownership entry past bag capacity (slot %u)",
                stack.slotIndex);
            continue;
        }

        const Engine::Systems::Inventory::Item item =
            catalog.CreateInventoryItem(stack.itemId, stack.quantity);
        if (item.id == 0) {
            LOG_WARNING_FMT(
                "Inventory",
                "Skipping unknown ownership item '%s' (qty %u)",
                stack.itemId.c_str(),
                stack.quantity);
            continue;
        }

        inventory.slots[stack.slotIndex].item = item;
        inventory.slots[stack.slotIndex].isEmpty = false;
        ++placedStacks;
    }

    LOG_INFO_FMT(
        "Inventory",
        "Ownership sync placed %zu stack(s) from %zu ownership entries",
        placedStacks,
        ownership.ownedItems.size());
    LogInventoryContents(inventory, &catalog, "ownership-sync");
}

inline void BuildOwnershipFromInventory(
    Engine::ECS::PlayerItemOwnershipComponent& ownership,
    const Engine::Systems::Inventory::InventoryComponent& inventory,
    const Game::Character::GameItemCatalog& catalog,
    Engine::Net::UserId ownerUserId) {
    ownership.ownerUserId = ownerUserId;
    ownership.Clear();

    for (size_t slotIndex = 0; slotIndex < inventory.slots.size(); ++slotIndex) {
        const Engine::Systems::Inventory::ItemSlot& slot = inventory.slots[slotIndex];
        if (slot.isEmpty || slot.item.quantity == 0) {
            continue;
        }

        const std::string* itemId = catalog.FindItemIdByNumericId(slot.item.id);
        if (!itemId) {
            LOG_WARNING_FMT(
                "Inventory",
                "Skipping bag slot %zu with unknown numeric_id %u",
                slotIndex,
                slot.item.id);
            continue;
        }

        Engine::ECS::OwnedItemStack stack;
        stack.slotIndex = static_cast<uint16_t>(slotIndex);
        stack.itemId = *itemId;
        stack.quantity = slot.item.quantity;
        stack.instanceId = 0;
        ownership.ownedItems.push_back(std::move(stack));
    }
}

inline Engine::Net::ReplicatedItemOwnership BuildReplicatedOwnership(
    const Engine::ECS::PlayerItemOwnershipComponent& ownership) {
    return Engine::Net::FromOwnershipComponent(ownership);
}

inline void ApplyLoadoutOwnership(
    Engine::ECS::PlayerItemOwnershipComponent& ownership,
    const Game::Character::CharacterLoadout& loadout,
    const Game::Character::GameItemCatalog& catalog,
    Engine::Net::UserId ownerUserId,
    uint16_t bagCapacity = Engine::ECS::kDefaultBagCapacity) {
    ownership.ownerUserId = ownerUserId;
    ownership.Clear();
    for (const Game::Character::LoadoutInventoryEntry& entry : loadout.inventory) {
        const uint32_t stackCap = catalog.GetStackCap(entry.itemId);
        if (!ownership.GrantItem(
                entry.itemId, entry.quantity, 0, stackCap, bagCapacity)) {
            LOG_WARNING_FMT(
                "Inventory",
                "Loadout grant failed for item_id='%s' qty=%u",
                entry.itemId.c_str(),
                entry.quantity);
        }
    }
}

} // namespace Inventory
} // namespace Game
