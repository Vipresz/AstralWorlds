#pragma once

#include "core/System.h"

namespace Engine {
namespace ECS {
class Registry;
}
} // namespace Engine

namespace Game {
namespace Character {

// Keeps equipped item entities aligned with their parent character transform.
class CharacterEquipmentSystem : public Engine::ECS::ISystem {
public:
    CharacterEquipmentSystem();

    void Update(float deltaTime, Engine::ECS::Registry& registry, uint64_t tickNumber) override;
};

} // namespace Character
} // namespace Game
