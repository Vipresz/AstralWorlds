#pragma once

#include "core/Types.h"
#include "core/network/NetTypes.h"

#include <glm/glm.hpp>
#include <string>

namespace Engine {
namespace ECS {
class Registry;
}
namespace Platform {
namespace Networking {
namespace Systems {
class NetworkSystem;
}
} // namespace Networking
} // namespace Platform
} // namespace Engine

namespace Game {
namespace Character {

// Auto-walks the local player toward a remote target until cancelled or in range.
class PlayerFollowController {
public:
    void StartFollow(Engine::Net::UserId targetUserId, const std::string& displayName);
    void CancelFollow(const char* reason = nullptr);

    bool IsFollowing() const { return m_targetUserId != Engine::Net::kInvalidUserId; }
    Engine::Net::UserId GetTargetUserId() const { return m_targetUserId; }
    const std::string& GetTargetDisplayName() const { return m_targetDisplayName; }

    // Returns true when the local player should keep moving toward the target.
    bool TryGetAutoMoveDirection(Engine::ECS::Registry& registry,
                                 Engine::Platform::Networking::Systems::NetworkSystem* network,
                                 Engine::EntityID localPlayer,
                                 glm::vec3& outDirection,
                                 bool& outRun) const;

private:
    Engine::Net::UserId m_targetUserId = Engine::Net::kInvalidUserId;
    std::string m_targetDisplayName;
    mutable bool m_autoRunActive = false;

    // Stop moving when within arm's reach of the target.
    static constexpr float kStopDistance = 2.5f;
    // Beyond this horizontal gap, auto-follow switches from walk to run.
    static constexpr float kRunDistance = 6.0f;
    // Hysteresis: drop back to walk only after closing inside this distance.
    static constexpr float kWalkResumeDistance = 4.5f;
};

} // namespace Character
} // namespace Game
