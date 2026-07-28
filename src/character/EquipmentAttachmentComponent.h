#pragma once

#include "core/Types.h"

#include <glm/glm.hpp>

namespace Game {
namespace Character {

struct EquipmentAttachmentComponent {
    Engine::EntityID parentEntity = Engine::INVALID_ENTITY;
    glm::vec3 localOffset{0.0f};
    glm::vec3 localRotationDegrees{0.0f};
    glm::vec3 localScale{1.0f};
    bool inheritParentYaw = true;
    bool inheritParentScale = true;
};

} // namespace Character
} // namespace Game
