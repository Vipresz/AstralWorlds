#include "Animation.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <algorithm>
#include <cmath>
#include <glm/gtc/quaternion.hpp>
#include "platform/Logger.h"

namespace Engine {
namespace Editors {
namespace Skeleton {

// BoneAnimationChannel implementation
void BoneAnimationChannel::GetTransformAtTime(float time, glm::vec3& outPosition, 
                                               glm::quat& outRotation, glm::vec3& outScale) const {
    if (keyframes.empty()) {
        outPosition = glm::vec3(0.0f);
        outRotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        outScale = glm::vec3(1.0f);
        return;
    }
    
    // If time is before first keyframe, use first keyframe
    if (time <= keyframes[0].time) {
        outPosition = keyframes[0].position;
        outRotation = keyframes[0].rotation;
        outScale = keyframes[0].scale;
        return;
    }
    
    // If time is after last keyframe, use last keyframe
    if (time >= keyframes.back().time) {
        outPosition = keyframes.back().position;
        outRotation = keyframes.back().rotation;
        outScale = keyframes.back().scale;
        return;
    }
    
    // Find the two keyframes to interpolate between
    size_t keyframeIndex = 0;
    for (size_t i = 0; i < keyframes.size() - 1; i++) {
        if (time >= keyframes[i].time && time <= keyframes[i + 1].time) {
            keyframeIndex = i;
            break;
        }
    }
    
    const BoneKeyframe& keyframe0 = keyframes[keyframeIndex];
    const BoneKeyframe& keyframe1 = keyframes[keyframeIndex + 1];
    
    // Calculate interpolation factor
    float deltaTime = keyframe1.time - keyframe0.time;
    float factor = (deltaTime > 0.0f) ? (time - keyframe0.time) / deltaTime : 0.0f;
    factor = std::max(0.0f, std::min(1.0f, factor)); // Clamp to [0, 1]
    
    // Interpolate position and scale (linear)
    outPosition = glm::mix(keyframe0.position, keyframe1.position, factor);
    outScale = glm::mix(keyframe0.scale, keyframe1.scale, factor);
    
    // Interpolate rotation (spherical linear interpolation for quaternions)
    outRotation = glm::slerp(keyframe0.rotation, keyframe1.rotation, factor);
}

// Animation implementation
Animation::Animation()
    : duration(0.0f)
    , ticksPerSecond(25.0f) // Default 25 FPS
{
}

Animation::~Animation() {
}

bool Animation::GetBoneTransformAtTime(const std::string& boneName, float time,
                                       glm::vec3& outPosition, glm::quat& outRotation, glm::vec3& outScale) const {
    auto it = channels.find(boneName);
    if (it == channels.end()) {
        return false; // Bone not animated
    }
    
    it->second.GetTransformAtTime(time, outPosition, outRotation, outScale);
    return true;
}

bool Animation::IsBoneAnimated(const std::string& boneName) const {
    return channels.find(boneName) != channels.end();
}

// AnimationPlayer implementation
AnimationPlayer::AnimationPlayer()
    : m_currentTime(0.0f)
    , m_isPlaying(false)
    , m_isPaused(false)
    , m_loop(true)
    , m_playbackSpeed(1.0f)
{
}

AnimationPlayer::~AnimationPlayer() {
}

bool AnimationPlayer::LoadAnimation(const std::string& filePath, const std::string& animationName) {
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(
        filePath,
        aiProcess_Triangulate |
        aiProcess_FlipUVs |
        aiProcess_GenSmoothNormals
    );
    
    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        LOG_ERROR_FMT("Animation", "Failed to load animation file: %s - %s", filePath.c_str(), importer.GetErrorString());
        return false;
    }
    
    if (!scene->HasAnimations()) {
        LOG_WARNING_FMT("Animation", "File has no animations: %s", filePath.c_str());
        return false;
    }
    
    // Load the first animation (FBX files typically have one animation)
    aiAnimation* aiAnim = scene->mAnimations[0];
    
    Animation anim;
    
