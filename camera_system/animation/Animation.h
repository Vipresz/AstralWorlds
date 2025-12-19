#pragma once

#include <string>
#include <vector>
#include <map>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Engine {
namespace Editors {
namespace CameraSystem {

// Keyframe for a single bone at a specific time
struct BoneKeyframe {
    float time;              // Time in seconds
    glm::vec3 position;      // Position at this keyframe
    glm::quat rotation;      // Rotation at this keyframe
    glm::vec3 scale;         // Scale at this keyframe (usually 1,1,1)
    
    BoneKeyframe()
        : time(0.0f)
        , position(0.0f, 0.0f, 0.0f)
        , rotation(1.0f, 0.0f, 0.0f, 0.0f)
        , scale(1.0f, 1.0f, 1.0f)
    {}
};

// Animation channel for a single bone (contains all keyframes for that bone)
struct BoneAnimationChannel {
    std::string boneName;
    std::vector<BoneKeyframe> keyframes;
    
    // Interpolate between keyframes to get transform at given time
    void GetTransformAtTime(float time, glm::vec3& outPosition, glm::quat& outRotation, glm::vec3& outScale) const;
};

// Complete animation (contains channels for all animated bones)
class Animation {
public:
    Animation();
    ~Animation();
    
    std::string name;                    // Animation name
    float duration;                      // Total animation duration in seconds
    float ticksPerSecond;                // Animation ticks per second (from FBX)
    
    // Animation channels indexed by bone name
    std::map<std::string, BoneAnimationChannel> channels;
    
    // Get transform for a bone at a specific time
    bool GetBoneTransformAtTime(const std::string& boneName, float time, 
                                glm::vec3& outPosition, glm::quat& outRotation, glm::vec3& outScale) const;
    
    // Check if bone is animated
    bool IsBoneAnimated(const std::string& boneName) const;
};

// Animation player that manages playing animations
class AnimationPlayer {
public:
    AnimationPlayer();
    ~AnimationPlayer();
    
    // Load animation from FBX file
    bool LoadAnimation(const std::string& filePath, const std::string& animationName = "");
    
    // Animation playback control
    void Play(const std::string& animationName);
    void Stop();
    void Pause();
    void Resume();
    
    // Update animation (call each frame with delta time)
    void Update(float deltaTime);
    
    // Get current bone transforms (applies current animation state)
    bool GetBoneTransform(const std::string& boneName, glm::vec3& outPosition, 
                         glm::quat& outRotation, glm::vec3& outScale) const;
    
    // Get bone transform at a specific time (for manual time control)
    bool GetBoneTransformAtTime(const std::string& boneName, float time, glm::vec3& outPosition,
                                glm::quat& outRotation, glm::vec3& outScale) const;
    
    // Animation state
    bool IsPlaying() const { return m_isPlaying && !m_isPaused; }
    bool IsPaused() const { return m_isPaused; }
    float GetCurrentTime() const { return m_currentTime; }
    float GetAnimationDuration() const;
    const std::string& GetCurrentAnimationName() const { return m_currentAnimationName; }
    
    // Get list of loaded animation names
    std::vector<std::string> GetLoadedAnimationNames() const;
    
    // Get list of bone names in a specific animation (for debugging/matching)
    std::vector<std::string> GetAnimationBoneNames(const std::string& animationName) const;
    
    // Playback settings
    void SetLoop(bool loop) { m_loop = loop; }
    void SetPlaybackSpeed(float speed) { m_playbackSpeed = speed; }
    float GetPlaybackSpeed() const { return m_playbackSpeed; }
    
private:
    std::map<std::string, Animation> m_animations;  // Loaded animations
    std::string m_currentAnimationName;             // Currently playing animation
    float m_currentTime;                            // Current playback time
    bool m_isPlaying;                               // Is animation playing
    bool m_isPaused;                                // Is animation paused
    bool m_loop;                                    // Loop animation
    float m_playbackSpeed;                          // Playback speed multiplier
};

} // namespace CameraSystem
} // namespace Editors
} // namespace Engine


