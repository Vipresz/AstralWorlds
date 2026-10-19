#include "character/SpeechBubbleComponent.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace Game {
namespace Character {

namespace {

constexpr float kMinBubbleDurationSeconds = 3.0f;
constexpr float kMaxBubbleDurationSeconds = 8.0f;

float BubbleDurationForText(const char* text) {
    if (!text) {
        return kMinBubbleDurationSeconds;
    }
    const float lengthFactor = static_cast<float>(std::strlen(text)) * 0.04f;
    return std::clamp(kMinBubbleDurationSeconds + lengthFactor,
                      kMinBubbleDurationSeconds,
                      kMaxBubbleDurationSeconds);
}

void CopyBubbleText(char* destination, size_t capacity, const char* source) {
    if (!destination || capacity == 0) {
        return;
    }
    destination[0] = '\0';
    if (!source || source[0] == '\0') {
        return;
    }

#if defined(_MSC_VER)
    strncpy_s(destination, capacity, source, SpeechBubbleComponent::kMaxTextLength);
#else
    std::strncpy(destination, source, SpeechBubbleComponent::kMaxTextLength);
    destination[SpeechBubbleComponent::kMaxTextLength] = '\0';
#endif
}

} // namespace

void SpeechBubbleComponent::Show(const char* message, float durationSeconds) {
    CopyBubbleText(text, sizeof(text), message);
    if (text[0] == '\0') {
        remainingSeconds = 0.0f;
        return;
    }
    remainingSeconds = durationSeconds > 0.0f ? durationSeconds : BubbleDurationForText(text);
}

void SpeechBubbleComponent::Tick(float deltaSeconds) {
    if (!IsActive()) {
        return;
    }
    remainingSeconds = std::max(0.0f, remainingSeconds - deltaSeconds);
    if (remainingSeconds <= 0.0f) {
        text[0] = '\0';
    }
}

void EnsureSpeechBubble(Engine::ECS::Registry& registry, Engine::EntityID entityId) {
    if (!registry.HasComponent<SpeechBubbleComponent>(entityId)) {
        registry.AddComponent<SpeechBubbleComponent>(entityId);
    }
}

void ShowSpeechBubble(Engine::ECS::Registry& registry, Engine::EntityID entityId, const char* text) {
    if (entityId == Engine::INVALID_ENTITY || !text || text[0] == '\0') {
        return;
    }
    EnsureSpeechBubble(registry, entityId);
    if (SpeechBubbleComponent* bubble = registry.GetComponent<SpeechBubbleComponent>(entityId)) {
        bubble->Show(text);
    }
}

void UpdateSpeechBubbles(Engine::ECS::Registry& registry, float deltaSeconds) {
    if (deltaSeconds <= 0.0f) {
        return;
    }
    const auto entities = registry.View<SpeechBubbleComponent>();
    for (Engine::EntityID entityId : entities) {
        if (SpeechBubbleComponent* bubble = registry.GetComponent<SpeechBubbleComponent>(entityId)) {
            bubble->Tick(deltaSeconds);
        }
    }
}

} // namespace Character
} // namespace Game
