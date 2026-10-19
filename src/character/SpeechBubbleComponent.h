#pragma once

#include "core/Types.h"
#include "core/ecs/Registry.h"
#include "core/network/ChatMessage.h"

namespace Game {
namespace Character {

struct SpeechBubbleComponent {
    static constexpr size_t kMaxTextLength = Engine::Net::kMaxChatMessageLength;

    char text[kMaxTextLength + 1] = {};
    float remainingSeconds = 0.0f;

    bool IsActive() const { return remainingSeconds > 0.0f && text[0] != '\0'; }
    void Show(const char* message, float durationSeconds = 5.0f);
    void Tick(float deltaSeconds);
};

void EnsureSpeechBubble(Engine::ECS::Registry& registry, Engine::EntityID entityId);
void ShowSpeechBubble(Engine::ECS::Registry& registry, Engine::EntityID entityId, const char* text);
void UpdateSpeechBubbles(Engine::ECS::Registry& registry, float deltaSeconds);

} // namespace Character
} // namespace Game
