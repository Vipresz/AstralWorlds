#pragma once

#include "core/System.h"
#include "core/Types.h"
#include "render/ICameraSource.h"
#include <functional>

namespace Engine {
namespace Platform {
class Window;
}
namespace ECS {
class Registry;
}
}

namespace Game {

// Game-specific mouse rules:
// - LMB hold: orbit camera (cursor captured)
// - RMB hold: orbit camera + turn character with camera (cursor captured)
// - otherwise: free cursor for UI
class GameCameraInput : public Engine::ECS::ISystem {
public:
    GameCameraInput(Engine::Platform::Window* window,
                    Engine::Platform::Render::ICameraSource* camera,
                    Engine::EntityID characterEntity);

    void SetInputBlockQuery(std::function<bool()> query) { m_inputBlockQuery = std::move(query); }

    void Initialize(Engine::ECS::Registry& registry) override;
    void Update(float deltaTime, Engine::ECS::Registry& registry, uint64_t tickNumber) override;
    void Shutdown(Engine::ECS::Registry& registry) override;

private:
    Engine::Platform::Window* m_window = nullptr;
    Engine::Platform::Render::ICameraSource* m_camera = nullptr;
    Engine::EntityID m_characterEntity = 0;
    std::function<bool()> m_inputBlockQuery;
    float m_characterTurnSpeed = 720.0f;

    bool IsBlocked() const { return m_inputBlockQuery && m_inputBlockQuery(); }
    void OrientCharacterToCamera(float deltaTime, Engine::ECS::Registry& registry);
};

} // namespace Game
