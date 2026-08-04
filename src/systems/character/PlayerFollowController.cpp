#include "systems/character/PlayerFollowController.h"

#include "Logger.h"
#include "core/ecs/Registry.h"
#include "network/systems/NetworkSystem.h"
#include "render/components/TransformComponent.h"

#include <cmath>

using Engine::Platform::Render::Components::TransformComponent;
using EngineLogging::Logger;

namespace Game {
namespace Character {

void PlayerFollowController::StartFollow(Engine::Net::UserId targetUserId,
                                         const std::string& displayName) {
    if (targetUserId == Engine::Net::kInvalidUserId) {
        return;
    }

    m_targetUserId = targetUserId;
    m_targetDisplayName = displayName;
    m_autoRunActive = false;
    LOG_INFO_FMT("PlayerFollow", "Following player '%s' (user %u)",
                 m_targetDisplayName.c_str(),
                 m_targetUserId);
}

void PlayerFollowController::CancelFollow(const char* reason) {
    if (!IsFollowing()) {
        return;
    }

    if (reason && reason[0] != '\0') {
        LOG_INFO_FMT("PlayerFollow",
                     "Stopped following '%s' — %s",
                     m_targetDisplayName.c_str(),
                     reason);
    } else {
        LOG_INFO_FMT("PlayerFollow", "Stopped following '%s'", m_targetDisplayName.c_str());
    }

    m_targetUserId = Engine::Net::kInvalidUserId;
    m_targetDisplayName.clear();
    m_autoRunActive = false;
}

bool PlayerFollowController::TryGetAutoMoveDirection(
    Engine::ECS::Registry& registry,
    Engine::Platform::Networking::Systems::NetworkSystem* network,
    Engine::EntityID localPlayer,
    glm::vec3& outDirection,
    bool& outRun) const {
    outDirection = glm::vec3(0.0f);
    outRun = false;

    if (!IsFollowing() || localPlayer == Engine::INVALID_ENTITY) {
        return false;
    }

    Engine::EntityID targetEntity = Engine::INVALID_ENTITY;
    if (network) {
        targetEntity = network->TryGetPlayerEntity(m_targetUserId);
    }

    if (targetEntity == Engine::INVALID_ENTITY) {
        return false;
    }

    const TransformComponent* localTransform =
        registry.GetComponent<TransformComponent>(localPlayer);
    const TransformComponent* targetTransform =
        registry.GetComponent<TransformComponent>(targetEntity);
    if (!localTransform || !targetTransform) {
        return false;
    }

    glm::vec3 delta = targetTransform->position - localTransform->position;
    delta.y = 0.0f;
    const float distance = glm::length(delta);
    if (distance <= kStopDistance) {
        return false;
    }

    outDirection = delta / distance;

    if (distance >= kRunDistance) {
        m_autoRunActive = true;
    } else if (distance <= kWalkResumeDistance) {
        m_autoRunActive = false;
    }
    outRun = m_autoRunActive;
    return true;
}

} // namespace Character
} // namespace Game
