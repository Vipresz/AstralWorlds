#pragma once

#include "ui/PlayerTargeting.h"

#include "core/ecs/Registry.h"
#include "render/ICameraSource.h"

#include <glm/glm.hpp>

namespace Engine {
namespace Platform {
class Window;
namespace Networking {
namespace Systems {
class NetworkSystem;
}
} // namespace Networking
} // namespace Platform
} // namespace Engine

namespace Game {
namespace UI {

// Tracks a remote player selected via LMB click for HUD display.
class PlayerTargetHud {
public:
    void Update(Engine::Platform::Window& window,
                Engine::ECS::Registry& registry,
                Engine::Platform::Networking::Systems::NetworkSystem* network,
                const Engine::Platform::Render::ICameraSource* camera,
                const glm::mat4& projection,
                int screenWidth,
                int screenHeight,
                bool gameInputBlocked,
                bool contextMenuOpen);

    bool HasTarget() const { return m_target.entity != Engine::INVALID_ENTITY; }
    const PlayerPickResult& GetTarget() const { return m_target; }
    void ClearTarget();

private:
    PlayerPickResult m_target{};
    bool m_lmbWasDown = false;
    float m_lmbDownX = 0.0f;
    float m_lmbDownY = 0.0f;
};

} // namespace UI
} // namespace Game
