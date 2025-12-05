#pragma once

#include <assimp/scene.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>
#include <vector>
#include <map>

namespace Engine {
namespace Platform {
namespace Render {

// Animation channel (keyframes for one bone)
struct AnimationChannel {
    std::string boneName;
    std::vector<float> positionTimestamps;
    std::vector<glm::vec3> positions;
    std::vector<float> rotationTimestamps;
    std::vector<glm::quat> rotations;
    std::vector<float> scaleTimestamps;
    std::vector<glm::vec3> scales;
    
    // Get interpolated transform at a given time
    glm::mat4 GetTransform(float animationTime, const glm::mat4& defaultTransform) const;
    
private:
    int FindPositionIndex(float animationTime) const;
    int FindRotationIndex(float animationTime) const;
    int FindScalingIndex(float animationTime) const;
    
    glm::vec3 InterpolatePosition(float animationTime) const;
    glm::quat InterpolateRotation(float animationTime) const;
    glm::vec3 InterpolateScale(float animationTime) const;
};

// Animation class - loads and manages a single animation from FBX
class Animation {
public:
    Animation();
    ~Animation();
    
    // Load animation from file
    bool LoadAnimation(const std::string& filePath);
    
    // Get animation duration in seconds
    float GetDuration() const { return m_duration; }
    
    // Get ticks per second (animation speed)
    float GetTicksPerSecond() const { return m_ticksPerSecond; }
    
    // Get bone transforms at a given time (in ticks)
    void GetBoneTransforms(float timeInTicks, std::map<std::string, glm::mat4>& boneTransforms) const;
    
    // Get all animation channel names (bone names)
    std::vector<std::string> GetBoneNames() const;
    
    // Check if animation is loaded
    bool IsLoaded() const { return m_loaded; }

private:
    bool m_loaded;
    float m_duration; // Total animation duration in ticks
    float m_ticksPerSecond; // Animation playback speed
    
    std::map<std::string, AnimationChannel> m_channels; // Animation channels per bone
    
    // Helper methods
    void LoadAnimationChannels(const aiAnimation* animation);
};

} // namespace Render
} // namespace Platform
} // namespace Engine

