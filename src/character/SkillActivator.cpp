#include "character/SkillActivator.h"

#include "Logger.h"
#include "character/SkillClipCache.h"
#include "character/VitalsComponent.h"
#include "render/Animation.h"
#include "render/components/AnimationComponent.h"

using Engine::Platform::Render::Animation;
using Engine::Platform::Render::Components::AnimationComponent;
using EngineLogging::Logger;

namespace Game {
namespace Character {

bool SkillActivator::TryActivate(Engine::ECS::Registry& registry,
                                 Engine::EntityID entityId,
                                 const SkillCatalog& catalog,
                                 const SkillDefinition& skill) {
    (void)catalog;

    if (entityId == Engine::INVALID_ENTITY || skill.id.empty()) {
        return false;
    }

    SkillComponent* skillComp = registry.GetComponent<SkillComponent>(entityId);
    if (!skillComp) {
        skillComp = &registry.AddComponent<SkillComponent>(entityId);
    }
    if (skillComp->isExecuting) {
        return false;
    }
    if (skillComp->IsOnCooldown(skill.id)) {
        return false;
    }

    AnimationComponent* animComp = registry.GetComponent<AnimationComponent>(entityId);
    if (!animComp || !animComp->skeleton) {
        LOG_WARNING("SkillActivator", "Cannot activate skill without animation component");
        return false;
    }

    std::shared_ptr<Animation> clip = SkillClipCache::Instance().GetOrLoad(skill.animationPath);
    if (!clip || !clip->IsLoaded()) {
        LOG_WARNING_FMT("SkillActivator", "Skill '%s' animation unavailable", skill.id.c_str());
        return false;
    }

    skillComp->isExecuting = true;
    skillComp->activeSkillId = skill.id;
    skillComp->elapsedSeconds = 0.0f;
    skillComp->durationSeconds = skill.durationSeconds;
    skillComp->damageApplied = false;

    if (skill.cooldownSeconds > 0.0f) {
        skillComp->StartCooldown(skill.id, skill.cooldownSeconds);
    }

    animComp->skillActive = true;
    animComp->skillFullBody = skill.fullBody;
    animComp->skillAnimation = clip.get();
    animComp->skillAnimationTime = 0.0f;
    animComp->skillDurationSeconds = skill.durationSeconds;

    LOG_INFO_FMT("SkillActivator", "Activated skill '%s' (%.2fs, %s body)",
                 skill.name.c_str(),
                 skill.durationSeconds,
                 skill.fullBody ? "full" : "upper");
    return true;
}

} // namespace Character
} // namespace Game
