#include "render/Material.h"

namespace Engine {
namespace Platform {
namespace Render {

Material::Material() {
    specularIntensity = 0.0f;
    shininess = 0.0f;
}

Material::Material(GLfloat sIntensity, GLfloat shine) {
    specularIntensity = sIntensity;
    shininess = shine;
}

void Material::UseMaterial(GLuint specularIntensityLocation, GLuint shininessLocation) {
    glUniform1f(specularIntensityLocation, specularIntensity);
    glUniform1f(shininessLocation, shininess);
}

Material::~Material() {
}

} // namespace Render
} // namespace Platform
} // namespace Engine



