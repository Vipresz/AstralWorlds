#include "GameCameraInput.h"
#include "core/ecs/Registry.h"
#include "render/Window.h"
#include "render/components/TransformComponent.h"
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <cmath>

namespace Game {

namespace {

bool IsMouseDown(Engine::Platform::Window* window, int button) {
    if (!window) {
        return false;
    }
    if (window->isMouseButtonDown(button)) {
        return true;
    }
    GLFWwindow* glfwWindow = window->GetGLFWWindow();
    return glfwWindow && glfwGetMouseButton(glfwWindow, button) == GLFW_PRESS;
}

float NormalizeDegrees(float angle) {
    while (angle > 180.0f) angle -= 360.0f;
    while (angle < -180.0f) angle += 360.0f;
    return angle;
}

} // namespace

GameCameraInput::GameCameraInput(Engine::Platform::Window* window,
                                 Engine::Platform::Render::ICameraSource* camera,
                                 Engine::EntityID characterEntity)
    : m_window(window), m_camera(camera), m_characterEntity(characterEntity) {
    // After InputSystem (0) and CameraSystem (10) so camera orbit + follow are current
    SetPriority(15);
}

void GameCameraInput::Initialize(Engine::ECS::Registry& /*registry*/) {}

void GameCameraInput::Update(float deltaTime, Engine::ECS::Registry& registry, uint64_t /*tickNumber*/) {
    if (!m_window || IsBlocked()) {
        if (m_window) {
            m_window->SetCursorCaptured(false);
        }
        return;
    }

    const bool lmb = IsMouseDown(m_window, GLFW_MOUSE_BUTTON_LEFT);
    const bool rmb = IsMouseDown(m_window, GLFW_MOUSE_BUTTON_RIGHT);

    // LMB: orbit camera only. RMB: orbit camera and turn character with it.
    const bool orbitCamera = lmb || rmb;
    m_window->SetCursorCaptured(orbitCamera);

    if (m_camera && orbitCamera) {
        const float dx = m_window->getXChange();
        const float dy = m_window->getYChange();
        if (dx != 0.0f || dy != 0.0f) {
            m_camera->OnMouseMove(dx, dy);
        }
    } else {
        (void)m_window->getXChange();
        (void)m_window->getYChange();
    }

    if (m_camera) {
        const float scroll = m_window->getScrollYChange();
        if (scroll != 0.0f) {
            m_camera->OnScroll(scroll);
        }
    }

    if (rmb) {
        OrientCharacterToCamera(deltaTime, registry);
    }
}

void GameCameraInput::Shutdown(Engine::ECS::Registry& /*registry*/) {
    if (m_window) {
        m_window->SetCursorCaptured(false);
    }
}

void GameCameraInput::OrientCharacterToCamera(float deltaTime, Engine::ECS::Registry& registry) {
    if (!m_camera || m_characterEntity == 0) {
        return;
    }

    auto* transform = registry.GetComponent<Engine::Platform::Render::Components::TransformComponent>(m_characterEntity);
    if (!transform) {
        return;
    }

    const float desiredYaw = NormalizeDegrees(m_camera->GetPlanarFacingYawDegrees());
    float currentYaw = NormalizeDegrees(transform->GetYaw());

    float angleDiff = NormalizeDegrees(desiredYaw - currentYaw);

    const float maxRotation = m_characterTurnSpeed * deltaTime;
    if (std::abs(angleDiff) > 0.5f) {
        const float rotation = std::copysign(std::min(std::abs(angleDiff), maxRotation), angleDiff);
        transform->SetYaw(NormalizeDegrees(currentYaw + rotation));
    } else {
        transform->SetYaw(desiredYaw);
    }
}

} // namespace Game
