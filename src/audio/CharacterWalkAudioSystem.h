#pragma once

#include "audio/AudioSystem.h"
#include "core/System.h"
#include "core/ecs/Registry.h"

#include <string>

namespace Engine {
namespace ECS {
class Registry;
}
} // namespace Engine

namespace Game {
namespace Audio {

// Loops walk_forest.wav while the local player is walking or running.
class CharacterWalkAudioSystem : public Engine::ECS::ISystem {
public:
    CharacterWalkAudioSystem(Engine::Audio::AudioSystem* audioSystem, Engine::EntityID playerEntity);

    void SetWalkSoundPath(const std::string& path);
    void SetWalkVolume(float volume);

    void Initialize(Engine::ECS::Registry& registry) override;
    void Update(float deltaTime, Engine::ECS::Registry& registry, uint64_t tickNumber) override;

private:
    void UpdateWalkLoop(Engine::ECS::Registry& registry);

    Engine::Audio::AudioSystem* m_audioSystem = nullptr;
    Engine::EntityID m_playerEntity = Engine::INVALID_ENTITY;
    std::string m_walkSoundPath;
    float m_walkVolume = 0.75f;
    bool m_walkLoopPlaying = false;
};

} // namespace Audio
} // namespace Game
