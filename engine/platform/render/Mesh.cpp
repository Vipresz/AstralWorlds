#include "render/Mesh.h"
#include "Logger.h"
#include <GL/glew.h>

namespace Engine {
namespace Platform {
namespace Render {

Mesh::Mesh() {
    VAO = 0;
    VBO = 0;
    IBO = 0;
    m_boneIDVBO = 0;
    m_boneWeightVBO = 0;
    indexCount = 0;
    m_hasBoneData = false;
}

void Mesh::CreateMesh(GLfloat* vertices, unsigned int* indices, 
                      unsigned int numOfVertices, unsigned int numOfIndices) {
    indexCount = numOfIndices;
    
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);
    
    glGenBuffers(1, &IBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices[0]) * numOfIndices, indices, GL_STATIC_DRAW);
    
    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices[0]) * numOfVertices, vertices, GL_STATIC_DRAW);
    
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vertices[0]) * 8, 0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(vertices[0]) * 8, (void*)(sizeof(vertices[0]) * 3));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(vertices[0]) * 8, (void*)(sizeof(vertices[0]) * 5));
    glEnableVertexAttribArray(2);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    
    glBindVertexArray(0);
}

void Mesh::CreateMeshWithBones(GLfloat* vertices, unsigned int* indices,
                               unsigned int numOfVertices, unsigned int numOfIndices,
                               GLint* boneIDs, GLfloat* boneWeights, unsigned int numOfVerticesWithBones) {
    indexCount = numOfIndices;
    m_hasBoneData = true;
    
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);
    
    // Create index buffer
    glGenBuffers(1, &IBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices[0]) * numOfIndices, indices, GL_STATIC_DRAW);
    
    // Create vertex buffer (position, UV, normal)
    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices[0]) * numOfVertices, vertices, GL_STATIC_DRAW);
    
    // Vertex attributes (location 0, 1, 2)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vertices[0]) * 8, 0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(vertices[0]) * 8, (void*)(sizeof(vertices[0]) * 3));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(vertices[0]) * 8, (void*)(sizeof(vertices[0]) * 5));
    glEnableVertexAttribArray(2);
    
    // Create bone ID buffer (location 3)
    glGenBuffers(1, &m_boneIDVBO);
    glBindBuffer(GL_ARRAY_BUFFER, m_boneIDVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(boneIDs[0]) * numOfVerticesWithBones * 4, boneIDs, GL_STATIC_DRAW);
    glVertexAttribIPointer(3, 4, GL_INT, sizeof(GLint) * 4, 0);
    glEnableVertexAttribArray(3);
    
    // Create bone weight buffer (location 4)
    glGenBuffers(1, &m_boneWeightVBO);
    glBindBuffer(GL_ARRAY_BUFFER, m_boneWeightVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(boneWeights[0]) * numOfVerticesWithBones * 4, boneWeights, GL_STATIC_DRAW);
    glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 4, 0);
    glEnableVertexAttribArray(4);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    
}

void Mesh::RenderMesh() {
    if (VAO == 0 || IBO == 0 || indexCount == 0) {
        return;
    }
    
    glBindVertexArray(VAO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IBO);
    glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void Mesh::ClearMesh() {
    if (m_boneWeightVBO != 0) {
        glDeleteBuffers(1, &m_boneWeightVBO);
        m_boneWeightVBO = 0;
    }
    
    if (m_boneIDVBO != 0) {
        glDeleteBuffers(1, &m_boneIDVBO);
        m_boneIDVBO = 0;
    }
    
    if (IBO != 0) {
        glDeleteBuffers(1, &IBO);
        IBO = 0;
    }
    
    if (VBO != 0) {
        glDeleteBuffers(1, &VBO);
        VBO = 0;
    }
    
    if (VAO != 0) {
        glDeleteVertexArrays(1, &VAO);
        VAO = 0;
    }
    
    indexCount = 0;
    m_hasBoneData = false;
}

Mesh::~Mesh() {
    ClearMesh();
}

} // namespace Render
} // namespace Platform
} // namespace Engine



