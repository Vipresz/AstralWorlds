#include "ui/WorldToScreen.h"

namespace Game {
namespace UI {

ScreenPoint WorldToScreen(const glm::vec3& worldPosition,
                          const glm::mat4& viewMatrix,
                          const glm::mat4& projectionMatrix,
                          int screenWidth,
                          int screenHeight) {
    ScreenPoint result;

    if (screenWidth <= 0 || screenHeight <= 0) {
        return result;
    }

    const glm::vec4 clip = projectionMatrix * viewMatrix * glm::vec4(worldPosition, 1.0f);
    if (clip.w <= 0.0001f) {
        return result;
    }

    const glm::vec3 ndc = glm::vec3(clip) / clip.w;
    if (ndc.z < -1.0f || ndc.z > 1.0f) {
        return result;
    }

    result.x = (ndc.x * 0.5f + 0.5f) * static_cast<float>(screenWidth);
    result.y = (1.0f - (ndc.y * 0.5f + 0.5f)) * static_cast<float>(screenHeight);
    result.visible = true;
    return result;
}

} // namespace UI
} // namespace Game
