#pragma once

#include "ui/FontLibrary.h"

struct nk_context;

namespace Game {
namespace UI {

struct SpeechBubbleStyle {
    Engine::Platform::UI::FontRole fontRole = Engine::Platform::UI::FontRole::Chat;
    float maxWidth = 220.0f;
    float padding = 8.0f;
    float lineSpacing = 2.0f;
    float cornerRadius = 6.0f;
};

struct SpeechBubbleLayout {
    float width = 0.0f;
    float height = 0.0f;
};

class SpeechBubbleText {
public:
    static SpeechBubbleLayout Measure(struct nk_context* ctx,
                                      const Engine::Platform::UI::FontLibrary& fonts,
                                      const char* text,
                                      const SpeechBubbleStyle& style = SpeechBubbleStyle{});

    static void DrawAbove(struct nk_context* ctx,
                          const Engine::Platform::UI::FontLibrary& fonts,
                          const char* text,
                          float screenX,
                          float anchorScreenY,
                          const SpeechBubbleStyle& style = SpeechBubbleStyle{});
};

} // namespace UI
} // namespace Game
