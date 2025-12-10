#pragma once

#include "SkeletonDefinition.h"
#include "SkeletonSerializer.h"
#include <string>
#include <vector>

namespace Engine {
namespace Editors {
namespace Skeleton {

// High-level editor interface for creating and editing skeletons
class SkeletonEditor {
public:
    SkeletonEditor();
    ~SkeletonEditor();
    
    // File operations
    bool NewSkeleton(const std::string& name = "Untitled Skeleton");
    bool LoadSkeleton(const std::string& filePath);
    bool SaveSkeleton(const std::string& filePath);
    bool HasUnsavedChanges() const { return m_hasUnsavedChanges; }
    void MarkSaved() { m_hasUnsavedChanges = false; }
    
    // Bone operations
    bool AddBone(const std::string& name, const std::string& parentName = "");
    bool RemoveBone(const std::string& name);
    bool RenameBone(const std::string& oldName, const std::string& newName);
    bool SetBoneParent(const std::string& boneName, const std::string& parentName);
    
    // Transform operations
    bool SetBonePosition(const std::string& name, float x, float y, float z);
    bool SetBoneRotationEuler(const std::string& name, float pitch, float yaw, float roll);
    bool SetBoneScale(const std::string& name, float x, float y, float z);
    
    // Query operations
    SkeletonDefinition* GetSkeleton() { return &m_skeleton; }
    const SkeletonDefinition* GetSkeleton() const { return &m_skeleton; }
    std::vector<std::string> GetAllBoneNames() const;
    std::vector<std::string> GetRootBones() const;
    std::vector<std::string> GetBoneChildren(const std::string& boneName) const;
    std::string GetBoneParent(const std::string& boneName) const;
    
    // Validation
    bool Validate() const;
    std::vector<std::string> GetValidationErrors() const;
    
    // Utility: Create a basic humanoid skeleton
    void CreateHumanoidSkeleton();
    
    // Utility: Create a basic quadruped skeleton
    void CreateQuadrupedSkeleton();

private:
    SkeletonDefinition m_skeleton;
    bool m_hasUnsavedChanges;
    std::string m_currentFilePath;
    
    void MarkChanged() { m_hasUnsavedChanges = true; }
};

} // namespace Skeleton
} // namespace Editors
} // namespace Engine

