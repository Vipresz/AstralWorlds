#pragma once

#include "character/CharacterLoader.h"
#include "character/CharacterReplicationComponent.h"
#include "character/CharacterTypes.h"
#include "core/System.h"
#include "core/ecs/Registry.h"
#include "network/systems/NetworkSystem.h"

#include <string>
#include <unordered_map>

namespace Game {
namespace Character {

// Applies replicated loadouts to remote player entities.
class RemoteLoadoutApplySystem : public Engine::ECS::ISystem {
public:
    RemoteLoadoutApplySystem(CharacterLoader& loader,
                             Engine::Platform::Networking::Systems::NetworkSystem* networkSystem,
                             Engine::EntityID localPlayerEntity);

    void Update(float deltaTime, Engine::ECS::Registry& registry, uint64_t tickNumber) override;

    CharacterInstance* TryGetRemoteInstance(Engine::EntityID entityId);
    const CharacterInstance* TryGetRemoteInstance(Engine::EntityID entityId) const;

private:
    CharacterLoader& m_loader;
    Engine::Platform::Networking::Systems::NetworkSystem* m_networkSystem;
    Engine::EntityID m_localPlayerEntity;
    std::unordered_map<Engine::EntityID, CharacterInstance> m_remoteInstances;
    std::unordered_map<Engine::EntityID, CharacterLoadout> m_lastApplied;
};

} // namespace Character
} // namespace Game
