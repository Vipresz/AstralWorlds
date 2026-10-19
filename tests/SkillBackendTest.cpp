#include <gtest/gtest.h>

#include "GameTestSupport.h"
#include "character/CharacterBodyAssetCache.h"
#include "character/ItemCatalog.h"
#include "character/SkillActivator.h"
#include "character/SkillCatalog.h"
#include "character/SkillClipCache.h"
#include "character/SkillComponent.h"
#include "character/SkillDamage.h"
#include "character/SkillDefinition.h"
#include "character/VitalsComponent.h"
#include "core/ecs/Registry.h"
#include "render/components/AnimationComponent.h"
#include "render/components/TransformComponent.h"
#include "systems/character/SkillExecutionSystem.h"

#include <cmath>
#include <string>
#include <vector>

using Engine::ECS::Registry;
using Engine::Platform::Render::Components::AnimationComponent;
using Engine::Platform::Render::Components::TransformComponent;
using Game::Character::ApplySkillDamage;
using Game::Character::CharacterBodyAssetCache;
using Game::Character::ItemCatalog;
using Game::Character::SkillActivator;
using Game::Character::SkillAreaShape;
using Game::Character::SkillAreaShapeFromString;
using Game::Character::SkillAreaShapeToString;
using Game::Character::SkillCatalog;
using Game::Character::SkillClipCache;
using Game::Character::SkillComponent;
using Game::Character::SkillDefinition;
using Game::Character::SkillExecutionSystem;
using Game::Character::SkillHitResult;
using Game::Character::SkillTargetType;
using Game::Character::SkillTargetTypeFromString;
using Game::Character::SkillTargetTypeToString;
using Game::Character::VitalsComponent;

TEST(SkillDefinitionTest, ParsesTargetAndAreaEnumsCaseInsensitively) {
    EXPECT_EQ(SkillTargetTypeFromString("self"), SkillTargetType::Self);
    EXPECT_EQ(SkillTargetTypeFromString("SELF"), SkillTargetType::Self);
    EXPECT_EQ(SkillTargetTypeFromString("ally"), SkillTargetType::Ally);
    EXPECT_EQ(SkillTargetTypeFromString("all_in_area"), SkillTargetType::AllInArea);
    EXPECT_EQ(SkillTargetTypeFromString("ALL"), SkillTargetType::AllInArea);
    EXPECT_EQ(SkillTargetTypeFromString("enemy"), SkillTargetType::Enemy);
    EXPECT_EQ(SkillTargetTypeFromString("unknown_value"), SkillTargetType::Enemy);

    EXPECT_STREQ(SkillTargetTypeToString(SkillTargetType::Self), "self");
    EXPECT_STREQ(SkillTargetTypeToString(SkillTargetType::Ally), "ally");
    EXPECT_STREQ(SkillTargetTypeToString(SkillTargetType::AllInArea), "all_in_area");
    EXPECT_STREQ(SkillTargetTypeToString(SkillTargetType::Enemy), "enemy");

    EXPECT_EQ(SkillAreaShapeFromString("single"), SkillAreaShape::Single);
    EXPECT_EQ(SkillAreaShapeFromString("sphere"), SkillAreaShape::Sphere);
    EXPECT_EQ(SkillAreaShapeFromString("CIRCLE"), SkillAreaShape::Sphere);
    EXPECT_EQ(SkillAreaShapeFromString("cone"), SkillAreaShape::Cone);
    EXPECT_EQ(SkillAreaShapeFromString("other"), SkillAreaShape::Cone);

    EXPECT_STREQ(SkillAreaShapeToString(SkillAreaShape::Single), "single");
    EXPECT_STREQ(SkillAreaShapeToString(SkillAreaShape::Sphere), "sphere");
    EXPECT_STREQ(SkillAreaShapeToString(SkillAreaShape::Cone), "cone");
}

