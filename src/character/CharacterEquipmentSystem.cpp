#include "character/CharacterEquipmentSystem.h"

#include "core/Types.h"
#include "core/ecs/Registry.h"
#include "character/EquipmentAttachmentComponent.h"
#include "render/components/TransformComponent.h"

#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

using Engine::ECS::Registry;
using Engine::Platform::Render::Components::TransformComponent;

namespace Game {
namespace Character {

namespace {

glm::vec3 RotateOffsetByYaw(const glm::vec3& offset, float yawDegrees) {
    const float yawRad = glm::radians(yawDegrees);
    const float cosYaw = std::cos(yawRad);
    const float sinYaw = std::sin(yawRad);
    return glm::vec3(
        offset.x * cosYaw + offset.z * sinYaw,
        offset.y,
        -offset.x * sinYaw + offset.z * cosYaw);
}

} // namespace

CharacterEquipmentSystem::CharacterEquipmentSystem() {
    SetPriority(24);
}

void CharacterEquipmentSystem::Update(float /*deltaTime*/, Registry& registry, uint64_t /*tickNumber*/) {
    auto entities = registry.View<EquipmentAttachmentComponent, TransformComponent>();
    for (Engine::EntityID entityId : entities) {
        auto* attachment = registry.GetComponent<EquipmentAttachmentComponent>(entityId);
        auto* childTransform = registry.GetComponent<TransformComponent>(entityId);
        if (!attachment || !childTransform || attachment->parentEntity == Engine::INVALID_ENTITY) {
            continue;
        }

        const TransformComponent* parentTransform =
            registry.GetComponent<TransformComponent>(attachment->parentEntity);
        if (!parentTransform) {
            continue;
        }

        glm::vec3 worldOffset = attachment->localOffset;
        if (attachment->inheritParentYaw) {
            worldOffset = RotateOffsetByYaw(worldOffset, parentTransform->rotation.y);
        }

        const float parentUniformScale = parentTransform->scale.x;
        if (attachment->inheritParentScale) {
            worldOffset *= parentUniformScale;
        }

        childTransform->position = parentTransform->position + worldOffset;
        childTransform->rotation = parentTransform->rotation + attachment->localRotationDegrees;

        if (attachment->inheritParentScale) {
            childTransform->scale = parentTransform->scale * attachment->localScale;
        } else {
            childTransform->scale = attachment->localScale;
        }
    }
}

} // namespace Character
} // namespace Game