    // Determine animation name
    if (!animationName.empty()) {
        anim.name = animationName;
    } else {
        anim.name = std::string(aiAnim->mName.C_Str());
        if (anim.name.empty()) {
            // Extract name from file path
            size_t lastSlash = filePath.find_last_of("/\\");
            size_t lastDot = filePath.find_last_of('.');
            if (lastSlash != std::string::npos && lastDot != std::string::npos && lastDot > lastSlash) {
                anim.name = filePath.substr(lastSlash + 1, lastDot - lastSlash - 1);
            } else {
                anim.name = "Animation";
            }
        }
    }
    
    // Get animation duration and ticks per second
    double durationInTicks = static_cast<double>(aiAnim->mDuration);
    double ticksPerSecond = static_cast<double>(aiAnim->mTicksPerSecond);
    
    LOG_INFO_FMT("Animation", "Raw animation data: duration=%.2f ticks, ticksPerSecond=%.2f", 
                durationInTicks, ticksPerSecond);
    
    if (ticksPerSecond <= 0.0) {
        ticksPerSecond = 25.0; // Default to 25 FPS
        LOG_WARNING_FMT("Animation", "Invalid ticksPerSecond, using default: %.2f", ticksPerSecond);
    }
    
    anim.ticksPerSecond = static_cast<float>(ticksPerSecond);
    
    // Convert duration from ticks to seconds
    anim.duration = static_cast<float>(durationInTicks / ticksPerSecond);
    
    LOG_INFO_FMT("Animation", "Converted duration: %.2f seconds", anim.duration);
    
    LOG_INFO_FMT("Animation", "Processing %d animation channels from FBX", aiAnim->mNumChannels);
    
