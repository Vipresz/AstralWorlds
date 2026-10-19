#pragma once

#include "character/SkillCatalog.h"
#include "character/SkillComponent.h"
#include "character/SkillDefinition.h"

#include "core/Types.h"
#include "core/ecs/Registry.h"

namespace Game {
namespace Character {

class SkillActivator {
public:
    static bool TryActivate(Engine::ECS::Registry& registry,
                            Engine::EntityID entityId,
                            const SkillCatalog& catalog,
                            const SkillDefinition& skill);
};

} // namespace Character
} // namespace Game
