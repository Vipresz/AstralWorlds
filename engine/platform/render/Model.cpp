#include "render/Model.h"
#include "Logger.h"
#include <cstdio>
#include <algorithm>
#include <stdexcept>
#include <limits>
#include <map>
#include <string>

namespace Engine {
namespace Platform {
namespace Render {

Model::Model() {
    m_textureBaseDir = "";
    m_modelDirectory = "";
    m_minX = m_minY = m_minZ = std::numeric_limits<float>::max();
    m_maxX = m_maxY = m_maxZ = std::numeric_limits<float>::lowest();
    m_boundsCalculated = false;
}

void Model::LoadModel(const std::string& fileName) {
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(
        fileName,
        aiProcess_Triangulate |
        aiProcess_FlipUVs |
        aiProcess_GenSmoothNormals |
        aiProcess_JoinIdenticalVertices
    );
    
        if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        std::string errorMsg = std::string("Model (") + fileName + ") failed to load: " + importer.GetErrorString();
        LOG_ERROR_FMT("Model", "%s", errorMsg.c_str());
        throw std::runtime_error(errorMsg);
    }
    
    // Extract model directory for texture path resolution
    size_t lastSlash = fileName.find_last_of("/\\");
    if (lastSlash != std::string::npos) {
        m_modelDirectory = fileName.substr(0, lastSlash + 1);
    } else {
        m_modelDirectory = "";
    }
    
    // Reset bounds before loading
    m_minX = m_minY = m_minZ = std::numeric_limits<float>::max();
    m_maxX = m_maxY = m_maxZ = std::numeric_limits<float>::lowest();
    m_boundsCalculated = false;
    
    LoadNode(scene->mRootNode, scene);
    LoadMaterials(scene);
    
    if (m_meshList.empty()) {
        throw std::runtime_error("Model loaded but contains no meshes: " + fileName);
    }
    
