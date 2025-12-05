#pragma once

#include "core/System.h"
#include "core/Types.h"
#include "core/ecs/Registry.h"
#include "core/ecs/Entity.h"
#include "render/Window.h"
#include "render/components/TransformComponent.h"

namespace Engine {
namespace Platform {
namespace Render {
    class Camera;
    class FollowCamera;
}
namespace Input {
namespace Systems {

// Input system - handles keyboard and mouse input
class InputSystem : public ECS::ISystem {
public:
    InputSystem(Window* window);
    
    void Initialize(ECS::Registry& registry) override;
    void Update(float deltaTime, ECS::Registry& registry, uint64_t tickNumber) override;
    void Shutdown(ECS::Registry& registry) override;
    
    // Camera control (legacy - for regular Camera)
    void SetCamera(Render::Camera* camera) { m_camera = camera; }
    
    // Follow camera control (RPG-style)
    void SetFollowCamera(Render::FollowCamera* followCamera) { m_followCamera = followCamera; }
    
    // Character entity tracking
    void SetCharacterEntity(EntityID entityID) { m_characterEntityID = entityID; }
    EntityID GetCharacterEntity() const { return m_characterEntityID; }
    
    // Character movement speed
    void SetMovementSpeed(float speed) { m_movementSpeed = speed; }
    float GetMovementSpeed() const { return m_movementSpeed; }

private:
    Window* m_window = nullptr;
    Render::Camera* m_camera = nullptr; // Legacy camera (may be null if using follow camera)
    Render::FollowCamera* m_followCamera = nullptr; // RPG-style follow camera
    EntityID m_characterEntityID = 0; // Track which entity is the character
    float m_movementSpeed = 5.0f; // units per second
    
    // Input state
    bool m_moveForward = false;
    bool m_moveBackward = false;
    bool m_moveLeft = false;
    bool m_moveRight = false;
    bool m_spacePressedLastFrame = false; // Track previous frame space key state for jump
    
    void HandleCameraInput(float deltaTime);
    void HandleCharacterMovement(float deltaTime, ECS::Registry& registry);
};

} // namespace Systems
} // namespace Input
} // namespace Platform
} // namespace Engine

