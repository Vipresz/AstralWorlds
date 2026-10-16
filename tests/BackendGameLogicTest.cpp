#include <gtest/gtest.h>

#include "GameNetworkConfig.h"
#include "GameTestSupport.h"
#include "ManifestTradeConfig.h"
#include "WorldSceneBootstrap.h"
#include "character/CharacterEquipmentSystem.h"
#include "character/CharacterReplication.h"
#include "character/CharacterReplicationComponent.h"
#include "character/CharacterStats.h"
#include "character/CharacterTypes.h"
#include "character/EquipmentAttachmentComponent.h"
#include "character/EquipmentRules.h"
#include "character/GameItemCatalog.h"
#include "character/NameplateComponent.h"
#include "character/SpeechBubbleComponent.h"
#include "character/VitalsComponent.h"
#include "character/VitalsReplication.h"
#include "core/ecs/Registry.h"
#include "core/ecs/components/PlayerItemOwnershipComponent.h"
#include "network/systems/ReplicationSystem.h"
#include "render/components/TransformComponent.h"
#include "systems/crafting/CraftingSystem.h"
#include "systems/economy/ResourceFlow.h"
#include "systems/inventory/InventoryBagLog.h"
#include "systems/inventory/InventoryComponent.h"
#include "systems/inventory/InventoryInteraction.h"
#include "systems/inventory/InventoryOwnershipSync.h"

#include <string>
#include <vector>

using Engine::ECS::PlayerItemOwnershipComponent;
using Engine::ECS::Registry;
using Engine::Platform::Networking::Systems::ReplicationSystem;
using Engine::Platform::Render::Components::TransformComponent;
using Engine::Systems::Inventory::InventoryComponent;
using Engine::Systems::Inventory::Item;
using Engine::Systems::Inventory::ItemType;
using Game::GameNetworkConfig;
using Game::Character::AttachNameplate;
using Game::Character::CharacterEquipmentSystem;
using Game::Character::CharacterLoadout;
using Game::Character::CharacterReplicationComponent;
using Game::Character::CharacterStats;
using Game::Character::EquipmentAttachmentComponent;
using Game::Character::EquipmentRules;
using Game::Character::EquipSlot;
using Game::Character::EquipSlotFromString;
using Game::Character::EquipSlotToString;
using Game::Character::EquippedItem;
using Game::Character::FromReplicatedLoadout;
using Game::Character::FromReplicatedVitals;
using Game::Character::GameItemCatalog;
using Game::Character::LoadCharacterStats;
using Game::Character::LoadoutInventoryEntry;
using Game::Character::NameplateComponent;
using Game::Character::RegisterLoadoutReplication;
using Game::Character::RegisterVitalsReplication;
using Game::Character::ShowSpeechBubble;
using Game::Character::SpeechBubbleComponent;
using Game::Character::ToReplicatedLoadout;
using Game::Character::ToReplicatedVitals;
using Game::Character::UpdateSpeechBubbles;
using Game::Character::VitalsComponent;
using Game::Inventory::ApplyCombinedUseAtSlots;
using Game::Inventory::ApplyDropAtSlot;
using Game::Inventory::ApplyLoadoutOwnership;
using Game::Inventory::ApplyUseAtSlot;
using Game::Inventory::BuildOwnershipFromInventory;
using Game::Inventory::DescribeInventorySlot;
using Game::Inventory::SyncInventoryFromOwnership;
using Game::UI::LoadManifestTradeConfig;
using Game::UI::ManifestTradeConfig;

