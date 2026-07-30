#include "ui/PlayerContextMenu.h"

#include "ui/OverlayWindow.h"
#include "ui/PlayerTargeting.h"
#include "network/systems/NetworkSystem.h"
#include "render/Window.h"

#include <GLFW/glfw3.h>
#include <nuklear.h>

#include <cmath>
#include <cstring>
#include <functional>
#include <memory>

namespace Game {
namespace UI {

namespace {

constexpr float kClickMoveThresholdPixels = 8.0f;
constexpr float kMenuWidth = 152.0f;
constexpr float kMenuPadding = 8.0f;
constexpr float kButtonHeight = 32.0f;
constexpr float kButtonSpacing = 4.0f;
constexpr int kButtonCount = 3;

bool IsMouseDown(Engine::Platform::Window& window, int button) {
    if (window.isMouseButtonDown(button)) {
        return true;
    }
    GLFWwindow* glfwWindow = window.GetGLFWWindow();
    return glfwWindow && glfwGetMouseButton(glfwWindow, button) == GLFW_PRESS;
}

bool PointInRect(float x, float y, const struct nk_rect& rect) {
    return x >= rect.x && y >= rect.y && x <= rect.x + rect.w && y <= rect.y + rect.h;
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

struct nk_rect PlayerContextMenu::ComputeMenuBounds(int screenWidth, int screenHeight) const {
    const float menuH =
        kMenuPadding * 2.0f + static_cast<float>(kButtonCount) * kButtonHeight +
        static_cast<float>(kButtonCount - 1) * kButtonSpacing;

    float x = m_anchorX;
    float y = m_anchorY;
    if (x + kMenuWidth > static_cast<float>(screenWidth)) {
        x = static_cast<float>(screenWidth) - kMenuWidth - 4.0f;
    }
    if (y + menuH > static_cast<float>(screenHeight)) {
        y = static_cast<float>(screenHeight) - menuH - 4.0f;
    }
    if (x < 4.0f) {
        x = 4.0f;
    }
    if (y < 4.0f) {
        y = 4.0f;
    }

    return nk_rect(x, y, kMenuWidth, menuH);
}

struct nk_rect PlayerContextMenu::ButtonBounds(int buttonIndex, const struct nk_rect& menuBounds) const {
    const float y =
        menuBounds.y + kMenuPadding + static_cast<float>(buttonIndex) * (kButtonHeight + kButtonSpacing);
    return nk_rect(menuBounds.x + kMenuPadding,
                   y,
                   menuBounds.w - kMenuPadding * 2.0f,
                   kButtonHeight);
}

void PlayerContextMenu::Close() {
    m_open = false;
}

void PlayerContextMenu::Update(Engine::Platform::Window& window,
                               Engine::ECS::Registry& registry,
                               Engine::Platform::Networking::Systems::NetworkSystem* network,
                               const Engine::Platform::Render::ICameraSource* camera,
                               const glm::mat4& projection,
                               int screenWidth,
                               int screenHeight,
                               bool gameInputBlocked) {
    float mouseX = 0.0f;
    float mouseY = 0.0f;
    GetMousePosition(window, mouseX, mouseY);

    const bool rmbDown = IsMouseDown(window, GLFW_MOUSE_BUTTON_RIGHT);
    const bool lmbDown = IsMouseDown(window, GLFW_MOUSE_BUTTON_LEFT);

    if (m_open) {
        const struct nk_rect menuBounds = ComputeMenuBounds(screenWidth, screenHeight);
        m_menuX = menuBounds.x;
        m_menuY = menuBounds.y;
        m_menuW = menuBounds.w;
        m_menuH = menuBounds.h;

        if (!lmbDown && m_lmbWasDown && !gameInputBlocked) {
            static constexpr PlayerContextAction kActions[] = {
                PlayerContextAction::Inspect,
                PlayerContextAction::Follow,
                PlayerContextAction::Trade,
            };
            for (int buttonIndex = 0; buttonIndex < kButtonCount; ++buttonIndex) {
                if (!PointInRect(mouseX, mouseY, ButtonBounds(buttonIndex, menuBounds))) {
                    continue;
                }
                if (m_actionCallback) {
                    m_actionCallback(kActions[buttonIndex], m_target);
                }
                Close();
                break;
            }
        }

        if (!rmbDown && !lmbDown && !gameInputBlocked) {
            if (!PointInRect(mouseX, mouseY, menuBounds)) {
                Close();
            }
        }
    }

    if (!m_open && (gameInputBlocked || !network || !camera)) {
        m_rmbWasDown = rmbDown;
        m_lmbWasDown = lmbDown;
        return;
    }

    if (rmbDown && !m_rmbWasDown) {
        m_rmbDownX = mouseX;
        m_rmbDownY = mouseY;
    }

    if (!rmbDown && m_rmbWasDown && !m_open) {
        const float dx = mouseX - m_rmbDownX;
        const float dy = mouseY - m_rmbDownY;
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
                m_anchorX = mouseX;
                m_anchorY = mouseY;
                m_open = true;
            }
        }
    }

