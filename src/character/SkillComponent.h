#pragma once

#include <string>
#include <unordered_map>

namespace Game {
namespace Character {

struct SkillCooldownState {
    float remainingSeconds = 0.0f;
    float totalSeconds = 0.0f;
};

struct SkillComponent {
    bool isExecuting = false;
    std::string activeSkillId;
    float elapsedSeconds = 0.0f;
    float durationSeconds = 1.0f;
    bool damageApplied = false;

    std::unordered_map<std::string, SkillCooldownState> cooldowns;

    float GetCooldownRemaining(const std::string& skillId) const;
    float GetCooldownTotal(const std::string& skillId) const;
    bool IsOnCooldown(const std::string& skillId) const;
    void StartCooldown(const std::string& skillId, float totalSeconds);
    void TickCooldowns(float deltaSeconds);
};

} // namespace Character
} // namespace Game