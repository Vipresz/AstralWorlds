#pragma once

#include <assimp/scene.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <string>
#include <map>
#include <vector>

namespace Engine {
namespace Platform {
namespace Render {

// Bone structure for skeleton
struct Bone {
    std::string name;
    int id;
    glm::mat4 offsetMatrix; // Inverse bind pose matrix (bone space to model space)
    glm::mat4 finalTransformation = glm::mat4(1.0f); // Final bone transform for rendering
    
    Bone() : id(-1), offsetMatrix(1.0f) {}
    Bone(const std::string& boneName, int boneID, const glm::mat4& offset)
        : name(boneName), id(boneID), offsetMatrix(offset) {}
};

// Skeleton class - manages bone hierarchy and transforms
class Skeleton {
public:
    Skeleton();
    ~Skeleton();
    
    // Load skeleton from Assimp scene (from FBX file)
    bool LoadSkeleton(const aiScene* scene);
    
    // Get bone by name
    Bone* GetBone(const std::string& boneName);
    const Bone* GetBone(const std::string& boneName) const;
    
    // Get bone by ID
    Bone* GetBone(int boneID);
    const Bone* GetBone(int boneID) const;
    
    // Get all bones
    const std::map<std::string, Bone>& GetBones() const { return m_bones; }
    
    // Get bone count
    size_t GetBoneCount() const { return m_bones.size(); }
    
    // Update bone transforms from animation
    // animationTransforms: bone name -> local transform matrix
    void UpdateBoneTransforms(const std::map<std::string, glm::mat4>& animationTransforms);
    
    // Get final bone matrices (for GPU skinning)
    // Returns matrices ready to send to shader (offsetMatrix * finalTransformation)
    void GetFinalBoneMatrices(std::vector<glm::mat4>& boneMatrices) const;
    
    // Get maximum number of bones (for shader array sizing)
    size_t GetMaxBoneCount() const { return m_maxBoneCount; }
    
    // Check if skeleton is loaded
    bool IsLoaded() const { return m_loaded; }
    
    // Get root bone name (if any)
    std::string GetRootBoneName() const { return m_rootBoneName; }
    
    // Matrix formula modes for debugging (0-3)
    // 0 = GlobalInverse * GlobalTransform * OffsetMatrix (current)
    // 1 = GlobalTransform * OffsetMatrix (no global inverse)
    // 2 = OffsetMatrix * GlobalTransform (reversed)
    // 3 = OffsetMatrix * GlobalTransform * GlobalInverse (fully reversed)
    void SetMatrixFormulaMode(int mode) { m_matrixFormulaMode = mode; }
    int GetMatrixFormulaMode() const { return m_matrixFormulaMode; }

private:
    bool m_loaded;
    std::map<std::string, Bone> m_bones; // Bones indexed by name
    std::vector<Bone*> m_boneArray;      // Bones indexed by ID (for GPU)
    size_t m_maxBoneCount;                // Maximum bone count (for shader arrays)
    std::string m_rootBoneName;
    int m_boneCounter;
    glm::mat4 m_globalInverseTransform;   // Global inverse transform of the skeleton root
    int m_matrixFormulaMode;              // Matrix formula mode for debugging (0-3)
    
    // Bone hierarchy: child bone name -> parent bone name (direct bone-to-bone relationships)
    std::map<std::string, std::string> m_boneParentMap;
    
    // Node hierarchy: node name -> parent node name (for traversing up to find bone ancestors)
    std::map<std::string, std::string> m_nodeParentMap;
    
    // Helper methods
    void ExtractBonesFromMeshes(const aiScene* scene);
    void BuildBoneHierarchy(aiNode* node, const aiScene* scene, const glm::mat4& parentTransform);
    void StoreNodeHierarchy(aiNode* node, const std::string& parentName); // Store ALL node parent-child relationships
    std::string FindBoneAncestor(const std::string& boneName); // Find nearest bone ancestor by traversing up
    int GetOrCreateBoneID(const std::string& boneName);
    void CalculateBoneTransforms(aiNode* node, const aiScene* scene, 
                                  const glm::mat4& parentTransform,
                                  const std::map<std::string, glm::mat4>& animationTransforms);
};

} // namespace Render
} // namespace Platform
} // namespace Engine
