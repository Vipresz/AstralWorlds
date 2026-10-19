#include "character/VitalsReplication.h"

#include "character/VitalsComponent.h"

namespace Game {
namespace Character {

void RegisterVitalsReplication(Engine::Platform::Networking::Systems::ReplicationSystem& replication) {
    Engine::Net::ReplicationComponentDescriptor vitalsDesc;
    vitalsDesc.id = Engine::Net::kVitalsReplicationComponent;
    vitalsDesc.name = "Vitals";
    replication.GetRegistry().Register(std::move(vitalsDesc));

    replication.RegisterComponentBinding(
        Engine::Platform::Networking::Systems::ReplicationComponentBinding{
            Engine::Net::kVitalsReplicationComponent,
            [](Engine::EntityID entity, Engine::ECS::Registry& registry, Engine::Net::NetBuffer& out) {
                const VitalsComponent* vitals = registry.GetComponent<VitalsComponent>(entity);
                if (!vitals) {
                    return false;
                }
                out = ToReplicatedVitals(*vitals).Serialize();
                return true;
            },
            [](Engine::EntityID entity,
               Engine::ECS::Registry& registry,
               const Engine::Net::NetBuffer& in) {
                Engine::Net::ReplicatedVitals replicated;
                if (!Engine::Net::ReplicatedVitals::Deserialize(in, replicated)) {
                    return false;
                }
                VitalsComponent* vitals = registry.GetComponent<VitalsComponent>(entity);
                if (!vitals) {
                    vitals = &registry.AddComponent<VitalsComponent>(entity);
                }
                *vitals = FromReplicatedVitals(replicated);
                return true;
            }});
}

} // namespace Character
} // namespace Game
