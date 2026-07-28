#pragma once

#include "render/Animation.h"
#include "render/Model.h"
#include "render/Skeleton.h"

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace Game {
namespace Character {

// Immutable body mesh, skeleton, and locomotion clips shared across character instances.
struct CharacterBodyAssets {
    std::shared_ptr<Engine::Platform::Render::Model> bodyModel;
    std::shared_ptr<Engine::Platform::Render::Skeleton> skeleton;
    std::shared_ptr<Engine::Platform::Render::Animation> idleAnimation;
    std::shared_ptr<Engine::Platform::Render::Animation> walkingAnimation;
    std::shared_ptr<Engine::Platform::Render::Animation> runningAnimation;
    std::shared_ptr<Engine::Platform::Render::Animation> jumpAnimation;
    std::shared_ptr<Engine::Platform::Render::Animation> jumpMovingAnimation;
    std::shared_ptr<Engine::Platform::Render::Animation> fallingAnimation;
    std::shared_ptr<Engine::Platform::Render::Animation> landingAnimation;
    std::shared_ptr<Engine::Platform::Render::Animation> leftTurnAnimation;
    std::shared_ptr<Engine::Platform::Render::Animation> rightTurnAnimation;
    float height = 0.0f;
    float bottomY = 0.0f;
    float scale = 1.0f;
};

// Path-keyed cache for character body assets. Thread-safe; one Assimp import per body model.
class CharacterBodyAssetCache {
public:
    static CharacterBodyAssetCache& Instance();

    std::shared_ptr<const CharacterBodyAssets> GetOrLoad(const std::string& cacheKey,
                                                         const std::string& modelPath);
    void Clear();

    size_t EntryCount() const;

private:
    CharacterBodyAssetCache() = default;

    mutable std::mutex m_mutex;
    std::unordered_map<std::string, std::weak_ptr<const CharacterBodyAssets>> m_entries;
};

} // namespace Character
} // namespace Game
