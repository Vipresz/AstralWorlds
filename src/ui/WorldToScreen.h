#pragma once

#include <glm/glm.hpp>

namespace Game {
namespace UI {

struct ScreenPoint {
    float x = 0.0f;
    float y = 0.0f;
    bool visible = false;
};

// Project a world position to framebuffer pixel coordinates (origin top-left).
ScreenPoint WorldToScreen(const glm::vec3& worldPosition,
                          const glm::mat4& viewMatrix,
                          const glm::mat4& projectionMatrix,
                          int screenWidth,
                          int screenHeight);

} // namespace UI
} // namespace Game