TEST(GameItemCatalogTest, LoadsItemsAndProvidesStackCapsAndUsability) {
    ASSERT_TRUE(Game::Tests::SetWorkingDirectoryToTestRuntime());
    GameItemCatalog catalog;
    ASSERT_TRUE(catalog.Load(Game::Tests::RequireAsset("character/game_items.json").c_str()));
    EXPECT_GE(catalog.Items().size(), 20u);

    const auto* maple = catalog.Find("maple");
    ASSERT_NE(maple, nullptr);
    EXPECT_EQ(maple->numericId, 2001u);
    EXPECT_EQ(maple->displayName, "Maple");
    EXPECT_EQ(maple->itemType, ItemType::Resource);
    EXPECT_EQ(catalog.FindByNumericId(2001u), maple);
    ASSERT_NE(catalog.FindItemIdByNumericId(2001u), nullptr);
    EXPECT_EQ(*catalog.FindItemIdByNumericId(2001u), "maple");

    EXPECT_EQ(catalog.GetStackCap("maple"), Game::Character::kDefaultResourceStackCap);
    EXPECT_EQ(catalog.GetStackCap(2001u), Game::Character::kDefaultResourceStackCap);
    EXPECT_EQ(GameItemCatalog::DefaultStackCapForType(ItemType::Weapon),
              Game::Character::kUniqueItemStackCap);
    EXPECT_EQ(GameItemCatalog::DefaultStackCapForType(ItemType::Armor),
              Game::Character::kUniqueItemStackCap);
    EXPECT_EQ(GameItemCatalog::DefaultStackCapForType(ItemType::Tool),
              Game::Character::kUniqueItemStackCap);

    EXPECT_TRUE(catalog.CanUseItem("maple"));
    EXPECT_TRUE(catalog.CanUseItem(2001u));
    EXPECT_FALSE(catalog.CanUseItem("unknown_item_id"));
    EXPECT_FALSE(catalog.CanUseItem(999999u));

    const Item created = catalog.CreateInventoryItem("maple", 50u);
    EXPECT_EQ(created.id, 2001u);
    EXPECT_EQ(created.name, "Maple");
    EXPECT_EQ(created.quantity, Game::Character::kDefaultResourceStackCap);

    const auto tooltip = catalog.BuildTooltipInfo("maple");
    EXPECT_EQ(tooltip.name, "Maple");
    EXPECT_EQ(tooltip.typeLabel, "Resource");
    EXPECT_FALSE(tooltip.description.empty());
}

TEST(InventoryBackendTest, SyncOwnershipAndApplySlotInteractions) {
    ASSERT_TRUE(Game::Tests::SetWorkingDirectoryToTestRuntime());
    GameItemCatalog catalog;
    ASSERT_TRUE(catalog.Load(Game::Tests::RequireAsset("character/game_items.json").c_str()));

    CharacterLoadout loadout;
    loadout.inventory.push_back(LoadoutInventoryEntry{"maple", 5});
    loadout.inventory.push_back(LoadoutInventoryEntry{"silver_birch", 3});

    PlayerItemOwnershipComponent ownership;
    ApplyLoadoutOwnership(ownership, loadout, catalog, 10u);
    EXPECT_EQ(ownership.ownerUserId, 10u);
    EXPECT_EQ(ownership.CountItem("maple"), 5u);
    EXPECT_EQ(ownership.CountItem("silver_birch"), 3u);

    InventoryComponent bag(20);
    SyncInventoryFromOwnership(bag, ownership, catalog);
    EXPECT_FALSE(bag.slots[0].isEmpty);
    EXPECT_EQ(bag.slots[0].item.quantity, 5u);
    EXPECT_FALSE(bag.slots[1].isEmpty);
    EXPECT_EQ(bag.slots[1].item.quantity, 3u);

    const std::string slotDesc = DescribeInventorySlot(0, bag.slots[0].item, &catalog);
    EXPECT_NE(slotDesc.find("maple"), std::string::npos);

    // Move item between slots and rebuild ownership
    EXPECT_TRUE(bag.MoveItemBetweenSlots(1, 4));
    EXPECT_TRUE(bag.slots[1].isEmpty);
    EXPECT_FALSE(bag.slots[4].isEmpty);

    BuildOwnershipFromInventory(ownership, bag, catalog, 10u);
    EXPECT_EQ(ownership.CountItem("maple"), 5u);
    EXPECT_EQ(ownership.CountItem("silver_birch"), 3u);

    // Use item at slot 0
    EXPECT_TRUE(ApplyUseAtSlot(bag, ownership, catalog, 0, 10u));
    EXPECT_EQ(bag.slots[0].item.quantity, 4u);
    EXPECT_EQ(ownership.CountItem("maple"), 4u);

    // Combined use at slots 0 and 4
    EXPECT_TRUE(ApplyCombinedUseAtSlots(bag, ownership, catalog, 0, 4, 10u));
    EXPECT_EQ(bag.slots[0].item.quantity, 3u);
    EXPECT_EQ(bag.slots[4].item.quantity, 2u);
    EXPECT_EQ(ownership.CountItem("maple"), 3u);
    EXPECT_EQ(ownership.CountItem("silver_birch"), 2u);

    // Drop item at slot 4
    EXPECT_TRUE(ApplyDropAtSlot(bag, ownership, catalog, 4, 10u));
    EXPECT_TRUE(bag.slots[4].isEmpty);
    EXPECT_EQ(ownership.CountItem("silver_birch"), 0u);
}

