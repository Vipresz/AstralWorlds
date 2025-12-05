#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <cmath> // For std::sin, std::cos

namespace Engine {
namespace Platform {
namespace Render {
namespace Components {

// Transform component - position, rotation, scale
struct TransformComponent {
    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 rotation = glm::vec3(0.0f);  // Euler angles (degrees)
    glm::vec3 scale = glm::vec3(1.0f);
    
    // Get transformation matrix
    glm::mat4 GetMatrix() const {
        glm::mat4 transform = glm::mat4(1.0f);
        
        // Translate
        transform = glm::translate(transform, position);
        
        // Rotate (convert degrees to radians)
        transform = glm::rotate(transform, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f)); // Roll
        transform = glm::rotate(transform, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f)); // Yaw
        transform = glm::rotate(transform, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f)); // Pitch
        
        // Scale
        transform = glm::scale(transform, scale);
        
        return transform;
    }
    
    // Helper methods
    void SetPosition(float x, float y, float z) { position = glm::vec3(x, y, z); }
    void SetRotation(float x, float y, float z) { rotation = glm::vec3(x, y, z); }
    void SetScale(float x, float y, float z) { scale = glm::vec3(x, y, z); }
    void SetScale(float uniform) { scale = glm::vec3(uniform); }
    
    void Translate(const glm::vec3& offset) { position += offset; }
    void Rotate(const glm::vec3& delta) { rotation += delta; }
    
    // Yaw (horizontal rotation) helpers
    float GetYaw() const { return rotation.y; } // Returns yaw in degrees
    void SetYaw(float yaw) { rotation.y = yaw; } // Set yaw in degrees
    
    // Get forward and right directions based on current yaw rotation
    glm::vec3 GetForwardDirection() const {
        float yawRad = glm::radians(rotation.y);
        return glm::vec3(-std::sin(yawRad), 0.0f, -std::cos(yawRad));
    }
    glm::vec3 GetRightDirection() const {
        float yawRad = glm::radians(rotation.y);
        return glm::vec3(std::cos(yawRad), 0.0f, -std::sin(yawRad));
    }
};

} // namespace Components
} // namespace Render
} // namespace Platform
} // namespace Engine



