#pragma once

#include "core/System.h"
#include "core/ecs/Registry.h"
#include "render/components/TransformComponent.h"
#include "render/components/MeshComponent.h"
#include "render/components/MaterialComponent.h"
#include "render/Shader.h"
#include "render/Camera.h"
#include "render/FollowCamera.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Engine {
namespace Platform {
namespace Render {
namespace Systems {

// Render system - renders all entities with mesh and transform components
class RenderSystem : public ECS::ISystem {
public:
    RenderSystem(Shader* shader, Camera* camera, const glm::mat4& projection);
    RenderSystem(Shader* shader, FollowCamera* followCamera, const glm::mat4& projection);
    
    void Initialize(ECS::Registry& registry) override;
    void Update(float deltaTime, ECS::Registry& registry, uint64_t tickNumber) override;
    void Shutdown(ECS::Registry& registry) override;
    
    void SetProjection(const glm::mat4& projection) { m_projection = projection; }
    void SetShader(Shader* shader) { m_shader = shader; }
    void SetCamera(Camera* camera) { m_camera = camera; m_followCamera = nullptr; }
    void SetFollowCamera(FollowCamera* followCamera) { m_followCamera = followCamera; m_camera = nullptr; }
    
private:
    Shader* m_shader = nullptr;
    Camera* m_camera = nullptr;      // Legacy camera (optional)
    FollowCamera* m_followCamera = nullptr; // RPG-style follow camera (optional)
    glm::mat4 m_projection;
    
    // Default shadow map texture (dummy white texture = no shadows)
    GLuint m_defaultShadowMapTexture = 0;
    
    // Default white texture (for models without textures)
    GLuint m_defaultWhiteTexture = 0;
    
    void CreateDefaultShadowMap();
    void BindDefaultShadowMap();
    void CreateDefaultWhiteTexture();
    void BindDefaultWhiteTexture();
};

} // namespace Systems
} // namespace Render
} // namespace Platform
} // namespace Engine



