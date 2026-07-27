#include "systems/character/RemoteLoadoutApplySystem.h"

#include <vector>

namespace Game {
namespace Character {

RemoteLoadoutApplySystem::RemoteLoadoutApplySystem(
    CharacterLoader& loader,
    Engine::Platform::Networking::Systems::NetworkSystem* networkSystem,
    Engine::EntityID localPlayerEntity)
    : m_loader(loader), m_networkSystem(networkSystem), m_localPlayerEntity(localPlayerEntity) {
    SetPriority(56);
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

        const CharacterReplicationComponent* replicationComp =
            registry.GetComponent<CharacterReplicationComponent>(entityId);
        if (!replicationComp) {
            continue;
        }

        const auto lastIt = m_lastApplied.find(entityId);
        if (lastIt != m_lastApplied.end() && lastIt->second.bodyItemId == replicationComp->loadout.bodyItemId &&
            lastIt->second.equipment.size() == replicationComp->loadout.equipment.size()) {
            bool same = true;
            for (size_t i = 0; i < replicationComp->loadout.equipment.size(); ++i) {
                if (lastIt->second.equipment[i].itemId != replicationComp->loadout.equipment[i].itemId ||
                    lastIt->second.equipment[i].slot != replicationComp->loadout.equipment[i].slot) {
                    same = false;
                    break;
                }
            }
            if (same) {
                continue;
            }
        }

        CharacterInstance& instance = m_remoteInstances[entityId];
        if (m_loader.ApplyLoadout(registry, instance, entityId, replicationComp->loadout)) {
            m_lastApplied[entityId] = replicationComp->loadout;
        }
    }
}

} // namespace Character
} // namespace Game
