#include "audio/CharacterWalkAudioSystem.h"

#include "audio/AudioPaths.h"
#include "Logger.h"
#include "render/components/AnimationComponent.h"

#include <algorithm>

using Engine::Platform::Render::Components::AnimationComponent;

namespace Game {
namespace Audio {

CharacterWalkAudioSystem::CharacterWalkAudioSystem(Engine::Audio::AudioSystem* audioSystem,
                                                   Engine::EntityID playerEntity)
    : m_audioSystem(audioSystem), m_playerEntity(playerEntity), m_walkSoundPath(kWalkForestPath) {
    SetPriority(141);
}

void CharacterWalkAudioSystem::SetWalkSoundPath(const std::string& path) {
    m_walkSoundPath = path;
}

void CharacterWalkAudioSystem::SetWalkVolume(float volume) {
    m_walkVolume = std::max(0.0f, volume);
}

void CharacterWalkAudioSystem::Initialize(Engine::ECS::Registry& registry) {
    (void)registry;
    if (!m_audioSystem || m_walkSoundPath.empty()) {
        return;
    }

    m_audioSystem->LoadSound(m_walkSoundPath);
    LOG_INFO_FMT("CharacterWalkAudioSystem", "Walk sound ready: %s", m_walkSoundPath.c_str());
}

void CharacterWalkAudioSystem::UpdateWalkLoop(Engine::ECS::Registry& registry) {
    if (!m_audioSystem || m_playerEntity == Engine::INVALID_ENTITY || m_walkSoundPath.empty()) {
        return;
    }

    const AnimationComponent* anim = registry.GetComponent<AnimationComponent>(m_playerEntity);
    const bool isWalking =
        anim &&
        !anim->isJumping &&
        (anim->currentState == AnimationComponent::AnimationState::Walking ||
         anim->currentState == AnimationComponent::AnimationState::Running);

    if (isWalking) {
        if (!m_walkLoopPlaying) {
            m_audioSystem->Play2D(m_walkSoundPath, m_walkVolume, true);
            m_walkLoopPlaying = true;
        }
        return;
    }

    if (m_walkLoopPlaying) {
        m_audioSystem->Stop(m_walkSoundPath);
        m_walkLoopPlaying = false;
    }
}

void CharacterWalkAudioSystem::Update(float deltaTime,
                                      Engine::ECS::Registry& registry,
                                      uint64_t tickNumber) {
    (void)deltaTime;
    (void)tickNumber;
    UpdateWalkLoop(registry);
}

} // namespace Audio
} // namespace Game
