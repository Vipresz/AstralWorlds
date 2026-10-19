#include "character/CharacterInventoryDatabase.h"

#include "AppPaths.h"
#include "Logger.h"

#include <sqlite3.h>

#include <cstring>
#include <unordered_set>

using EngineLogging::Logger;

namespace Game {
namespace Character {

namespace {

constexpr const char* kAccountsTableSql =
    "CREATE TABLE IF NOT EXISTS accounts ("
    "  account_id INTEGER PRIMARY KEY"
    ");";

constexpr const char* kBagSlotsTableSql =
    "CREATE TABLE IF NOT EXISTS account_bag_slots ("
    "  account_id INTEGER NOT NULL,"
    "  slot_index INTEGER NOT NULL,"
    "  item_id TEXT NOT NULL,"
    "  quantity INTEGER NOT NULL CHECK (quantity > 0),"
    "  instance_id INTEGER NOT NULL DEFAULT 0,"
    "  PRIMARY KEY (account_id, slot_index),"
    "  FOREIGN KEY (account_id) REFERENCES accounts(account_id) ON DELETE CASCADE"
    ");";

} // namespace

CharacterInventoryDatabase::CharacterInventoryDatabase(const char* databasePath)
    : m_databasePath(
          Game::ResolveDataPath(databasePath ? databasePath : "character/astral_worlds.db")) {}

bool CharacterInventoryDatabase::Open() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_db) {
        return true;
    }

    if (sqlite3_open(m_databasePath.c_str(), &m_db) != SQLITE_OK) {
        LOG_ERROR_FMT(
            "CharacterInventoryDatabase",
            "Failed to open database %s: %s",
            m_databasePath.c_str(),
            m_db ? sqlite3_errmsg(m_db) : "unknown error");
        if (m_db) {
            sqlite3_close(m_db);
            m_db = nullptr;
        }
        return false;
    }

    if (!InitializeSchema()) {
        sqlite3_close(m_db);
        m_db = nullptr;
        return false;
    }

    LOG_INFO_FMT("CharacterInventoryDatabase", "Opened inventory database %s", m_databasePath.c_str());
    return true;
}

void CharacterInventoryDatabase::Close() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_db) {
        sqlite3_close(m_db);
        m_db = nullptr;
    }
}

bool CharacterInventoryDatabase::InitializeSchema() {
    char* errorMessage = nullptr;
    if (sqlite3_exec(m_db, kAccountsTableSql, nullptr, nullptr, &errorMessage) != SQLITE_OK) {
        LOG_ERROR_FMT(
            "CharacterInventoryDatabase",
            "Failed to create accounts table: %s",
            errorMessage ? errorMessage : "unknown error");
        sqlite3_free(errorMessage);
        return false;
    }

    if (sqlite3_exec(m_db, kBagSlotsTableSql, nullptr, nullptr, &errorMessage) != SQLITE_OK) {
        LOG_ERROR_FMT(
            "CharacterInventoryDatabase",
            "Failed to create account_bag_slots table: %s",
            errorMessage ? errorMessage : "unknown error");
        sqlite3_free(errorMessage);
        return false;
    }

    return true;
}

bool CharacterInventoryDatabase::AccountExistsLocked(Engine::Net::UserId accountId) const {
    sqlite3_stmt* statement = nullptr;
    constexpr const char* sql = "SELECT 1 FROM accounts WHERE account_id = ? LIMIT 1;";
    if (sqlite3_prepare_v2(m_db, sql, static_cast<int>(std::strlen(sql)), &statement, nullptr) !=
        SQLITE_OK) {
        LOG_ERROR_FMT(
            "CharacterInventoryDatabase",
            "Prepare account lookup failed: %s",
            sqlite3_errmsg(m_db));
        return false;
    }

    sqlite3_bind_int(statement, 1, static_cast<int>(accountId));
    const bool exists = sqlite3_step(statement) == SQLITE_ROW;
    sqlite3_finalize(statement);
    return exists;
}

bool CharacterInventoryDatabase::HasAccount(Engine::Net::UserId accountId) const {
    if (accountId == Engine::Net::kInvalidUserId) {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_db) {
        return false;
    }
    return AccountExistsLocked(accountId);
}

bool CharacterInventoryDatabase::LoadOwnership(Engine::Net::UserId accountId,
                                             Engine::Net::ReplicatedItemOwnership& out) const {
    if (accountId == Engine::Net::kInvalidUserId) {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_db || !AccountExistsLocked(accountId)) {
        return false;
    }

    out = Engine::Net::ReplicatedItemOwnership{};
    out.ownerUserId = accountId;

    sqlite3_stmt* statement = nullptr;
    constexpr const char* sql =
        "SELECT slot_index, item_id, quantity, instance_id "
        "FROM account_bag_slots WHERE account_id = ? ORDER BY slot_index;";
    if (sqlite3_prepare_v2(m_db, sql, static_cast<int>(std::strlen(sql)), &statement, nullptr) !=
        SQLITE_OK) {
        LOG_ERROR_FMT(
            "CharacterInventoryDatabase",
            "Prepare bag slot load failed: %s",
            sqlite3_errmsg(m_db));
        return false;
    }

    sqlite3_bind_int(statement, 1, static_cast<int>(accountId));
    while (sqlite3_step(statement) == SQLITE_ROW) {
        const int slotIndex = sqlite3_column_int(statement, 0);
        const char* itemIdText = reinterpret_cast<const char*>(sqlite3_column_text(statement, 1));
        if (!itemIdText || itemIdText[0] == '\0' || slotIndex < 0) {
            continue;
        }

        const int quantity = sqlite3_column_int(statement, 2);
        if (quantity <= 0) {
            continue;
        }

        Engine::Net::ReplicatedOwnedItem stack;
        stack.slotIndex = static_cast<uint16_t>(slotIndex);
        stack.itemId = itemIdText;
        stack.quantity = static_cast<uint32_t>(quantity);
        stack.instanceId = static_cast<uint64_t>(sqlite3_column_int64(statement, 3));
        out.items.push_back(std::move(stack));
    }

    sqlite3_finalize(statement);
    return true;
}