    // Process all animation channels (one per bone)
    for (unsigned int i = 0; i < aiAnim->mNumChannels; i++) {
        aiNodeAnim* channel = aiAnim->mChannels[i];
        if (!channel) {
            LOG_WARNING_FMT("Animation", "Channel %d is null, skipping", i);
            continue;
        }
        
        std::string boneName = std::string(channel->mNodeName.C_Str());
        if (boneName.empty()) {
            LOG_WARNING_FMT("Animation", "Channel %d has empty bone name, skipping", i);
            continue;
        }
        
        LOG_INFO_FMT("Animation", "Processing channel %d: bone='%s', posKeys=%d, rotKeys=%d, scaleKeys=%d", 
                    i, boneName.c_str(), channel->mNumPositionKeys, channel->mNumRotationKeys, channel->mNumScalingKeys);
        
        BoneAnimationChannel boneChannel;
        boneChannel.boneName = boneName;
        
        // Extract position keyframes
        for (unsigned int j = 0; j < channel->mNumPositionKeys; j++) {
            aiVectorKey key = channel->mPositionKeys[j];
            if (!channel->mPositionKeys) {
                LOG_WARNING_FMT("Animation", "Bone '%s' has mNumPositionKeys=%d but mPositionKeys is null!", 
                               boneName.c_str(), channel->mNumPositionKeys);
                break;
            }
            
            BoneKeyframe keyframe;
            // Convert time from ticks to seconds
            double timeInTicks = static_cast<double>(key.mTime);
            keyframe.time = static_cast<float>(timeInTicks / anim.ticksPerSecond);
            keyframe.position = glm::vec3(key.mValue.x, key.mValue.y, key.mValue.z);
            
            // Try to find existing keyframe at this time (for rotation/scale)
            auto existingIt = std::find_if(boneChannel.keyframes.begin(), boneChannel.keyframes.end(),
                [keyframe](const BoneKeyframe& kf) { return std::abs(kf.time - keyframe.time) < 0.001f; });
            
            if (existingIt != boneChannel.keyframes.end()) {
                existingIt->position = keyframe.position;
            } else {
                // Default rotation and scale
                keyframe.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
                keyframe.scale = glm::vec3(1.0f, 1.0f, 1.0f);
                boneChannel.keyframes.push_back(keyframe);
            }
        }
        
        // Extract rotation keyframes
        if (channel->mNumRotationKeys > 0 && !channel->mRotationKeys) {
            LOG_WARNING_FMT("Animation", "Bone '%s' has mNumRotationKeys=%d but mRotationKeys is null!", 
                           boneName.c_str(), channel->mNumRotationKeys);
        }
        
        for (unsigned int j = 0; j < channel->mNumRotationKeys; j++) {
            if (!channel->mRotationKeys) break;
            
            aiQuatKey key = channel->mRotationKeys[j];
            // Convert time from ticks to seconds
            double timeInTicks = static_cast<double>(key.mTime);
            float time = static_cast<float>(timeInTicks / anim.ticksPerSecond);
            
            // Find or create keyframe at this time
            auto keyframeIt = std::find_if(boneChannel.keyframes.begin(), boneChannel.keyframes.end(),
                [time](const BoneKeyframe& kf) { return std::abs(kf.time - time) < 0.001f; });
            
            if (keyframeIt != boneChannel.keyframes.end()) {
                keyframeIt->rotation = glm::quat(key.mValue.w, key.mValue.x, key.mValue.y, key.mValue.z);
            } else {
                BoneKeyframe keyframe;
                keyframe.time = time;
                keyframe.position = glm::vec3(0.0f);
                keyframe.rotation = glm::quat(key.mValue.w, key.mValue.x, key.mValue.y, key.mValue.z);
                keyframe.scale = glm::vec3(1.0f, 1.0f, 1.0f);
                boneChannel.keyframes.push_back(keyframe);
            }
        }
        
        // Extract scale keyframes
        if (channel->mNumScalingKeys > 0 && !channel->mScalingKeys) {
            LOG_WARNING_FMT("Animation", "Bone '%s' has mNumScalingKeys=%d but mScalingKeys is null!", 
                           boneName.c_str(), channel->mNumScalingKeys);
        }
        
        for (unsigned int j = 0; j < channel->mNumScalingKeys; j++) {
            if (!channel->mScalingKeys) break;
            
            aiVectorKey key = channel->mScalingKeys[j];
            // Convert time from ticks to seconds
            double timeInTicks = static_cast<double>(key.mTime);
            float time = static_cast<float>(timeInTicks / anim.ticksPerSecond);
            
            // Find or create keyframe at this time
            auto keyframeIt = std::find_if(boneChannel.keyframes.begin(), boneChannel.keyframes.end(),
                [time](const BoneKeyframe& kf) { return std::abs(kf.time - time) < 0.001f; });
            
            if (keyframeIt != boneChannel.keyframes.end()) {
                keyframeIt->scale = glm::vec3(key.mValue.x, key.mValue.y, key.mValue.z);
            } else {
                BoneKeyframe keyframe;
                keyframe.time = time;
                keyframe.position = glm::vec3(0.0f);
                keyframe.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
                keyframe.scale = glm::vec3(key.mValue.x, key.mValue.y, key.mValue.z);
                boneChannel.keyframes.push_back(keyframe);
            }
        }
        
        // Sort keyframes by time
        std::sort(boneChannel.keyframes.begin(), boneChannel.keyframes.end(),
            [](const BoneKeyframe& a, const BoneKeyframe& b) { return a.time < b.time; });
        
        if (boneChannel.keyframes.empty()) {
            LOG_WARNING_FMT("Animation", "Bone '%s' has no keyframes after processing!", boneName.c_str());
        } else {
            LOG_INFO_FMT("Animation", "Bone '%s' has %zu keyframes (time range: %.2f to %.2f)", 
                        boneName.c_str(), boneChannel.keyframes.size(), 
                        boneChannel.keyframes.front().time, boneChannel.keyframes.back().time);
        }
        
        anim.channels[boneName] = boneChannel;
    }
    
    LOG_INFO_FMT("Animation", "Total bones processed: %zu", anim.channels.size());
    
    m_animations[anim.name] = anim;
    LOG_INFO_FMT("Animation", "Loaded animation '%s' from %s (duration: %.2fs, bones: %d)", 
                anim.name.c_str(), filePath.c_str(), anim.duration, static_cast<int>(anim.channels.size()));
    
    // Log bone names in animation for debugging
    if (anim.channels.size() > 0) {
        LOG_INFO_FMT("Animation", "Animation '%s' contains bones:", anim.name.c_str());
        int count = 0;
        for (const auto& pair : anim.channels) {
            if (count < 10) { // Log first 10 bones
                LOG_INFO_FMT("Animation", "  - %s (%zu keyframes)", pair.first.c_str(), pair.second.keyframes.size());
            }
            count++;
        }
        if (count > 10) {
            LOG_INFO_FMT("Animation", "  ... and %d more bones", count - 10);
        }
    }
    
