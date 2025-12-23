#include "ConfigManager.h"
#include "../CameraSystemEditor.h"  // For SceneObject definition
#include "../../engine/platform/Logger.h"
#include <algorithm>
#include <cmath>

using EngineLogging::Logger;
using namespace Engine::Editors::CameraSystem::Constants;

namespace Engine {
namespace Editors {
namespace CameraSystem {

ConfigManager::CaptureSettings ConfigManager::LoadCaptureSettings(const SystemConfig& config, bool preserveExisting, bool existingEnabled) {
    CaptureSettings settings;
    
    if (preserveExisting) {
        settings.enabled = existingEnabled;
    } else {
        settings.enabled = config.captureEnabled;
    }
    
    if (!config.captureOutputDir.empty()) {
        settings.outputDir = config.captureOutputDir;
    }
    
    if (config.captureInterval > 0) {
        settings.interval = config.captureInterval;
    } else {
        settings.interval = 1;  // Default to every frame if not specified or invalid
    }
    
    return settings;
}

void ConfigManager::LoadLegacyCamera(
    const SystemConfig& config,
    std::vector<Camera>& cameras,
    int& totalFrames,
    int& currentFrame,
    glm::vec3& freeCameraPos,
    float& freeCameraYaw,
    float& freeCameraPitch,
    CameraSystem& cameraSystem
) {
    // Ensure free camera exists
    cameraSystem.EnsureFreeCameraExists(cameras, freeCameraPos, freeCameraYaw, freeCameraPitch);
    
    if (cameras.empty()) {
        cameraSystem.AddCamera("Free Camera", glm::vec3(0.0f, 0.0f, 5.0f), glm::vec3(0.0f, 0.0f, 0.0f), true, cameras);
        if (!cameras.empty()) {
            cameraSystem.SetActiveCameraIndex(0);
            freeCameraPos = cameras[0].position;
        }
    } else {
        size_t anchorIdx = cameraSystem.FindAnchorCameraIndex(cameras);
        if (!cameraSystem.HasFreeCamera(cameras) && anchorIdx < cameras.size()) {
            const Camera& anchorCam = cameras[anchorIdx];
            glm::vec3 direction = glm::normalize(anchorCam.target - anchorCam.position);
            float yaw, pitch;
            CameraSystem::CalculateYawPitchFromDirection(direction, yaw, pitch);
            cameraSystem.AddCamera("Free Camera", anchorCam.position, anchorCam.target, true, cameras);
            freeCameraPos = anchorCam.position;
            freeCameraYaw = yaw;
            freeCameraPitch = pitch;
        } else if (anchorIdx < cameras.size()) {
            // Update existing free camera to match anchor
            const Camera& anchorCam = cameras[anchorIdx];
            for (size_t i = 0; i < cameras.size(); i++) {
                if (cameras[i].canMove) {
                    cameras[i].position = anchorCam.position;
                    cameras[i].target = anchorCam.target;
                    freeCameraPos = anchorCam.position;
                    glm::vec3 direction = glm::normalize(anchorCam.target - anchorCam.position);
                    CameraSystem::CalculateYawPitchFromDirection(direction, freeCameraYaw, freeCameraPitch);
                    break;
                }
            }
        }
    }
    
    // Add config camera if it's not a free camera
    if (!cameras.empty() && !cameras[0].canMove) {
        cameraSystem.AddCamera("Config Camera", config.camera.position, config.camera.target, false, cameras);
        cameraSystem.EnsureFreeCameraExists(cameras, freeCameraPos, freeCameraYaw, freeCameraPitch);
    }
    
    totalFrames = 0;
    currentFrame = 0;
}


} // namespace CameraSystem
} // namespace Editors
} // namespace Engine

