#include "character/SkillDefinition.h"

#include <algorithm>
#include <cctype>

namespace Game {
namespace Character {

namespace {

std::string ToLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

} // namespace

SkillTargetType SkillTargetTypeFromString(const std::string& value) {
    const std::string lower = ToLower(value);
    if (lower == "self") {
        return SkillTargetType::Self;
    }
    if (lower == "ally") {
        return SkillTargetType::Ally;
    }
    if (lower == "all_in_area" || lower == "all") {
        return SkillTargetType::AllInArea;
    }
    return SkillTargetType::Enemy;
}

SkillAreaShape SkillAreaShapeFromString(const std::string& value) {
    const std::string lower = ToLower(value);
    if (lower == "single") {
        return SkillAreaShape::Single;
    }
    if (lower == "sphere" || lower == "circle") {
        return SkillAreaShape::Sphere;
    }
    return SkillAreaShape::Cone;
}

const char* SkillTargetTypeToString(SkillTargetType type) {
    switch (type) {
        case SkillTargetType::Self:
            return "self";
        case SkillTargetType::Ally:
            return "ally";
        case SkillTargetType::AllInArea:
            return "all_in_area";
        case SkillTargetType::Enemy:
        default:
            return "enemy";
    }
}

const char* SkillAreaShapeToString(SkillAreaShape shape) {
    switch (shape) {
        case SkillAreaShape::Single:
            return "single";
        case SkillAreaShape::Sphere:
            return "sphere";
        case SkillAreaShape::Cone:
        default:
            return "cone";
    }
}

} // namespace Character
} // namespace Game
