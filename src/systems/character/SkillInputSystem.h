#pragma once

#include "character/SkillCatalog.h"
#include "core/System.h"
#include "core/Types.h"
#include "render/Window.h"

#include <functional>

namespace Game {
namespace Character {

class SkillInputSystem : public Engine::ECS::ISystem {
public:
    SkillInputSystem(Engine::Platform::Window* window, const SkillCatalog* catalog);

    void Initialize(Engine::ECS::Registry& registry) override;
    void Update(float deltaTime, Engine::ECS::Registry& registry, uint64_t tickNumber) override;
    void Shutdown(Engine::ECS::Registry& registry) override;

    void SetCharacterEntity(Engine::EntityID entityId) { m_characterEntityId = entityId; }
    void SetInputBlockQuery(std::function<bool()> query) { m_inputBlockQuery = std::move(query); }
    void SetSlotSkillResolver(std::function<const SkillDefinition*(int slotIndex)> resolver) {
        m_slotSkillResolver = std::move(resolver);
    }

private:
    Engine::Platform::Window* m_window = nullptr;
    const SkillCatalog* m_catalog = nullptr;
    std::function<const SkillDefinition*(int slotIndex)> m_slotSkillResolver;
    Engine::EntityID m_characterEntityId = 0;
    std::function<bool()> m_inputBlockQuery;
    bool m_hotkeyPressedLastFrame[10] = {};
};

} // namespace Character
} // namespace Game
