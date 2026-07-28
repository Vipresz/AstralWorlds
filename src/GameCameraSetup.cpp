#include "GameCameraSetup.h"

namespace Game {

void GameCameraRig::ConfigureFollowTarget(Engine::EntityID target, Engine::ECS::Registry& registry,
                                          float minDistance, float maxDistance,
                                          float rotationSpeed, float zoomSpeed) {
    followCamera.SetTarget(target, &registry);
    followCamera.SetMinDistance(minDistance);
    followCamera.SetMaxDistance(maxDistance);
    followCamera.SetRotationSpeed(rotationSpeed);
    followCamera.SetZoomSpeed(zoomSpeed);
    source = &followCamera;
}

void GameCameraRig::SetHeightOffset(float headHeight) {
    followCamera.SetHeightOffset(headHeight);
}

void WireGameCamera(GameCameraRig& rig,
                    const std::shared_ptr<Engine::Platform::Input::Systems::InputSystem>& inputSystem,
                    const std::shared_ptr<Engine::Platform::Render::Systems::CameraSystem>& cameraSystem,
                    const std::shared_ptr<Engine::Platform::Render::Systems::RenderSystem>& renderSystem,
                    bool useGameMouseRules) {
    if (!rig.source) {
        rig.source = &rig.followCamera;
    }

    if (inputSystem) {
        inputSystem->SetCameraSource(rig.source);
        if (useGameMouseRules) {
            inputSystem->SetBuiltInCameraInputEnabled(false);
        }
    }
    if (cameraSystem) {
        cameraSystem->SetSource(rig.source);
    }
    if (renderSystem) {
        renderSystem->SetCameraSource(rig.source);
    }
}

} // namespace Game
