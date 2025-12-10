#include "SkeletonDefinition.h"
#include <algorithm>
#include <glm/gtc/quaternion.hpp>

namespace Engine {
namespace Editors {
namespace Skeleton {

glm::mat4 BoneDefinition::GetLocalTransform() const {
    glm::mat4 T = glm::translate(glm::mat4(1.0f), position);
    glm::mat4 R = glm::mat4_cast(rotation);
    glm::mat4 S = glm::scale(glm::mat4(1.0f), scale);
    return T * R * S;
}

void BoneDefinition::CalculateOffsetMatrix(const glm::mat4& bindPoseTransform) {
    offsetMatrix = glm::inverse(bindPoseTransform);
}

SkeletonDefinition::SkeletonDefinition()
    : m_nextBoneID(0)
    , m_name("Untitled Skeleton")
    , m_description("")
{
}

SkeletonDefinition::~SkeletonDefinition() {
    Clear();
}

int SkeletonDefinition::AddBone(const std::string& name, const std::string& parentName) {
    if (m_bones.find(name) != m_bones.end()) {
        return -1;
    }
    
    if (!parentName.empty() && m_bones.find(parentName) == m_bones.end()) {
        return -1;
    }
    
    BoneDefinition bone;
    bone.name = name;
    bone.parentName = parentName;
    bone.id = GetNextBoneID();
    
    m_bones[name] = bone;
    m_boneArray.push_back(&m_bones[name]);
    UpdateChildrenMap();
    
    return bone.id;
}

bool SkeletonDefinition::RemoveBone(const std::string& name) {
    auto it = m_bones.find(name);
    if (it == m_bones.end()) {
        return false;
    }
    
    auto childrenIt = m_childrenMap.find(name);
    if (childrenIt != m_childrenMap.end() && !childrenIt->second.empty()) {
        return false;
    }
    
    if (!it->second.parentName.empty()) {
        auto parentIt = m_childrenMap.find(it->second.parentName);
        if (parentIt != m_childrenMap.end()) {
            auto& children = parentIt->second;
            children.erase(std::remove(children.begin(), children.end(), name), children.end());
        }
    }
    
    m_boneArray.erase(
        std::remove_if(m_boneArray.begin(), m_boneArray.end(),
            [&name](BoneDefinition* bone) { return bone->name == name; }),
        m_boneArray.end()
    );
    
    m_bones.erase(it);
    m_childrenMap.erase(name);
    
    return true;
}

BoneDefinition* SkeletonDefinition::GetBone(const std::string& name) {
    auto it = m_bones.find(name);
    return (it != m_bones.end()) ? &it->second : nullptr;
}

const BoneDefinition* SkeletonDefinition::GetBone(const std::string& name) const {
    auto it = m_bones.find(name);
    return (it != m_bones.end()) ? &it->second : nullptr;
}

BoneDefinition* SkeletonDefinition::GetBone(int id) {
    // Search through all bones to find the one with matching ID
    // (Bone IDs are not necessarily sequential, so we can't use array indexing)
    for (auto& pair : m_bones) {
        if (pair.second.id == id) {
            return &pair.second;
        }
    }
    return nullptr;
}

const BoneDefinition* SkeletonDefinition::GetBone(int id) const {
    // Search through all bones to find the one with matching ID
    // (Bone IDs are not necessarily sequential, so we can't use array indexing)
    for (const auto& pair : m_bones) {
        if (pair.second.id == id) {
            return &pair.second;
        }
    }
    return nullptr;
}

bool SkeletonDefinition::SetBoneParent(const std::string& boneName, const std::string& parentName) {
    auto boneIt = m_bones.find(boneName);
    if (boneIt == m_bones.end()) {
        return false;
    }
    
    if (parentName.empty()) {
        boneIt->second.parentName = "";
        UpdateChildrenMap();
        return true;
    }
    
    if (m_bones.find(parentName) == m_bones.end()) {
        return false;
    }
    
    std::string current = parentName;
    while (!current.empty()) {
        if (current == boneName) {
            return false;
        }
        auto it = m_bones.find(current);
        if (it == m_bones.end()) break;
        current = it->second.parentName;
    }
    
    if (!boneIt->second.parentName.empty()) {
        auto oldParentIt = m_childrenMap.find(boneIt->second.parentName);
        if (oldParentIt != m_childrenMap.end()) {
            auto& children = oldParentIt->second;
            children.erase(std::remove(children.begin(), children.end(), boneName), children.end());
        }
    }
    
    boneIt->second.parentName = parentName;
    UpdateChildrenMap();
    
    return true;
}

std::string SkeletonDefinition::GetBoneParent(const std::string& boneName) const {
    const BoneDefinition* bone = GetBone(boneName);
    return bone ? bone->parentName : "";
}

std::vector<std::string> SkeletonDefinition::GetBoneChildren(const std::string& boneName) const {
    auto it = m_childrenMap.find(boneName);
    return (it != m_childrenMap.end()) ? it->second : std::vector<std::string>();
}

std::vector<std::string> SkeletonDefinition::GetRootBones() const {
    std::vector<std::string> roots;
    for (const auto& pair : m_bones) {
        if (pair.second.parentName.empty()) {
            roots.push_back(pair.first);
        }
    }
    return roots;
}

void SkeletonDefinition::SetBonePosition(const std::string& name, const glm::vec3& position) {
    BoneDefinition* bone = GetBone(name);
    if (bone) {
        bone->position = position;
    }
}

void SkeletonDefinition::SetBoneRotation(const std::string& name, const glm::quat& rotation) {
    BoneDefinition* bone = GetBone(name);
    if (bone) {
        bone->rotation = rotation;
    }
}

void SkeletonDefinition::SetBoneRotation(const std::string& name, float x, float y, float z, float w) {
    SetBoneRotation(name, glm::quat(w, x, y, z));
}

void SkeletonDefinition::SetBoneRotationEuler(const std::string& name, float pitch, float yaw, float roll) {
    BoneDefinition* bone = GetBone(name);
    if (bone) {
        bone->rotation = glm::quat(glm::vec3(pitch, yaw, roll));
    }
}

void SkeletonDefinition::SetBoneScale(const std::string& name, const glm::vec3& scale) {
    BoneDefinition* bone = GetBone(name);
    if (bone) {
        bone->scale = scale;
    }
}

void SkeletonDefinition::SetBoneOffsetMatrix(const std::string& name, const glm::mat4& offsetMatrix) {
    BoneDefinition* bone = GetBone(name);
    if (bone) {
        bone->offsetMatrix = offsetMatrix;
    }
}

bool SkeletonDefinition::Validate() const {
    return GetValidationErrors().empty();
}

std::vector<std::string> SkeletonDefinition::GetValidationErrors() const {
    std::vector<std::string> errors;
    
    std::map<std::string, bool> visited;
    std::map<std::string, bool> recStack;
    
    for (const auto& pair : m_bones) {
        visited[pair.first] = false;
        recStack[pair.first] = false;
    }
    
    for (const auto& pair : m_bones) {
        if (HasCycle(pair.first, visited, recStack)) {
            errors.push_back("Cycle detected in bone hierarchy");
            break;
        }
    }
    
    for (const auto& pair : m_bones) {
        if (!pair.second.parentName.empty()) {
            if (m_bones.find(pair.second.parentName) == m_bones.end()) {
                errors.push_back("Bone '" + pair.first + "' has invalid parent '" + pair.second.parentName + "'");
            }
        }
    }
    
    std::vector<int> ids;
    for (const auto& pair : m_bones) {
        if (std::find(ids.begin(), ids.end(), pair.second.id) != ids.end()) {
            errors.push_back("Duplicate bone ID: " + std::to_string(pair.second.id));
        }
        ids.push_back(pair.second.id);
    }
    
    return errors;
}

std::vector<std::string> SkeletonDefinition::GetAllBoneNames() const {
    std::vector<std::string> names;
    names.reserve(m_bones.size());
    for (const auto& pair : m_bones) {
        names.push_back(pair.first);
    }
    return names;
}

void SkeletonDefinition::Clear() {
    m_bones.clear();
    m_boneArray.clear();
    m_childrenMap.clear();
    m_nextBoneID = 0;
}

void SkeletonDefinition::UpdateChildrenMap() {
    m_childrenMap.clear();
    
    for (const auto& pair : m_bones) {
        m_childrenMap[pair.first] = std::vector<std::string>();
    }
    
    for (const auto& pair : m_bones) {
        if (!pair.second.parentName.empty()) {
            auto parentIt = m_childrenMap.find(pair.second.parentName);
            if (parentIt != m_childrenMap.end()) {
                parentIt->second.push_back(pair.first);
            }
        }
    }
}

int SkeletonDefinition::GetNextBoneID() {
    return m_nextBoneID++;
}

bool SkeletonDefinition::HasCycle(const std::string& boneName, 
                                   std::map<std::string, bool>& visited,
                                   std::map<std::string, bool>& recStack) const {
    if (!visited[boneName]) {
        visited[boneName] = true;
        recStack[boneName] = true;
        
        auto it = m_bones.find(boneName);
        if (it != m_bones.end() && !it->second.parentName.empty()) {
            const std::string& parent = it->second.parentName;
            if (!visited[parent] && HasCycle(parent, visited, recStack)) {
                return true;
            } else if (recStack[parent]) {
                return true;
            }
        }
    }
    
    recStack[boneName] = false;
    return false;
}

} // namespace Skeleton
} // namespace Editors
} // namespace Engine

