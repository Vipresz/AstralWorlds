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
    
    // Update vertex positions (for animation/skinning)
    // vertices: array of vertex data (position, UV, normal) - same format as CreateMesh
    // numOfVertices: number of vertices (not floats, actual vertex count)
    void UpdateVertexPositions(GLfloat* vertices, unsigned int numOfVertices);
    
    // Set vertex colors (RGB per vertex)
    // colors: array of RGB colors (3 floats per vertex)
    // numOfVertices: number of vertices
    void SetVertexColors(GLfloat* colors, unsigned int numOfVertices);
    
    bool HasBoneData() const { return m_hasBoneData; }
    
    ~Mesh();

private:
    GLuint VAO, VBO, IBO;
    GLuint m_boneIDVBO, m_boneWeightVBO; // Separate VBOs for bone data
    GLuint m_colorVBO; // Separate VBO for vertex colors
    GLsizei indexCount;
    bool m_hasBoneData;
    bool m_hasColorData;
};

} // namespace Render
} // namespace Platform
} // namespace Engine



