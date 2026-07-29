#include <gtest/gtest.h>

#include "character/CharacterInventoryDatabase.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <string>

using Game::Character::CharacterInventoryDatabase;

namespace {

std::string TempDatabasePath() {
    const auto path =
        std::filesystem::temp_directory_path() /
        ("character_inventory_test_" +
         std::to_string(
             std::chrono::steady_clock::now().time_since_epoch().count()) +
         ".db");
    return path.string();
}

void DeleteIfExists(const std::string& path) {
    std::remove(path.c_str());
}

} // namespace

TEST(CharacterInventoryDatabaseTest, NewAccountLoadsAsMissing) {
    const std::string path = TempDatabasePath();
    DeleteIfExists(path);

    CharacterInventoryDatabase database(path.c_str());
    ASSERT_TRUE(database.Open());

    EXPECT_FALSE(database.HasAccount(42));

    Engine::Net::ReplicatedItemOwnership loaded;
    EXPECT_FALSE(database.LoadOwnership(42, loaded));

    database.Close();
    DeleteIfExists(path);
}

TEST(CharacterInventoryDatabaseTest, RoundTripOwnership) {
    const std::string path = TempDatabasePath();
    DeleteIfExists(path);

    CharacterInventoryDatabase database(path.c_str());
    ASSERT_TRUE(database.Open());

    Engine::Net::ReplicatedItemOwnership ownership;
    ownership.ownerUserId = 7;
    Engine::Net::ReplicatedOwnedItem maple;
    maple.slotIndex = 0;
    maple.itemId = "maple";
    maple.quantity = 10;
    ownership.items.push_back(maple);

    ASSERT_TRUE(database.SaveOwnership(7, ownership));
    EXPECT_TRUE(database.HasAccount(7));

    Engine::Net::ReplicatedItemOwnership loaded;
    ASSERT_TRUE(database.LoadOwnership(7, loaded));
    EXPECT_EQ(loaded.ownerUserId, 7u);
    ASSERT_EQ(loaded.items.size(), 1u);
    EXPECT_EQ(loaded.items[0].itemId, "maple");
    EXPECT_EQ(loaded.items[0].slotIndex, 0u);
    EXPECT_EQ(loaded.items[0].quantity, 10u);

    maple.quantity = 3;
    ownership.items[0] = maple;
    ASSERT_TRUE(database.SaveOwnership(7, ownership));

    loaded = Engine::Net::ReplicatedItemOwnership{};
    ASSERT_TRUE(database.LoadOwnership(7, loaded));
    ASSERT_EQ(loaded.items.size(), 1u);
    EXPECT_EQ(loaded.items[0].quantity, 3u);

    database.Close();
    DeleteIfExists(path);
}
