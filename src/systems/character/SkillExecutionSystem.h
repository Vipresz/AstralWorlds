#pragma once

#include "character/SkillCatalog.h"
#include "core/System.h"

namespace Game {
namespace Character {

class SkillExecutionSystem : public Engine::ECS::ISystem {
public:
    explicit SkillExecutionSystem(const SkillCatalog* catalog);

    void Initialize(Engine::ECS::Registry& registry) override;
    void Update(float deltaTime, Engine::ECS::Registry& registry, uint64_t tickNumber) override;
    void Shutdown(Engine::ECS::Registry& registry) override;

private:
    const SkillCatalog* m_catalog = nullptr;
};

} // namespace Character
} // namespace Game