TEST(CharacterReplicationBindingsTest, RoundTripsLoadoutAndVitalsThroughReplicationSystem) {
    ReplicationSystem replication;
    RegisterLoadoutReplication(replication);
    RegisterVitalsReplication(replication);

    Registry serverRegistry;
    const Engine::EntityID serverEntity = serverRegistry.CreateEntity().id;
    auto& serverLoadoutComp = serverRegistry.AddComponent<CharacterReplicationComponent>(serverEntity);
    serverLoadoutComp.loadout.bodyItemId = "body_male_default";
    serverLoadoutComp.loadout.equipment.push_back(EquippedItem{"prop_cube", EquipSlot::Back});

    auto& serverVitals = serverRegistry.AddComponent<VitalsComponent>(serverEntity);
    serverVitals.currentHealth = 72.5f;
    serverVitals.maxHealth = 100.0f;
    serverVitals.currentMana = 40.0f;
    serverVitals.maxMana = 80.0f;

    replication.RegisterReplicatedEntity(1, serverEntity);
    const Engine::Net::EntitySnapshot snapshot =
        replication.BuildEntitySnapshot(1, serverRegistry, 42);
    EXPECT_GE(snapshot.components.size(), 2u);

    Registry clientRegistry;
    const Engine::EntityID clientEntity = clientRegistry.CreateEntity().id;
    ReplicationSystem clientReplication;
    RegisterLoadoutReplication(clientReplication);
    RegisterVitalsReplication(clientReplication);
    clientReplication.RegisterReplicatedEntity(1, clientEntity);
    clientReplication.ApplyEntitySnapshot(snapshot, clientRegistry);

    const auto* clientLoadoutComp =
        clientRegistry.GetComponent<CharacterReplicationComponent>(clientEntity);
    ASSERT_NE(clientLoadoutComp, nullptr);
    EXPECT_EQ(clientLoadoutComp->loadout.bodyItemId, "body_male_default");
    ASSERT_EQ(clientLoadoutComp->loadout.equipment.size(), 1u);
    EXPECT_EQ(clientLoadoutComp->loadout.equipment[0].itemId, "prop_cube");
    EXPECT_EQ(clientLoadoutComp->loadout.equipment[0].slot, EquipSlot::Back);

    const auto* clientVitals = clientRegistry.GetComponent<VitalsComponent>(clientEntity);
    ASSERT_NE(clientVitals, nullptr);
    EXPECT_FLOAT_EQ(clientVitals->currentHealth, 72.5f);
    EXPECT_FLOAT_EQ(clientVitals->maxHealth, 100.0f);
    EXPECT_FLOAT_EQ(clientVitals->currentMana, 40.0f);
    EXPECT_FLOAT_EQ(clientVitals->maxMana, 80.0f);
}

TEST(CharacterEquipmentAndChatComponentsTest, UpdatesAttachmentsNameplatesAndSpeechBubbles) {
    EXPECT_EQ(EquipSlotFromString("main_hand"), EquipSlot::MainHand);
    EXPECT_EQ(EquipSlotFromString("off_hand"), EquipSlot::OffHand);
    EXPECT_STREQ(EquipSlotToString(EquipSlot::MainHand), "mainhand");

    Registry registry;
    const Engine::EntityID parent = registry.CreateEntity().id;
    auto& parentTransform = registry.AddComponent<TransformComponent>(parent);
    parentTransform.SetPosition(10.0f, 2.0f, 5.0f);
    parentTransform.SetRotation(0.0f, 0.0f, 0.0f);
    parentTransform.SetScale(2.0f);

    const Engine::EntityID child = registry.CreateEntity().id;
    registry.AddComponent<TransformComponent>(child);
    auto& attachment = registry.AddComponent<EquipmentAttachmentComponent>(child);
    attachment.parentEntity = parent;
    attachment.localOffset = glm::vec3(1.0f, 0.5f, 0.0f);
    attachment.localScale = glm::vec3(0.5f);
    attachment.inheritParentScale = true;
    attachment.inheritParentYaw = true;

    CharacterEquipmentSystem equipSystem;
    equipSystem.Update(1.0f / 60.0f, registry, 1);

    const auto* childTransform = registry.GetComponent<TransformComponent>(child);
    ASSERT_NE(childTransform, nullptr);
    EXPECT_FLOAT_EQ(childTransform->position.x, 12.0f);
    EXPECT_FLOAT_EQ(childTransform->position.y, 3.0f);
    EXPECT_FLOAT_EQ(childTransform->position.z, 5.0f);
    EXPECT_FLOAT_EQ(childTransform->scale.x, 1.0f);

    AttachNameplate(registry, parent, "TestHero", 2.2f);
    const auto* nameplate = registry.GetComponent<NameplateComponent>(parent);
    ASSERT_NE(nameplate, nullptr);
    EXPECT_TRUE(nameplate->visible);
    EXPECT_STREQ(nameplate->GetDisplayName(), "TestHero");

    ShowSpeechBubble(registry, parent, "Hello world!");
    const auto* bubble = registry.GetComponent<SpeechBubbleComponent>(parent);
    ASSERT_NE(bubble, nullptr);
    EXPECT_TRUE(bubble->IsActive());
    EXPECT_STREQ(bubble->text, "Hello world!");

    UpdateSpeechBubbles(registry, 10.0f);
    EXPECT_FALSE(bubble->IsActive());
}

