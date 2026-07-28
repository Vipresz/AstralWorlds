#pragma once

#ifdef PLATFORM_HAS_NUKLEAR

#include "ui/MenuStyle.h"
#include "ui/UISystem.h"

struct nk_context;

namespace Game {
namespace UI {

// Simple test menu — toggle with B, three buttons. All game UI logic lives in src/.
class TestMenu {
public:
    // Call each frame after polling input (edge-detects B to open/close)
    void UpdateInput(bool bKeyDown);

    bool IsOpen() const { return m_open; }
    // Floating panel — does not block WASD/camera when open
    bool BlocksGameInput() const { return false; }
    bool IsQuitRequested() const { return m_quitRequested; }

    void Build(struct nk_context* ctx, const Engine::Platform::UI::MenuStyle& style,
               Engine::Platform::UI::UISystem& uiSystem, int screenWidth, int screenHeight);

private:
    bool m_open = false;
    bool m_bPressedLastFrame = false;
    bool m_quitRequested = false;
};

} // namespace UI
} // namespace Game

#endif // PLATFORM_HAS_NUKLEAR
