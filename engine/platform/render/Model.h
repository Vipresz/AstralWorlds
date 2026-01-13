#pragma once

#include <vector>
#include <string>
#include <memory>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <glm/glm.hpp>

#include "render/Mesh.h"
#include "render/Texture.h"

namespace Engine {
namespace Platform {
namespace Render {

class Model {
public:
    Model();
    
    void LoadModel(const std::string& fileName);
    void RenderModel();
    void ClearModel();
    
    // Set base directory for resolving texture paths
    void SetTextureBaseDir(const std::string& baseDir) { m_textureBaseDir = baseDir; }
    
    // Get mesh count
    size_t GetMeshCount() const { return m_meshList.size(); }
    
    // Get model bounding box/extents (for camera positioning, etc.)
    // Returns: minX, maxX, minY, maxY, minZ, maxZ
    void GetBounds(float& minX, float& maxX, float& minY, float& maxY, float& minZ, float& maxZ) const;
    
    // Get the top-most Y coordinate (head height) of the model
    float GetTopY() const { return m_maxY; }
    
    // Get the bottom-most Y coordinate (base height) of the model
    float GetBottomY() const { return m_minY; }
    
    // Get the height of the model (top - bottom)
    float GetHeight() const { return m_maxY - m_minY; }
    
    // Create a simple plane programmatically (for ground, etc.)
    // sizeX, sizeZ: dimensions of the plane
    void CreatePlane(float sizeX, float sizeZ);
    
    // Get bone name by mesh bone index (for bone remapping)
    // Returns empty string if bone index is invalid
    std::string GetBoneNameByIndex(unsigned int meshBoneIndex) const;
    
    // Get the number of bones in the model (max mesh bone index + 1)
    size_t GetBoneCount() const { return m_boneNames.size(); }
    
    // Update vertex positions for animation (applies to all meshes)
    // vertexData: array of vertex data (position, UV, normal) per mesh
    // meshVertexCounts: number of vertices per mesh
    void UpdateVertexPositions(std::vector<std::vector<GLfloat>>& vertexData, 
                               const std::vector<unsigned int>& meshVertexCounts);
    
    // Apply rainbow color gradient based on Z position
    // minZ, maxZ: Z range across all objects (calculated externally)
    void ApplyRainbowColors(float minZ, float maxZ);
    
    // Check if model has any valid (non-null) textures
    bool HasTextures() const { 
        for (const auto* tex : m_textureList) {
            if (tex) return true;
        }
        return false;
    }
    
    ~Model();

private:
    void LoadNode(aiNode* node, const aiScene* scene);
    void LoadMesh(aiMesh* mesh, const aiScene* scene);
    void LoadMaterials(const aiScene* scene);
    
    std::string ResolveTexturePath(const std::string& texturePath);
    
    std::vector<Mesh*> m_meshList;
    std::vector<Texture*> m_textureList;
    std::vector<unsigned int> m_meshToTex;
    std::string m_textureBaseDir;
    std::string m_modelDirectory;
    
    // Bounding box extents (for camera positioning)
    float m_minX, m_maxX;
    float m_minY, m_maxY;
    float m_minZ, m_maxZ;
    bool m_boundsCalculated;
    
    // Bone name storage: mesh bone index → bone name
    // Used for remapping bone matrices to match skeleton bone IDs
    std::vector<std::string> m_boneNames; // Index = mesh bone index, value = bone name
    
    // Store vertex positions for color application
    std::vector<std::vector<glm::vec3>> m_vertexPositions; // Per-mesh vertex positions
    
    void CalculateBounds(const aiScene* scene);
};

} // namespace Render
} // namespace Platform
} // namespace Engine



