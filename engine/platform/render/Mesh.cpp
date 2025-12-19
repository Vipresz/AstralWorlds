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
    m_colorVBO = 0;
    indexCount = 0;
    m_hasBoneData = false;
    m_hasColorData = false;
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

void Mesh::UpdateVertexPositions(GLfloat* vertices, unsigned int numOfVertices) {
    if (VBO == 0 || !vertices || numOfVertices == 0) {
        return;
    }
    
    // Update vertex buffer with new positions
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices[0]) * numOfVertices * 8, vertices);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void Mesh::RenderMesh() {
    if (VAO == 0 || IBO == 0 || indexCount == 0) {
        return;
    }
    
    glBindVertexArray(VAO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IBO);
    
    // Enable color attribute if color data exists
    if (m_hasColorData && m_colorVBO != 0) {
        glEnableVertexAttribArray(5); // Use location 5 for colors
    }
    
    glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);
    
    if (m_hasColorData && m_colorVBO != 0) {
        glDisableVertexAttribArray(5);
    }
    
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void Mesh::SetVertexColors(GLfloat* colors, unsigned int numOfVertices) {
    if (!colors || numOfVertices == 0) {
        return;
    }
    
    glBindVertexArray(VAO);
    
    if (m_colorVBO == 0) {
        glGenBuffers(1, &m_colorVBO);
    }
    
    glBindBuffer(GL_ARRAY_BUFFER, m_colorVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 3 * numOfVertices, colors, GL_STATIC_DRAW);
    glVertexAttribPointer(5, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), 0);
    glEnableVertexAttribArray(5);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    
    m_hasColorData = true;
}

void Mesh::ClearMesh() {
    if (m_colorVBO != 0) {
        glDeleteBuffers(1, &m_colorVBO);
        m_colorVBO = 0;
    }
    
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
    m_hasColorData = false;
}

Mesh::~Mesh() {
    ClearMesh();
}

} // namespace Render
} // namespace Platform
} // namespace Engine