    return true;
}

void AnimationPlayer::Play(const std::string& animationName) {
    if (m_animations.find(animationName) == m_animations.end()) {
        LOG_WARNING_FMT("Animation", "Animation '%s' not found", animationName.c_str());
        return;
    }
    
    m_currentAnimationName = animationName;
    m_currentTime = 0.0f;
    m_isPlaying = true;
    m_isPaused = false;
    LOG_INFO_FMT("Animation", "Playing animation: %s", animationName.c_str());
}

void AnimationPlayer::Stop() {
    m_isPlaying = false;
    m_isPaused = false;
    m_currentTime = 0.0f;
    m_currentAnimationName.clear();
}

void AnimationPlayer::Pause() {
    if (m_isPlaying) {
        m_isPaused = true;
    }
}

void AnimationPlayer::Resume() {
    if (m_isPlaying && m_isPaused) {
        m_isPaused = false;
    }
}

void AnimationPlayer::Update(float deltaTime) {
    if (!m_isPlaying || m_isPaused) {
        return;
    }
    
    auto animIt = m_animations.find(m_currentAnimationName);
    if (animIt == m_animations.end()) {
        return;
    }
    
    const Animation& anim = animIt->second;
    
    // Update time
    m_currentTime += deltaTime * m_playbackSpeed;
    
    // Handle looping
    if (m_loop && m_currentTime >= anim.duration) {
        m_currentTime = std::fmod(m_currentTime, anim.duration);
    } else if (!m_loop && m_currentTime >= anim.duration) {
        m_currentTime = anim.duration;
        m_isPlaying = false; // Stop at end if not looping
    }
}

bool AnimationPlayer::GetBoneTransform(const std::string& boneName, glm::vec3& outPosition,
                                       glm::quat& outRotation, glm::vec3& outScale) const {
    if (!m_isPlaying || m_currentAnimationName.empty()) {
        return false;
    }
    
    auto animIt = m_animations.find(m_currentAnimationName);
    if (animIt == m_animations.end()) {
        return false;
    }
    
    return animIt->second.GetBoneTransformAtTime(boneName, m_currentTime, outPosition, outRotation, outScale);
}

bool AnimationPlayer::GetBoneTransformAtTime(const std::string& boneName, float time, glm::vec3& outPosition,
                                              glm::quat& outRotation, glm::vec3& outScale) const {
    // If no current animation, try to use first loaded animation
    std::string animName = m_currentAnimationName;
    if (animName.empty() && !m_animations.empty()) {
        animName = m_animations.begin()->first;
    }
    
    if (animName.empty()) {
        return false;
    }
    
    auto animIt = m_animations.find(animName);
    if (animIt == m_animations.end()) {
        return false;
    }
    
    return animIt->second.GetBoneTransformAtTime(boneName, time, outPosition, outRotation, outScale);
}

float AnimationPlayer::GetAnimationDuration() const {
    if (m_currentAnimationName.empty()) {
        return 0.0f;
    }
    
    auto animIt = m_animations.find(m_currentAnimationName);
    if (animIt == m_animations.end()) {
        return 0.0f;
    }
    
    return animIt->second.duration;
}

std::vector<std::string> AnimationPlayer::GetLoadedAnimationNames() const {
    std::vector<std::string> names;
    names.reserve(m_animations.size());
    for (const auto& pair : m_animations) {
        names.push_back(pair.first);
    }
    return names;
}

std::vector<std::string> AnimationPlayer::GetAnimationBoneNames(const std::string& animationName) const {
    std::vector<std::string> boneNames;
    auto it = m_animations.find(animationName);
    if (it != m_animations.end()) {
        boneNames.reserve(it->second.channels.size());
        for (const auto& pair : it->second.channels) {
            boneNames.push_back(pair.first);
        }
    }
    return boneNames;
}

} // namespace Skeleton
} // namespace Editors
} // namespace Engine


