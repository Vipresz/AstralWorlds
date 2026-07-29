#include "character/CharacterSpeechBubbleRenderer.h"

#include "character/NameplateComponent.h"
#include "character/SpeechBubbleComponent.h"
#include "render/components/TransformComponent.h"
#include "ui/OverlayWindow.h"
#include "ui/SpeechBubbleText.h"
#include "ui/WorldToScreen.h"

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#include "nuklear.h"

#include <cmath>

using Engine::ECS::Registry;
using Engine::Platform::Render::Components::TransformComponent;
using Engine::Platform::UI::OverlayScope;
using Game::UI::SpeechBubbleText;
using Game::UI::WorldToScreen;

namespace Game {
namespace Character {

namespace {

constexpr float kBubbleGapAboveNameplate = 0.45f;
constexpr float kDefaultBubbleHeight = 2.35f;

float ResolveBubbleWorldOffset(const NameplateComponent* nameplate) {
    if (nameplate && nameplate->visible) {
        return nameplate->worldHeightOffset + kBubbleGapAboveNameplate;
    }
    return kDefaultBubbleHeight;
}

} // namespace

void CharacterSpeechBubbleRenderer::Draw(struct nk_context* ctx,
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
    const auto entities = registry.View<SpeechBubbleComponent, TransformComponent>();

    OverlayScope overlay(
        ctx,
        "CharacterSpeechBubbles",
        OverlayScope::FullScreen(screenWidth, screenHeight),
        NK_WINDOW_NO_INPUT);
    if (!overlay.IsOpen() || !overlay.Canvas()) {
        return;
    }

    for (Engine::EntityID entityId : entities) {
        const SpeechBubbleComponent* bubble = registry.GetComponent<SpeechBubbleComponent>(entityId);
        const TransformComponent* transform = registry.GetComponent<TransformComponent>(entityId);
        if (!bubble || !transform || !bubble->IsActive()) {
            continue;
        }

        const NameplateComponent* nameplate = registry.GetComponent<NameplateComponent>(entityId);
        const glm::vec3 bubbleWorldPos =
            transform->position + glm::vec3(0.0f, ResolveBubbleWorldOffset(nameplate), 0.0f);
        const Game::UI::ScreenPoint screen =
            WorldToScreen(bubbleWorldPos, viewMatrix, projectionMatrix, screenWidth, screenHeight);
        if (!screen.visible) {
            continue;
        }

        SpeechBubbleText::DrawAbove(
            ctx, fonts, bubble->text, std::floor(screen.x + 0.5f), std::floor(screen.y + 0.5f));
    }
}

} // namespace Character
} // namespace Game
