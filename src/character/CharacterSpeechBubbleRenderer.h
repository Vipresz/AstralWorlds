#pragma once

#include "core/ecs/Registry.h"
#include "render/ICameraSource.h"
#include "ui/FontLibrary.h"

#include <glm/glm.hpp>

struct nk_context;

namespace Game {
namespace Character {

class CharacterSpeechBubbleRenderer {
public:
    static void Draw(struct nk_context* ctx,
                     Engine::ECS::Registry& registry,
                     const Engine::Platform::UI::FontLibrary& fonts,
                     const Engine::Platform::Render::ICameraSource& camera,
                     const glm::mat4& projectionMatrix,
                     int screenWidth,
                     int screenHeight,
                     bool enabled = true);
};

} // namespace Character
} // namespace Game
