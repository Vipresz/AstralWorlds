#include "render/FollowCamera.h"
#include "core/ecs/Registry.h"
#include "Logger.h"
#include <cmath>
#include <algorithm>

namespace Engine {
namespace Platform {
namespace Render {

FollowCamera::FollowCamera()
    : m_targetEntityID(0)
    , m_registry(nullptr)
    , m_cameraPosition(0.0f, 0.0f, 0.0f)
    , m_lookAtTarget(0.0f, 0.0f, 0.0f)
    , m_yaw(180.0f)   // Start behind character (180 degrees = behind, 0 = in front)
    , m_pitch(10.0f)  // Slightly looking down at character from head level
    , m_distance(5.0f) // Start 5 units away
    , m_heightOffset(1.7f) // Head height offset (character head level)
    , m_minDistance(0.1f)  // Can zoom in very close (inside head/first person)
    , m_maxDistance(20.0f) // Can zoom out far
    , m_rotationSpeed(0.5f)
    , m_zoomSpeed(0.5f) // Slower zoom for better control
{
}

void FollowCamera::SetTarget(EntityID entityID, ECS::Registry* registry) {
    m_targetEntityID = entityID;
    m_registry = registry;
}

void FollowCamera::Update(float deltaTime) {
    UpdateCameraPosition();
}

void FollowCamera::HandleMouseInput(GLfloat xChange, GLfloat yChange) {
    m_yaw += xChange * m_rotationSpeed;
    m_pitch -= yChange * m_rotationSpeed;
    m_pitch = std::max(-80.0f, std::min(80.0f, m_pitch));
    UpdateCameraPosition();
}

void FollowCamera::HandleScrollInput(GLfloat scrollDelta) {
    m_distance -= scrollDelta * m_zoomSpeed;
    m_distance = std::max(m_minDistance, std::min(m_maxDistance, m_distance));
    UpdateCameraPosition();
}

void FollowCamera::UpdateCameraPosition() {
    if (m_targetEntityID == 0 || !m_registry) {
        return;
    }
    
    // CRITICAL: Read character position directly from registry every frame
    // This ensures camera always uses the most up-to-date position
    auto* transform = m_registry->GetComponent<Components::TransformComponent>(m_targetEntityID);
    if (!transform) {
        return;
    }
    
    // Get character's world position (this is the base/feet position after positioning)
    glm::vec3 characterBasePos = transform->position;
    
    // Calculate head position: character base + head height offset
    // This gives us the exact world position of the character's head
    glm::vec3 headWorldPos = characterBasePos + glm::vec3(0.0f, m_heightOffset, 0.0f);
    
    // CRITICAL: Camera ALWAYS looks directly at the head position
    // This keeps the head perfectly centered on screen at all times
    m_lookAtTarget = headWorldPos;
    
    // Calculate camera position in spherical coordinates around the head
    // Camera orbits around head position, keeping head centered
    float yawRad = glm::radians(m_yaw);
    float pitchRad = glm::radians(m_pitch);
    
    // Calculate offset from head to camera
    float horizontalDistance = m_distance * cos(pitchRad);
    float offsetX = horizontalDistance * sin(yawRad);
    float offsetY = m_distance * sin(pitchRad);
    float offsetZ = horizontalDistance * cos(yawRad);
    
    // Position camera relative to head - head stays at center of view
    m_cameraPosition = headWorldPos + glm::vec3(offsetX, offsetY, offsetZ);
}

glm::mat4 FollowCamera::CalculateViewMatrix() const {
    return glm::lookAt(m_cameraPosition, m_lookAtTarget, glm::vec3(0.0f, 1.0f, 0.0f));
}

glm::vec3 FollowCamera::GetForwardDirection() const {
    glm::vec3 direction = m_lookAtTarget - m_cameraPosition;
    direction.y = 0.0f;
    
    if (glm::length(direction) > 0.01f) {
        return glm::normalize(direction);
    }
    
    float yawRad = glm::radians(m_yaw);
    return glm::normalize(glm::vec3(-sin(yawRad), 0.0f, -cos(yawRad)));
}

glm::vec3 FollowCamera::GetRightDirection() const {
    glm::vec3 forward = GetForwardDirection();
    glm::vec3 right = glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f));
    
    if (glm::length(right) < 0.01f) {
        float yawRad = glm::radians(m_yaw);
        right = glm::vec3(cos(yawRad), 0.0f, -sin(yawRad));
    }
    
    return glm::normalize(right);
}

FollowCamera::~FollowCamera() {
}

} // namespace Render
} // namespace Platform
} // namespace Engine
