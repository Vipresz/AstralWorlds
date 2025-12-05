#pragma once

#include <cstdint>
#include <vector>
#include <string>

namespace Engine {
namespace Systems {
namespace Inventory {

// Item type definitions
using ItemID = uint32_t;

enum class ItemType {
    Resource,
    Tool,
    Weapon,
    Armor,
    Consumable,
    Building
};

struct Item {
    ItemID id;
    std::string name;
    ItemType type;
    uint32_t stackSize;      // Max quantity in one slot (1 = unique)
    uint32_t quantity;       // Current quantity
    float durability;        // For tools/weapons (0.0 - 1.0)
    
    bool IsStackable() const {
        return stackSize > 1;
    }
    
    bool CanStackWith(const Item& other) const {
        return id == other.id && IsStackable();
    }
};

struct ItemSlot {
    Item item;
    bool isEmpty;
    
    ItemSlot() : isEmpty(true) {}
    
    bool AddItem(const Item& newItem) {
        if (isEmpty) {
            item = newItem;
            isEmpty = false;
            return true;
        }
        
        if (item.CanStackWith(newItem)) {
            uint32_t spaceLeft = item.stackSize - item.quantity;
            if (spaceLeft > 0) {
                uint32_t toAdd = std::min(spaceLeft, newItem.quantity);
                item.quantity += toAdd;
                return toAdd == newItem.quantity;
            }
        }
        
        return false;
    }
    
    bool RemoveItem(uint32_t quantity) {
        if (isEmpty) return false;
        
        if (item.quantity <= quantity) {
            isEmpty = true;
            return true;
        }
        
        item.quantity -= quantity;
        return false;
    }
};

// Component for entities that can hold items
struct InventoryComponent {
    std::vector<ItemSlot> slots;
    uint32_t capacity;
    
    InventoryComponent() : capacity(20) {
        slots.resize(capacity);
    }
    
    InventoryComponent(uint32_t capacity) : capacity(capacity) {
        slots.resize(capacity);
    }
    
    // Try to add item to inventory
    bool AddItem(const Item& item) {
        // First try to stack with existing items
        for (auto& slot : slots) {
            if (!slot.isEmpty && slot.item.CanStackWith(item)) {
                uint32_t spaceLeft = slot.item.stackSize - slot.item.quantity;
                if (spaceLeft > 0) {
                    uint32_t toAdd = std::min(spaceLeft, item.quantity);
                    slot.item.quantity += toAdd;
                    
                    if (toAdd == item.quantity) {
                        return true; // Fully added
                    }
                    
                    // Create new item with remaining quantity
                    Item remaining = item;
                    remaining.quantity -= toAdd;
                    return AddItem(remaining);
                }
            }
        }
        
        // Find empty slot
        for (auto& slot : slots) {
            if (slot.isEmpty) {
                slot.item = item;
                slot.isEmpty = false;
                return true;
            }
        }
        
        return false; // Inventory full
    }
    
    // Remove item from inventory
    bool RemoveItem(ItemID itemId, uint32_t quantity) {
        for (auto& slot : slots) {
            if (!slot.isEmpty && slot.item.id == itemId) {
                if (slot.item.quantity >= quantity) {
                    slot.RemoveItem(quantity);
                    return true;
                } else {
                    quantity -= slot.item.quantity;
                    slot.isEmpty = true;
                }
            }
        }
        return false;
    }
    
    // Check if inventory has item
    uint32_t GetItemCount(ItemID itemId) const {
        uint32_t count = 0;
        for (const auto& slot : slots) {
            if (!slot.isEmpty && slot.item.id == itemId) {
                count += slot.item.quantity;
            }
        }
        return count;
    }
};

} // namespace Inventory
} // namespace Systems
} // namespace Engine