bool CharacterInventoryDatabase::SaveOwnership(Engine::Net::UserId accountId,
                                             const Engine::Net::ReplicatedItemOwnership& ownership) {
    if (accountId == Engine::Net::kInvalidUserId) {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_db) {
        return false;
    }

    char* errorMessage = nullptr;
    if (sqlite3_exec(m_db, "BEGIN IMMEDIATE;", nullptr, nullptr, &errorMessage) != SQLITE_OK) {
        LOG_ERROR_FMT(
            "CharacterInventoryDatabase",
            "BEGIN failed: %s",
            errorMessage ? errorMessage : "unknown error");
        sqlite3_free(errorMessage);
        return false;
    }

    bool committed = false;
    bool success = true;

    sqlite3_stmt* accountStatement = nullptr;
    constexpr const char* accountSql = "INSERT OR IGNORE INTO accounts(account_id) VALUES (?);";
    if (sqlite3_prepare_v2(
            m_db, accountSql, static_cast<int>(std::strlen(accountSql)), &accountStatement, nullptr) !=
        SQLITE_OK) {
        LOG_ERROR_FMT(
            "CharacterInventoryDatabase",
            "Prepare account insert failed: %s",
            sqlite3_errmsg(m_db));
        success = false;
    } else {
        sqlite3_bind_int(accountStatement, 1, static_cast<int>(accountId));
        if (sqlite3_step(accountStatement) != SQLITE_DONE) {
            LOG_ERROR_FMT(
                "CharacterInventoryDatabase",
                "Account insert failed: %s",
                sqlite3_errmsg(m_db));
            success = false;
        }
        sqlite3_finalize(accountStatement);
    }

    if (success) {
        sqlite3_stmt* deleteStatement = nullptr;
        constexpr const char* deleteSql = "DELETE FROM account_bag_slots WHERE account_id = ?;";
        if (sqlite3_prepare_v2(
                m_db, deleteSql, static_cast<int>(std::strlen(deleteSql)), &deleteStatement, nullptr) !=
            SQLITE_OK) {
            LOG_ERROR_FMT(
                "CharacterInventoryDatabase",
                "Prepare bag slot delete failed: %s",
                sqlite3_errmsg(m_db));
            success = false;
        } else {
            sqlite3_bind_int(deleteStatement, 1, static_cast<int>(accountId));
            if (sqlite3_step(deleteStatement) != SQLITE_DONE) {
                LOG_ERROR_FMT(
                    "CharacterInventoryDatabase",
                    "Bag slot delete failed: %s",
                    sqlite3_errmsg(m_db));
                success = false;
            }
            sqlite3_finalize(deleteStatement);
        }
    }

    if (success) {
        sqlite3_stmt* insertStatement = nullptr;
        constexpr const char* insertSql =
            "INSERT INTO account_bag_slots(account_id, slot_index, item_id, quantity, instance_id) "
            "VALUES (?, ?, ?, ?, ?);";
        if (sqlite3_prepare_v2(
                m_db, insertSql, static_cast<int>(std::strlen(insertSql)), &insertStatement, nullptr) !=
            SQLITE_OK) {
            LOG_ERROR_FMT(
                "CharacterInventoryDatabase",
                "Prepare bag slot insert failed: %s",
                sqlite3_errmsg(m_db));
            success = false;
        } else {
            for (const Engine::Net::ReplicatedOwnedItem& stack : ownership.items) {
                if (stack.quantity == 0 || stack.itemId.empty()) {
                    continue;
                }

                sqlite3_reset(insertStatement);
                sqlite3_clear_bindings(insertStatement);
                sqlite3_bind_int(insertStatement, 1, static_cast<int>(accountId));
                sqlite3_bind_int(insertStatement, 2, static_cast<int>(stack.slotIndex));
                sqlite3_bind_text(
                    insertStatement, 3, stack.itemId.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_int(insertStatement, 4, static_cast<int>(stack.quantity));
                sqlite3_bind_int64(insertStatement, 5, static_cast<sqlite3_int64>(stack.instanceId));
                if (sqlite3_step(insertStatement) != SQLITE_DONE) {
                    LOG_ERROR_FMT(
                        "CharacterInventoryDatabase",
                        "Bag slot insert failed for slot %u '%s': %s",
                        stack.slotIndex,
                        stack.itemId.c_str(),
                        sqlite3_errmsg(m_db));
                    success = false;
                    break;
                }
            }
            sqlite3_finalize(insertStatement);
        }
    }

    if (success) {
        if (sqlite3_exec(m_db, "COMMIT;", nullptr, nullptr, &errorMessage) != SQLITE_OK) {
            LOG_ERROR_FMT(
                "CharacterInventoryDatabase",
                "COMMIT failed: %s",
                errorMessage ? errorMessage : "unknown error");
            sqlite3_free(errorMessage);
            success = false;
        } else {
            committed = true;
        }
    }

    if (!committed) {
        sqlite3_exec(m_db, "ROLLBACK;", nullptr, nullptr, nullptr);
    }

    if (success) {
        LOG_INFO_FMT(
            "CharacterInventoryDatabase",
            "Saved inventory for account %u (%zu slot stack(s))",
            accountId,
            ownership.items.size());
    }

    return success;
}

} // namespace Character
} // namespace Game
