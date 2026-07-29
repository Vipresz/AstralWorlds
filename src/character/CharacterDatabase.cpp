#include "character/CharacterDatabase.h"

#include "AppPaths.h"
#include "Logger.h"
#include "character/JsonUtil.h"

using EngineLogging::Logger;

namespace Game {
namespace Character {

namespace {

std::string JoinPath(const std::string& root, const std::string& file) {
    if (root.empty()) {
        return file;
    }
    if (root.back() == '/' || root.back() == '\\') {
        return root + file;
    }
    return root + "/" + file;
}

bool ParseInventoryEntry(const std::string& objectJson, LoadoutInventoryEntry& out) {
    if (!JsonTryGetString(objectJson, "item_id", out.itemId)) {
        JsonTryGetString(objectJson, "id", out.itemId);
    }
    if (out.itemId.empty()) {
        return false;
    }

    uint32_t quantity = 1;
    JsonTryGetUInt(objectJson, "quantity", quantity);
    out.quantity = quantity > 0 ? quantity : 1;
    return true;
}

bool ParseEquippedItem(const std::string& objectJson, EquippedItem& out) {
    if (!JsonTryGetString(objectJson, "item_id", out.itemId)) {
        JsonTryGetString(objectJson, "id", out.itemId);
    }
    if (out.itemId.empty()) {
        return false;
    }

    std::string slotName;
    if (JsonTryGetString(objectJson, "slot", slotName)) {
        out.slot = EquipSlotFromString(slotName);
    }
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

} // namespace

ServerLoadoutDatabase::ServerLoadoutDatabase(const char* loadoutsDir) {
    const char* relative = loadoutsDir ? loadoutsDir : "character/loadouts";
    m_loadoutsDir = Game::ResolveDataPath(relative);
    if (m_loadoutsDir.empty()) {
        m_loadoutsDir = relative;
    }
    LOG_INFO_FMT("CharacterDatabase", "Loadout directory resolved to %s", m_loadoutsDir.c_str());
}

std::string ServerLoadoutDatabase::ResolveLoadoutFilePath(const std::string& fileName) const {
    return JoinPath(m_loadoutsDir, fileName);
}

bool ServerLoadoutDatabase::LoadFromFile(const std::string& path, CharacterLoadout& out) const {
    const std::string json = ReadTextFile(path.c_str());
    if (json.empty()) {
        LOG_WARNING_FMT("CharacterDatabase", "Loadout not found or empty at %s", path.c_str());
        return false;
    }

    out = CharacterLoadout{};
    JsonTryGetString(json, "body_item_id", out.bodyItemId);

    const std::string equipmentArray = JsonExtractArray(json, "equipment");
    for (const std::string& objectJson : SplitTopLevelObjects(equipmentArray)) {
        EquippedItem item;
        if (ParseEquippedItem(objectJson, item)) {
            out.equipment.push_back(item);
        }
    }

    const std::string inventoryArray = JsonExtractArray(json, "inventory");
    for (const std::string& objectJson : SplitTopLevelObjects(inventoryArray)) {
        LoadoutInventoryEntry entry;
        if (ParseInventoryEntry(objectJson, entry)) {
            out.inventory.push_back(entry);
        }
    }

    return !out.bodyItemId.empty();
}

bool ServerLoadoutDatabase::FetchLoadout(const CharacterLoginContext& context, CharacterLoadout& out) {
    const std::string defaultPath = ResolveLoadoutFilePath("default.json");

    if (context.userId != Engine::Net::kInvalidUserId) {
        const std::string userPath =
            ResolveLoadoutFilePath(std::to_string(context.userId) + ".json");
        if (LoadFromFile(userPath, out)) {
            if (out.inventory.empty()) {
                CharacterLoadout defaultLoadout;
                if (LoadFromFile(defaultPath, defaultLoadout) && !defaultLoadout.inventory.empty()) {
                    out.inventory = defaultLoadout.inventory;
                    LOG_INFO_FMT(
                        "CharacterDatabase",
                        "Merged %zu default starter inventory entries for user %u",
                        out.inventory.size(),
                        context.userId);
                }
            }
            LOG_INFO_FMT("CharacterDatabase",
                         "Loaded server loadout for user %u from %s",
                         context.userId,
                         userPath.c_str());
            return true;
        }
    }

    if (LoadFromFile(defaultPath, out)) {
        LOG_INFO_FMT("CharacterDatabase",
                     "Loaded default loadout for '%s' from %s",
                     context.displayName.c_str(),
                     defaultPath.c_str());
        return true;
    }

    out = CharacterLoadout{};
    out.bodyItemId = "body_male_default";
    LOG_WARNING("CharacterDatabase", "No loadout file found — using empty default body");
    return true;
}

} // namespace Character
} // namespace Game
