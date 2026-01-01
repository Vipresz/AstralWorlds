#pragma once

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <vector>

namespace Engine {
namespace Platform {
namespace Render {
    class Shader;
}
}

namespace Editors {
namespace CameraSystem {

class World {
public:
    World();
    ~World();
    
    // Initialize world (grid buffers, shaders)
    bool Initialize();
    
    // Cleanup
    void Cleanup();
    
    // Grid rendering
    void RenderGrid(const glm::mat4& projection, const glm::mat4& view);
    
    // Grid visibility
    void SetGridVisible(bool visible) { m_gridVisible = visible; }
    bool IsGridVisible() const { return m_gridVisible; }
    
    // Access to shader (needed for initialization)
    Platform::Render::Shader* GetGridShader() { return m_gridShader; }
    
private:
    // Grid rendering
    Platform::Render::Shader* m_gridShader;
    GLuint m_gridVAO;
    GLuint m_gridVBO;
    bool m_buffersInitialized;
    bool m_gridVisible;
    
    // Initialize OpenGL buffers for grid
    bool InitializeBuffers();
    
    // Create grid shader
    bool CreateGridShader();
};

} // namespace CameraSystem
} // namespace Editors
}









