#pragma once

#include "character/CharacterBodyAssetCache.h"
#include "core/Types.h"
#include "core/ecs/Registry.h"
#include "render/Model.h"
#include "render/Skeleton.h"

#include <memory>
#include <vector>

namespace Game {
namespace Character {

struct CharacterLoadout;

// Owns loaded models and ECS entities that make up a character appearance.
class CharacterInstance {
public:
    static constexpr float kTargetHeightMeters = 1.8f;

    Engine::EntityID rootEntity = Engine::INVALID_ENTITY;
    float scale = 1.0f;
    float height = 0.0f;
    float bottomY = 0.0f;

    Engine::Platform::Render::Model* GetBodyModel() const;
    bool HasSkinnedBody() const;
    float GetHeadHeightOffset() const;

    ~CharacterInstance();

    void WireAnimationComponent(Engine::ECS::Registry& registry);
    void Clear(Engine::ECS::Registry& registry);

private:
    friend class CharacterLoader;

    void SetBodyAssets(std::shared_ptr<const CharacterBodyAssets> assets);

    std::shared_ptr<const CharacterBodyAssets> m_bodyAssets;
    std::unique_ptr<Engine::Platform::Render::Skeleton> m_instanceSkeleton;
    std::vector<std::shared_ptr<Engine::Platform::Render::Model>> m_itemModels;
    std::vector<Engine::EntityID> m_equipmentEntities;
};

} // namespace Character
} // namespace Game
