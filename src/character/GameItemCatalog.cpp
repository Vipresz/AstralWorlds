#include "character/GameItemCatalog.h"

#include "Logger.h"
#include "character/JsonUtil.h"

using EngineLogging::Logger;

namespace Game {
namespace Character {

namespace {

Engine::Systems::Inventory::ItemType ItemTypeFromString(const std::string& value) {
    if (value == "tool") {
        return Engine::Systems::Inventory::ItemType::Tool;
    }
    if (value == "weapon") {
        return Engine::Systems::Inventory::ItemType::Weapon;
    }
    if (value == "armor") {
        return Engine::Systems::Inventory::ItemType::Armor;
    }
    if (value == "consumable") {
        return Engine::Systems::Inventory::ItemType::Consumable;
    }
    if (value == "building") {
        return Engine::Systems::Inventory::ItemType::Building;
    }
    return Engine::Systems::Inventory::ItemType::Resource;
}

bool JsonTryGetUInt(const std::string& json, const char* key, uint32_t& out) {
    const std::string needle = std::string("\"") + key + "\"";
    const size_t keyPos = json.find(needle);
    if (keyPos == std::string::npos) {
        return false;
    }

    const size_t colonPos = json.find(':', keyPos + needle.size());
    if (colonPos == std::string::npos) {
        return false;
    }

    size_t cursor = colonPos + 1;
    while (cursor < json.size() && std::isspace(static_cast<unsigned char>(json[cursor]))) {
        ++cursor;
    }
    if (cursor >= json.size()) {
        return false;
    }

    size_t end = cursor;
    while (end < json.size() && std::isdigit(static_cast<unsigned char>(json[end]))) {
        ++end;
    }
    if (end == cursor) {
        return false;
    }

    out = static_cast<uint32_t>(std::stoul(json.substr(cursor, end - cursor)));
    return true;
}

std::vector<std::string> SplitTopLevelObjects(const std::string& arrayJson) {
    std::vector<std::string> objects;
    if (arrayJson.size() < 2) {
        return objects;
    }

    int depth = 0;
    size_t objectStart = std::string::npos;
    for (size_t i = 0; i < arrayJson.size(); ++i) {
        const char c = arrayJson[i];
        if (c == '{') {
            if (depth == 0) {
                objectStart = i;
            }
            ++depth;
        } else if (c == '}') {
            --depth;
            if (depth == 0 && objectStart != std::string::npos) {
                objects.push_back(arrayJson.substr(objectStart, i - objectStart + 1));
                objectStart = std::string::npos;
            }
        }
    }
    return objects;
}

bool ParseGameItemObject(const std::string& objectJson, GameItemDefinition& out) {
    if (!JsonTryGetString(objectJson, "id", out.id) || out.id.empty()) {
        return false;
    }
    if (!JsonTryGetUInt(objectJson, "numeric_id", out.numericId) || out.numericId == 0) {
        return false;
    }

    JsonTryGetString(objectJson, "name", out.displayName);
    if (out.displayName.empty()) {
        out.displayName = out.id;
    }

    JsonTryGetString(objectJson, "category", out.category);
    JsonTryGetString(objectJson, "description", out.description);

    std::string iconRelative;
    if (JsonTryGetString(objectJson, "icon", iconRelative) && !iconRelative.empty()) {
        out.iconPath = "menus/" + iconRelative;
    }

    std::string typeName;
    if (JsonTryGetString(objectJson, "type", typeName)) {
        out.itemType = ItemTypeFromString(typeName);
    }

    uint32_t stackSize = out.stackSize;
    if (JsonTryGetUInt(objectJson, "stack_size", stackSize) && stackSize > 0) {
        out.stackSize = stackSize;
    }

    return true;
}

} // namespace

bool GameItemCatalog::Load(const char* path) {
    m_items.clear();
    m_numericToId.clear();

    const std::string json = ReadTextFile(path);
    if (json.empty()) {
        LOG_WARNING_FMT("GameItemCatalog", "Catalog not found at %s", path);
        return false;
    }

    const std::string itemsArray = JsonExtractArray(json, "items");
    for (const std::string& objectJson : SplitTopLevelObjects(itemsArray)) {
        GameItemDefinition item;
        if (!ParseGameItemObject(objectJson, item)) {
            continue;
        }

        if (m_items.find(item.id) != m_items.end()) {
            LOG_WARNING_FMT("GameItemCatalog", "Duplicate item id '%s'", item.id.c_str());
            continue;
        }
        if (m_numericToId.find(item.numericId) != m_numericToId.end()) {
            LOG_WARNING_FMT(
                "GameItemCatalog",
                "Duplicate numeric_id %u for '%s'",
                item.numericId,
                item.id.c_str());
            continue;
        }

        m_items.emplace(item.id, item);
        m_numericToId.emplace(item.numericId, item.id);
    }

    LOG_INFO_FMT("GameItemCatalog", "Loaded %zu game items from %s", m_items.size(), path);
    return !m_items.empty();
}

uint32_t GameItemCatalog::DefaultStackCapForType(Engine::Systems::Inventory::ItemType type) {
    switch (type) {
        case Engine::Systems::Inventory::ItemType::Weapon:
        case Engine::Systems::Inventory::ItemType::Armor:
            return kUniqueItemStackCap;
        case Engine::Systems::Inventory::ItemType::Tool:
            return kUniqueItemStackCap;
        case Engine::Systems::Inventory::ItemType::Resource:
        case Engine::Systems::Inventory::ItemType::Consumable:
        case Engine::Systems::Inventory::ItemType::Building:
        default:
            return kDefaultResourceStackCap;
    }
}

const GameItemDefinition* GameItemCatalog::Find(const std::string& itemId) const {
    const auto it = m_items.find(itemId);
    return it == m_items.end() ? nullptr : &it->second;
}

const GameItemDefinition* GameItemCatalog::FindByNumericId(uint32_t numericId) const {
    const auto it = m_numericToId.find(numericId);
    if (it == m_numericToId.end()) {
        return nullptr;
    }
    return Find(it->second);
}

const std::string* GameItemCatalog::FindItemIdByNumericId(uint32_t numericId) const {
    const auto it = m_numericToId.find(numericId);
    if (it == m_numericToId.end()) {
        return nullptr;
    }
    return &it->second;
}

uint32_t GameItemCatalog::GetStackCap(const std::string& itemId) const {
    const GameItemDefinition* definition = Find(itemId);
    if (!definition) {
        return kDefaultResourceStackCap;
    }
    return DefaultStackCapForType(definition->itemType);
}

uint32_t GameItemCatalog::GetStackCap(uint32_t numericId) const {
    const GameItemDefinition* definition = FindByNumericId(numericId);
    if (!definition) {
        return kDefaultResourceStackCap;
    }
    return DefaultStackCapForType(definition->itemType);
}

Engine::Systems::Inventory::Item GameItemCatalog::CreateInventoryItem(
    uint32_t numericId,
    uint32_t quantity) const {
    Engine::Systems::Inventory::Item item{};
    const GameItemDefinition* definition = FindByNumericId(numericId);
    if (!definition) {
        return item;
    }

    item.id = definition->numericId;
    item.name = definition->displayName;
    item.type = definition->itemType;
    item.stackSize = DefaultStackCapForType(definition->itemType);
    item.quantity = std::min(quantity, item.stackSize);
    item.durability = 1.0f;
    return item;
}

Engine::Systems::Inventory::Item GameItemCatalog::CreateInventoryItem(
    const std::string& itemId,
    uint32_t quantity) const {
    const GameItemDefinition* definition = Find(itemId);
    if (!definition) {
        return Engine::Systems::Inventory::Item{};
    }
    return CreateInventoryItem(definition->numericId, quantity);
}

namespace {

GameItemTooltipInfo BuildTooltipInfoFromDefinition(const GameItemDefinition* definition) {
    GameItemTooltipInfo info;
    if (!definition) {
        return info;
    }

    info.name = definition->displayName;
    info.typeLabel = GameItemCatalog::ItemTypeDisplayName(definition->itemType);
    if (!definition->description.empty()) {
        info.description = definition->description;
    } else if (!definition->category.empty()) {
        info.description = "A " + definition->category + " crafting material.";
    }
    return info;
}

} // namespace

std::string GameItemCatalog::ItemTypeDisplayName(Engine::Systems::Inventory::ItemType type) {
    switch (type) {
        case Engine::Systems::Inventory::ItemType::Tool:
            return "Tool";
        case Engine::Systems::Inventory::ItemType::Weapon:
            return "Weapon";
        case Engine::Systems::Inventory::ItemType::Armor:
            return "Armor";
        case Engine::Systems::Inventory::ItemType::Consumable:
            return "Consumable";
        case Engine::Systems::Inventory::ItemType::Building:
            return "Building";
        case Engine::Systems::Inventory::ItemType::Resource:
        default:
            return "Resource";
    }
}

std::string GameItemCatalog::ItemTypeDisplayName(const std::string& typeKey) {
    if (typeKey == "tool") {
        return "Tool";
    }
    if (typeKey == "weapon") {
        return "Weapon";
    }
    if (typeKey == "armor") {
        return "Armor";
    }
    if (typeKey == "consumable") {
        return "Consumable";
    }
    if (typeKey == "building") {
        return "Building";
    }
    return "Resource";
}

GameItemTooltipInfo GameItemCatalog::BuildTooltipInfo(uint32_t numericId) const {
    return BuildTooltipInfoFromDefinition(FindByNumericId(numericId));
}

GameItemTooltipInfo GameItemCatalog::BuildTooltipInfo(const std::string& itemId) const {
    return BuildTooltipInfoFromDefinition(Find(itemId));
}

bool GameItemCatalog::CanUseItem(const std::string& itemId) const {
    const GameItemDefinition* definition = Find(itemId);
    if (!definition) {
        return false;
    }
    switch (definition->itemType) {
        case Engine::Systems::Inventory::ItemType::Consumable:
        case Engine::Systems::Inventory::ItemType::Resource:
            return true;
        case Engine::Systems::Inventory::ItemType::Tool:
        case Engine::Systems::Inventory::ItemType::Weapon:
        case Engine::Systems::Inventory::ItemType::Armor:
        case Engine::Systems::Inventory::ItemType::Building:
        default:
            return false;
    }
}

bool GameItemCatalog::CanUseItem(uint32_t numericId) const {
    const GameItemDefinition* definition = FindByNumericId(numericId);
    if (!definition) {
        return false;
    }
    return CanUseItem(definition->id);
}

} // namespace Character
} // namespace Game
