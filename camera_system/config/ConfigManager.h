#pragma once

#include "../helper/ConfigLoader.h"
#include "../camera/CameraSystem.h"
#include "../objects/ObjectManager.h"
#include "../GlobalConstants.h"
#include "../../engine/platform/Logger.h"
#include <string>
#include <vector>
#include <filesystem>
#include <memory>
#include <algorithm>
#include <glm/glm.hpp>

namespace Engine {
namespace Editors {
namespace CameraSystem {

// Forward declarations
struct Camera;
struct SceneObject;
class CameraSystemEditor;

// Frame data structures - forward declare to match CameraSystemEditor
// These will be defined to match CameraSystemEditor::CameraFrame and CameraSystemEditor::ObjectFrame
struct CameraFrame {
    int tick;
    glm::vec3 position;
    glm::vec3 orientation;
};

struct ObjectFrame {
    int frame;
    glm::vec3 position;
};

// Constants (matching CameraSystemEditor)
static constexpr int MAX_CAMERAS = 100;
static constexpr int MAX_FRAMES = 300;
static constexpr int MAX_OBJECTS = 20;

class ConfigManager {
public:
    // Capture settings structure
    struct CaptureSettings {
        bool enabled;
        std::string outputDir;
        int interval;
    };
    
    // Load capture settings from config
    static CaptureSettings LoadCaptureSettings(const SystemConfig& config, bool preserveExisting = false, bool existingEnabled = false);
    
    // Load animated cameras from config
    // Note: frames parameter accepts CameraSystemEditor::CameraFrame[MAX_CAMERAS][MAX_FRAMES]
    template<typename CameraFrameType>
    static void LoadAnimatedCameras(
        const SystemConfig& config,
        std::vector<Camera>& cameras,
        CameraFrameType frames[MAX_CAMERAS][MAX_FRAMES],
        int frameCounts[MAX_CAMERAS],
        int& numCameras,
        int& totalFrames,
        int& currentFrame,
        bool& isPlaying,
        glm::vec3& freeCameraPos,
        float& freeCameraYaw,
        float& freeCameraPitch,
        CameraSystem& cameraSystem
    );
    
    // Load legacy camera config
    static void LoadLegacyCamera(
        const SystemConfig& config,
        std::vector<Camera>& cameras,
        int& totalFrames,
        int& currentFrame,
        glm::vec3& freeCameraPos,
        float& freeCameraYaw,
        float& freeCameraPitch,
        CameraSystem& cameraSystem
    );
    
    // Load objects from config
    // Note: objectFrames parameter accepts CameraSystemEditor::ObjectFrame[MAX_OBJECTS][MAX_FRAMES]
    template<typename ObjectFrameType>
    static void LoadObjects(
        const SystemConfig& config,
        const std::string& configFile,
        ObjectManager& objectManager,
        std::vector<std::unique_ptr<SceneObject>>& objects,
        std::vector<std::vector<PathPoint>>& objectPaths,
        std::vector<float>& objectStartTimes,
        ObjectFrameType objectFrames[MAX_OBJECTS][MAX_FRAMES],
        int objectFrameCounts[MAX_OBJECTS],
        bool objectsUseFrames[MAX_OBJECTS],
        int totalFrames,
        int& numCameras
    );
    
    // Process keyframes into frame arrays
    template<typename CameraFrameType>
    static void FillCameraKeyframes(
        int cameraIndex,
        const std::vector<CameraKeyframe>& sortedKeyframes,
        int totalFrames,
        CameraFrameType frames[MAX_CAMERAS][MAX_FRAMES],
        int frameCounts[MAX_CAMERAS]
    );
    
    template<typename ObjectFrameType>
    static void FillObjectKeyframes(
        size_t objectIndex,
        const std::vector<ObjectKeyframe>& sortedKeyframes,
        int totalFrames,
        ObjectFrameType frames[MAX_OBJECTS][MAX_FRAMES],
        int frameCounts[MAX_OBJECTS],
        bool useFrames[MAX_OBJECTS],
        std::vector<std::unique_ptr<SceneObject>>& objects
    );
};

} // namespace CameraSystem
} // namespace Editors
} // namespace Engine

// Include template implementations
#include "ConfigManager.inl"