    m_rmbWasDown = rmbDown;
    m_lmbWasDown = lmbDown;
}

void PlayerContextMenu::Build(struct nk_context* ctx, int screenWidth, int screenHeight) {
    if (!ctx || !m_open) {
        return;
    }

    const struct nk_rect menuBounds = ComputeMenuBounds(screenWidth, screenHeight);
    m_menuX = menuBounds.x;
    m_menuY = menuBounds.y;
    m_menuW = menuBounds.w;
    m_menuH = menuBounds.h;

    Engine::Platform::UI::OverlayScope overlay(ctx, "PlayerContextMenu", menuBounds);
    if (!overlay.IsOpen()) {
        return;
    }

    struct nk_command_buffer* canvas = overlay.Canvas();
    if (!canvas) {
        return;
    }

    const struct nk_user_font* font = ctx->style.font;
    const struct nk_color panelFill = nk_rgba(24, 28, 38, 236);
    const struct nk_color panelBorder = nk_rgba(96, 110, 140, 220);
    const struct nk_color buttonFill = nk_rgba(42, 50, 68, 240);
    const struct nk_color buttonHover = nk_rgba(56, 66, 88, 245);
    const struct nk_color buttonText = nk_rgba(232, 238, 248, 255);
    const struct nk_color transparent = nk_rgba(0, 0, 0, 0);

    nk_fill_rect(canvas, menuBounds, 6.0f, panelFill);
    nk_stroke_rect(canvas, menuBounds, 6.0f, 1.0f, panelBorder);

    float mouseX = 0.0f;
    float mouseY = 0.0f;
    if (GLFWwindow* glfwWindow = glfwGetCurrentContext()) {
        double cursorX = 0.0;
        double cursorY = 0.0;
        glfwGetCursorPos(glfwWindow, &cursorX, &cursorY);
        mouseX = static_cast<float>(cursorX);
        mouseY = static_cast<float>(cursorY);
    }

    static constexpr const char* kLabels[] = {"Inspect", "Follow", "Trade"};
    for (int buttonIndex = 0; buttonIndex < kButtonCount; ++buttonIndex) {
        const struct nk_rect buttonBounds = ButtonBounds(buttonIndex, menuBounds);
        const bool hovered = PointInRect(mouseX, mouseY, buttonBounds);
        nk_fill_rect(canvas, buttonBounds, 4.0f, hovered ? buttonHover : buttonFill);
        nk_stroke_rect(canvas, buttonBounds, 4.0f, 1.0f, panelBorder);

        if (font) {
            const char* label = kLabels[buttonIndex];
            const int labelLen = static_cast<int>(std::strlen(label));
            const float textW = font->width(font->userdata, font->height, label, labelLen);
            nk_draw_text(canvas,
                         nk_rect(buttonBounds.x + (buttonBounds.w - textW) * 0.5f,
                                 buttonBounds.y + (buttonBounds.h - font->height) * 0.5f,
                                 textW,
                                 font->height),
                         label,
                         labelLen,
                         font,
                         transparent,
                         buttonText);
        }
    }
}

} // namespace UI
} // namespace Game

std::unique_ptr<Game::UI::PlayerContextMenu> Game::UI::CreatePlayerContextMenu() {
    return std::make_unique<Game::UI::PlayerContextMenu>();
}
