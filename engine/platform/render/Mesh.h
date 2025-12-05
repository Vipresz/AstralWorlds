#pragma once

#include <GL/glew.h>

namespace Engine {
namespace Platform {
namespace Render {

class Mesh {
public:
    Mesh();
    
    void CreateMesh(GLfloat* vertices, unsigned int* indices, 
                    unsigned int numOfVertices, unsigned int numOfIndices);
    
    // Create mesh with bone data for GPU skinning
    void CreateMeshWithBones(GLfloat* vertices, unsigned int* indices,
                             unsigned int numOfVertices, unsigned int numOfIndices,
                             GLint* boneIDs, GLfloat* boneWeights, unsigned int numOfVerticesWithBones);
    
    void RenderMesh();
    void ClearMesh();
    
    bool HasBoneData() const { return m_hasBoneData; }
    
    ~Mesh();

private:
    GLuint VAO, VBO, IBO;
    GLuint m_boneIDVBO, m_boneWeightVBO; // Separate VBOs for bone data
    GLsizei indexCount;
    bool m_hasBoneData;
};

} // namespace Render
} // namespace Platform
} // namespace Engine