TEST(SkillCatalogTest, LoadsFighterSkillsAndResolvesByIdAndHotkey) {
    ASSERT_TRUE(Game::Tests::SetWorkingDirectoryToTestRuntime());
    const std::string catalogPath = Game::Tests::RequireAsset("character/skills/catalog.json");

    SkillCatalog catalog;
    ASSERT_TRUE(catalog.Load(catalogPath.c_str()));
    EXPECT_GE(catalog.GetAll().size(), 3u);

    const SkillDefinition* punch = catalog.Find("punch");
    ASSERT_NE(punch, nullptr);
    EXPECT_EQ(punch->name, "Punch");
    EXPECT_EQ(punch->hotkey, "1");
    EXPECT_FLOAT_EQ(punch->damage, 12.0f);
    EXPECT_FLOAT_EQ(punch->cooldownSeconds, 2.0f);
    EXPECT_FALSE(punch->fullBody);
    EXPECT_EQ(punch->area.shape, SkillAreaShape::Cone);
    EXPECT_EQ(catalog.FindByHotkey("1"), punch);

    const SkillDefinition* kneeKick = catalog.Find("knee_kick");
    ASSERT_NE(kneeKick, nullptr);
    EXPECT_EQ(kneeKick->hotkey, "2");
    EXPECT_FLOAT_EQ(kneeKick->damage, 18.0f);
    EXPECT_FALSE(kneeKick->fullBody);
    EXPECT_EQ(catalog.FindByHotkey("2"), kneeKick);

    const SkillDefinition* roundhouse = catalog.Find("roundhouse");
    ASSERT_NE(roundhouse, nullptr);
    EXPECT_EQ(roundhouse->hotkey, "3");
    EXPECT_FLOAT_EQ(roundhouse->damage, 24.0f);
    EXPECT_TRUE(roundhouse->fullBody);
    EXPECT_EQ(roundhouse->area.shape, SkillAreaShape::Sphere);
    EXPECT_FLOAT_EQ(roundhouse->area.radius, 2.5f);
    EXPECT_EQ(catalog.FindByHotkey("3"), roundhouse);

    EXPECT_EQ(catalog.Find("nonexistent_skill"), nullptr);
    EXPECT_EQ(catalog.FindByHotkey("9"), nullptr);
    EXPECT_EQ(catalog.FindByHotkey(""), nullptr);
}

TEST(SkillComponentTest, CooldownStartAndTickLifecycle) {
    SkillComponent comp;
    EXPECT_FALSE(comp.IsOnCooldown("punch"));
    EXPECT_FLOAT_EQ(comp.GetCooldownRemaining("punch"), 0.0f);
    EXPECT_FLOAT_EQ(comp.GetCooldownTotal("punch"), 0.0f);

    comp.StartCooldown("", 2.0f);
    comp.StartCooldown("punch", 0.0f);
    EXPECT_FALSE(comp.IsOnCooldown("punch"));

    comp.StartCooldown("punch", 2.0f);
    EXPECT_TRUE(comp.IsOnCooldown("punch"));
    EXPECT_FLOAT_EQ(comp.GetCooldownTotal("punch"), 2.0f);
    EXPECT_FLOAT_EQ(comp.GetCooldownRemaining("punch"), 2.0f);

    comp.TickCooldowns(0.75f);
    EXPECT_TRUE(comp.IsOnCooldown("punch"));
    EXPECT_NEAR(comp.GetCooldownRemaining("punch"), 1.25f, 1e-4f);

    comp.TickCooldowns(2.0f);
    EXPECT_FALSE(comp.IsOnCooldown("punch"));
    EXPECT_FLOAT_EQ(comp.GetCooldownRemaining("punch"), 0.0f);
    EXPECT_FLOAT_EQ(comp.GetCooldownTotal("punch"), 2.0f);
}

