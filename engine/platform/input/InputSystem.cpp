#include "input/InputSystem.h"
#include "render/Camera.h"
#include "render/FollowCamera.h"
#include "render/components/TransformComponent.h"
#include "render/components/AnimationComponent.h"
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath> // For std::atan2, std::copysign, std::abs, etc.

namespace Engine {
namespace Platform {
namespace Input {
namespace Systems {

InputSystem::InputSystem(Window* window) : m_window(window) {
    SetPriority(0); // Input system should run first
}

void InputSystem::Initialize(ECS::Registry& registry) {
    // Initialize input state
}

void InputSystem::Update(float deltaTime, ECS::Registry& registry, uint64_t tickNumber) {
    if (!m_window) {
        return;
    }
    
    // Poll events (done by window, but ensure it's called)
    m_window->pollEvents();
    
    // FollowCamera mode: Character moves with WASD, camera follows
    // Handle character movement FIRST (so camera can follow the new position)
    HandleCharacterMovement(deltaTime, registry);
    
    // Handle camera input AFTER character moves (mouse rotation + scroll wheel zoom)
    HandleCameraInput(deltaTime);
}

void InputSystem::Shutdown(ECS::Registry& registry) {
    // Cleanup if needed
}

void InputSystem::HandleCameraInput(float deltaTime) {
    if (!m_window) {
        return;
    }
    
    // Handle follow camera if it exists
    if (m_followCamera) {
        // Process mouse input for camera rotation (orbit around character)
        GLfloat xChange = m_window->getXChange();
        GLfloat yChange = m_window->getYChange();
        
        if (xChange != 0.0f || yChange != 0.0f) {
            m_followCamera->HandleMouseInput(xChange, yChange);
        }
        
        // Process scroll wheel for distance control
        GLfloat scrollChange = m_window->getScrollYChange();
        if (scrollChange != 0.0f) {
            m_followCamera->HandleScrollInput(scrollChange);
        }
        
        // CRITICAL: Update camera position every frame to follow character
        // This reads the character's current position and updates camera immediately
        // This happens AFTER character movement, so camera is always synchronized
        m_followCamera->Update(deltaTime);
    } else if (m_camera) {
        // Legacy camera support (mouse look only, no keyboard)
        GLfloat xChange = m_window->getXChange();
        GLfloat yChange = m_window->getYChange();
        m_camera->mouseControl(xChange, yChange);
    }
}

// ============================================================================
// CHARACTER MOVEMENT HANDLER
// WASD moves the character, camera follows via FollowCamera
// ============================================================================
void InputSystem::HandleCharacterMovement(float deltaTime, ECS::Registry& registry) {
    if (!m_window || m_characterEntityID == 0) {
        return;
    }
    
    bool* keys = m_window->getsKeys();
    
    // Get the character entity's transform
    auto* transform = registry.GetComponent<Render::Components::TransformComponent>(m_characterEntityID);
    
    if (!transform) {
        return;
    }
    
    // Calculate movement direction relative to camera (WASD controls)
    glm::vec3 moveDirection(0.0f);
    
    // Get camera forward and right vectors for camera-relative movement
    glm::vec3 forwardDir(0.0f, 0.0f, 0.0f);
    glm::vec3 rightDir(0.0f, 0.0f, 0.0f);
    
    if (m_followCamera) {
        // Use camera's forward/right directions for movement
        forwardDir = m_followCamera->GetForwardDirection();
        rightDir = m_followCamera->GetRightDirection();
    } else {
        // Fallback: use world space directions if no follow camera
        forwardDir = glm::vec3(0.0f, 0.0f, -1.0f);
        rightDir = glm::vec3(1.0f, 0.0f, 0.0f);
    }
    
    // Build movement direction based on camera orientation
    if (keys[GLFW_KEY_W] || keys[GLFW_KEY_UP]) {
        moveDirection += forwardDir; // Forward (relative to camera)
    }
    if (keys[GLFW_KEY_S] || keys[GLFW_KEY_DOWN]) {
        moveDirection -= forwardDir; // Backward (relative to camera)
    }
    if (keys[GLFW_KEY_A] || keys[GLFW_KEY_LEFT]) {
        moveDirection -= rightDir; // Left (relative to camera)
    }
    if (keys[GLFW_KEY_D] || keys[GLFW_KEY_RIGHT]) {
        moveDirection += rightDir; // Right (relative to camera)
    }
    
    // Get animation component if it exists
    auto* animComp = registry.GetComponent<Render::Components::AnimationComponent>(m_characterEntityID);
    
    // Check for running (Shift+W)
    bool isRunning = (keys[GLFW_KEY_LEFT_SHIFT] || keys[GLFW_KEY_RIGHT_SHIFT]) && (keys[GLFW_KEY_W] || keys[GLFW_KEY_UP]);
    
    // Determine movement speed based on running state
    // Walking: base speed (1.4 m/s typical)
    // Running: ~2.5x faster (3.5 m/s typical human run)
    float currentMovementSpeed = isRunning ? (m_movementSpeed * 2.5f) : m_movementSpeed;
    
    // Update animation component state if it exists
    if (animComp) {
        animComp->isRunning = isRunning;
    }
    
    // Normalize movement direction if not zero
    bool isMoving = glm::length(moveDirection) > 0.0f;
    
    if (isMoving) {
        moveDirection = glm::normalize(moveDirection);
        
        // When moving, make character face the movement direction
        if (m_followCamera && glm::length(moveDirection) > 0.0f) {
            // Calculate desired yaw angle from movement direction
            glm::vec3 normalizedMove = glm::normalize(moveDirection);
            float desiredYaw = glm::degrees(std::atan2(-normalizedMove.x, -normalizedMove.z)) + 180.0f;
            
            // Get current character yaw
            float currentYaw = transform->GetYaw();
            
            // Normalize angles to -180 to 180 range for smooth rotation
            while (desiredYaw > 180.0f) desiredYaw -= 360.0f;
            while (desiredYaw < -180.0f) desiredYaw += 360.0f;
            while (currentYaw > 180.0f) currentYaw -= 360.0f;
            while (currentYaw < -180.0f) currentYaw += 360.0f;
            
            // Calculate angle difference (shortest rotation path)
            float angleDiff = desiredYaw - currentYaw;
            
            // Normalize angle difference to -180 to 180
            while (angleDiff > 180.0f) angleDiff -= 360.0f;
            while (angleDiff < -180.0f) angleDiff += 360.0f;
            
            // Rotate character towards movement direction with smooth interpolation
            float rotationSpeed = 720.0f; // degrees per second
            float maxRotation = rotationSpeed * deltaTime;
            
            if (std::abs(angleDiff) > 1.0f) {
                float rotation = std::copysign(std::min(std::abs(angleDiff), maxRotation), angleDiff);
                transform->SetYaw(currentYaw + rotation);
            } else {
                transform->SetYaw(desiredYaw);
            }
        }
        
        // Apply horizontal movement (X and Z only - Y will be handled by jump physics)
        glm::vec3 horizontalMove = moveDirection;
        horizontalMove.y = 0.0f; // Only horizontal movement
        horizontalMove = glm::normalize(horizontalMove);
        horizontalMove *= currentMovementSpeed * deltaTime;
        transform->Translate(horizontalMove);
        
        // Update animation state
        if (animComp) {
            if (!animComp->isJumping) {
                animComp->currentState = isRunning ? 
                    Render::Components::AnimationComponent::AnimationState::Running : 
                    Render::Components::AnimationComponent::AnimationState::Walking;
            }
        }
    } else {
        // Not moving - update animation state to idle (unless jumping)
        if (animComp && !animComp->isJumping) {
            animComp->currentState = Render::Components::AnimationComponent::AnimationState::Idle;
        }
    }
    
    // Handle jump physics
    if (animComp) {
        // Check for jump input (Space bar) - only if on ground
        // Allow jump if character is very close to ground level (within 0.2 units)
        bool isOnGround = std::abs(transform->position.y - animComp->groundLevel) < 0.2f;
        
        // Check if space was just pressed (not held) to prevent multiple jumps
        bool spaceJustPressed = keys[GLFW_KEY_SPACE] && !m_spacePressedLastFrame;
        m_spacePressedLastFrame = keys[GLFW_KEY_SPACE];
        
        if (spaceJustPressed && !animComp->isJumping && isOnGround) {
            // Start jump
            animComp->isJumping = true;
            animComp->jumpVelocity = animComp->jumpInitialVelocity;
            animComp->currentState = Render::Components::AnimationComponent::AnimationState::Jumping;
        }
        
        // Apply jump physics
        if (animComp->isJumping) {
            // Update vertical velocity with gravity
            animComp->jumpVelocity += animComp->gravity * deltaTime;
            
            // Apply vertical movement
            float verticalMove = animComp->jumpVelocity * deltaTime;
            transform->Translate(glm::vec3(0.0f, verticalMove, 0.0f));
            
            // Check if landed
            // Character's base Y position should be at groundLevel (where feet touch ground)
            // Character transform is at character's base, so check if it hits the ground
            float characterBaseY = transform->position.y;
            if (characterBaseY <= animComp->groundLevel) {
                // Landed - reset jump state and clamp position to ground
                transform->position.y = animComp->groundLevel;
                animComp->isJumping = false;
                animComp->jumpVelocity = 0.0f;
                
                // Return to appropriate animation state based on movement
                if (isMoving) {
                    animComp->currentState = isRunning ? 
                        Render::Components::AnimationComponent::AnimationState::Running : 
                        Render::Components::AnimationComponent::AnimationState::Walking;
                } else {
                    animComp->currentState = Render::Components::AnimationComponent::AnimationState::Idle;
                }
            }
        }
    }
}

} // namespace Systems
} // namespace Input
} // namespace Platform
} // namespace Engine

