#include <gtest/gtest.h>

#include "GameTestSupport.h"
#include "Logger.h"
#include "character/CharacterDatabase.h"
#include "character/CharacterInstance.h"
#include "character/CharacterLoader.h"
#include "character/CharacterTypes.h"
#include "character/EquipmentRules.h"
#include "character/ItemCatalog.h"
#include "core/ecs/Registry.h"
#include "render/Model.h"
#include "render/components/MeshComponent.h"

using Engine::ECS::Registry;
using Game::Character::CharacterInstance;
using Game::Character::CharacterLoader;
using Game::Character::CharacterLoginContext;
using Game::Character::CharacterLoadout;
using Game::Character::EquipmentRules;
using Game::Character::ItemCatalog;
using Engine::Platform::Render::Components::MeshComponent;
using Engine::Platform::Render::Model;

namespace {

class E2eLoadoutDatabase : public Game::Character::ICharacterDatabase {
public:
    bool FetchLoadout(const CharacterLoginContext& /*context*/, CharacterLoadout& out) override {
        out.bodyItemId = "body_male_default";
        out.equipment.clear();
        Game::Character::EquippedItem prop;
        prop.slot = Game::Character::EquipSlot::Back;
        prop.itemId = "prop_cube";
        out.equipment.push_back(prop);
        return true;
    }
};

} // namespace

class CharacterLoadE2ETest : public ::testing::Test {
protected:
    void SetUp() override {
        EngineLogging::Logger::GetInstance().Initialize("character_load_e2e.log");
        ASSERT_TRUE(Game::Tests::SetWorkingDirectoryToTestRuntime());

        const std::string catalogPath = Game::Tests::RequireAsset("tests/fixtures/e2e_items.json");
        ASSERT_TRUE(m_catalog.Load(catalogPath.c_str())) << catalogPath;
        ASSERT_TRUE(m_rules.Load(Game::Tests::RequireAsset("character/equipment_rules.json").c_str()));
    }

    ItemCatalog m_catalog;
    EquipmentRules m_rules;
    E2eLoadoutDatabase m_database;
};

TEST_F(CharacterLoadE2ETest, LoadsObjPropMesh) {
    const std::string objPath = Game::Tests::RequireAsset("tests/fixtures/meshes/prop_cube.obj");
    if (!Game::Tests::FileExists(objPath)) {
        GTEST_SKIP() << "OBJ fixture missing from runtime directory";
    }

    Model model;
    ASSERT_NO_THROW(model.LoadModel(objPath));
    EXPECT_GT(model.GetMeshCount(), 0u);
    EXPECT_GT(model.GetHeight(), 0.01f);
}

TEST_F(CharacterLoadE2ETest, LoadsSkinnedBodyAndObjEquipment) {
    const std::string bodyPath =
        Game::Tests::RequireAsset("graphics/Models/character/animation/magic/FinalBaseMesh.fbx");
    if (!Game::Tests::FileExists(bodyPath)) {
        GTEST_SKIP() << "Character mesh assets not present in runtime directory";
    }
    if (!Game::Tests::FileExists(Game::Tests::RequireAsset("tests/fixtures/meshes/prop_cube.obj"))) {
        GTEST_SKIP() << "OBJ fixture missing from runtime directory";
    }

    Registry registry;
    CharacterInstance instance;
    const Engine::EntityID rootEntity = registry.CreateEntity().id;

    CharacterLoader loader(m_catalog, m_rules, m_database);
    CharacterLoginContext login;
    login.displayName = "e2e_loader";
    login.isOnlineLogin = false;

    ASSERT_TRUE(loader.LoginAndBuild(registry, instance, rootEntity, login));
    ASSERT_NE(instance.GetBodyModel(), nullptr);
    EXPECT_GT(instance.GetBodyModel()->GetMeshCount(), 0u);
    EXPECT_GT(instance.GetBodyModel()->GetHeight(), 0.1f);

    const MeshComponent* mesh = registry.GetComponent<MeshComponent>(rootEntity);
    ASSERT_NE(mesh, nullptr);
    EXPECT_NE(mesh->model, nullptr);
    EXPECT_EQ(mesh->model, instance.GetBodyModel());
}
