#include "systems/character/SkillExecutionSystem.h"

#include "Logger.h"
#include "character/SkillComponent.h"
#include "character/SkillDamage.h"
#include "render/components/AnimationComponent.h"

#include <algorithm>
#include <vector>

using Engine::Platform::Render::Components::AnimationComponent;
using EngineLogging::Logger;

namespace Game {
namespace Character {

SkillExecutionSystem::SkillExecutionSystem(const SkillCatalog* catalog) : m_catalog(catalog) {
    // After movement/input, before animation pose is written.
    SetPriority(57);
}

void SkillExecutionSystem::Initialize(Engine::ECS::Registry& registry) {
    (void)registry;
    LOG_INFO("SkillExecutionSystem", "Skill execution system initialized");
}

void SkillExecutionSystem::Shutdown(Engine::ECS::Registry& registry) {
    (void)registry;
}

void SkillExecutionSystem::Update(float deltaTime, Engine::ECS::Registry& registry, uint64_t tickNumber) {
    (void)tickNumber;
    if (!m_catalog) {
        return;
    }

    const auto entities = registry.View<SkillComponent>();
    for (Engine::EntityID entityId : entities) {
        SkillComponent* skillComp = registry.GetComponent<SkillComponent>(entityId);
        if (!skillComp) {
            continue;
        }

        skillComp->TickCooldowns(deltaTime);

        AnimationComponent* animComp = registry.GetComponent<AnimationComponent>(entityId);
        if (!animComp || !skillComp->isExecuting) {
            continue;
        }

        skillComp->elapsedSeconds += std::max(0.0f, deltaTime);

        const SkillDefinition* skill = m_catalog->Find(skillComp->activeSkillId);
        if (skill && !skillComp->damageApplied &&
            skillComp->elapsedSeconds >= skill->damageDelaySeconds) {
            std::vector<SkillHitResult> hits;
            ApplySkillDamage(registry, entityId, *skill, hits);
            skillComp->damageApplied = true;
        }

        if (skillComp->elapsedSeconds >= skillComp->durationSeconds) {
            skillComp->isExecuting = false;
            skillComp->activeSkillId.clear();
            skillComp->elapsedSeconds = 0.0f;
            skillComp->damageApplied = false;

            animComp->skillActive = false;
            animComp->skillFullBody = false;
            animComp->skillAnimation = nullptr;
            animComp->skillAnimationTime = 0.0f;
            animComp->skillDurationSeconds = 1.0f;
        }
    }
}

} // namespace Character
} // namespace Game
