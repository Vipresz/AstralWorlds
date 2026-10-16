#include "ui/CooldownOverlay.h"

#ifdef PLATFORM_HAS_NUKLEAR

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#include "nuklear.h"

namespace Game {
namespace UI {

namespace {

constexpr float kMinDisplaySeconds = 0.1f;

float QuantizeCooldownDisplaySeconds(float remainingSeconds) {
    if (remainingSeconds <= 0.0f) {
        return 0.0f;
    }
    const int tenths = static_cast<int>(std::ceil(remainingSeconds * 10.0f));
    return static_cast<float>(tenths) / 10.0f;
}

} // namespace

std::string FormatCooldownCountdown(float remainingSeconds) {
    const float display = QuantizeCooldownDisplaySeconds(remainingSeconds);
    if (display <= 0.0f) {
        return {};
    }

    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "%.1f", display);
    return buffer;
}

void DrawCooldownOverlay(struct nk_command_buffer* canvas,
                         const struct nk_rect& bounds,
                         float remainingSeconds,
                         float totalSeconds,
                         const struct nk_user_font* font) {
    if (!canvas || remainingSeconds <= 0.0f) {
        return;
    }

  (void)totalSeconds;

    const float overlayAlpha =
        std::clamp(0.45f + 0.25f * std::min(1.0f, remainingSeconds), 0.45f, 0.7f);
    const unsigned char alpha = static_cast<unsigned char>(overlayAlpha * 255.0f);
    nk_fill_rect(canvas, bounds, 4.0f, nk_rgba(0, 0, 0, alpha));

    if (!font) {
        return;
    }

    const std::string label = FormatCooldownCountdown(remainingSeconds);
    if (label.empty()) {
        return;
    }

    const int labelLen = static_cast<int>(label.size());
    const float textW = font->width(font->userdata, font->height, label.c_str(), labelLen);
    const float textH = font->height;
    const float textX = bounds.x + (bounds.w - textW) * 0.5f;
    const float textY = bounds.y + (bounds.h - textH) * 0.5f;
  nk_draw_text(canvas,
               nk_rect(textX, textY, textW + 2.0f, textH),
               label.c_str(),
               labelLen,
               font,
               nk_rgba(0, 0, 0, 200),
               nk_rgba(255, 255, 255, 255));
}

} // namespace UI
} // namespace Game

#endif
