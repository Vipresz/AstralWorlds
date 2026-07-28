#include "GameCameraObstruction.h"

#include "character/CharacterReplicationComponent.h"
#include "character/EquipmentAttachmentComponent.h"
#include "render/Model.h"
#include "render/components/AnimationComponent.h"
#include "render/components/MeshComponent.h"
#include "render/components/TransformComponent.h"

#include <glm/glm.hpp>
#include <limits>

namespace Game {

using Character::CharacterReplicationComponent;
using Character::EquipmentAttachmentComponent;

bool GameCameraObstruction::QuerySegment(const glm::vec3& from, const glm::vec3& to,
                                         Engine::EntityID skipEntityId,
                                         float& outHitDistance) const {
    if (!m_registry || (m_inputBlocked && m_inputBlocked())) {
        return false;
    }

    const glm::vec3 segment = to - from;
    const float maxDistance = glm::length(segment);
    if (maxDistance < 1e-4f) {
        return false;
    }

    const glm::vec3 dir = segment / maxDistance;
    float closest = maxDistance;
    bool hit = false;

    using Engine::Platform::Render::Components::AnimationComponent;
    using Engine::Platform::Render::Components::MeshComponent;
    using Engine::Platform::Render::Components::TransformComponent;

    const auto entities = m_registry->View<MeshComponent, TransformComponent>();
    for (const Engine::EntityID entityId : entities) {
        if (entityId == skipEntityId) {
            continue;
        }
        // Characters (local equipment pieces and other players) should not block the camera.
        if (m_registry->HasComponent<AnimationComponent>(entityId) ||
            m_registry->HasComponent<EquipmentAttachmentComponent>(entityId) ||
            m_registry->HasComponent<CharacterReplicationComponent>(entityId)) {
            continue;
        }

        const MeshComponent* meshComp = m_registry->GetComponent<MeshComponent>(entityId);
        const TransformComponent* transformComp = m_registry->GetComponent<TransformComponent>(entityId);
        if (!meshComp || !transformComp || !meshComp->IsLoaded() || !meshComp->model) {
            continue;
        }

        float t = 0.0f;
        if (meshComp->model->RaycastWorld(transformComp->GetMatrix(), from, dir, closest, t) && t < closest) {
            closest = t;
            hit = true;
        }
    }

    if (!hit) {
        return false;
    }

    outHitDistance = closest;
    return true;
}

} // namespace Game
