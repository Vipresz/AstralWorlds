#include "character/ItemCatalog.h"

#include "Logger.h"
#include "character/JsonUtil.h"

using EngineLogging::Logger;

namespace Game {
namespace Character {

namespace {

bool ParseItemObject(const std::string& objectJson, ItemDefinition& out) {
    if (!JsonTryGetString(objectJson, "id", out.id)) {
        return false;
    }

    std::string slotName;
    if (JsonTryGetString(objectJson, "slot", slotName)) {
        out.slot = EquipSlotFromString(slotName);
    }

    JsonTryGetString(objectJson, "model", out.modelPath);
    JsonTryGetString(objectJson, "name", out.displayName);
    if (out.displayName.empty()) {
        out.displayName = out.id;
    }
    return !out.modelPath.empty();
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

} // namespace

bool ItemCatalog::Load(const char* path) {
    m_items.clear();

    const std::string json = ReadTextFile(path);
    if (json.empty()) {
        LOG_WARNING_FMT("ItemCatalog", "Catalog not found at %s — using built-in defaults", path);

        ItemDefinition body;
        body.id = "body_male_default";
        body.slot = EquipSlot::Body;
        body.modelPath = "graphics/Models/character/graphics/male_model.obj";
        body.displayName = "Default Body";
        m_items.emplace(body.id, body);
        m_defaultBodyId = body.id;
        return false;
    }

    JsonTryGetString(json, "default_body", m_defaultBodyId);

    const std::string itemsArray = JsonExtractArray(json, "items");
    for (const std::string& objectJson : SplitTopLevelObjects(itemsArray)) {
        ItemDefinition item;
        if (ParseItemObject(objectJson, item)) {
            m_items.emplace(item.id, item);
        }
    }

    if (m_items.find(m_defaultBodyId) == m_items.end()) {
        LOG_WARNING_FMT("ItemCatalog", "default_body '%s' missing from catalog", m_defaultBodyId.c_str());
    }

    LOG_INFO_FMT("ItemCatalog", "Loaded %zu items from %s", m_items.size(), path);
    return true;
}

const ItemDefinition* ItemCatalog::Find(const std::string& itemId) const {
    const auto it = m_items.find(itemId);
    return it == m_items.end() ? nullptr : &it->second;
}

const ItemDefinition* ItemCatalog::DefaultBody() const {
    return Find(m_defaultBodyId);
}

} // namespace Character
} // namespace Game
