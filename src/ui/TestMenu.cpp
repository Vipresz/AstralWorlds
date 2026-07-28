#include "ui/TestMenu.h"
#include "Logger.h"

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#include "nuklear.h"

namespace Game {
namespace UI {

void TestMenu::UpdateInput(bool bKeyDown) {
    const bool bJustPressed = bKeyDown && !m_bPressedLastFrame;
    m_bPressedLastFrame = bKeyDown;
    if (bJustPressed) {
        m_open = !m_open;
        LOG_INFO_FMT("TestMenu", "Menu %s", m_open ? "opened" : "closed");
    }
}

void TestMenu::Build(struct nk_context* ctx, const Engine::Platform::UI::MenuStyle& style,
                     Engine::Platform::UI::UISystem& uiSystem, int screenWidth, int screenHeight) {
    if (!ctx || !m_open) {
        return;
    }

    (void)screenWidth;
    (void)screenHeight;

    const float panelW = style.metrics.menuButtonWidth + style.metrics.windowPaddingX * 2.0f;
    const float panelH = style.metrics.menuButtonHeight * 3.0f + style.metrics.itemSpacing * 2.0f +
                         style.metrics.titleHeight + 28.0f;

    // Compact floating box (top-left), not full-screen
    const float x = 24.0f;
    const float y = 24.0f;

    if (nk_begin(ctx, "TestMenu", nk_rect(x, y, panelW, panelH),
                 NK_WINDOW_BORDER | NK_WINDOW_TITLE | NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_MOVABLE)) {
        nk_layout_row_dynamic(ctx, style.metrics.menuButtonHeight, 1);

        if (uiSystem.MenuButton(ctx, "Button One")) {
            LOG_INFO("TestMenu", "Button One clicked");
        }
        nk_layout_row_dynamic(ctx, style.metrics.itemSpacing, 1);
        nk_spacing(ctx, 1);

        if (uiSystem.MenuButton(ctx, "Button Two")) {
            LOG_INFO("TestMenu", "Button Two clicked");
        }
        nk_layout_row_dynamic(ctx, style.metrics.itemSpacing, 1);
        nk_spacing(ctx, 1);

        if (uiSystem.MenuButton(ctx, "Quit Game")) {
            m_quitRequested = true;
        }
    }
    nk_end(ctx);
}

} // namespace UI
} // namespace Game
