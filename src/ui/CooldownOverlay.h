#pragma once

#ifdef PLATFORM_HAS_NUKLEAR

#include <string>

struct nk_command_buffer;
struct nk_rect;
struct nk_user_font;

namespace Game {
namespace UI {

struct CooldownTimerSnapshot {
    float remainingSeconds = 0.0f;
    float totalSeconds = 0.0f;
};

// Formats remaining time to one decimal place (100 ms resolution).
std::string FormatCooldownCountdown(float remainingSeconds);

// Dark translucent overlay plus centered countdown label.
void DrawCooldownOverlay(struct nk_command_buffer* canvas,
                         const struct nk_rect& bounds,
                         float remainingSeconds,
                         float totalSeconds,
                         const struct nk_user_font* font);

} // namespace UI
} // namespace Game

#endif
