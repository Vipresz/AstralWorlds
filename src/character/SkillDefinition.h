#pragma once

#include <string>

namespace Game {
namespace Character {

enum class SkillTargetType {
    Enemy,
    Self,
    Ally,
    AllInArea
};

enum class SkillAreaShape {
    Single,
    Cone,
    Sphere
};

struct SkillAreaDefinition {
    SkillAreaShape shape = SkillAreaShape::Cone;
    float range = 2.5f;
    float angleDegrees = 60.0f;
    float radius = 2.0f;
};

struct SkillDefinition {
    std::string id;
    std::string name;
    std::string animationPath;
    std::string iconUnpressed;
    std::string iconPressed;
    std::string hotkey;
    float durationSeconds = 1.0f;
    float cooldownSeconds = 2.0f;
    bool fullBody = false;
    float damage = 0.0f;
    float damageDelaySeconds = 0.4f;
    SkillTargetType target = SkillTargetType::Enemy;
    SkillAreaDefinition area;
};

SkillTargetType SkillTargetTypeFromString(const std::string& value);
SkillAreaShape SkillAreaShapeFromString(const std::string& value);
const char* SkillTargetTypeToString(SkillTargetType type);
const char* SkillAreaShapeToString(SkillAreaShape shape);

} // namespace Character
} // namespace Game
