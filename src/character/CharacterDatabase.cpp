#include "character/CharacterDatabase.h"

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

ServerLoadoutDatabase::ServerLoadoutDatabase(const char* loadoutsDir)
    : m_loadoutsDir(loadoutsDir ? loadoutsDir : "character/loadouts") {}

bool ServerLoadoutDatabase::LoadFromFile(const std::string& path, CharacterLoadout& out) const {
    const std::string json = ReadTextFile(path.c_str());
    if (json.empty()) {
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

    return !out.bodyItemId.empty();
}

bool ServerLoadoutDatabase::FetchLoadout(const CharacterLoginContext& context, CharacterLoadout& out) {
    if (context.userId != Engine::Net::kInvalidUserId) {
        const std::string userPath =
            JoinPath(m_loadoutsDir, std::to_string(context.userId) + ".json");
        if (LoadFromFile(userPath, out)) {
            LOG_INFO_FMT("CharacterDatabase",
                         "Loaded server loadout for user %u from %s",
                         context.userId,
                         userPath.c_str());
            return true;
        }
    }

    const std::string defaultPath = JoinPath(m_loadoutsDir, "default.json");
    if (LoadFromFile(defaultPath, out)) {
        LOG_INFO_FMT("CharacterDatabase",
                     "Loaded default loadout for '%s' from %s",
                     context.displayName.c_str(),
                     defaultPath.c_str());
        return true;
    }

    out.bodyItemId = "body_male_default";
    out.equipment.clear();
    LOG_WARNING("CharacterDatabase", "No loadout file found — using empty default body");
    return true;
}

} // namespace Character
} // namespace Game
