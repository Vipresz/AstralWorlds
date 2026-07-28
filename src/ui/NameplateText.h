#pragma once

#include "ui/FontLibrary.h"

struct nk_context;
struct nk_user_font;

namespace Game {
namespace UI {

struct NameplateStyle {
    Engine::Platform::UI::FontRole fontRole = Engine::Platform::UI::FontRole::Hud;
    float shadowOffset = 1.5f;
};

// Draw centered screen-space text (for character nameplates).
class NameplateText {
public:
    static void DrawCentered(struct nk_context* ctx,
                             const Engine::Platform::UI::FontLibrary& fonts,
                             const char* text,
                             float screenX,
                             float screenY,
                             const NameplateStyle& style = NameplateStyle{});
};

} // namespace UI
} // namespace Game
