#include "Logger.h"
#include "render/systems/RenderSystem.h"
#include "render/FollowCamera.h"
#include "render/components/TransformComponent.h"
#include "render/components/MeshComponent.h"
#include "render/components/MaterialComponent.h"
#include "render/components/AnimationComponent.h"
#include "render/Model.h"
#include "render/Skeleton.h"
#include <algorithm>
#include <GL/glew.h>
#include <cstring>

namespace Engine {
namespace Platform {
namespace Render {
namespace Systems {

static void CheckGLError(const char* location) {
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        const char* errorStr = nullptr;
        switch (error) {
            case GL_INVALID_ENUM: errorStr = "GL_INVALID_ENUM"; break;
            case GL_INVALID_VALUE: errorStr = "GL_INVALID_VALUE"; break;
            case GL_INVALID_OPERATION: errorStr = "GL_INVALID_OPERATION"; break;
            case GL_OUT_OF_MEMORY: errorStr = "GL_OUT_OF_MEMORY"; break;
            case GL_INVALID_FRAMEBUFFER_OPERATION: errorStr = "GL_INVALID_FRAMEBUFFER_OPERATION"; break;
            default: errorStr = "Unknown GL error"; break;
        }
        LOG_ERROR_FMT("RenderSystem", "OpenGL Error at %s: %s (0x%X)", location, errorStr, error);
    }
}

RenderSystem::RenderSystem(Shader* shader, Camera* camera, const glm::mat4& projection)
    : m_shader(shader), m_camera(camera), m_followCamera(nullptr), m_projection(projection) {
    SetPriority(100);
}

RenderSystem::RenderSystem(Shader* shader, FollowCamera* followCamera, const glm::mat4& projection)
    : m_shader(shader), m_camera(nullptr), m_followCamera(followCamera), m_projection(projection) {
    SetPriority(100);
}

void RenderSystem::Initialize(ECS::Registry& registry) {
    LOG_INFO("RenderSystem", "Initializing render system...");
    glClearColor(0.1f, 0.1f, 0.2f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    CreateDefaultShadowMap();
    CreateDefaultWhiteTexture();
    LOG_INFO("RenderSystem", "Render system initialized");
}

// ============================================================================
// RENDERING - COMPLETELY INDEPENDENT OF MOVEMENT
// This system only reads camera and entity positions - it never modifies them
// All movement is handled in InputSystem
// ============================================================================
void RenderSystem::Update(float deltaTime, ECS::Registry& registry, uint64_t tickNumber) {
    if (!m_shader || (!m_camera && !m_followCamera)) {
        return;
    }
    
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    m_shader->UseShader();
    if (!m_shader->IsValid()) {
        return;
    }
    
    glm::mat4 view;
    glm::vec3 eyePos;
    
    if (m_followCamera) {
        // Camera is already updated by InputSystem after character movement
        // Just calculate the view matrix here - don't update again to avoid lag
        view = m_followCamera->CalculateViewMatrix();
        eyePos = m_followCamera->GetCameraPosition();
    } else {
        view = m_camera->calculateViewMatrix();
        eyePos = m_camera->getCameraPosition();
    }
    
    GLint uniformModel = m_shader->GetModelLocation();
    GLint uniformView = m_shader->GetViewLocation();
    GLint uniformProjection = m_shader->GetProjectionLocation();
    GLint uniformEyePosition = m_shader->GetEyePositionLocation();
    
    if (uniformView == -1 || uniformProjection == -1) {
        return;
    }
    
    glUniformMatrix4fv(uniformView, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(uniformProjection, 1, GL_FALSE, glm::value_ptr(m_projection));
    if (uniformEyePosition != -1) {
        glUniform3f(uniformEyePosition, eyePos.x, eyePos.y, eyePos.z);
    }
    
    GLuint shaderProgramID = m_shader->GetShaderID();
    GLint uniformPointLightCount = glGetUniformLocation(shaderProgramID, "pointLightCount");
    GLint uniformSpotLightCount = glGetUniformLocation(shaderProgramID, "spotLightCount");
    GLint uniformDirectionalLightColour = glGetUniformLocation(shaderProgramID, "directionalLight.base.colour");
    GLint uniformDirectionalLightAmbient = glGetUniformLocation(shaderProgramID, "directionalLight.base.ambientIntensity");
    GLint uniformDirectionalLightDiffuse = glGetUniformLocation(shaderProgramID, "directionalLight.base.diffuseIntensity");
    GLint uniformDirectionalLightDirection = glGetUniformLocation(shaderProgramID, "directionalLight.direction");
    
    if (uniformPointLightCount != -1) glUniform1i(uniformPointLightCount, 0);
    if (uniformSpotLightCount != -1) glUniform1i(uniformSpotLightCount, 0);
    if (uniformDirectionalLightColour != -1) glUniform3f(uniformDirectionalLightColour, 1.0f, 1.0f, 1.0f);
    if (uniformDirectionalLightAmbient != -1) glUniform1f(uniformDirectionalLightAmbient, 0.3f);
    if (uniformDirectionalLightDiffuse != -1) glUniform1f(uniformDirectionalLightDiffuse, 0.7f);
    if (uniformDirectionalLightDirection != -1) glUniform3f(uniformDirectionalLightDirection, 0.0f, -1.0f, -0.5f);
    
    m_shader->SetTexture(1);
    glm::mat4 lightTransform = glm::mat4(1.0f);
    m_shader->SetDirectionalLightTransform(&lightTransform);
    BindDefaultShadowMap();
    BindDefaultWhiteTexture();
    
    auto entities = registry.View<Components::MeshComponent, Components::TransformComponent>();
    
    static bool firstFrame = true;
    if (firstFrame) {
        size_t entityCount = 0;
        for (auto e : entities) { entityCount++; }
        LOG_INFO_FMT("RenderSystem", "Found %zu entities to render (first frame)", entityCount);
        
        if (m_followCamera) {
            glm::vec3 camPos = m_followCamera->GetCameraPosition();
            glm::vec3 camTarget = m_followCamera->GetCameraTarget();
            LOG_INFO_FMT("RenderSystem", "Camera: pos(%.2f, %.2f, %.2f) target(%.2f, %.2f, %.2f)", 
                camPos.x, camPos.y, camPos.z, camTarget.x, camTarget.y, camTarget.z);
        }
        firstFrame = false;
    }
    
    for (auto entityID : entities) {
        auto* meshComp = registry.GetComponent<Components::MeshComponent>(entityID);
        auto* transformComp = registry.GetComponent<Components::TransformComponent>(entityID);
        auto* materialComp = registry.GetComponent<Components::MaterialComponent>(entityID);
        
        if (!meshComp || !transformComp || !meshComp->IsLoaded()) {
            continue;
        }
        
        glm::mat4 modelMatrix = transformComp->GetMatrix();
        if (uniformModel != -1) {
            glUniformMatrix4fv(uniformModel, 1, GL_FALSE, glm::value_ptr(modelMatrix));
        }
        
        GLfloat specularIntensity = 0.5f;
        GLfloat shininess = 32.0f;
        if (materialComp) {
            specularIntensity = materialComp->specularIntensity;
            shininess = materialComp->shininess;
        }
        
        GLint uniformSpecularIntensity = m_shader->GetSpecularIntensityLocation();
        GLint uniformShininess = m_shader->GetShininessLocation();
        if (uniformSpecularIntensity != -1) glUniform1f(uniformSpecularIntensity, specularIntensity);
        if (uniformShininess != -1) glUniform1f(uniformShininess, shininess);
        
        // ANIMATIONS DISABLED - Set boneCount to 0 to disable skeletal animation
        GLint uniformBoneCount = glGetUniformLocation(shaderProgramID, "boneCount");
        if (uniformBoneCount != -1) {
            glUniform1i(uniformBoneCount, 0);
        }
        
        if (meshComp->model) {
            glActiveTexture(GL_TEXTURE1);
            GLint boundTexture = 0;
            glGetIntegerv(GL_TEXTURE_BINDING_2D, &boundTexture);
            if (boundTexture == 0 && m_defaultWhiteTexture != 0) {
                glBindTexture(GL_TEXTURE_2D, m_defaultWhiteTexture);
            }
            
            meshComp->model->RenderModel();
        }
    }
}

void RenderSystem::Shutdown(ECS::Registry& registry) {
    if (m_defaultShadowMapTexture != 0) {
        glDeleteTextures(1, &m_defaultShadowMapTexture);
        m_defaultShadowMapTexture = 0;
    }
    if (m_defaultWhiteTexture != 0) {
        glDeleteTextures(1, &m_defaultWhiteTexture);
        m_defaultWhiteTexture = 0;
    }
}

void RenderSystem::CreateDefaultShadowMap() {
    glGenTextures(1, &m_defaultShadowMapTexture);
    glBindTexture(GL_TEXTURE_2D, m_defaultShadowMapTexture);
    
    unsigned char whitePixel[4] = { 255, 255, 255, 255 };
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, whitePixel);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    glBindTexture(GL_TEXTURE_2D, 0);
}

void RenderSystem::BindDefaultShadowMap() {
    if (m_defaultShadowMapTexture == 0) return;
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_defaultShadowMapTexture);
    
    GLuint shaderProgramID = m_shader->GetShaderID();
    GLint uniformShadowMap = glGetUniformLocation(shaderProgramID, "directionalShadowMap");
    if (uniformShadowMap != -1) {
        glUniform1i(uniformShadowMap, 0);
    }
}

void RenderSystem::CreateDefaultWhiteTexture() {
    glGenTextures(1, &m_defaultWhiteTexture);
    glBindTexture(GL_TEXTURE_2D, m_defaultWhiteTexture);
    
    unsigned char whitePixel[4] = { 255, 255, 255, 255 };
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, whitePixel);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    
    glBindTexture(GL_TEXTURE_2D, 0);
}

void RenderSystem::BindDefaultWhiteTexture() {
    if (m_defaultWhiteTexture == 0) return;
    
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_defaultWhiteTexture);
}

} // namespace Systems
} // namespace Render
} // namespace Platform
} // namespace Engine