    CalculateBounds(scene);
    LOG_INFO_FMT("Model", "Model loaded: %zu meshes, height: %.2f", m_meshList.size(), GetHeight());
}

void Model::LoadNode(aiNode* node, const aiScene* scene) {
    // Load all meshes in this node
    for (size_t i = 0; i < node->mNumMeshes; i++) {
        LoadMesh(scene->mMeshes[node->mMeshes[i]], scene);
    }
    
    // Recursively load child nodes
    for (size_t i = 0; i < node->mNumChildren; i++) {
        LoadNode(node->mChildren[i], scene);
    }
}

void Model::LoadMesh(aiMesh* mesh, const aiScene* scene) {
    std::vector<GLfloat> vertices;
    std::vector<unsigned int> indices;
    
    // Prepare bone data structures
    std::vector<GLint> boneIDs;      // Bone IDs per vertex (up to 4 per vertex)
    std::vector<GLfloat> boneWeights; // Bone weights per vertex (up to 4 per vertex)
    bool hasBones = mesh->HasBones();
    
    if (hasBones) {
        LOG_INFO_FMT("Model", "Extracting bone data: %u bones, %u vertices", mesh->mNumBones, mesh->mNumVertices);
        boneIDs.resize(mesh->mNumVertices * 4, 0);
        boneWeights.resize(mesh->mNumVertices * 4, 0.0f);
        
        if (m_boneNames.empty()) {
            m_boneNames.resize(mesh->mNumBones);
            for (unsigned int i = 0; i < mesh->mNumBones; i++) {
                m_boneNames[i] = std::string(mesh->mBones[i]->mName.C_Str());
            }
        }
        
        std::map<std::string, unsigned int> boneNameToIndex;
        for (unsigned int i = 0; i < mesh->mNumBones; i++) {
            boneNameToIndex[std::string(mesh->mBones[i]->mName.C_Str())] = i;
        }
        
        for (unsigned int boneIdx = 0; boneIdx < mesh->mNumBones; boneIdx++) {
            const aiBone* bone = mesh->mBones[boneIdx];
            unsigned int boneIndex = boneIdx;
            
            for (unsigned int weightIdx = 0; weightIdx < bone->mNumWeights; weightIdx++) {
                const aiVertexWeight& weight = bone->mWeights[weightIdx];
                unsigned int vertexID = weight.mVertexId;
                float vertexWeight = weight.mWeight;
                
                for (int slot = 0; slot < 4; slot++) {
                    if (boneWeights[vertexID * 4 + slot] == 0.0f) {
                        boneIDs[vertexID * 4 + slot] = static_cast<GLint>(boneIndex);
                        boneWeights[vertexID * 4 + slot] = vertexWeight;
                        break;
                    }
                }
            }
        }
    }
    
    // Load vertex data (position, texture coordinates, normals)
    for (size_t i = 0; i < mesh->mNumVertices; i++) {
        // Position
        vertices.insert(vertices.end(), {
            mesh->mVertices[i].x,
            mesh->mVertices[i].y,
            mesh->mVertices[i].z
        });
        
        // Texture coordinates
        if (mesh->mTextureCoords[0]) {
            vertices.insert(vertices.end(), {
                mesh->mTextureCoords[0][i].x,
                mesh->mTextureCoords[0][i].y
            });
        } else {
            vertices.insert(vertices.end(), { 0.0f, 0.0f });
        }
        
        // Normals (flipped for OpenGL coordinate system)
        vertices.insert(vertices.end(), {
            -mesh->mNormals[i].x,
            -mesh->mNormals[i].y,
            -mesh->mNormals[i].z
        });
    }
    
    // Load indices
    for (size_t i = 0; i < mesh->mNumFaces; i++) {
        aiFace face = mesh->mFaces[i];
        for (size_t j = 0; j < face.mNumIndices; j++) {
            indices.push_back(face.mIndices[j]);
        }
    }
    
    // Create mesh with bone data if available
    Mesh* newMesh = new Mesh();
    if (hasBones && !boneIDs.empty()) {
        newMesh->CreateMeshWithBones(&vertices[0], &indices[0],
                                    static_cast<unsigned int>(vertices.size()),
                                    static_cast<unsigned int>(indices.size()),
                                    &boneIDs[0], &boneWeights[0],
                                    static_cast<unsigned int>(mesh->mNumVertices));
    } else {
        newMesh->CreateMesh(&vertices[0], &indices[0], 
                           static_cast<unsigned int>(vertices.size()), 
                           static_cast<unsigned int>(indices.size()));
    }
    m_meshList.push_back(newMesh);
    m_meshToTex.push_back(mesh->mMaterialIndex);
}

void Model::LoadMaterials(const aiScene* scene) {
    m_textureList.resize(scene->mNumMaterials);
    
    for (size_t i = 0; i < scene->mNumMaterials; i++) {
        aiMaterial* material = scene->mMaterials[i];
        m_textureList[i] = nullptr;
        
        // Try to load diffuse texture
        if (material->GetTextureCount(aiTextureType_DIFFUSE) > 0) {
            aiString path;
            if (material->GetTexture(aiTextureType_DIFFUSE, 0, &path) == AI_SUCCESS) {
                std::string texturePath = ResolveTexturePath(std::string(path.data));
                
                m_textureList[i] = new Texture(texturePath.c_str());
                
                // Try loading with alpha, fall back to no alpha
                if (!m_textureList[i]->LoadTextureA()) {
                    if (!m_textureList[i]->LoadTexture()) {
                        delete m_textureList[i];
                        m_textureList[i] = nullptr;
                    }
                }
            }
        }
        
        // If no texture was loaded, create a default white texture
        if (!m_textureList[i]) {
            // Try to load a default texture, or we'll handle this in rendering
            // For now, set to nullptr - we can handle default textures in the renderer
            m_textureList[i] = nullptr;
        }
    }
}

std::string Model::ResolveTexturePath(const std::string& texturePath) {
    // First, try to extract just the filename
    size_t lastSlash = texturePath.find_last_of("/\\");
    std::string filename = (lastSlash != std::string::npos) ? 
                           texturePath.substr(lastSlash + 1) : texturePath;
    
    // If textureBaseDir is set, use it
    if (!m_textureBaseDir.empty()) {
        std::string basePath = m_textureBaseDir;
        // Ensure baseDir ends with a separator
        if (basePath.back() != '/' && basePath.back() != '\\') {
            basePath += "/";
        }
        return basePath + filename;
    }
    
    // Otherwise, try relative to model directory
    if (!m_modelDirectory.empty()) {
        return m_modelDirectory + filename;
    }
    
    // Last resort: try just the filename
    return filename;
}

void Model::RenderModel() {
    if (m_meshList.empty()) {
        return;
    }
    
    for (size_t i = 0; i < m_meshList.size(); i++) {
        if (!m_meshList[i]) {
            continue;
        }
        
        unsigned int materialIndex = m_meshToTex[i];
        
        if (materialIndex < m_textureList.size() && m_textureList[materialIndex]) {
            m_textureList[materialIndex]->UseTexture();
        }
        
        m_meshList[i]->RenderMesh();
    }
}

void Model::ClearModel() {
    // Clean up meshes
    for (size_t i = 0; i < m_meshList.size(); i++) {
        if (m_meshList[i]) {
            delete m_meshList[i];
            m_meshList[i] = nullptr;
        }
    }
    m_meshList.clear();
    
    // Clean up textures
    for (size_t i = 0; i < m_textureList.size(); i++) {
        if (m_textureList[i]) {
            delete m_textureList[i];
            m_textureList[i] = nullptr;
        }
    }
    m_textureList.clear();
    
    m_meshToTex.clear();
    
    // Clear bone names
    m_boneNames.clear();
}

void Model::CreatePlane(float sizeX, float sizeZ) {
    // Clear any existing meshes
    ClearModel();
    
    // Create a simple plane mesh
    // Vertex format: 8 floats per vertex (3 position, 2 UV, 3 normal)
    // Plane is centered at origin, facing up (normal = 0, 1, 0)
    float halfX = sizeX * 0.5f;
    float halfZ = sizeZ * 0.5f;
    
    // 4 vertices for the plane quad
    GLfloat vertices[] = {
        // Position (X, Y, Z)        // Texture Coords (U, V)    // Normal (X, Y, Z)
        -halfX, 0.0f, -halfZ,        0.0f, 0.0f,                 0.0f, 1.0f, 0.0f,  // Top-left
         halfX, 0.0f, -halfZ,        1.0f, 0.0f,                 0.0f, 1.0f, 0.0f,  // Top-right
         halfX, 0.0f,  halfZ,        1.0f, 1.0f,                 0.0f, 1.0f, 0.0f,  // Bottom-right
        -halfX, 0.0f,  halfZ,        0.0f, 1.0f,                 0.0f, 1.0f, 0.0f   // Bottom-left
    };
    
    // 2 triangles (6 indices)
    unsigned int indices[] = {
        0, 1, 2,  // First triangle
        0, 2, 3   // Second triangle
    };
    
    // Create the mesh
    Mesh* planeMesh = new Mesh();
    planeMesh->CreateMesh(vertices, indices, 32, 6); // 32 floats (4 vertices * 8), 6 indices
    
    m_meshList.push_back(planeMesh);
    m_meshToTex.push_back(0); // Use texture index 0
    
    // No texture for plane - RenderSystem will bind default white texture
    m_textureList.resize(1);
    m_textureList[0] = nullptr;
    
    // Set bounds for plane (centered at origin, flat on XZ plane)
    m_minX = -halfX;
    m_maxX = halfX;
    m_minY = 0.0f;
    m_maxY = 0.0f;
    m_minZ = -halfZ;
    m_maxZ = halfZ;
    m_boundsCalculated = true;
}

void Model::CalculateBounds(const aiScene* scene) {
    // Reset bounds
    m_minX = m_minY = m_minZ = std::numeric_limits<float>::max();
    m_maxX = m_maxY = m_maxZ = std::numeric_limits<float>::lowest();
    
    // Iterate through all meshes to find the bounding box
    for (unsigned int i = 0; i < scene->mNumMeshes; i++) {
        const aiMesh* mesh = scene->mMeshes[i];
        
        // Check all vertices in this mesh
        for (unsigned int j = 0; j < mesh->mNumVertices; j++) {
            const aiVector3D& vertex = mesh->mVertices[j];
            
            // Update min/max for each axis
            if (vertex.x < m_minX) m_minX = vertex.x;
            if (vertex.x > m_maxX) m_maxX = vertex.x;
            
            if (vertex.y < m_minY) m_minY = vertex.y;
            if (vertex.y > m_maxY) m_maxY = vertex.y;
            
            if (vertex.z < m_minZ) m_minZ = vertex.z;
            if (vertex.z > m_maxZ) m_maxZ = vertex.z;
        }
    }
    
    m_boundsCalculated = true;
}

void Model::GetBounds(float& minX, float& maxX, float& minY, float& maxY, float& minZ, float& maxZ) const {
    minX = m_minX;
    maxX = m_maxX;
    minY = m_minY;
    maxY = m_maxY;
    minZ = m_minZ;
    maxZ = m_maxZ;
}

std::string Model::GetBoneNameByIndex(unsigned int meshBoneIndex) const {
    if (meshBoneIndex < m_boneNames.size()) {
        return m_boneNames[meshBoneIndex];
    }
    return std::string(); // Return empty string if index is invalid
}

Model::~Model() {
    ClearModel();
}

} // namespace Render
} // namespace Platform
} // namespace Engine

