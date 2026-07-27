#pragma once

#include "render/FollowCamera.h"
#include "render/ICameraSource.h"
#include "render/systems/CameraSystem.h"
#include "render/systems/RenderSystem.h"
#include "input/InputSystem.h"
#include "core/ecs/Registry.h"
#include "core/Types.h"
#include <glm/glm.hpp>
#include <memory>

namespace Engine {
namespace ECS {
class SystemManager;
}
}

namespace Game {

// Game-owned camera wiring — keeps FollowCamera setup in src/, not RenderingEngine.
struct GameCameraRig {
    Engine::Platform::Render::FollowCamera followCamera;
    Engine::Platform::Render::ICameraSource* source = nullptr;

    void ConfigureFollowTarget(Engine::EntityID target, Engine::ECS::Registry& registry,
                               float minDistance = 0.1f, float maxDistance = 100.0f,
                               float rotationSpeed = 0.5f, float zoomSpeed = 0.5f);

    void SetHeightOffset(float headHeight);
};

// Wire engine systems to the game camera rig (same pattern you can swap for a custom ICameraSource).
void WireGameCamera(GameCameraRig& rig,
                    const std::shared_ptr<Engine::Platform::Input::Systems::InputSystem>& inputSystem,
                    const std::shared_ptr<Engine::Platform::Render::Systems::CameraSystem>& cameraSystem,
                    const std::shared_ptr<Engine::Platform::Render::Systems::RenderSystem>& renderSystem,
                    bool useGameMouseRules = false);

} // namespace Game