TEST(SkillDamageTest, ConeHitsTargetsInFrontAndIgnoresBehindOrOutOfRange) {
    Registry registry;
    const Engine::EntityID caster = registry.CreateEntity().id;
    auto& casterTransform = registry.AddComponent<TransformComponent>(caster);
    casterTransform.SetPosition(0.0f, 0.0f, 0.0f);
    casterTransform.SetYaw(0.0f); // Forward is (0, 0, -1)
    registry.AddComponent<VitalsComponent>(caster);

    const Engine::EntityID frontTarget = registry.CreateEntity().id;
    registry.AddComponent<TransformComponent>(frontTarget).SetPosition(0.0f, 0.0f, -1.5f);
    registry.AddComponent<VitalsComponent>(frontTarget).currentHealth = 100.0f;

    const Engine::EntityID behindTarget = registry.CreateEntity().id;
    registry.AddComponent<TransformComponent>(behindTarget).SetPosition(0.0f, 0.0f, 1.5f);
    registry.AddComponent<VitalsComponent>(behindTarget).currentHealth = 100.0f;

    const Engine::EntityID farTarget = registry.CreateEntity().id;
    registry.AddComponent<TransformComponent>(farTarget).SetPosition(0.0f, 0.0f, -5.0f);
    registry.AddComponent<VitalsComponent>(farTarget).currentHealth = 100.0f;

    SkillDefinition punch;
    punch.id = "punch";
    punch.damage = 15.0f;
    punch.target = SkillTargetType::Enemy;
    punch.area.shape = SkillAreaShape::Cone;
    punch.area.range = 2.2f;
    punch.area.angleDegrees = 70.0f;

    std::vector<SkillHitResult> hits;
    ApplySkillDamage(registry, caster, punch, hits);

    ASSERT_EQ(hits.size(), 1u);
    EXPECT_EQ(hits[0].entity, frontTarget);
    EXPECT_FLOAT_EQ(registry.GetComponent<VitalsComponent>(frontTarget)->currentHealth, 85.0f);
    EXPECT_FLOAT_EQ(registry.GetComponent<VitalsComponent>(behindTarget)->currentHealth, 100.0f);
    EXPECT_FLOAT_EQ(registry.GetComponent<VitalsComponent>(farTarget)->currentHealth, 100.0f);
    EXPECT_FLOAT_EQ(registry.GetComponent<VitalsComponent>(caster)->currentHealth, 100.0f);
}

TEST(SkillDamageTest, SphereHitsAllSurroundingTargetsWithinRadius) {
    Registry registry;
    const Engine::EntityID caster = registry.CreateEntity().id;
    registry.AddComponent<TransformComponent>(caster).SetPosition(5.0f, 0.0f, 5.0f);
    registry.AddComponent<VitalsComponent>(caster);

    const Engine::EntityID nearA = registry.CreateEntity().id;
    registry.AddComponent<TransformComponent>(nearA).SetPosition(6.5f, 0.0f, 5.0f);
    registry.AddComponent<VitalsComponent>(nearA).currentHealth = 50.0f;

    const Engine::EntityID nearB = registry.CreateEntity().id;
    registry.AddComponent<TransformComponent>(nearB).SetPosition(5.0f, 0.0f, 7.0f);
    registry.AddComponent<VitalsComponent>(nearB).currentHealth = 20.0f;

    const Engine::EntityID outside = registry.CreateEntity().id;
    registry.AddComponent<TransformComponent>(outside).SetPosition(9.0f, 0.0f, 5.0f);
    registry.AddComponent<VitalsComponent>(outside).currentHealth = 50.0f;

    SkillDefinition roundhouse;
    roundhouse.id = "roundhouse";
    roundhouse.damage = 25.0f;
    roundhouse.target = SkillTargetType::Enemy;
    roundhouse.area.shape = SkillAreaShape::Sphere;
    roundhouse.area.radius = 2.5f;

    std::vector<SkillHitResult> hits;
    ApplySkillDamage(registry, caster, roundhouse, hits);

    EXPECT_EQ(hits.size(), 2u);
    EXPECT_FLOAT_EQ(registry.GetComponent<VitalsComponent>(nearA)->currentHealth, 25.0f);
    // Clamped at 0.0f
    EXPECT_FLOAT_EQ(registry.GetComponent<VitalsComponent>(nearB)->currentHealth, 0.0f);
    EXPECT_FLOAT_EQ(registry.GetComponent<VitalsComponent>(outside)->currentHealth, 50.0f);
}

