#include "render/Skeleton.h"
#include "Logger.h"
#include <assimp/Importer.hpp>
#include <algorithm>
#include <functional> // For std::function

namespace Engine {
namespace Platform {
namespace Render {

Skeleton::Skeleton()
    : m_loaded(false)
    , m_maxBoneCount(0)
    , m_boneCounter(0)
    , m_rootBoneName("")
    , m_globalInverseTransform(1.0f)  // Identity matrix by default
    , m_matrixFormulaMode(0)          // Default: GlobalInverse * GlobalTransform * OffsetMatrix
{
}

Skeleton::~Skeleton() {
}

bool Skeleton::LoadSkeleton(const aiScene* scene) {
    if (!scene || !scene->mRootNode) {
        LOG_ERROR("Skeleton", "Cannot load skeleton: invalid scene or root node");
        return false;
    }
    
    m_bones.clear();
    m_boneArray.clear();
    m_boneParentMap.clear();
    m_nodeParentMap.clear();
    m_boneCounter = 0;
    m_maxBoneCount = 0;
    m_loaded = false;
    
    ExtractBonesFromMeshes(scene);
    
    if (m_bones.empty()) {
        BuildBoneHierarchy(scene->mRootNode, scene, glm::mat4(1.0f));
    }
    
    if (m_bones.empty()) {
        LOG_ERROR("Skeleton", "Failed to extract any bones from scene");
        return false;
    }
    
    m_rootBoneName = std::string(scene->mRootNode->mName.C_Str());
    
    const aiMatrix4x4& rootTransform = scene->mRootNode->mTransformation;
    glm::mat4 rootTransformGLM(
        rootTransform.a1, rootTransform.b1, rootTransform.c1, rootTransform.d1,
        rootTransform.a2, rootTransform.b2, rootTransform.c2, rootTransform.d2,
        rootTransform.a3, rootTransform.b3, rootTransform.c3, rootTransform.d3,
        rootTransform.a4, rootTransform.b4, rootTransform.c4, rootTransform.d4
    );
    m_globalInverseTransform = glm::inverse(rootTransformGLM);
    
    m_nodeParentMap.clear();
    StoreNodeHierarchy(scene->mRootNode, "");
    
    m_boneParentMap.clear();
    for (const auto& bonePair : m_bones) {
        const std::string& boneName = bonePair.first;
        std::string boneAncestor = FindBoneAncestor(boneName);
        if (!boneAncestor.empty() && boneAncestor != boneName) {
            m_boneParentMap[boneName] = boneAncestor;
        }
    }
    
    m_boneArray.resize(m_maxBoneCount, nullptr);
    for (auto& pair : m_bones) {
        Bone& bone = pair.second;
        if (bone.id >= 0 && bone.id < static_cast<int>(m_boneArray.size())) {
            m_boneArray[bone.id] = &bone;
        }
    }
    
    m_loaded = true;
    LOG_INFO_FMT("Skeleton", "Skeleton loaded: %zu bones", m_bones.size());
    
    return true;
}

void Skeleton::ExtractBonesFromMeshes(const aiScene* scene) {
    // Iterate through all meshes to find bones
    for (unsigned int meshIndex = 0; meshIndex < scene->mNumMeshes; meshIndex++) {
        const aiMesh* mesh = scene->mMeshes[meshIndex];
        
        // Check if this mesh has bones
        if (!mesh->HasBones()) {
            continue;
        }
        
        for (unsigned int boneIndex = 0; boneIndex < mesh->mNumBones; boneIndex++) {
            const aiBone* aiBone = mesh->mBones[boneIndex];
            std::string boneName = std::string(aiBone->mName.C_Str());
            
            const aiMatrix4x4& offset = aiBone->mOffsetMatrix;
            glm::mat4 offsetMatrix(
                offset.a1, offset.b1, offset.c1, offset.d1,
                offset.a2, offset.b2, offset.c2, offset.d2,
                offset.a3, offset.b3, offset.c3, offset.d3,
                offset.a4, offset.b4, offset.c4, offset.d4
            );
            
            int boneID = GetOrCreateBoneID(boneName);
            
            auto it = m_bones.find(boneName);
            if (it == m_bones.end()) {
                m_bones[boneName] = Bone(boneName, boneID, offsetMatrix);
            } else {
                it->second.offsetMatrix = offsetMatrix;
            }
        }
    }
    
    // Update max bone count
    m_maxBoneCount = m_bones.size();
}

void Skeleton::BuildBoneHierarchy(aiNode* node, const aiScene* scene, const glm::mat4& parentTransform) {
    std::string nodeName = std::string(node->mName.C_Str());
    
    // Convert node transform from Assimp to GLM
    const aiMatrix4x4& nodeTransform = node->mTransformation;
    glm::mat4 localTransform(
        nodeTransform.a1, nodeTransform.b1, nodeTransform.c1, nodeTransform.d1,
        nodeTransform.a2, nodeTransform.b2, nodeTransform.c2, nodeTransform.d2,
        nodeTransform.a3, nodeTransform.b3, nodeTransform.c3, nodeTransform.d3,
        nodeTransform.a4, nodeTransform.b4, nodeTransform.c4, nodeTransform.d4
    );
    
    glm::mat4 globalTransform = parentTransform * localTransform;
    
    // Check if this node corresponds to a bone we've seen in meshes
    auto boneIt = m_bones.find(nodeName);
    if (boneIt == m_bones.end()) {
        // This node might be a bone but wasn't found in meshes
        // Create a bone entry with identity offset matrix
        int boneID = GetOrCreateBoneID(nodeName);
        m_bones[nodeName] = Bone(nodeName, boneID, glm::mat4(1.0f));
        m_maxBoneCount = std::max(m_maxBoneCount, m_bones.size());
    }
    
    // Recursively process children
    for (unsigned int i = 0; i < node->mNumChildren; i++) {
        BuildBoneHierarchy(node->mChildren[i], scene, globalTransform);
    }
}

void Skeleton::StoreNodeHierarchy(aiNode* node, const std::string& parentName) {
    std::string nodeName = std::string(node->mName.C_Str());
    
    // Store ALL node parent-child relationships (not just bones)
    if (!parentName.empty()) {
        m_nodeParentMap[nodeName] = parentName;
    }
    
    // Recursively process children with this node as their parent
    for (unsigned int i = 0; i < node->mNumChildren; i++) {
        StoreNodeHierarchy(node->mChildren[i], nodeName);
    }
}

std::string Skeleton::FindBoneAncestor(const std::string& boneName) {
    // Traverse up the node hierarchy to find the nearest bone ancestor
    std::string currentNode = boneName;
    int maxDepth = 20; // Safety limit to avoid infinite loops
    int depth = 0;
    
    while (depth < maxDepth) {
        // Find current node's parent
        auto parentIt = m_nodeParentMap.find(currentNode);
        if (parentIt == m_nodeParentMap.end()) {
            // No parent found - reached root
            break;
        }
        
        const std::string& parentName = parentIt->second;
        
        // Check if parent is a bone
        if (m_bones.find(parentName) != m_bones.end()) {
            return parentName; // Found bone ancestor!
        }
        
        // Parent is not a bone - continue traversing up
        currentNode = parentName;
        depth++;
    }
    
    // No bone ancestor found
    return "";
}

int Skeleton::GetOrCreateBoneID(const std::string& boneName) {
    auto it = m_bones.find(boneName);
    if (it != m_bones.end()) {
        return it->second.id; // Bone already exists
    }
    
    // Create new bone ID
    int newID = m_boneCounter++;
    return newID;
}

Bone* Skeleton::GetBone(const std::string& boneName) {
    auto it = m_bones.find(boneName);
    if (it != m_bones.end()) {
        return &it->second;
    }
    return nullptr;
}

const Bone* Skeleton::GetBone(const std::string& boneName) const {
    auto it = m_bones.find(boneName);
    if (it != m_bones.end()) {
        return &it->second;
    }
    return nullptr;
}

Bone* Skeleton::GetBone(int boneID) {
    if (boneID >= 0 && boneID < static_cast<int>(m_boneArray.size())) {
        return m_boneArray[boneID];
    }
    return nullptr;
}

const Bone* Skeleton::GetBone(int boneID) const {
    if (boneID >= 0 && boneID < static_cast<int>(m_boneArray.size())) {
        return m_boneArray[boneID];
    }
    return nullptr;
}

void Skeleton::UpdateBoneTransforms(const std::map<std::string, glm::mat4>& animationTransforms) {
    if (!m_loaded) {
        return;
    }
    
    // Reset all bone transforms
    for (auto& pair : m_bones) {
        pair.second.finalTransformation = glm::mat4(1.0f);
    }
    
    // Helper function to calculate global transform for a bone recursively
    // Use memoization to avoid recalculating the same bone multiple times
    std::map<std::string, glm::mat4> globalTransformCache;
    std::function<glm::mat4(const std::string&)> calculateGlobalTransform = [&](const std::string& boneName) -> glm::mat4 {
        // Check cache first
        auto cacheIt = globalTransformCache.find(boneName);
        if (cacheIt != globalTransformCache.end()) {
            return cacheIt->second;
        }
        
        // Get local transform from animation (default to identity if not animated)
        glm::mat4 localTransform = glm::mat4(1.0f);
        auto animIt = animationTransforms.find(boneName);
        if (animIt != animationTransforms.end()) {
            localTransform = animIt->second;
        }
        
        // Get parent bone transform
        auto parentIt = m_boneParentMap.find(boneName);
        glm::mat4 globalTransform;
        if (parentIt != m_boneParentMap.end() && m_bones.find(parentIt->second) != m_bones.end()) {
            // Has a parent bone - recursively calculate parent's global transform
            const std::string& parentBoneName = parentIt->second;
            glm::mat4 parentGlobal = calculateGlobalTransform(parentBoneName);
            globalTransform = parentGlobal * localTransform;
        } else {
            // Root bone (no bone parent) - local transform is global
            globalTransform = localTransform;
        }
        
        // Cache the result
        globalTransformCache[boneName] = globalTransform;
        return globalTransform;
    };
    
    // Calculate global transforms for all bones
    for (auto& pair : m_bones) {
        const std::string& boneName = pair.first;
        pair.second.finalTransformation = calculateGlobalTransform(boneName);
    }
}

void Skeleton::GetFinalBoneMatrices(std::vector<glm::mat4>& boneMatrices) const {
    if (!m_loaded) {
        return;
    }
    
    // Resize to max bone count, initialized to identity
    boneMatrices.resize(m_maxBoneCount, glm::mat4(1.0f));
    
    // Calculate final matrices for GPU skinning
    // Standard formula: FinalMatrix = GlobalBoneTransform * BoneOffsetMatrix
    // Where GlobalBoneTransform is the animated bone's global transform
    // and BoneOffsetMatrix is the inverse bind pose
    for (const auto& pair : m_bones) {
        const Bone& bone = pair.second;
        if (bone.id >= 0 && bone.id < static_cast<int>(boneMatrices.size())) {
            // Configurable matrix formula for debugging
            // Try different modes to see which one works correctly
            switch (m_matrixFormulaMode) {
                case 0: // GlobalInverse * GlobalTransform * OffsetMatrix (standard)
                    boneMatrices[bone.id] = m_globalInverseTransform * bone.finalTransformation * bone.offsetMatrix;
                    break;
                case 1: // GlobalTransform * OffsetMatrix (no global inverse)
                    boneMatrices[bone.id] = bone.finalTransformation * bone.offsetMatrix;
                    break;
                case 2: // OffsetMatrix * GlobalTransform (reversed)
                    boneMatrices[bone.id] = bone.offsetMatrix * bone.finalTransformation;
                    break;
                case 3: // OffsetMatrix * GlobalTransform * GlobalInverse (fully reversed)
                    boneMatrices[bone.id] = bone.offsetMatrix * bone.finalTransformation * m_globalInverseTransform;
                    break;
                default:
                    boneMatrices[bone.id] = m_globalInverseTransform * bone.finalTransformation * bone.offsetMatrix;
                    break;
            }
        }
    }
}

void Skeleton::CalculateBoneTransforms(aiNode* node, const aiScene* scene, 
                                       const glm::mat4& parentTransform,
                                       const std::map<std::string, glm::mat4>& animationTransforms) {
    std::string nodeName = std::string(node->mName.C_Str());
    
    // Get local transform from animation or use node transform
    glm::mat4 localTransform = glm::mat4(1.0f);
    auto animIt = animationTransforms.find(nodeName);
    if (animIt != animationTransforms.end()) {
        localTransform = animIt->second;
    } else {
        // Use node's default transform
        const aiMatrix4x4& nodeTransform = node->mTransformation;
        localTransform = glm::mat4(
            nodeTransform.a1, nodeTransform.b1, nodeTransform.c1, nodeTransform.d1,
            nodeTransform.a2, nodeTransform.b2, nodeTransform.c2, nodeTransform.d2,
            nodeTransform.a3, nodeTransform.b3, nodeTransform.c3, nodeTransform.d3,
            nodeTransform.a4, nodeTransform.b4, nodeTransform.c4, nodeTransform.d4
        );
    }
    
    glm::mat4 globalTransform = parentTransform * localTransform;
    
    // Update bone's final transformation
    auto boneIt = m_bones.find(nodeName);
    if (boneIt != m_bones.end()) {
        boneIt->second.finalTransformation = globalTransform;
    }
    
    // Recursively process children
    for (unsigned int i = 0; i < node->mNumChildren; i++) {
        CalculateBoneTransforms(node->mChildren[i], scene, globalTransform, animationTransforms);
    }
}

} // namespace Render
} // namespace Platform
} // namespace Engine

