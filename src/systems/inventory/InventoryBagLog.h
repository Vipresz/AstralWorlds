#pragma once

#include "character/GameItemCatalog.h"
#include "Logger.h"
#include "systems/inventory/InventoryComponent.h"

#include <sstream>
#include <string>

namespace Game {
namespace Inventory {

inline std::string DescribeInventorySlot(
    size_t slotIndex,
    const Engine::Systems::Inventory::Item& item,
    const Game::Character::GameItemCatalog* catalog) {
    std::ostringstream out;
    out << "slot " << slotIndex << ": numeric_id=" << item.id;

    if (catalog) {
        const Game::Character::GameItemDefinition* definition =
            catalog->FindByNumericId(item.id);
        if (definition) {
            out << " item_id=\"" << definition->id << "\" name=\"" << definition->displayName
                << "\"";
        }
    }

    out << " qty=" << item.quantity;
    return out.str();
}

inline void LogInventoryContents(
    const Engine::Systems::Inventory::InventoryComponent& inventory,
    const Game::Character::GameItemCatalog* catalog,
    const char* context) {
    std::ostringstream summary;
    size_t occupied = 0;

    for (size_t slotIndex = 0; slotIndex < inventory.slots.size(); ++slotIndex) {
        const auto& slot = inventory.slots[slotIndex];
        if (slot.isEmpty) {
            continue;
        }

        ++occupied;
        summary << DescribeInventorySlot(slotIndex, slot.item, catalog) << " | ";
    }

    if (occupied == 0) {
        LOG_INFO_FMT("Inventory", "[%s] bag empty (capacity %u)", context, inventory.capacity);
        return;
    }

    LOG_INFO_FMT(
        "Inventory",
        "[%s] bag has %zu item slot(s) / capacity %u: %s",
        context,
        occupied,
        inventory.capacity,
        summary.str().c_str());
}

} // namespace Inventory
} // namespace Game
