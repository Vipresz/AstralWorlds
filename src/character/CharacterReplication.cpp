#include "character/CharacterReplication.h"

#include "character/CharacterReplicationComponent.h"

namespace Game {
namespace Character {

Engine::Net::ReplicatedLoadout ToReplicatedLoadout(const CharacterLoadout& loadout) {
    Engine::Net::ReplicatedLoadout replicated;
    replicated.bodyItemId = loadout.bodyItemId;
    replicated.equipment.reserve(loadout.equipment.size());
    for (const EquippedItem& item : loadout.equipment) {
        Engine::Net::ReplicatedEquippedItem entry;
        entry.slot = static_cast<uint16_t>(item.slot);
        entry.itemId = item.itemId;
        replicated.equipment.push_back(std::move(entry));
    }
    return replicated;
}

CharacterLoadout FromReplicatedLoadout(const Engine::Net::ReplicatedLoadout& replicated) {
    CharacterLoadout loadout;
    loadout.bodyItemId = replicated.bodyItemId;
    loadout.equipment.reserve(replicated.equipment.size());
    for (const Engine::Net::ReplicatedEquippedItem& entry : replicated.equipment) {
        EquippedItem item;
        item.slot = static_cast<EquipSlot>(entry.slot);
        item.itemId = entry.itemId;
        loadout.equipment.push_back(std::move(item));
    }
    return loadout;
}

void RegisterLoadoutReplication(Engine::Platform::Networking::Systems::ReplicationSystem& replication) {
    Engine::Net::ReplicationComponentDescriptor loadoutDesc;
    loadoutDesc.id = Engine::Net::kLoadoutReplicationComponent;
    loadoutDesc.name = "Loadout";
    replication.GetRegistry().Register(std::move(loadoutDesc));

    replication.RegisterComponentBinding(
        Engine::Platform::Networking::Systems::ReplicationComponentBinding{
            Engine::Net::kLoadoutReplicationComponent,
            [](Engine::EntityID entity, Engine::ECS::Registry& registry, Engine::Net::NetBuffer& out) {
                const CharacterReplicationComponent* replicationComp =
                    registry.GetComponent<CharacterReplicationComponent>(entity);
                if (!replicationComp) {
                    return false;
                }
                const Engine::Net::NetBuffer buffer =
                    ToReplicatedLoadout(replicationComp->loadout).Serialize();
                out = buffer;
                return true;
            },
            [](Engine::EntityID entity,
               Engine::ECS::Registry& registry,
               const Engine::Net::NetBuffer& in) {
                CharacterReplicationComponent* replicationComp =
                    registry.GetComponent<CharacterReplicationComponent>(entity);
                if (!replicationComp) {
                    replicationComp = &registry.AddComponent<CharacterReplicationComponent>(entity);
                }
                Engine::Net::ReplicatedLoadout replicated;
                if (!Engine::Net::ReplicatedLoadout::Deserialize(in, replicated)) {
                    return false;
                }
                replicationComp->loadout = FromReplicatedLoadout(replicated);
                return true;
            }});
}

} // namespace Character
} // namespace Game
