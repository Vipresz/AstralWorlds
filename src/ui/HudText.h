#pragma once

#include "ui/FontLibrary.h"

struct nk_context;
struct nk_font;

namespace Game {
namespace UI {

// Helpers for drawing in-game HUD text with engine fonts via Nuklear.
class HudText {
public:
    static void Label(struct nk_context* ctx,
                      const Engine::Platform::UI::FontLibrary& fonts,
                      Engine::Platform::UI::FontRole role,
                      const char* text,
                      float x,
                      float y);

    static void Mono(struct nk_context* ctx,
                     const Engine::Platform::UI::FontLibrary& fonts,
                     const char* text,
                     float x,
                     float y);
};

} // namespace UI
} // namespace Game
