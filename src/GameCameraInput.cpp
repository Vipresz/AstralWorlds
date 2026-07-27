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

} // namespace

GameCameraInput::GameCameraInput(Engine::Platform::Window* window,
                                 Engine::Platform::Render::ICameraSource* camera,
                                 Engine::EntityID characterEntity)
    : m_window(window), m_camera(camera), m_characterEntity(characterEntity) {
    // Run before InputSystem (0) so RMB/LMB orbit updates camera facing used by movement.
    SetPriority(-5);
}

void GameCameraInput::Initialize(Engine::ECS::Registry& /*registry*/) {}

void GameCameraInput::Update(float deltaTime, Engine::ECS::Registry& registry, uint64_t /*tickNumber*/) {
    (void)deltaTime;
    (void)registry;

    if (!m_window || IsBlocked()) {
        if (m_window) {
            m_window->SetCursorCaptured(false);
        }
        return;
    }

    const bool lmb = IsMouseDown(m_window, GLFW_MOUSE_BUTTON_LEFT);
    const bool rmb = IsMouseDown(m_window, GLFW_MOUSE_BUTTON_RIGHT);

    // LMB: orbit camera only. RMB: orbit camera; character yaw comes from InputSystem
    // via MovementCommand.facingYawDegrees so prediction/server reconcile stay coherent.
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
}

void GameCameraInput::Shutdown(Engine::ECS::Registry& /*registry*/) {
    if (m_window) {
        m_window->SetCursorCaptured(false);
    }
}

} // namespace Game
