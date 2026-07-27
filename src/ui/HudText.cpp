#include "ui/HudText.h"

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#include "nuklear.h"

namespace Game {
namespace UI {

void HudText::Label(struct nk_context* ctx,
                    const Engine::Platform::UI::FontLibrary& fonts,
                    Engine::Platform::UI::FontRole role,
                    const char* text,
                    float x,
                    float y) {
    if (!ctx || !text) {
        return;
    }

    struct nk_font* font = fonts.GetFont(role);
    if (font) {
        nk_style_push_font(ctx, &font->handle);
    }

    if (nk_begin(ctx, "HudOverlay", nk_rect(x, y, 800.0f, 40.0f),
                 NK_WINDOW_NO_INPUT | NK_WINDOW_NO_SCROLLBAR)) {
        nk_layout_row_dynamic(ctx, 24.0f, 1);
        nk_label(ctx, text, NK_TEXT_LEFT);
    }
    nk_end(ctx);

    if (font) {
        nk_style_pop_font(ctx);
    }
}

void HudText::Mono(struct nk_context* ctx,
                   const Engine::Platform::UI::FontLibrary& fonts,
                   const char* text,
                   float x,
                   float y) {
    Label(ctx, fonts, Engine::Platform::UI::FontRole::Mono, text, x, y);
}

} // namespace UI
} // namespace Game
