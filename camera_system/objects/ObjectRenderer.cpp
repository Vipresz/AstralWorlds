#include "ObjectRenderer.h"
#include "../CameraSystemEditor.h"
#include "../../engine/platform/render/Shader.h"
#include "../../engine/platform/Logger.h"
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

using namespace Engine::Platform::Render;
using EngineLogging::Logger;

namespace Engine {
namespace Editors {
namespace CameraSystem {

ObjectRenderer::ObjectRenderer()
    : m_defaultWhiteTexture(0)
    , m_textureInitialized(false)
{
}

ObjectRenderer::~ObjectRenderer() {
    if (m_textureInitialized && m_defaultWhiteTexture != 0) {
        glDeleteTextures(1, &m_defaultWhiteTexture);
        m_defaultWhiteTexture = 0;
        m_textureInitialized = false;
    }
}

void ObjectRenderer::EnsureDefaultTexture() {
    if (m_textureInitialized && m_defaultWhiteTexture != 0) {
        return;
    }
    
    glGenTextures(1, &m_defaultWhiteTexture);
    glBindTexture(GL_TEXTURE_2D, m_defaultWhiteTexture);
    unsigned char whitePixel[4] = { 255, 255, 255, 255 };
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, whitePixel);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glBindTexture(GL_TEXTURE_2D, 0);
    m_textureInitialized = true;
}

void ObjectRenderer::RenderObject(SceneObject* obj, Platform::Render::Shader* shader) {
    if (!obj || !obj->model || !obj->visible || !shader || !shader->IsValid()) {
        return;
    }
    
    if (obj->model->GetMeshCount() == 0) {
        return;
    }
    
    // Validate position before rendering (safety check)
    if (std::isnan(obj->position.x) || std::isinf(obj->position.x) ||
        std::isnan(obj->position.y) || std::isinf(obj->position.y) ||
        std::isnan(obj->position.z) || std::isinf(obj->position.z)) {
        return;
    }
    
    // Build model matrix from object transform
    glm::mat4 modelMatrix = glm::mat4(1.0f);
    modelMatrix = glm::translate(modelMatrix, obj->position);
    modelMatrix = glm::rotate(modelMatrix, glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    modelMatrix = glm::rotate(modelMatrix, glm::radians(-90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    modelMatrix = glm::rotate(modelMatrix, glm::radians(obj->rotation.z), glm::vec3(0.0f, 1.0f, 0.0f));
    modelMatrix = glm::rotate(modelMatrix, glm::radians(obj->rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
    modelMatrix = glm::rotate(modelMatrix, glm::radians(obj->rotation.y), glm::vec3(0.0f, 0.0f, 1.0f));
    modelMatrix = glm::scale(modelMatrix, obj->scale);
    
    // Set model matrix uniform
    GLint uniformModel = shader->GetModelLocation();
    if (uniformModel != -1) {
        glUniformMatrix4fv(uniformModel, 1, GL_FALSE, glm::value_ptr(modelMatrix));
    }
    
    // Set material properties
    GLfloat specularIntensity = 0.5f;
    GLfloat shininess = 32.0f;
    GLint uniformSpecularIntensity = shader->GetSpecularIntensityLocation();
    GLint uniformShininess = shader->GetShininessLocation();
    if (uniformSpecularIntensity != -1) glUniform1f(uniformSpecularIntensity, specularIntensity);
    if (uniformShininess != -1) glUniform1f(uniformShininess, shininess);
    
    // Set bone count (for animation support, currently disabled)
    GLuint shaderProgramID = shader->GetShaderID();
    GLint uniformBoneCount = glGetUniformLocation(shaderProgramID, "boneCount");
    if (uniformBoneCount != -1) {
        glUniform1i(uniformBoneCount, 0);
    }
    
    // Set up texture (use default white texture if none bound)
    glActiveTexture(GL_TEXTURE1);
    GLint boundTexture = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &boundTexture);
    if (boundTexture == 0) {
        EnsureDefaultTexture();
        glBindTexture(GL_TEXTURE_2D, m_defaultWhiteTexture);
    }
    
    // Set up OpenGL state
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDisable(GL_CULL_FACE);
    
    // Render the model
    obj->model->RenderModel();
}

} // namespace CameraSystem
} // namespace Editors
} // namespace Engine

