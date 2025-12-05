#pragma once

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "render/components/TransformComponent.h"
#include "core/Types.h"

namespace Engine {
namespace ECS {
    class Registry;
}
namespace Platform {
namespace Render {

// Third-person follow camera for RPG-style character control
// Camera follows character and looks at character's head
class FollowCamera {
public:
    FollowCamera();
    
    // Set the target entity to follow (character) - uses EntityID for safe access
    void SetTarget(EntityID entityID, ECS::Registry* registry);
    
    // Update camera position based on target - MUST be called every frame
    void Update(float deltaTime);
    
    // Handle mouse input for camera rotation around target
    void HandleMouseInput(GLfloat xChange, GLfloat yChange);
    
    // Handle mouse wheel for distance control
    void HandleScrollInput(GLfloat scrollDelta);
    
    // Camera settings
    void SetHeightOffset(float offset) { m_heightOffset = offset; } // Height above character base position (head level)
    void SetMinDistance(float minDist) { m_minDistance = minDist; } // Closest (inside head/first person)
    void SetMaxDistance(float maxDist) { m_maxDistance = maxDist; } // Farthest (very far)
    void SetRotationSpeed(float speed) { m_rotationSpeed = speed; }
    void SetZoomSpeed(float speed) { m_zoomSpeed = speed; }
    
    // Get current camera values
    float GetDistance() const { return m_distance; }
    float GetYaw() const { return m_yaw; }
    float GetPitch() const { return m_pitch; }
    
    // Get camera position and view matrix
    glm::vec3 GetCameraPosition() const { return m_cameraPosition; }
    glm::vec3 GetCameraTarget() const { return m_lookAtTarget; }
    glm::mat4 CalculateViewMatrix() const;
    
    // Get camera direction vectors for character movement (relative to camera view)
    glm::vec3 GetForwardDirection() const; // Forward direction (where camera is looking)
    glm::vec3 GetRightDirection() const;   // Right direction (perpendicular to forward)
    
    ~FollowCamera();

private:
    EntityID m_targetEntityID; // Entity ID to follow
    ECS::Registry* m_registry; // Registry to access transform component
    
    // Camera position
    glm::vec3 m_cameraPosition;
    glm::vec3 m_lookAtTarget;
    
    // Camera rotation (spherical coordinates around target)
    float m_yaw;      // Horizontal rotation around target
    float m_pitch;    // Vertical rotation (looking up/down)
    float m_distance; // Distance from target
    
    // Settings
    float m_heightOffset;   // Height above target position (head level)
    float m_minDistance;    // Minimum distance (first person / inside head)
    float m_maxDistance;    // Maximum distance (very far)
    float m_rotationSpeed;  // Mouse sensitivity for rotation
    float m_zoomSpeed;      // Scroll wheel sensitivity
    
    void UpdateCameraPosition();
};

} // namespace Render
} // namespace Platform
} // namespace Engine
