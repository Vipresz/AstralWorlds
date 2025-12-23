#include "CameraSystem.h"
#include "../CameraSystemEditor.h"  // For Camera struct
#include "../../engine/platform/Logger.h"
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>

using EngineLogging::Logger;

namespace Engine {
namespace Editors {
namespace CameraSystem {

CameraSystem::CameraSystem()
    : m_activeCameraIndex(0)
{
}

CameraSystem::~CameraSystem() {
}

void CameraSystem::Initialize() {
    m_activeCameraIndex = 0;
}

void CameraSystem::AddCamera(const std::string& name, const glm::vec3& position, const glm::vec3& target, bool canMove, std::vector<Camera>& cameras) {
    cameras.emplace_back(name, position, target, canMove);
}

void CameraSystem::InitializeDefaultCameras(std::vector<Camera>& cameras) {
    cameras.clear();
    
    // Default free camera (can be moved with WASD/mouse)
    // World plane is x-y, z is vertical (up)
    AddCamera("Free Camera", glm::vec3(0.0f, 0.0f, 5.0f), glm::vec3(0.0f, 0.0f, 0.0f), true, cameras);
    
    // 4 cameras positioned in a square around center in x-y plane, z units above ground
    const float squareSize = 10.0f;
    const float height = 3.0f;  // Height above x-y plane (z coordinate)
    glm::vec3 center(0.0f, 0.0f, 0.0f);
    
    // North camera (positive y direction)
    AddCamera("North Camera", glm::vec3(0.0f, squareSize, height), center, false, cameras);
    // South camera (negative y direction)
    AddCamera("South Camera", glm::vec3(0.0f, -squareSize, height), center, false, cameras);
    // East camera (positive x direction)
    AddCamera("East Camera", glm::vec3(squareSize, 0.0f, height), center, false, cameras);
    // West camera (negative x direction)
    AddCamera("West Camera", glm::vec3(-squareSize, 0.0f, height), center, false, cameras);
}

void CameraSystem::SwitchCamera(size_t cameraIndex, std::vector<Camera>& cameras, size_t& activeCameraIndex,
                                glm::vec3& freeCameraPos, glm::mat4& view) {
    try {
        if (cameras.empty() || cameraIndex >= cameras.size()) {
            return;
        }
        
        m_activeCameraIndex = cameraIndex;
        activeCameraIndex = cameraIndex;
        
        // If switching to free camera, reset it to anchor camera 1's configuration
        if (activeCameraIndex < cameras.size() && cameras[activeCameraIndex].canMove) {
            // Find anchor camera 1 (first camera with canMove=false)
            size_t anchorCamera1Index = SIZE_MAX;
            for (size_t i = 0; i < cameras.size(); i++) {
                if (!cameras[i].canMove) {
                    anchorCamera1Index = i;
                    break;
                }
            }
            
            if (anchorCamera1Index < cameras.size()) {
                const Camera& anchorCam1 = cameras[anchorCamera1Index];
                cameras[activeCameraIndex].position = anchorCam1.position;
                cameras[activeCameraIndex].target = anchorCam1.target;
                freeCameraPos = anchorCam1.position;
            }
        }
        
        UpdateCamera(cameras, activeCameraIndex, freeCameraPos, 0.0f, 0.0f, view);
        
    } catch (const std::exception& e) {
        LOG_ERROR_FMT("CameraSystem", "Exception in SwitchCamera: %s", e.what());
    } catch (...) {
        LOG_ERROR("CameraSystem", "Unknown exception in SwitchCamera");
    }
}

void CameraSystem::UpdateCamera(std::vector<Camera>& cameras, size_t activeCameraIndex,
                               glm::vec3& freeCameraPos, float freeCameraYaw, float freeCameraPitch,
                               glm::mat4& view) {
    try {
        if (cameras.empty() || activeCameraIndex >= cameras.size()) {
            return;
        }
        
        Camera& activeCam = cameras[activeCameraIndex];
        
        if (activeCam.canMove) {
            // Free camera - use yaw/pitch to calculate forward direction
            glm::vec3 forward(
                cosf(freeCameraPitch) * sinf(freeCameraYaw),
                cosf(freeCameraPitch) * cosf(freeCameraYaw),
                -sinf(freeCameraPitch)
            );
            
            // Update target based on current position and forward direction
            activeCam.position = freeCameraPos;
            activeCam.target = freeCameraPos + forward;
            
            // Calculate up vector the same way as anchor cameras
            glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 0.0f, 1.0f)));
            glm::vec3 up = glm::cross(right, forward);
            if (glm::length(up) < 0.0001f) {
                up = glm::vec3(0.0f, 0.0f, 1.0f);
            } else {
                up = glm::normalize(up);
            }
            
