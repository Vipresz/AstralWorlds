#include <gtest/gtest.h>

#include "GameTestSupport.h"
#include "character/CharacterBodyAssetCache.h"
#include "character/ItemCatalog.h"

using Game::Character::CharacterBodyAssetCache;
using Game::Character::ItemCatalog;

TEST(CharacterBodyAssetCacheTest, LoadsDefaultBodyOnce) {
    ItemCatalog catalog;
    ASSERT_TRUE(catalog.Load(Game::Tests::RequireAsset("tests/fixtures/e2e_items.json").c_str()));

    const auto* body = catalog.DefaultBody();
    ASSERT_NE(body, nullptr);
    ASSERT_FALSE(body->modelPath.empty());

    CharacterBodyAssetCache::Instance().Clear();

    auto first = CharacterBodyAssetCache::Instance().GetOrLoad(body->id, body->modelPath);
    auto second = CharacterBodyAssetCache::Instance().GetOrLoad(body->id, body->modelPath);

    ASSERT_TRUE(first);
    ASSERT_TRUE(second);
    EXPECT_EQ(first.get(), second.get());
    EXPECT_GE(first->height, 0.01f);
    EXPECT_TRUE(first->bodyModel);
    EXPECT_TRUE(first->skeleton);
    EXPECT_EQ(CharacterBodyAssetCache::Instance().EntryCount(), 1u);
}
