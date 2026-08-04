#pragma once

#include "network/systems/ReplicationSystem.h"

namespace Game {
namespace Character {

void RegisterVitalsReplication(Engine::Platform::Networking::Systems::ReplicationSystem& replication);

} // namespace Character
} // namespace Game
