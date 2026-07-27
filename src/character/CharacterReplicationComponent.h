#pragma once

#include "character/CharacterTypes.h"

namespace Game {
namespace Character {

// Holds the active loadout for network replication (local + remote players).
struct CharacterReplicationComponent {
    CharacterLoadout loadout;
};

} // namespace Character
} // namespace Game
