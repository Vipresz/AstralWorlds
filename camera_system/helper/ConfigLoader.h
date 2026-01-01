#pragma once

#include <string>
#include <vector>
#include <glm/glm.hpp>

namespace Engine {
namespace Editors {
namespace CameraSystem {

struct PathPoint {
    glm::vec3 position;
    float time;
};

// Object keyframe for frame-based animation
struct ObjectKeyframe {
    glm::vec3 position;
    int frame;  // Frame number at which this keyframe applies (0, 30, 60, ...)
};

struct ObjectConfig {
    std::string name;
    std::string objFile;
    std::string animationFbx;
    std::string skelFile;
    std::string weightsFile;
    std::vector<PathPoint> path;  // Legacy: time-based path (for backward compatibility)
    std::vector<ObjectKeyframe> keyframes;  // New: frame-based positions
    float startTime;
    bool visible;
    glm::vec3 scale;
    glm::vec3 rotation;
};

struct CameraConfig {
    glm::vec3 position;
    glm::vec3 target;
    float distance;
    float yaw;
    float pitch;
};

// Camera keyframe for frame-based animation
struct CameraKeyframe {
    glm::vec3 position;
    glm::vec3 direction;  // Normalized direction vector
    int frame;            // Frame number at which this keyframe applies (0, 30, 60, ...)
};

// Animated camera configuration
struct AnimatedCameraConfig {
    std::string name;
    bool canMove;  // Can this camera be moved with WASD/mouse?
    std::vector<CameraKeyframe> keyframes;  // Position and direction at each time tick
};

// Light configuration
enum class LightType {
    Directional,
    Point,
    Spot
};

struct LightConfig {
    std::string name;
    LightType type;
    glm::vec3 colour;           // Light color (RGB)
    float ambientIntensity;      // Ambient light intensity
    float diffuseIntensity;      // Diffuse light intensity
    
    // For directional light: direction vector
    // For point/spot light: position
    glm::vec3 directionOrPosition;
    
    // For point/spot lights only:
    float constant;              // Attenuation constant
    float linear;                 // Attenuation linear
    float exponent;               // Attenuation exponent
    
    // For spot lights only:
    float edge;                   // Spot light edge angle (cosine)
    
    LightConfig() 
        : type(LightType::Directional)
        , colour(1.0f, 1.0f, 1.0f)
        , ambientIntensity(0.3f)
        , diffuseIntensity(0.7f)
        , directionOrPosition(0.0f, 0.0f, -1.0f)
        , constant(1.0f)
        , linear(0.09f)
        , exponent(0.032f)
        , edge(0.0f)
    {}
};

struct SystemConfig {
    std::string version;
    CameraConfig camera;  // Legacy single camera config (for backward compatibility)
    float totalDuration;
    std::vector<ObjectConfig> objects;
    
    // New: Animated cameras with frame-based keyframes
    std::vector<AnimatedCameraConfig> animatedCameras;
    int totalFrames;  // Total number of frames (for looping)
    
    // Lights
    std::vector<LightConfig> lights;
    
    // Capture settings
    bool captureEnabled;
    std::string captureOutputDir;
    int captureInterval;  // Frame interval for capturing (default: 1 = every frame)
};

class ConfigLoader {
public:
    // Load configuration from JSON file
    static bool LoadFromFile(const std::string& filepath, SystemConfig& config);
    
    // Save configuration to JSON file (for debugging)
    static bool SaveToFile(const std::string& filepath, const SystemConfig& config);
    
private:
    static bool ParseJsonFile(const std::string& filepath, SystemConfig& config);
};

} // namespace CameraSystem
} // namespace Editors
} // namespace Engine


