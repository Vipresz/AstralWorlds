#pragma once

#include <string>
#include <vector>
#include <map>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Engine {
namespace Editors {
namespace Skeleton {

// Bone definition for skeleton editor
struct BoneDefinition {
    std::string name;
    std::string parentName;  // Empty string if root bone
    int id;                  // Unique ID (auto-assigned)
    
    // Transform data
    glm::vec3 position;      // Local position relative to parent
    glm::quat rotation;      // Local rotation (quaternion)
    glm::vec3 scale;        // Local scale (default: 1,1,1)
    
    // Offset matrix (inverse bind pose) - used for skinning
    glm::mat4 offsetMatrix;
    
    BoneDefinition()
        : id(-1)
        , position(0.0f, 0.0f, 0.0f)
        , rotation(1.0f, 0.0f, 0.0f, 0.0f)  // Identity quaternion
        , scale(1.0f, 1.0f, 1.0f)
        , offsetMatrix(1.0f)
    {}
    
    // Calculate local transform matrix from position, rotation, scale
    glm::mat4 GetLocalTransform() const;
    
    // Calculate offset matrix from transform (inverse bind pose)
    void CalculateOffsetMatrix(const glm::mat4& bindPoseTransform);
};

// Complete skeleton definition
class SkeletonDefinition {
public:
    SkeletonDefinition();
    ~SkeletonDefinition();
    
    // Bone management
    int AddBone(const std::string& name, const std::string& parentName = "");
    bool RemoveBone(const std::string& name);
    BoneDefinition* GetBone(const std::string& name);
    const BoneDefinition* GetBone(const std::string& name) const;
    BoneDefinition* GetBone(int id);
    const BoneDefinition* GetBone(int id) const;
    
    // Bone hierarchy
    bool SetBoneParent(const std::string& boneName, const std::string& parentName);
    std::string GetBoneParent(const std::string& boneName) const;
    std::vector<std::string> GetBoneChildren(const std::string& boneName) const;
    std::vector<std::string> GetRootBones() const;
    
    // Bone transforms
    void SetBonePosition(const std::string& name, const glm::vec3& position);
    void SetBoneRotation(const std::string& name, const glm::quat& rotation);
    void SetBoneRotation(const std::string& name, float x, float y, float z, float w);
    void SetBoneRotationEuler(const std::string& name, float pitch, float yaw, float roll);
    void SetBoneScale(const std::string& name, const glm::vec3& scale);
    void SetBoneOffsetMatrix(const std::string& name, const glm::mat4& offsetMatrix);
    
    // Validation
    bool Validate() const;  // Check for cycles, orphaned bones, etc.
    std::vector<std::string> GetValidationErrors() const;
    
    // Getters
    size_t GetBoneCount() const { return m_bones.size(); }
    const std::map<std::string, BoneDefinition>& GetBones() const { return m_bones; }
    std::vector<std::string> GetAllBoneNames() const;
    
    // Clear all bones
    void Clear();
    
    // Metadata
    void SetName(const std::string& name) { m_name = name; }
    const std::string& GetName() const { return m_name; }
    
    void SetDescription(const std::string& desc) { m_description = desc; }
    const std::string& GetDescription() const { return m_description; }

private:
    std::map<std::string, BoneDefinition> m_bones;  // Bones indexed by name
    std::vector<BoneDefinition*> m_boneArray;       // Bones indexed by ID
    std::map<std::string, std::vector<std::string>> m_childrenMap;  // Parent -> children
    int m_nextBoneID;
    
    std::string m_name;
    std::string m_description;
    
    void UpdateChildrenMap();
    int GetNextBoneID();
    bool HasCycle(const std::string& boneName, std::map<std::string, bool>& visited, 
                  std::map<std::string, bool>& recStack) const;
};

} // namespace Skeleton
} // namespace Editors
} // namespace Engine

