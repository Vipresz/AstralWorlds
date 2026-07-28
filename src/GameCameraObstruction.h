#pragma once

#include "render/ICameraObstructionQuery.h"
#include "core/ecs/Registry.h"
#include "core/Types.h"

#include <functional>

namespace Game {

// Camera collision against terrain and static meshes; skips character bodies/equipment.
class GameCameraObstruction : public Engine::Platform::Render::ICameraObstructionQuery {
public:
    void SetRegistry(Engine::ECS::Registry* registry) { m_registry = registry; }
    void SetInputBlockedQuery(std::function<bool()> query) { m_inputBlocked = std::move(query); }

    bool QuerySegment(const glm::vec3& from, const glm::vec3& to, Engine::EntityID skipEntityId,
                      float& outHitDistance) const override;

private:
    Engine::ECS::Registry* m_registry = nullptr;
    std::function<bool()> m_inputBlocked;
};

} // namespace Game
