#pragma once

#include "core/network/VitalsReplication.h"

namespace Game {
namespace Character {

struct VitalsComponent {
    float currentHealth = 100.0f;
    float maxHealth = 100.0f;
    float currentMana = 100.0f;
    float maxMana = 100.0f;
};

inline Engine::Net::ReplicatedVitals ToReplicatedVitals(const VitalsComponent& vitals) {
    Engine::Net::ReplicatedVitals replicated;
    replicated.currentHealth = vitals.currentHealth;
    replicated.maxHealth = vitals.maxHealth;
    replicated.currentMana = vitals.currentMana;
    replicated.maxMana = vitals.maxMana;
    return replicated;
}

inline VitalsComponent FromReplicatedVitals(const Engine::Net::ReplicatedVitals& replicated) {
    VitalsComponent vitals;
    vitals.currentHealth = replicated.currentHealth;
    vitals.maxHealth = replicated.maxHealth;
    vitals.currentMana = replicated.currentMana;
    vitals.maxMana = replicated.maxMana;
    return vitals;
}

} // namespace Character
} // namespace Game