TEST(SkillDamageTest, SingleTargetHitsOnlyNearestAndSelfTargetHitsCaster) {
    Registry registry;
    const Engine::EntityID caster = registry.CreateEntity().id;
    registry.AddComponent<TransformComponent>(caster).SetPosition(0.0f, 0.0f, 0.0f);
    registry.AddComponent<VitalsComponent>(caster).currentHealth = 100.0f;

    const Engine::EntityID closer = registry.CreateEntity().id;
    registry.AddComponent<TransformComponent>(closer).SetPosition(1.0f, 0.0f, 0.0f);
    registry.AddComponent<VitalsComponent>(closer).currentHealth = 100.0f;

    const Engine::EntityID farther = registry.CreateEntity().id;
    registry.AddComponent<TransformComponent>(farther).SetPosition(1.8f, 0.0f, 0.0f);
    registry.AddComponent<VitalsComponent>(farther).currentHealth = 100.0f;

    SkillDefinition singleStrike;
    singleStrike.id = "jab";
    singleStrike.damage = 30.0f;
    singleStrike.target = SkillTargetType::Enemy;
    singleStrike.area.shape = SkillAreaShape::Single;
    singleStrike.area.range = 2.5f;

    std::vector<SkillHitResult> hits;
    ApplySkillDamage(registry, caster, singleStrike, hits);

    ASSERT_EQ(hits.size(), 1u);
    EXPECT_EQ(hits[0].entity, closer);
    EXPECT_FLOAT_EQ(registry.GetComponent<VitalsComponent>(closer)->currentHealth, 70.0f);
    EXPECT_FLOAT_EQ(registry.GetComponent<VitalsComponent>(farther)->currentHealth, 100.0f);

    SkillDefinition selfSkill = singleStrike;
    selfSkill.target = SkillTargetType::Self;
    selfSkill.damage = 10.0f;
    ApplySkillDamage(registry, caster, selfSkill, hits);

    ASSERT_EQ(hits.size(), 1u);
    EXPECT_EQ(hits[0].entity, caster);
    EXPECT_FLOAT_EQ(registry.GetComponent<VitalsComponent>(caster)->currentHealth, 90.0f);
}

TEST(SkillExecutionSystemTest, ActivatesExecutesAppliesDelayedDamageAndFinishes) {
    ASSERT_TRUE(Game::Tests::SetWorkingDirectoryToTestRuntime());

    SkillCatalog catalog;
    ASSERT_TRUE(catalog.Load(Game::Tests::RequireAsset("character/skills/catalog.json").c_str()));
    const SkillDefinition* punch = catalog.Find("punch");
    ASSERT_NE(punch, nullptr);

    ItemCatalog itemCatalog;
    ASSERT_TRUE(itemCatalog.Load(Game::Tests::RequireAsset("tests/fixtures/e2e_items.json").c_str()));
    const auto* defaultBody = itemCatalog.DefaultBody();
    ASSERT_NE(defaultBody, nullptr);

    auto bodyAsset = CharacterBodyAssetCache::Instance().GetOrLoad(defaultBody->id, defaultBody->modelPath);
    ASSERT_TRUE(bodyAsset);
    ASSERT_TRUE(bodyAsset->skeleton);

    Registry registry;
    const Engine::EntityID caster = registry.CreateEntity().id;
    auto& casterTransform = registry.AddComponent<TransformComponent>(caster);
    casterTransform.SetPosition(0.0f, 0.0f, 0.0f);
    casterTransform.SetYaw(0.0f);
    registry.AddComponent<VitalsComponent>(caster);
    auto& anim = registry.AddComponent<AnimationComponent>(caster);
    anim.skeleton = bodyAsset->skeleton.get();

    const Engine::EntityID dummy = registry.CreateEntity().id;
    registry.AddComponent<TransformComponent>(dummy).SetPosition(0.0f, 0.0f, -1.2f);
    registry.AddComponent<VitalsComponent>(dummy).currentHealth = 100.0f;

    SkillClipCache::Instance().Clear();
    ASSERT_TRUE(SkillActivator::TryActivate(registry, caster, catalog, *punch));

    // Cannot re-activate while executing
    EXPECT_FALSE(SkillActivator::TryActivate(registry, caster, catalog, *punch));

    const SkillComponent* skillComp = registry.GetComponent<SkillComponent>(caster);
    ASSERT_NE(skillComp, nullptr);
    EXPECT_TRUE(skillComp->isExecuting);
    EXPECT_EQ(skillComp->activeSkillId, "punch");
    EXPECT_TRUE(skillComp->IsOnCooldown("punch"));
    EXPECT_TRUE(anim.skillActive);
    EXPECT_NE(anim.skillAnimation, nullptr);

    SkillExecutionSystem executionSystem(&catalog);
    executionSystem.Initialize(registry);

    // Tick before damage delay (0.35s)
    executionSystem.Update(0.20f, registry, 1);
    EXPECT_TRUE(skillComp->isExecuting);
    EXPECT_FALSE(skillComp->damageApplied);
    EXPECT_FLOAT_EQ(registry.GetComponent<VitalsComponent>(dummy)->currentHealth, 100.0f);

    // Tick past damage delay (total 0.40s >= 0.35s)
    executionSystem.Update(0.20f, registry, 2);
    EXPECT_TRUE(skillComp->isExecuting);
    EXPECT_TRUE(skillComp->damageApplied);
    EXPECT_FLOAT_EQ(registry.GetComponent<VitalsComponent>(dummy)->currentHealth,
                    100.0f - punch->damage);

    // Tick past skill duration (total 1.10s >= 1.0s)
    executionSystem.Update(0.70f, registry, 3);
    EXPECT_FALSE(skillComp->isExecuting);
    EXPECT_TRUE(skillComp->activeSkillId.empty());
    EXPECT_FALSE(anim.skillActive);
    EXPECT_EQ(anim.skillAnimation, nullptr);

    // Still on cooldown (2.0s total, 1.10s elapsed)
    EXPECT_TRUE(skillComp->IsOnCooldown("punch"));
    EXPECT_FALSE(SkillActivator::TryActivate(registry, caster, catalog, *punch));

    // Tick remaining cooldown
    executionSystem.Update(1.0f, registry, 4);
    EXPECT_FALSE(skillComp->IsOnCooldown("punch"));
    EXPECT_TRUE(SkillActivator::TryActivate(registry, caster, catalog, *punch));
}

