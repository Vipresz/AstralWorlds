#pragma once

// Forward declaration
namespace Engine { namespace Platform { namespace Render { class Animation; } } }
namespace Engine { namespace Platform { namespace Render { class Skeleton; } } }

namespace Engine {
namespace Platform {
namespace Render {
namespace Components {

// Animation component - stores animation state and physics for an entity
struct AnimationComponent {
    // Current animation state
    enum class AnimationState {
        Idle,
        Walking,
        Running,
        Jumping
    };
    
    AnimationState currentState = AnimationState::Idle;
    
    // Animation references (managed externally, set by main.cpp)
    Animation* walkingAnimation = nullptr;
    Animation* runningAnimation = nullptr;
    Animation* jumpAnimation = nullptr;
    
    // Skeleton reference (managed externally, set by main.cpp)
    Skeleton* skeleton = nullptr;
    
    // Current animation playback
    Animation* currentAnimation = nullptr;
    float currentAnimationTime = 0.0f; // Current time in seconds
    bool isLooping = true;
    
    // Note: Bone transforms are stored in the Skeleton, not here (to keep component POD-compliant)
    
    // Jump physics state
    bool isJumping = false;
    float jumpVelocity = 0.0f; // Current vertical velocity (units per second)
    float gravity = -9.81f; // Gravity acceleration (units per second^2)
    float jumpInitialVelocity = 5.0f; // Initial jump velocity when space is pressed
    float groundLevel = 0.0f; // Y position of the ground (for landing detection)
    
    // Movement state
    bool isRunning = false; // True when Shift+W is pressed
    bool wasMovingLastFrame = false; // Track movement state changes
};

} // namespace Components
} // namespace Render
} // namespace Platform
} // namespace Engine

