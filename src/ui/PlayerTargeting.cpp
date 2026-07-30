#include "ui/PlayerTargeting.h"

#include "character/NameplateComponent.h"
#include "network/systems/NetworkSystem.h"
#include "render/components/TransformComponent.h"

#include <glm/glm.hpp>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace Game {
namespace UI {

namespace {

constexpr float kDefaultCharacterHeight = 1.8f;
constexpr float kFeetLift = 0.05f;
constexpr float kScreenPaddingX = 10.0f;
constexpr float kScreenPaddingY = 8.0f;
constexpr float kMinHalfWidthPixels = 26.0f;
constexpr float kWidthToHeightRatio = 0.34f;

bool WorldToScreen(const glm::vec3& worldPosition,
                   const glm::mat4& view,
                   const glm::mat4& projection,
                   int screenWidth,
                   int screenHeight,
                   glm::vec2& outScreen) {
    const glm::vec4 clip = projection * view * glm::vec4(worldPosition, 1.0f);
    if (clip.w <= 0.001f) {
        return false;
    }

    const glm::vec3 ndc = glm::vec3(clip) / clip.w;
    if (ndc.x < -1.2f || ndc.x > 1.2f || ndc.y < -1.2f || ndc.y > 1.2f) {
        return false;
    }

    outScreen.x = (ndc.x * 0.5f + 0.5f) * static_cast<float>(screenWidth);
    outScreen.y = (1.0f - (ndc.y * 0.5f + 0.5f)) * static_cast<float>(screenHeight);
    return true;
}

struct ScreenRect {
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
};

bool BuildCharacterScreenBounds(const glm::vec3& feetPosition,
                                float characterHeight,
                                const glm::mat4& view,
                                const glm::mat4& projection,
                                int screenWidth,
                                int screenHeight,
                                ScreenRect& outBounds) {
    const glm::vec3 headPosition = feetPosition + glm::vec3(0.0f, characterHeight, 0.0f);
    glm::vec2 screenFeet{};
    glm::vec2 screenHead{};
    if (!WorldToScreen(feetPosition, view, projection, screenWidth, screenHeight, screenFeet) ||
        !WorldToScreen(headPosition, view, projection, screenWidth, screenHeight, screenHead)) {
        return false;
    }

    const float topY = std::min(screenFeet.y, screenHead.y) - kScreenPaddingY;
    const float bottomY = std::max(screenFeet.y, screenHead.y) + kScreenPaddingY;
    const float bodyHeight = std::max(12.0f, bottomY - topY);
    const float halfWidth =
        std::max(kMinHalfWidthPixels, bodyHeight * kWidthToHeightRatio);
    const float centerX = (screenFeet.x + screenHead.x) * 0.5f;

    outBounds.x = centerX - halfWidth - kScreenPaddingX;
    outBounds.y = topY;
    outBounds.w = (halfWidth + kScreenPaddingX) * 2.0f;
    outBounds.h = bodyHeight;
    return outBounds.w > 0.0f && outBounds.h > 0.0f;
}

bool PointInRect(float x, float y, const ScreenRect& rect) {
    return x >= rect.x && y >= rect.y && x <= rect.x + rect.w && y <= rect.y + rect.h;
}

} // namespace

bool TryPickRemotePlayerAtScreen(Engine::ECS::Registry& registry,
                                 Engine::Platform::Networking::Systems::NetworkSystem& network,
                                 const Engine::Platform::Render::ICameraSource& camera,
                                 const glm::mat4& projection,
                                 int screenWidth,
                                 int screenHeight,
                                 float mouseX,
                                 float mouseY,
                                 PlayerPickResult& out,
                                 float /*maxScreenDistancePixels*/) {
    out = PlayerPickResult{};
    if (screenWidth <= 0 || screenHeight <= 0) {
        return false;
    }

    std::vector<Engine::EntityID> remoteEntities;
    network.CollectRemotePlayerEntities(remoteEntities);
    if (remoteEntities.empty()) {
        return false;
    }

    const glm::mat4 view = camera.GetViewMatrix();
    const glm::vec3 cameraPosition = camera.GetEyePosition();
    float bestDistance = 1e9f;
    Engine::EntityID bestEntity = Engine::INVALID_ENTITY;

    for (Engine::EntityID entity : remoteEntities) {
        const auto* transform =
            registry.GetComponent<Engine::Platform::Render::Components::TransformComponent>(entity);
        if (!transform) {
            continue;
        }

        float characterHeight = kDefaultCharacterHeight;
        if (const auto* nameplate = registry.GetComponent<Game::Character::NameplateComponent>(entity)) {
            characterHeight = std::max(1.2f, nameplate->worldHeightOffset);
        }

        const glm::vec3 feetPosition = transform->position + glm::vec3(0.0f, kFeetLift, 0.0f);
        ScreenRect screenBounds{};
        if (!BuildCharacterScreenBounds(feetPosition,
                                        characterHeight,
                                        view,
                                        projection,
                                        screenWidth,
                                        screenHeight,
                                        screenBounds)) {
            continue;
        }

        if (!PointInRect(mouseX, mouseY, screenBounds)) {
            continue;
        }

        const float worldDistance = glm::length(cameraPosition - transform->position);
        if (worldDistance >= bestDistance) {
            continue;
        }

        bestDistance = worldDistance;
        bestEntity = entity;
    }

    if (bestEntity == Engine::INVALID_ENTITY) {
        return false;
    }

    out.entity = bestEntity;
    out.userId = network.TryGetUserIdForEntity(bestEntity);
    if (const auto* nameplate =
            registry.GetComponent<Game::Character::NameplateComponent>(bestEntity)) {
        out.displayName = nameplate->GetDisplayName();
    } else {
        out.displayName = network.GetPeerDisplayName(out.userId);
    }
    return out.userId != Engine::Net::kInvalidUserId;
}

bool TryGetHorizontalDistanceBetweenEntities(Engine::ECS::Registry& registry,
                                             Engine::EntityID firstEntity,
                                             Engine::EntityID secondEntity,
                                             float& outDistance) {
    outDistance = 0.0f;
    if (firstEntity == Engine::INVALID_ENTITY || secondEntity == Engine::INVALID_ENTITY) {
        return false;
    }

    const auto* firstTransform =
        registry.GetComponent<Engine::Platform::Render::Components::TransformComponent>(
            firstEntity);
    const auto* secondTransform =
        registry.GetComponent<Engine::Platform::Render::Components::TransformComponent>(
            secondEntity);
    if (!firstTransform || !secondTransform) {
        return false;
    }

    const float dx = firstTransform->position.x - secondTransform->position.x;
    const float dz = firstTransform->position.z - secondTransform->position.z;
    outDistance = std::sqrt(dx * dx + dz * dz);
    return true;
}

} // namespace UI
} // namespace Game