TEST(ConfigAndBootstrapTest, ParsesCommandLineAndLoadsStatsAndTradeConfig) {
    ASSERT_TRUE(Game::Tests::SetWorkingDirectoryToTestRuntime());

    CharacterStats stats;
    EXPECT_TRUE(LoadCharacterStats(
        stats, Game::Tests::RequireAsset("character/character_stats.json").c_str()));
    EXPECT_GT(stats.walkSpeed, 0.0f);
    EXPECT_GE(stats.runSpeed, stats.walkSpeed);

    ManifestTradeConfig tradeConfig;
    EXPECT_TRUE(LoadManifestTradeConfig(
        tradeConfig, Game::Tests::RequireAsset("menus/manifest.json").c_str()));
    EXPECT_GT(tradeConfig.maxDistance, 0.0f);

    const char* argv[] = {
        "game",
        "--host",
        "127.0.0.1",
        "--port",
        "7788",
        "--name",
        "Tester",
        "--user",
        "alice",
        "--password",
        "secret",
        "--auth-token",
        "tok123",
        "--tls-insecure",
        "--e2e-test",
        "--e2e-seconds",
        "12",
    };
    const int argc = static_cast<int>(sizeof(argv) / sizeof(argv[0]));
    const GameNetworkConfig netConfig =
        GameNetworkConfig::ParseCommandLine(argc, const_cast<char**>(argv));
    EXPECT_EQ(netConfig.serverHost, "127.0.0.1");
    EXPECT_EQ(netConfig.serverPort, 7788u);
    EXPECT_EQ(netConfig.displayName, "Tester");
    EXPECT_EQ(netConfig.username, "alice");
    EXPECT_EQ(netConfig.password, "secret");
    EXPECT_EQ(netConfig.authToken, "tok123");
    EXPECT_TRUE(netConfig.useTls);
    EXPECT_TRUE(netConfig.tlsInsecure);
    EXPECT_TRUE(netConfig.e2eTest);
    EXPECT_EQ(netConfig.e2eRunSeconds, 12);
    EXPECT_EQ(netConfig.ResolveTlsServerName(), "localhost");
}

TEST(EconomyResourceFlowTest, NodeExtractsDepletesRespawnsAndTransfersStorage) {
    Engine::Systems::Economy::ResourceNodeComponent node;
    node.remainingAmount = 10.0f;
    node.maxAmount = 10.0f;
    node.extractionRate = 4.0f;
    node.respawnTime = 5.0f;

    Engine::Systems::Economy::ResourceFlowSystem flowSystem;
    EXPECT_FLOAT_EQ(flowSystem.ExtractFromNode(node, 1.0f), 4.0f);
    EXPECT_FALSE(node.isDepleted);
    EXPECT_FLOAT_EQ(flowSystem.ExtractFromNode(node, 2.0f), 6.0f);
    EXPECT_TRUE(node.isDepleted);
    EXPECT_FLOAT_EQ(flowSystem.ExtractFromNode(node, 1.0f), 0.0f);

    node.Update(5.0f);
    EXPECT_FALSE(node.isDepleted);
    EXPECT_FLOAT_EQ(node.remainingAmount, 10.0f);

    Engine::Systems::Economy::ResourceStorageComponent storage;
    storage.SetCapacity(1, 20.0f);
    EXPECT_TRUE(storage.Add(1, 15.0f));
    EXPECT_FALSE(storage.Add(1, 10.0f));
    EXPECT_FLOAT_EQ(storage.GetAmount(1), 15.0f);
    EXPECT_TRUE(storage.Remove(1, 5.0f));
    EXPECT_FLOAT_EQ(storage.GetAmount(1), 10.0f);
}
