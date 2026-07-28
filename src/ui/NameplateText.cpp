#include "ui/NameplateText.h"

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#include "nuklear.h"

#include <cmath>

namespace Game {
namespace UI {

void NameplateText::DrawCentered(struct nk_context* ctx,
                                 const Engine::Platform::UI::FontLibrary& fonts,
                                 const char* text,
                                 float screenX,
                                 float screenY,
                                 const NameplateStyle& style) {
    if (!ctx || !text || text[0] == '\0') {
        return;
    }

    struct nk_font* font = fonts.GetFont(style.fontRole);
    if (!font) {
        font = fonts.GetFont(Engine::Platform::UI::FontRole::UiBold);
    }
    if (!font) {
        return;
    }

    const struct nk_user_font* userFont = &font->handle;
    const int textLen = static_cast<int>(nk_strlen(text));
    const float textW = userFont->width(userFont->userdata, userFont->height, text, textLen);
    const float textH = userFont->height;
    // Pixel-snap so the label doesn't shimmer into a double-image while airborne.
    const float textX = std::floor(screenX - textW * 0.5f + 0.5f);
    const float textY = std::floor(screenY - textH + 0.5f);
    const struct nk_rect textRect = nk_rect(textX, textY, textW, textH);

    struct nk_command_buffer* canvas = nk_window_get_canvas(ctx);
    if (!canvas) {
        return;
    }

    const struct nk_color transparent = nk_rgba(0, 0, 0, 0);
    const struct nk_color white = nk_rgba(255, 255, 255, 255);
    const struct nk_color shadow = nk_rgba(0, 0, 0, 200);
    const float step = style.shadowOffset;

    const float offsets[][2] = {
        {-step, 0.0f}, {step, 0.0f}, {0.0f, -step}, {0.0f, step},
        {-step, -step}, {step, -step}, {-step, step}, {step, step},
    };
    for (const auto& offset : offsets) {
        struct nk_rect shadowRect = textRect;
        shadowRect.x += offset[0];
        shadowRect.y += offset[1];
        nk_draw_text(canvas, shadowRect, text, textLen, userFont, transparent, shadow);
    }

    nk_draw_text(canvas, textRect, text, textLen, userFont, transparent, white);
}

} // namespace UI
} // namespace Game
