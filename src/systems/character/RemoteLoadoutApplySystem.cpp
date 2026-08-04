#include "systems/character/RemoteLoadoutApplySystem.h"

#include "character/CharacterStats.h"
#include "render/components/AnimationComponent.h"

#include <vector>

using Engine::Platform::Render::Components::AnimationComponent;

namespace Game {
namespace Character {

namespace {

bool IsRemoteAnimationWired(Engine::ECS::Registry& registry, Engine::EntityID entityId) {
    const AnimationComponent* anim = registry.GetComponent<AnimationComponent>(entityId);
    return anim && anim->skeleton && anim->skeleton->IsLoaded() && anim->walkingAnimation != nullptr;
}

void ApplyLocomotionReferenceSpeeds(Engine::ECS::Registry& registry, Engine::EntityID entityId) {
    AnimationComponent* anim = registry.GetComponent<AnimationComponent>(entityId);
    if (!anim) {
        return;
    }

    CharacterStats stats;
    LoadCharacterStats(stats);
    anim->walkAnimReferenceSpeed = stats.walkSpeed;
    anim->runAnimReferenceSpeed = stats.runSpeed;
}

bool LoadoutsMatch(const CharacterLoadout& a, const CharacterLoadout& b) {
    if (a.bodyItemId != b.bodyItemId || a.equipment.size() != b.equipment.size()) {
        return false;
    }
    for (size_t i = 0; i < a.equipment.size(); ++i) {
        if (a.equipment[i].itemId != b.equipment[i].itemId ||
            a.equipment[i].slot != b.equipment[i].slot) {
            return false;
        }
    }
    return true;
}

} // namespace

RemoteLoadoutApplySystem::RemoteLoadoutApplySystem(
    CharacterLoader& loader,
    Engine::Platform::Networking::Systems::NetworkSystem* networkSystem,
    Engine::EntityID localPlayerEntity)
    : m_loader(loader), m_networkSystem(networkSystem), m_localPlayerEntity(localPlayerEntity) {
    SetPriority(56);
}

CharacterInstance* RemoteLoadoutApplySystem::TryGetRemoteInstance(Engine::EntityID entityId) {
    const auto it = m_remoteInstances.find(entityId);
    return it != m_remoteInstances.end() ? &it->second : nullptr;
}

const CharacterInstance* RemoteLoadoutApplySystem::TryGetRemoteInstance(
    Engine::EntityID entityId) const {
    const auto it = m_remoteInstances.find(entityId);
    return it != m_remoteInstances.end() ? &it->second : nullptr;
}

void RemoteLoadoutApplySystem::Update(float deltaTime,
                                      Engine::ECS::Registry& registry,
                                      uint64_t tickNumber) {
    (void)deltaTime;
    (void)tickNumber;
    if (!m_networkSystem) {
        return;
    }

    std::vector<Engine::EntityID> remoteEntities;
    m_networkSystem->CollectRemotePlayerEntities(remoteEntities);
    for (Engine::EntityID entityId : remoteEntities) {
        if (entityId == m_localPlayerEntity) {
            continue;
        }

        CharacterLoadout desiredLoadout;
        const CharacterReplicationComponent* replicationComp =
            registry.GetComponent<CharacterReplicationComponent>(entityId);
        if (replicationComp && !replicationComp->loadout.bodyItemId.empty()) {
            desiredLoadout = replicationComp->loadout;
        } else if (!m_loader.BuildDefaultLoadout(desiredLoadout)) {
            continue;
        }

        const auto lastIt = m_lastApplied.find(entityId);
        const bool loadoutUnchanged =
            lastIt != m_lastApplied.end() && LoadoutsMatch(lastIt->second, desiredLoadout);
        const bool animationWired = IsRemoteAnimationWired(registry, entityId);
        if (loadoutUnchanged && animationWired) {
            continue;
        }

        CharacterInstance& instance = m_remoteInstances[entityId];
        if (m_loader.ApplyLoadout(registry, instance, entityId, desiredLoadout)) {
            instance.WireAnimationComponent(registry);
            ApplyLocomotionReferenceSpeeds(registry, entityId);
            m_lastApplied[entityId] = desiredLoadout;
        } else if (!animationWired) {
            instance.WireAnimationComponent(registry);
            ApplyLocomotionReferenceSpeeds(registry, entityId);
        }
    }
}

} // namespace Character
} // namespace Game
