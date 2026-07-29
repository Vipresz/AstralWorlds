#include "character/NameplateComponent.h"
#include "character/SpeechBubbleComponent.h"

namespace Game {
namespace Character {

void NameplateComponent::SetDisplayName(const char* name) {
    displayName[0] = '\0';
    visible = false;
    if (!name || name[0] == '\0') {
        return;
    }

#if defined(_MSC_VER)
    strncpy_s(displayName, sizeof(displayName), name, kMaxNameLength);
#else
    std::strncpy(displayName, name, kMaxNameLength);
    displayName[kMaxNameLength] = '\0';
#endif
    visible = true;
}

void AttachNameplate(Engine::ECS::Registry& registry,
                     Engine::EntityID entityId,
                     const char* displayName,
                     float worldHeightOffset) {
    if (!registry.HasComponent<NameplateComponent>(entityId)) {
        registry.AddComponent<NameplateComponent>(entityId);
    }

    if (NameplateComponent* nameplate = registry.GetComponent<NameplateComponent>(entityId)) {
        nameplate->SetDisplayName(displayName);
        nameplate->worldHeightOffset = worldHeightOffset;
    }
    EnsureSpeechBubble(registry, entityId);
}

} // namespace Character
} // namespace Game
