#pragma once

#include "core/Types.h"
#include "character/CharacterInstance.h"

#include <GL/glew.h>

#include <chrono>

namespace Engine {
namespace ECS {
class Registry;
}
namespace Platform {
namespace Render {
class Shader;
}
} // namespace Platform
} // namespace Engine

namespace Game {
namespace Character {

// Renders the local character (body + attached equipment) for paperdoll UI panels.
class CharacterPaperdollRenderer {
public:
    ~CharacterPaperdollRenderer();

    void Shutdown();

    // Yaw around the body center vertical axis (degrees). Paperdoll-only; does not affect world.
    void SetYawDegrees(float yawDegrees) { m_yawDegrees = yawDegrees; }
    float GetYawDegrees() const { return m_yawDegrees; }

    bool RenderToTexture(Engine::ECS::Registry& registry,
                         Engine::EntityID characterEntity,
                         const CharacterInstance& instance,
                         Engine::Platform::Render::Shader& shader,
                         int pixelWidth,
                         int pixelHeight);

    GLuint GetColorTexture() const { return m_colorTexture; }
    int GetWidth() const { return m_width; }
    int GetHeight() const { return m_height; }

private:
    bool RenderScene(Engine::ECS::Registry& registry,
                     Engine::EntityID characterEntity,
                     const CharacterInstance& instance,
                     Engine::Platform::Render::Shader& shader,
                     int viewportWidth,
                     int viewportHeight);

    void EnsureFramebuffer(int width, int height);
    void EnsureDefaultTextures();
    void DestroyFramebuffer();
    void DestroyDefaultTextures();

    GLuint m_fbo = 0;
    GLuint m_colorTexture = 0;
    GLuint m_depthRenderbuffer = 0;
    int m_width = 0;
    int m_height = 0;
    GLuint m_defaultWhiteTexture = 0;
    GLuint m_defaultShadowMap = 0;
    float m_yawDegrees = 0.0f;
    float m_idleAnimationTime = 0.0f;
    bool m_hasIdleAnimClock = false;
    std::chrono::steady_clock::time_point m_lastIdleAnimTime{};
};

} // namespace Character
} // namespace Game
