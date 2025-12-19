#pragma once

#include "../../engine/platform/render/Shader.h"
#include <GL/glew.h>
#include <glm/glm.hpp>

namespace Engine {
namespace Editors {
namespace CameraSystem {

// Forward declaration
struct SceneObject;

class ObjectRenderer {
public:
    ObjectRenderer();
    ~ObjectRenderer();
    
    // Render a single object
    void RenderObject(SceneObject* obj, Platform::Render::Shader* shader);
    
private:
    // Initialize default white texture (lazy initialization)
    void EnsureDefaultTexture();
    
    GLuint m_defaultWhiteTexture;
    bool m_textureInitialized;
};

} // namespace CameraSystem
} // namespace Editors
} // namespace Engine

