#pragma once

#include "core/network/ItemOwnershipReplication.h"
#include "core/network/NetTypes.h"

#include <mutex>
#include <string>

struct sqlite3;

namespace Game {
namespace Character {

// Local SQLite persistence for per-account bag slots (slot_index, item_id, quantity).
class CharacterInventoryDatabase {
public:
    explicit CharacterInventoryDatabase(const char* databasePath = "character/astral_worlds.db");

    bool Open();
    void Close();

    bool HasAccount(Engine::Net::UserId accountId) const;
    bool LoadOwnership(Engine::Net::UserId accountId, Engine::Net::ReplicatedItemOwnership& out) const;
    bool SaveOwnership(Engine::Net::UserId accountId,
                       const Engine::Net::ReplicatedItemOwnership& ownership);

private:
    bool InitializeSchema();
    bool AccountExistsLocked(Engine::Net::UserId accountId) const;

    std::string m_databasePath;
    sqlite3* m_db = nullptr;
    mutable std::mutex m_mutex;
};

} // namespace Character
} // namespace Game
