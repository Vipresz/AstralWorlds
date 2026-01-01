#include "World.h"
#include "../GlobalConstants.h"
#include "../../engine/platform/render/Shader.h"
#include "../../engine/platform/Logger.h"
#include <vector>

using namespace Engine::Platform::Render;
using EngineLogging::Logger;
using namespace Engine::Editors::CameraSystem::Constants;

namespace Engine {
namespace Editors {
namespace CameraSystem {

World::World()
    : m_gridShader(nullptr)
    , m_gridVAO(0)
    , m_gridVBO(0)
    , m_buffersInitialized(false)
    , m_gridVisible(true)
{
}

World::~World() {
    Cleanup();
}

bool World::Initialize() {
    if (!CreateGridShader()) {
        LOG_ERROR("World", "Failed to create grid shader");
        return false;
    }
    
    if (!InitializeBuffers()) {
        LOG_ERROR("World", "Failed to initialize buffers");
        return false;
    }
    
    return true;
}

void World::Cleanup() {
    if (m_gridShader) {
        delete m_gridShader;
        m_gridShader = nullptr;
    }
    
    if (m_buffersInitialized) {
        if (m_gridVAO != 0) {
            glDeleteVertexArrays(1, &m_gridVAO);
            m_gridVAO = 0;
        }
        if (m_gridVBO != 0) {
            glDeleteBuffers(1, &m_gridVBO);
            m_gridVBO = 0;
        }
        m_buffersInitialized = false;
    }
}

bool World::CreateGridShader() {
    const char* gridVertShader = R"(
        #version 330 core
        layout (location = 0) in vec3 position;
        layout (location = 1) in vec3 vertexColor;
        
        uniform mat4 projection;
        uniform mat4 view;
        uniform mat4 model;
        uniform vec3 color;
        
        out vec3 FragColor;
        
        void main() {
            gl_Position = projection * view * model * vec4(position, 1.0);
            // Use vertex color if available (length > 0), otherwise use uniform color
            if (length(vertexColor) > 0.001) {
                FragColor = vertexColor;
            } else {
                FragColor = color;
            }
        }
    )";
    
    const char* gridFragShader = R"(
        #version 330 core
        in vec3 FragColor;
        out vec4 color;
        
        void main() {
            color = vec4(FragColor, 1.0);
        }
    )";
    
    m_gridShader = new Shader();
    m_gridShader->CreateFromString(gridVertShader, gridFragShader);
    
    if (!m_gridShader->IsValid()) {
        LOG_ERROR("World", "Grid shader compilation failed");
        return false;
    }
    
    LOG_INFO("World", "Grid shader created successfully");
    return true;
}

bool World::InitializeBuffers() {
    LOG_INFO("World", "Initializing OpenGL buffers...");
    
    glGenVertexArrays(1, &m_gridVAO);
    glGenBuffers(1, &m_gridVBO);
    
    glBindVertexArray(m_gridVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_gridVBO);
    
    std::vector<float> gridVertices;
    gridVertices.reserve(200 * 3);
    
    glBufferData(GL_ARRAY_BUFFER, gridVertices.size() * sizeof(float), gridVertices.data(), GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    glBindVertexArray(0);
    
    m_buffersInitialized = true;
    LOG_INFO("World", "OpenGL buffers initialized successfully");
    return true;
}

void World::RenderGrid(const glm::mat4& projection, const glm::mat4& view) {
    if (!m_gridShader || !m_buffersInitialized || !m_gridShader->IsValid() || !m_gridVisible) {
        return;
    }
    
    GLuint shaderID = m_gridShader->GetShaderID();
    if (shaderID == 0) return;
    
    m_gridShader->UseShader();
    
    GLint projLoc = glGetUniformLocation(shaderID, "projection");
    GLint viewLoc = glGetUniformLocation(shaderID, "view");
    GLint modelLoc = glGetUniformLocation(shaderID, "model");
    GLint colorLoc = glGetUniformLocation(shaderID, "color");
    
    if (projLoc == -1 || viewLoc == -1 || modelLoc == -1 || colorLoc == -1) {
        return;
    }
    
    glm::mat4 model = glm::mat4(1.0f);
    glm::vec3 color(0.3f, 0.3f, 0.3f);
    
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
    glUniform3fv(colorLoc, 1, glm::value_ptr(color));
    
    std::vector<float> gridVertices;
    // Use global constants for grid configuration
    int halfGridCells = GRID_CELL_COUNT / 2;
    for (int i = -halfGridCells; i <= halfGridCells; i++) {
        float pos = (float)i * WORLD_GRID_SPACING;
        gridVertices.push_back(-WORLD_GRID_SIZE * 0.5f); gridVertices.push_back(pos); gridVertices.push_back(0.0f);
        gridVertices.push_back(WORLD_GRID_SIZE * 0.5f); gridVertices.push_back(pos); gridVertices.push_back(0.0f);
        gridVertices.push_back(pos); gridVertices.push_back(-WORLD_GRID_SIZE * 0.5f); gridVertices.push_back(0.0f);
        gridVertices.push_back(pos); gridVertices.push_back(WORLD_GRID_SIZE * 0.5f); gridVertices.push_back(0.0f);
    }
    
    if (!gridVertices.empty()) {
        glBindVertexArray(m_gridVAO);
        glBindBuffer(GL_ARRAY_BUFFER, m_gridVBO);
        glBufferData(GL_ARRAY_BUFFER, gridVertices.size() * sizeof(float), gridVertices.data(), GL_DYNAMIC_DRAW);
        glEnableVertexAttribArray(0);
        glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(gridVertices.size() / 3));
        glBindVertexArray(0);
    }
}

} // namespace CameraSystem
} // namespace Editors
}

