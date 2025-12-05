#include "render/Animation.h"
#include "Logger.h"
#include <algorithm>
#include <cmath>

namespace Engine {
namespace Platform {
namespace Render {

Animation::Animation() 
    : m_loaded(false)
    , m_duration(0.0f)
    , m_ticksPerSecond(25.0f) // Default animation speed
{
}

Animation::~Animation() {
}

bool Animation::LoadAnimation(const std::string& filePath) {
    m_loaded = false;
    m_channels.clear();
    m_duration = 0.0f;
    m_ticksPerSecond = 25.0f;
    
    Assimp::Importer importer;
    
    // Import animation file - for animations, we only need basic processing
    // We're loading animation data, not meshes, so minimal post-processing is needed
    const aiScene* scene = importer.ReadFile(
        filePath.c_str(),
        aiProcess_Triangulate |  // Basic processing (in case file has meshes)
        aiProcess_JoinIdenticalVertices
    );
    
    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        LOG_ERROR_FMT("Animation", "Failed to load animation file: %s", filePath.c_str());
        LOG_ERROR_FMT("Animation", "Assimp error: %s", importer.GetErrorString());
        return false;
    }
    
    if (!scene->HasAnimations()) {
        LOG_WARNING_FMT("Animation", "Animation file %s has no animations", filePath.c_str());
        return false;
    }
    
    const aiAnimation* animation = scene->mAnimations[0];
    
    m_duration = static_cast<float>(animation->mDuration);
    m_ticksPerSecond = static_cast<float>(animation->mTicksPerSecond);
    
    if (m_ticksPerSecond <= 0.0f) {
        m_ticksPerSecond = 25.0f;
    }
    
    LoadAnimationChannels(animation);
    
    m_loaded = true;
    float durationSeconds = m_duration / m_ticksPerSecond;
    LOG_INFO_FMT("Animation", "Animation loaded: %s (%.2f seconds, %zu channels)", filePath.c_str(), durationSeconds, m_channels.size());
    
