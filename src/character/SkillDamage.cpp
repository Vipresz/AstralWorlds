#include "character/SkillDamage.h"

#include "Logger.h"
#include "character/VitalsComponent.h"
#include "render/components/TransformComponent.h"

#include <glm/glm.hpp>
#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

using Engine::Platform::Render::Components::TransformComponent;
using EngineLogging::Logger;

namespace Game {
namespace Character {

namespace {

glm::vec3 PlanarForwardFromYaw(float yawDegrees) {
    const float radians = glm::radians(yawDegrees);
    return glm::normalize(glm::vec3(-std::sin(radians), 0.0f, -std::cos(radians)));
}

float PlanarDistance(const glm::vec3& a, const glm::vec3& b) {
    const float dx = a.x - b.x;
    const float dz = a.z - b.z;
    return std::sqrt(dx * dx + dz * dz);
}

bool IsInCone(const glm::vec3& origin,
              const glm::vec3& forward,
              const glm::vec3& target,
              float range,
              float angleDegrees) {
    const float distance = PlanarDistance(origin, target);
    if (distance > range || distance < 0.01f) {
        return false;
    }

    glm::vec3 toTarget = target - origin;
    toTarget.y = 0.0f;
    const float cosHalf = std::cos(glm::radians(angleDegrees * 0.5f));
    return glm::dot(glm::normalize(toTarget), forward) >= cosHalf;
}

bool MatchesTargetFilter(SkillTargetType targetType,
                         Engine::EntityID casterEntity,
                         Engine::EntityID candidateEntity) {
    switch (targetType) {
        case SkillTargetType::Self:
            return candidateEntity == casterEntity;
        case SkillTargetType::Enemy:
        case SkillTargetType::AllInArea:
        case SkillTargetType::Ally:
        default:
            return candidateEntity != casterEntity;
    }
}

bool EntityInSkillArea(const SkillDefinition& skill,
                       const glm::vec3& casterPos,
                       float casterYaw,
                       const glm::vec3& targetPos) {
    const glm::vec3 forward = PlanarForwardFromYaw(casterYaw);

    switch (skill.area.shape) {
        case SkillAreaShape::Sphere:
            return PlanarDistance(casterPos, targetPos) <= skill.area.radius;
        case SkillAreaShape::Single: {
            const float range = skill.area.range > 0.01f ? skill.area.range : 2.0f;
            return PlanarDistance(casterPos, targetPos) <= range;
        }
        case SkillAreaShape::Cone:
        default: {
            const float range = skill.area.range > 0.01f ? skill.area.range : 2.5f;
            const float angle = skill.area.angleDegrees > 1.0f ? skill.area.angleDegrees : 60.0f;
            return IsInCone(casterPos, forward, targetPos, range, angle);
        }
    }
}

} // namespace

void ApplySkillDamage(Engine::ECS::Registry& registry,
                      Engine::EntityID casterEntity,
                      const SkillDefinition& skill,
                      std::vector<SkillHitResult>& outHits) {
    outHits.clear();
    if (skill.damage <= 0.0f) {
        return;
    }

    const TransformComponent* casterTransform = registry.GetComponent<TransformComponent>(casterEntity);
    if (!casterTransform) {
        return;
    }

    if (skill.target == SkillTargetType::Self) {
        VitalsComponent* vitals = registry.GetComponent<VitalsComponent>(casterEntity);
        if (!vitals) {
            return;
        }
        vitals->currentHealth = std::max(0.0f, vitals->currentHealth - skill.damage);
        outHits.push_back(SkillHitResult{casterEntity, 0.0f});
        return;
    }

    const glm::vec3 casterPos = casterTransform->position;
    const float casterYaw = casterTransform->GetYaw();

    SkillHitResult nearestHit{};
    float nearestDistance = std::numeric_limits<float>::max();

    const auto candidates = registry.View<VitalsComponent, TransformComponent>();
    for (Engine::EntityID entityId : candidates) {
        if (!MatchesTargetFilter(skill.target, casterEntity, entityId)) {
            continue;
        }

        const TransformComponent* targetTransform = registry.GetComponent<TransformComponent>(entityId);
        if (!targetTransform) {
            continue;
        }

        if (!EntityInSkillArea(skill, casterPos, casterYaw, targetTransform->position)) {
            continue;
        }

        const float distance = PlanarDistance(casterPos, targetTransform->position);
        VitalsComponent* vitals = registry.GetComponent<VitalsComponent>(entityId);
        if (!vitals) {
            continue;
        }

        if (skill.area.shape == SkillAreaShape::Single) {
            if (distance < nearestDistance) {
                nearestDistance = distance;
                nearestHit.entity = entityId;
                nearestHit.distance = distance;
            }
            continue;
        }

        vitals->currentHealth = std::max(0.0f, vitals->currentHealth - skill.damage);
        outHits.push_back(SkillHitResult{entityId, distance});
    }

    if (skill.area.shape == SkillAreaShape::Single && nearestHit.entity != Engine::INVALID_ENTITY) {
        if (VitalsComponent* vitals = registry.GetComponent<VitalsComponent>(nearestHit.entity)) {
            vitals->currentHealth = std::max(0.0f, vitals->currentHealth - skill.damage);
            outHits.push_back(nearestHit);
        }
    }

    if (!outHits.empty()) {
        LOG_INFO_FMT("SkillDamage",
                     "Skill '%s' hit %zu target(s) for %.1f damage",
                     skill.id.c_str(),
                     outHits.size(),
                     skill.damage);
    }
}

} // namespace Character
} // namespace Game
