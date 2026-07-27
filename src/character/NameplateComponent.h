#pragma once

#include "core/Types.h"
#include "core/ecs/Registry.h"

#include <cstring>

namespace Game {
namespace Character {

struct NameplateComponent {
    static constexpr size_t kMaxNameLength = 63;

    char displayName[kMaxNameLength + 1] = {};
    float worldHeightOffset = 2.0f;
    bool visible = true;

    void SetDisplayName(const char* name);
    const char* GetDisplayName() const { return displayName; }
};

void AttachNameplate(Engine::ECS::Registry& registry,
                     Engine::EntityID entityId,
                     const char* displayName,
                     float worldHeightOffset = 2.0f);

} // namespace Character
} // namespace Game