    return true;
}

void Animation::LoadAnimationChannels(const aiAnimation* animation) {
    for (unsigned int i = 0; i < animation->mNumChannels; i++) {
        const aiNodeAnim* channel = animation->mChannels[i];
        std::string boneName = std::string(channel->mNodeName.C_Str());
        
        AnimationChannel animChannel;
        animChannel.boneName = boneName;
        
        // Load position keyframes
        animChannel.positionTimestamps.reserve(channel->mNumPositionKeys);
        animChannel.positions.reserve(channel->mNumPositionKeys);
        for (unsigned int j = 0; j < channel->mNumPositionKeys; j++) {
            const aiVectorKey& key = channel->mPositionKeys[j];
            animChannel.positionTimestamps.push_back(static_cast<float>(key.mTime));
            
            const aiVector3D& pos = key.mValue;
            animChannel.positions.push_back(glm::vec3(pos.x, pos.y, pos.z));
        }
        
        // Load rotation keyframes
        animChannel.rotationTimestamps.reserve(channel->mNumRotationKeys);
        animChannel.rotations.reserve(channel->mNumRotationKeys);
        for (unsigned int j = 0; j < channel->mNumRotationKeys; j++) {
            const aiQuatKey& key = channel->mRotationKeys[j];
            animChannel.rotationTimestamps.push_back(static_cast<float>(key.mTime));
            
            const aiQuaternion& rot = key.mValue;
            animChannel.rotations.push_back(glm::quat(rot.w, rot.x, rot.y, rot.z));
        }
        
        // Load scale keyframes
        animChannel.scaleTimestamps.reserve(channel->mNumScalingKeys);
        animChannel.scales.reserve(channel->mNumScalingKeys);
        for (unsigned int j = 0; j < channel->mNumScalingKeys; j++) {
            const aiVectorKey& key = channel->mScalingKeys[j];
            animChannel.scaleTimestamps.push_back(static_cast<float>(key.mTime));
            
            const aiVector3D& scale = key.mValue;
            animChannel.scales.push_back(glm::vec3(scale.x, scale.y, scale.z));
        }
        
        m_channels[boneName] = animChannel;
    }
}

void Animation::GetBoneTransforms(float timeInTicks, std::map<std::string, glm::mat4>& boneTransforms) const {
    if (!m_loaded) {
        return;
    }
    
    // Loop through all channels and get interpolated transforms
    for (const auto& pair : m_channels) {
        const AnimationChannel& channel = pair.second;
        
        // Get interpolated transform at this time
        glm::mat4 transform = channel.GetTransform(timeInTicks, glm::mat4(1.0f));
        boneTransforms[channel.boneName] = transform;
    }
}

// AnimationChannel implementation
int AnimationChannel::FindPositionIndex(float animationTime) const {
    for (size_t i = 0; i < positionTimestamps.size() - 1; i++) {
        if (animationTime < positionTimestamps[i + 1]) {
            return static_cast<int>(i);
        }
    }
    return static_cast<int>(positionTimestamps.size() - 1);
}

int AnimationChannel::FindRotationIndex(float animationTime) const {
    for (size_t i = 0; i < rotationTimestamps.size() - 1; i++) {
        if (animationTime < rotationTimestamps[i + 1]) {
            return static_cast<int>(i);
        }
    }
    return static_cast<int>(rotationTimestamps.size() - 1);
}

int AnimationChannel::FindScalingIndex(float animationTime) const {
    for (size_t i = 0; i < scaleTimestamps.size() - 1; i++) {
        if (animationTime < scaleTimestamps[i + 1]) {
            return static_cast<int>(i);
        }
    }
    return static_cast<int>(scaleTimestamps.size() - 1);
}

glm::vec3 AnimationChannel::InterpolatePosition(float animationTime) const {
    if (positions.size() == 1) {
        return positions[0];
    }
    
    int p0Index = FindPositionIndex(animationTime);
    int p1Index = p0Index + 1;
    
    if (p1Index >= static_cast<int>(positions.size())) {
        return positions.back();
    }
    
    float deltaTime = positionTimestamps[p1Index] - positionTimestamps[p0Index];
    float factor = (deltaTime > 0.0f) ? (animationTime - positionTimestamps[p0Index]) / deltaTime : 0.0f;
    factor = std::max(0.0f, std::min(1.0f, factor)); // Clamp to [0, 1]
    
    const glm::vec3& start = positions[p0Index];
    const glm::vec3& end = positions[p1Index];
    
    return glm::mix(start, end, factor);
}

glm::quat AnimationChannel::InterpolateRotation(float animationTime) const {
    if (rotations.size() == 1) {
        return rotations[0];
    }
    
    int p0Index = FindRotationIndex(animationTime);
    int p1Index = p0Index + 1;
    
    if (p1Index >= static_cast<int>(rotations.size())) {
        return rotations.back();
    }
    
    float deltaTime = rotationTimestamps[p1Index] - rotationTimestamps[p0Index];
    float factor = (deltaTime > 0.0f) ? (animationTime - rotationTimestamps[p0Index]) / deltaTime : 0.0f;
    factor = std::max(0.0f, std::min(1.0f, factor)); // Clamp to [0, 1]
    
    const glm::quat& start = rotations[p0Index];
    const glm::quat& end = rotations[p1Index];
    
    return glm::slerp(start, end, factor);
}

glm::vec3 AnimationChannel::InterpolateScale(float animationTime) const {
    if (scales.size() == 1) {
        return scales[0];
    }
    
    int p0Index = FindScalingIndex(animationTime);
    int p1Index = p0Index + 1;
    
    if (p1Index >= static_cast<int>(scales.size())) {
        return scales.back();
    }
    
    float deltaTime = scaleTimestamps[p1Index] - scaleTimestamps[p0Index];
    float factor = (deltaTime > 0.0f) ? (animationTime - scaleTimestamps[p0Index]) / deltaTime : 0.0f;
    factor = std::max(0.0f, std::min(1.0f, factor)); // Clamp to [0, 1]
    
    const glm::vec3& start = scales[p0Index];
    const glm::vec3& end = scales[p1Index];
    
    return glm::mix(start, end, factor);
}

glm::mat4 AnimationChannel::GetTransform(float animationTime, const glm::mat4& defaultTransform) const {
    // Clamp animation time to valid range
    float maxTime = 0.0f;
    if (!positionTimestamps.empty()) {
        maxTime = std::max(maxTime, positionTimestamps.back());
    }
    if (!rotationTimestamps.empty()) {
        maxTime = std::max(maxTime, rotationTimestamps.back());
    }
    if (!scaleTimestamps.empty()) {
        maxTime = std::max(maxTime, scaleTimestamps.back());
    }
    
    if (maxTime > 0.0f) {
        animationTime = std::fmod(animationTime, maxTime); // Loop animation
    }
    
    // Get interpolated values
    glm::vec3 position = positions.empty() ? glm::vec3(0.0f) : InterpolatePosition(animationTime);
    glm::quat rotation = rotations.empty() ? glm::quat(1.0f, 0.0f, 0.0f, 0.0f) : InterpolateRotation(animationTime);
    glm::vec3 scale = scales.empty() ? glm::vec3(1.0f) : InterpolateScale(animationTime);
    
    // Build transformation matrix: T * R * S
    glm::mat4 transform = glm::mat4(1.0f);
    transform = glm::translate(transform, position);
    transform = transform * glm::mat4_cast(rotation);
    transform = glm::scale(transform, scale);
    
    return transform;
}

std::vector<std::string> Animation::GetBoneNames() const {
    std::vector<std::string> boneNames;
    boneNames.reserve(m_channels.size());
    for (const auto& pair : m_channels) {
        boneNames.push_back(pair.first);
    }
    return boneNames;
}

} // namespace Render
} // namespace Platform
} // namespace Engine

