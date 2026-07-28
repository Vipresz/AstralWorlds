#pragma once

#include "character/CharacterTypes.h"
#include "core/network/LoadoutReplication.h"
#include "network/systems/ReplicationSystem.h"

namespace Game {
namespace Character {

Engine::Net::ReplicatedLoadout ToReplicatedLoadout(const CharacterLoadout& loadout);
CharacterLoadout FromReplicatedLoadout(const Engine::Net::ReplicatedLoadout& replicated);

void RegisterLoadoutReplication(Engine::Platform::Networking::Systems::ReplicationSystem& replication);

} // namespace Character
} // namespace Game