            view = glm::lookAt(activeCam.position, activeCam.target, up);
        } else {
            // Fixed camera - use stored position and target directly
            glm::vec3 dir = activeCam.target - activeCam.position;
            if (glm::length(dir) < 0.0001f) {
                activeCam.target = activeCam.position + glm::vec3(0.0f, 0.0f, -1.0f);
            }
            view = glm::lookAt(activeCam.position, activeCam.target, glm::vec3(0.0f, 0.0f, 1.0f));
        }
    } catch (const std::exception& e) {
        LOG_ERROR_FMT("CameraSystem", "Exception in UpdateCamera: %s", e.what());
    } catch (...) {
        LOG_ERROR("CameraSystem", "Unknown exception in UpdateCamera");
    }
}

void CameraSystem::UpdateFreeCamera(float deltaTime, std::vector<Camera>& cameras, size_t activeCameraIndex,
                                   glm::vec3& freeCameraPos, float& freeCameraYaw, float& freeCameraPitch,
                                   float movementSpeed, GLFWwindow* window) {
    if (cameras.empty() || activeCameraIndex >= cameras.size()) {
        return;
    }
    
    if (!cameras[activeCameraIndex].canMove) {
        return;
    }
    
    if (!window) {
        return;
    }
    
    if (deltaTime <= 0.0f || deltaTime > 1.0f) {
        return;
    }
    
    try {
        float moveSpeed = movementSpeed * deltaTime;
        
        float cosPitch = cosf(freeCameraPitch);
        float sinYaw = sinf(freeCameraYaw);
        float cosYaw = cosf(freeCameraYaw);
        
        glm::vec3 forward(cosPitch * sinYaw, cosPitch * cosYaw, 0.0f);
        float forwardLength = glm::length(forward);
        if (forwardLength > 0.0001f) {
            forward = forward / forwardLength;
        } else {
            forward = glm::vec3(0.0f, 1.0f, 0.0f);
        }
        
        glm::vec3 right(cosYaw, -sinYaw, 0.0f);
        float rightLength = glm::length(right);
        if (rightLength > 0.0001f) {
            right = right / rightLength;
        } else {
            right = glm::vec3(1.0f, 0.0f, 0.0f);
        }
        
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
            freeCameraPos += forward * moveSpeed;
        }
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
            freeCameraPos -= forward * moveSpeed;
        }
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
            freeCameraPos -= right * moveSpeed;
        }
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
            freeCameraPos += right * moveSpeed;
        }
        
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
            freeCameraPos.z += moveSpeed;
        }
        if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS || 
            glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS) {
            freeCameraPos.z -= moveSpeed;
        }
    } catch (const std::exception& e) {
        LOG_ERROR_FMT("CameraSystem", "Exception in UpdateFreeCamera: %s", e.what());
    } catch (...) {
        LOG_ERROR("CameraSystem", "Unknown exception in UpdateFreeCamera");
    }
}

glm::mat4 CameraSystem::CalculateCameraView(const Camera& cam) const {
    if (cam.canMove) {
        glm::vec3 dir = cam.target - cam.position;
        if (glm::length(dir) < 0.0001f) {
            dir = glm::vec3(0.0f, 0.0f, -1.0f);
        }
        glm::vec3 forward = glm::normalize(dir);
        glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 0.0f, 1.0f)));
        glm::vec3 up = glm::cross(right, forward);
        if (glm::length(up) < 0.0001f) {
            up = glm::vec3(0.0f, 0.0f, 1.0f);
        } else {
            up = glm::normalize(up);
        }
        return glm::lookAt(cam.position, cam.target, up);
    } else {
        glm::vec3 dir = cam.target - cam.position;
        if (glm::length(dir) < 0.0001f) {
            dir = glm::vec3(0.0f, 0.0f, -1.0f);
        }
        return glm::lookAt(cam.position, cam.target, glm::vec3(0.0f, 0.0f, 1.0f));
    }
}

void CameraSystem::HandleMouseInput(std::vector<Camera>& cameras, size_t activeCameraIndex,
                                   bool& mouseRightDown, double& lastMouseX, double& lastMouseY,
                                   glm::vec3& freeCameraPos, float& freeCameraYaw, float& freeCameraPitch,
                                   GLFWwindow* window) {
    if (cameras.empty() || activeCameraIndex >= cameras.size()) {
        return;
    }
    
    if (!cameras[activeCameraIndex].canMove) {
        return;
    }
    
    if (!window) return;
    
    double mouseX, mouseY;
    glfwGetCursorPos(window, &mouseX, &mouseY);
    
    int rightButton = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT);
    bool rightPressed = (rightButton == GLFW_PRESS);
    
    if (mouseRightDown) {
        float sensitivity = 0.01f;
        float deltaX = static_cast<float>(mouseX - lastMouseX) * sensitivity;
        float deltaY = static_cast<float>(mouseY - lastMouseY) * sensitivity;
        
        freeCameraYaw += deltaX;
        freeCameraPitch += deltaY;
        
        glm::vec3 forward(
            cosf(freeCameraPitch) * sinf(freeCameraYaw),
            cosf(freeCameraPitch) * cosf(freeCameraYaw),
            -sinf(freeCameraPitch)
        );
        cameras[activeCameraIndex].target = freeCameraPos + forward;
    }
    
    if (rightPressed) {
        if (!mouseRightDown) {
            mouseRightDown = true;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        }
    } else {
        if (mouseRightDown) {
            mouseRightDown = false;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
    }
    
    lastMouseX = mouseX;
    lastMouseY = mouseY;
}