namespace {

std::string g_overlayBoneName;

bool OverlayBoneMatches(const std::string& nodeName) {
    return nodeName == g_overlayBoneName;
}

} // namespace

TEST(SkeletonLayeredTransformsTest, MergesOverlayBoneTransformsWhenPredicateMatches) {
    ASSERT_TRUE(Game::Tests::SetWorkingDirectoryToTestRuntime());

    ItemCatalog itemCatalog;
    ASSERT_TRUE(itemCatalog.Load(Game::Tests::RequireAsset("tests/fixtures/e2e_items.json").c_str()));
    const auto* defaultBody = itemCatalog.DefaultBody();
    ASSERT_NE(defaultBody, nullptr);

    auto bodyAsset = CharacterBodyAssetCache::Instance().GetOrLoad(defaultBody->id, defaultBody->modelPath);
    ASSERT_TRUE(bodyAsset);
    ASSERT_TRUE(bodyAsset->skeleton);

    Engine::Platform::Render::Skeleton skeleton = bodyAsset->skeleton->Clone();
    ASSERT_TRUE(skeleton.IsLoaded());
    ASSERT_GT(skeleton.GetBoneCount(), 0u);

    const std::string targetBoneName = skeleton.GetBones().begin()->first;
    std::map<std::string, glm::mat4> primaryTransforms;
    std::map<std::string, glm::mat4> overlayTransforms;
    for (const auto& entry : skeleton.GetBones()) {
        primaryTransforms[entry.first] = glm::mat4(1.0f);
        overlayTransforms[entry.first] = glm::mat4(2.0f);
    }

    g_overlayBoneName = targetBoneName;
    skeleton.UpdateBoneTransformsLayered(
        primaryTransforms,
        overlayTransforms,
        &OverlayBoneMatches);
    EXPECT_FALSE(skeleton.IsBindPoseActive());

    std::vector<glm::mat4> layeredMatrices;
    skeleton.GetFinalBoneMatrices(layeredMatrices);
    EXPECT_EQ(layeredMatrices.size(), skeleton.GetMaxBoneCount());
}
