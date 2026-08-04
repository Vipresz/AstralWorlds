#include "ui/PlayerTargetHud.h"

#include "network/systems/NetworkSystem.h"
#include "render/Window.h"

#include <GLFW/glfw3.h>

#include <cmath>
#include <vector>
#include <algorithm>

namespace Game {
namespace UI {

namespace {

constexpr float kClickMoveThresholdPixels = 8.0f;

bool IsMouseDown(Engine::Platform::Window& window, int button) {
    if (window.isMouseButtonDown(button)) {
        return true;
    }
    GLFWwindow* glfwWindow = window.GetGLFWWindow();
    return glfwWindow && glfwGetMouseButton(glfwWindow, button) == GLFW_PRESS;
}

void GetMousePosition(Engine::Platform::Window& window, float& outX, float& outY) {
    GLFWwindow* glfwWindow = window.GetGLFWWindow();
    double mouseX = 0.0;
    double mouseY = 0.0;
    if (glfwWindow) {
        glfwGetCursorPos(glfwWindow, &mouseX, &mouseY);
    }
    outX = static_cast<float>(mouseX);
    outY = static_cast<float>(mouseY);
}

} // namespace

void PlayerTargetHud::ClearTarget() {
    m_target = PlayerPickResult{};
}

void PlayerTargetHud::Update(Engine::Platform::Window& window,
                             Engine::ECS::Registry& registry,
                             Engine::Platform::Networking::Systems::NetworkSystem* network,
                             const Engine::Platform::Render::ICameraSource* camera,
                             const glm::mat4& projection,
                             int screenWidth,
                             int screenHeight,
                             bool gameInputBlocked,
                             bool contextMenuOpen) {
    float mouseX = 0.0f;
    float mouseY = 0.0f;
    GetMousePosition(window, mouseX, mouseY);

    const bool lmbDown = IsMouseDown(window, GLFW_MOUSE_BUTTON_LEFT);

    if (HasTarget() && network) {
        std::vector<Engine::EntityID> remoteEntities;
        network->CollectRemotePlayerEntities(remoteEntities);
        const bool stillRemote = std::find(remoteEntities.begin(),
                                           remoteEntities.end(),
                                           m_target.entity) != remoteEntities.end();
        if (!stillRemote) {
            ClearTarget();
        }
    }

    if (gameInputBlocked || contextMenuOpen || !network || !camera) {
        m_lmbWasDown = lmbDown;
        return;
    }

    if (lmbDown && !m_lmbWasDown) {
        m_lmbDownX = mouseX;
        m_lmbDownY = mouseY;
    }

    if (!lmbDown && m_lmbWasDown) {
        const float dx = mouseX - m_lmbDownX;
        const float dy = mouseY - m_lmbDownY;
        const float travel = std::sqrt(dx * dx + dy * dy);
        if (travel <= kClickMoveThresholdPixels) {
            PlayerPickResult picked{};
            if (TryPickRemotePlayerAtScreen(registry,
                                            *network,
                                            *camera,
                                            projection,
                                            screenWidth,
                                            screenHeight,
                                            mouseX,
                                            mouseY,
                                            picked)) {
                m_target = picked;
            } else {
                ClearTarget();
            }
        }
    }

    m_lmbWasDown = lmbDown;
}

} // namespace UI
} // namespace Game
