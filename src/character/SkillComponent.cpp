#include "character/SkillComponent.h"

#include <algorithm>

namespace Game {
namespace Character {

float SkillComponent::GetCooldownRemaining(const std::string& skillId) const {
    const auto it = cooldowns.find(skillId);
    if (it == cooldowns.end()) {
        return 0.0f;
    }
    return std::max(0.0f, it->second.remainingSeconds);
}

float SkillComponent::GetCooldownTotal(const std::string& skillId) const {
    const auto it = cooldowns.find(skillId);
    if (it == cooldowns.end()) {
        return 0.0f;
    }
    return std::max(0.0f, it->second.totalSeconds);
}

bool SkillComponent::IsOnCooldown(const std::string& skillId) const {
    return GetCooldownRemaining(skillId) > 0.0f;
}

void SkillComponent::StartCooldown(const std::string& skillId, float totalSeconds) {
    if (skillId.empty() || totalSeconds <= 0.0f) {
        return;
    }

    SkillCooldownState& state = cooldowns[skillId];
    state.totalSeconds = totalSeconds;
    state.remainingSeconds = totalSeconds;
}

void SkillComponent::TickCooldowns(float deltaSeconds) {
    if (deltaSeconds <= 0.0f) {
        return;
    }

    for (auto& entry : cooldowns) {
        if (entry.second.remainingSeconds > 0.0f) {
            entry.second.remainingSeconds =
                std::max(0.0f, entry.second.remainingSeconds - deltaSeconds);
        }
    }
}

} // namespace Character
} // namespace Game