void CameraSystem::HandleKeyboardInput(std::vector<Camera>& cameras, size_t& pendingCameraSwitch) {
    // Number keys 1-5 to switch cameras
    // Note: This function doesn't have access to GLFW window, so it just sets pendingCameraSwitch
    // The actual switching should be handled by the caller
}

void CameraSystem::InitializeFromConfig(const std::vector<Camera>& cameras, size_t& activeCameraIndex,
                                        glm::vec3& freeCameraPos, float& freeCameraYaw, float& freeCameraPitch) {
    // Find first free camera and initialize it like anchor camera 1
    if (cameras.empty()) {
        return;
    }
    
    // Find anchor camera 1 (first camera with canMove=false)
    size_t anchorCamera1Index = SIZE_MAX;
    for (size_t i = 0; i < cameras.size(); i++) {
        if (!cameras[i].canMove) {
            anchorCamera1Index = i;
            break;
        }
    }
    
    // Find free camera
    size_t freeCameraIndex = SIZE_MAX;
    for (size_t i = 0; i < cameras.size(); i++) {
        if (cameras[i].canMove) {
            freeCameraIndex = i;
            break;
        }
    }
    
    if (anchorCamera1Index < cameras.size() && freeCameraIndex < cameras.size()) {
        const Camera& anchorCam1 = cameras[anchorCamera1Index];
        freeCameraPos = anchorCam1.position;
        
        // Calculate yaw and pitch from anchor camera's direction
        glm::vec3 dir = anchorCam1.target - anchorCam1.position;
        if (glm::length(dir) > 0.0001f) {
            dir = glm::normalize(dir);
            freeCameraYaw = atan2f(dir.x, dir.y);
            freeCameraPitch = -asinf(dir.z);
        }
        
        // Set free camera as active
        activeCameraIndex = freeCameraIndex;
    } else if (freeCameraIndex < cameras.size()) {
        // No anchor camera, just use free camera
        activeCameraIndex = freeCameraIndex;
        freeCameraPos = cameras[freeCameraIndex].position;
    } else if (!cameras.empty()) {
        // No free camera, use first camera
        activeCameraIndex = 0;
    }
}

size_t CameraSystem::FindAnchorCameraIndex(const std::vector<Camera>& cameras) {
    for (size_t i = 0; i < cameras.size(); i++) {
        if (!cameras[i].canMove) {
            return i;
        }
    }
    return SIZE_MAX;
}

bool CameraSystem::HasFreeCamera(const std::vector<Camera>& cameras) {
    for (const auto& cam : cameras) {
        if (cam.canMove) {
            return true;
        }
    }
    return false;
}

void CameraSystem::CalculateYawPitchFromDirection(const glm::vec3& direction, float& yaw, float& pitch) {
    yaw = atan2f(direction.x, direction.y);
    pitch = -asinf(std::max(-1.0f, std::min(1.0f, direction.z)));
}

void CameraSystem::EnsureFreeCameraExists(
    std::vector<Camera>& cameras,
    glm::vec3& freeCameraPos,
    float& freeCameraYaw,
    float& freeCameraPitch
) {
    if (HasFreeCamera(cameras)) {
        return;  // Already has a free camera
    }
    
    size_t anchorIdx = FindAnchorCameraIndex(cameras);
    glm::vec3 freeCamPos(10.0f, 10.0f, 5.0f);
    glm::vec3 target(0.0f, 0.0f, 0.0f);
    
    if (anchorIdx < cameras.size()) {
        const Camera& anchorCam = cameras[anchorIdx];
        freeCamPos = anchorCam.position;
        target = anchorCam.target;
    }
    
    glm::vec3 direction = glm::normalize(target - freeCamPos);
    float yaw, pitch;
    CalculateYawPitchFromDirection(direction, yaw, pitch);
    
    AddCamera("Free Camera", freeCamPos, target, true, cameras);
    freeCameraPos = freeCamPos;
    freeCameraYaw = yaw;
    freeCameraPitch = pitch;
}

} // namespace CameraSystem
} // namespace Editors
}

