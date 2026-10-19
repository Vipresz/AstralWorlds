#pragma once

#include "character/SkillDefinition.h"

#include "core/Types.h"
#include "core/ecs/Registry.h"

namespace Game {
namespace Character {

struct SkillHitResult {
    Engine::EntityID entity = Engine::INVALID_ENTITY;
    float distance = 0.0f;
};

// Resolves skill area/target rules and applies damage to matching entities.
void ApplySkillDamage(Engine::ECS::Registry& registry,
                      Engine::EntityID casterEntity,
                      const SkillDefinition& skill,
                      std::vector<SkillHitResult>& outHits);

} // namespace Character
} // namespace Game
