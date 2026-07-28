#include "character/CharacterNameplateRenderer.h"

#include "character/NameplateComponent.h"
#include "render/components/TransformComponent.h"
#include "ui/OverlayWindow.h"
#include "ui/WorldToScreen.h"

#include <cmath>

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#include "nuklear.h"

using Engine::ECS::Registry;
using Engine::Platform::Render::Components::TransformComponent;
using Engine::Platform::UI::OverlayScope;
using Game::UI::NameplateText;
using Game::UI::WorldToScreen;

namespace Game {
namespace Character {

void CharacterNameplateRenderer::Draw(struct nk_context* ctx,
                                      Registry& registry,
                                      const Engine::Platform::UI::FontLibrary& fonts,
                                      const Engine::Platform::Render::ICameraSource& camera,
                                      const glm::mat4& projectionMatrix,
                                      int screenWidth,
                                      int screenHeight,
                                      bool enabled) {
    if (!ctx || !enabled || screenWidth <= 0 || screenHeight <= 0) {
        return;
    }

    const glm::mat4 viewMatrix = camera.GetViewMatrix();
    const auto entities = registry.View<NameplateComponent, TransformComponent>();

    OverlayScope overlay(
        ctx,
        "CharacterNameplates",
        OverlayScope::FullScreen(screenWidth, screenHeight),
        NK_WINDOW_NO_INPUT);
    if (!overlay.IsOpen() || !overlay.Canvas()) {
        return;
    }

    for (Engine::EntityID entityId : entities) {
        const NameplateComponent* nameplate = registry.GetComponent<NameplateComponent>(entityId);
        const TransformComponent* transform = registry.GetComponent<TransformComponent>(entityId);
        if (!nameplate || !transform || !nameplate->visible || nameplate->displayName[0] == '\0') {
            continue;
        }

        const glm::vec3 labelWorldPos = transform->position +
            glm::vec3(0.0f, nameplate->worldHeightOffset, 0.0f);
        const Game::UI::ScreenPoint screen = WorldToScreen(
            labelWorldPos, viewMatrix, projectionMatrix, screenWidth, screenHeight);
        if (!screen.visible) {
            continue;
        }

        NameplateText::DrawCentered(
            ctx,
            fonts,
            nameplate->displayName,
            std::floor(screen.x + 0.5f),
            std::floor(screen.y + 0.5f));
    }
}

} // namespace Character
} // namespace Game
