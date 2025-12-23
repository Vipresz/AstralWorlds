#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <cstddef>

// Forward declarations
struct GLFWwindow;

namespace Engine {
namespace Editors {
namespace CameraSystem {

// Camera definition
struct Camera {
    std::string name;
    glm::vec3 position;
    glm::vec3 target;
    bool canMove;  // Can this camera be moved with WASD/mouse?
    
    Camera(const std::string& n, const glm::vec3& pos, const glm::vec3& tgt, bool movable = false)
        : name(n), position(pos), target(tgt), canMove(movable) {}
};

class CameraSystem {
public:
    CameraSystem();
    ~CameraSystem();
    
    // Initialize camera system
    void Initialize();
    
    // Camera management
    void AddCamera(const std::string& name, const glm::vec3& position, const glm::vec3& target, bool canMove, std::vector<Camera>& cameras);
    void InitializeDefaultCameras(std::vector<Camera>& cameras);
    void SwitchCamera(size_t cameraIndex, std::vector<Camera>& cameras, size_t& activeCameraIndex,
                     glm::vec3& freeCameraPos, glm::mat4& view);
    
    // Camera updates
    void UpdateCamera(std::vector<Camera>& cameras, size_t activeCameraIndex, 
                     glm::vec3& freeCameraPos, float freeCameraYaw, float freeCameraPitch,
                     glm::mat4& view);
    void UpdateFreeCamera(float deltaTime, std::vector<Camera>& cameras, size_t activeCameraIndex,
                         glm::vec3& freeCameraPos, float& freeCameraYaw, float& freeCameraPitch,
                         float movementSpeed, GLFWwindow* window);
    
    // Calculate view matrix for a camera
    glm::mat4 CalculateCameraView(const Camera& cam) const;
    
    // Handle input
    void HandleMouseInput(std::vector<Camera>& cameras, size_t activeCameraIndex,
                         bool& mouseRightDown, double& lastMouseX, double& lastMouseY,
                         glm::vec3& freeCameraPos, float& freeCameraYaw, float& freeCameraPitch,
                         GLFWwindow* window);
    void HandleKeyboardInput(std::vector<Camera>& cameras, size_t& pendingCameraSwitch);
    
    // Initialize from config
    void InitializeFromConfig(const std::vector<Camera>& cameras, size_t& activeCameraIndex,
                             glm::vec3& freeCameraPos, float& freeCameraYaw, float& freeCameraPitch);
    
    // Getters
    size_t GetActiveCameraIndex() const { return m_activeCameraIndex; }
    void SetActiveCameraIndex(size_t index) { m_activeCameraIndex = index; }
    
    // Camera query functions
    static size_t FindAnchorCameraIndex(const std::vector<Camera>& cameras);
    static bool HasFreeCamera(const std::vector<Camera>& cameras);
    
    // Camera utility functions
    static void CalculateYawPitchFromDirection(const glm::vec3& direction, float& yaw, float& pitch);
    
    // Camera management
    void EnsureFreeCameraExists(
        std::vector<Camera>& cameras,
        glm::vec3& freeCameraPos,
        float& freeCameraYaw,
        float& freeCameraPitch
    );
    
private:
    size_t m_activeCameraIndex;
};

} // namespace CameraSystem
} // namespace Editors
}

