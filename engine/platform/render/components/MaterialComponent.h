#pragma once

#include <GL/glew.h>

namespace Engine {
namespace Platform {
namespace Render {
namespace Components {

// Material component - stores material properties
struct MaterialComponent {
    GLfloat specularIntensity = 0.5f;
    GLfloat shininess = 32.0f;
    
    MaterialComponent() = default;
    MaterialComponent(GLfloat sIntensity, GLfloat shine) 
        : specularIntensity(sIntensity), shininess(shine) {}
};

} // namespace Components
} // namespace Render
} // namespace Platform
} // namespace Engine



