#include "ui/SpeechBubbleText.h"

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_FONT_BAKING
#include "nuklear.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace Game {
namespace UI {

namespace {

struct nk_font* ResolveFont(const Engine::Platform::UI::FontLibrary& fonts,
                            Engine::Platform::UI::FontRole role) {
    struct nk_font* font = fonts.GetFont(role);
    if (!font) {
        font = fonts.GetFont(Engine::Platform::UI::FontRole::Hud);
    }
    if (!font) {
        font = fonts.GetFont(Engine::Platform::UI::FontRole::UiBold);
    }
    return font;
}

float MeasureTextWidth(const struct nk_user_font* userFont, const char* text, int length) {
    if (!userFont || !text || length <= 0) {
        return 0.0f;
    }
    return userFont->width(userFont->userdata, userFont->height, text, length);
}

std::vector<std::string> WrapText(const struct nk_user_font* userFont,
                                    const char* text,
                                    float maxTextWidth) {
    std::vector<std::string> lines;
    if (!text || text[0] == '\0' || !userFont || maxTextWidth <= 1.0f) {
        return lines;
    }

    std::string word;
    std::string currentLine;
    const auto flushWord = [&]() {
        if (word.empty()) {
            return;
        }
        const std::string candidate =
            currentLine.empty() ? word : (currentLine + " " + word);
        if (MeasureTextWidth(userFont, candidate.c_str(), static_cast<int>(candidate.size())) <=
            maxTextWidth) {
            currentLine = candidate;
        } else if (currentLine.empty()) {
            lines.push_back(word);
            currentLine.clear();
        } else {
            lines.push_back(currentLine);
            currentLine = word;
        }
        word.clear();
    };
    const auto flushLine = [&]() {
        flushWord();
        if (!currentLine.empty()) {
            lines.push_back(currentLine);
            currentLine.clear();
        }
    };

    for (const char* cursor = text; *cursor != '\0'; ++cursor) {
        const char ch = *cursor;
        if (ch == '\n' || ch == '\r') {
            flushLine();
            continue;
        }
        if (ch == ' ' || ch == '\t') {
            flushWord();
            continue;
        }
        word.push_back(ch);
    }
    flushLine();
    if (lines.empty()) {
        lines.emplace_back(text);
    }
    return lines;
}

SpeechBubbleLayout BuildLayout(const struct nk_user_font* userFont,
                               const std::vector<std::string>& lines,
                               const SpeechBubbleStyle& style) {
    SpeechBubbleLayout layout;
    if (!userFont || lines.empty()) {
        return layout;
    }

    const float lineHeight = userFont->height + style.lineSpacing;
    float maxLineWidth = 0.0f;
    for (const std::string& line : lines) {
        maxLineWidth =
            std::max(maxLineWidth,
                     MeasureTextWidth(userFont, line.c_str(), static_cast<int>(line.size())));
    }

    layout.width = std::min(style.maxWidth, maxLineWidth + style.padding * 2.0f);
    layout.height = style.padding * 2.0f + lineHeight * static_cast<float>(lines.size()) -
                    style.lineSpacing;
    return layout;
}

} // namespace

SpeechBubbleLayout SpeechBubbleText::Measure(struct nk_context* ctx,
                                             const Engine::Platform::UI::FontLibrary& fonts,
                                             const char* text,
                                             const SpeechBubbleStyle& style) {
    (void)ctx;
    struct nk_font* font = ResolveFont(fonts, style.fontRole);
    if (!font || !text || text[0] == '\0') {
        return {};
    }

    const float maxTextWidth = std::max(32.0f, style.maxWidth - style.padding * 2.0f);
    const std::vector<std::string> lines =
        WrapText(&font->handle, text, maxTextWidth);
    return BuildLayout(&font->handle, lines, style);
}

void SpeechBubbleText::DrawAbove(struct nk_context* ctx,
                                 const Engine::Platform::UI::FontLibrary& fonts,
                                 const char* text,
                                 float screenX,
                                 float anchorScreenY,
                                 const SpeechBubbleStyle& style) {
    if (!ctx || !text || text[0] == '\0') {
        return;
    }

    struct nk_font* font = ResolveFont(fonts, style.fontRole);
    if (!font) {
        return;
    }

    struct nk_command_buffer* canvas = nk_window_get_canvas(ctx);
    if (!canvas) {
        return;
    }

    const struct nk_user_font* userFont = &font->handle;
    const float maxTextWidth = std::max(32.0f, style.maxWidth - style.padding * 2.0f);
    const std::vector<std::string> lines = WrapText(userFont, text, maxTextWidth);
    if (lines.empty()) {
        return;
    }

    const SpeechBubbleLayout layout = BuildLayout(userFont, lines, style);
    if (layout.width <= 0.0f || layout.height <= 0.0f) {
        return;
    }

    const float boxX = std::floor(screenX - layout.width * 0.5f + 0.5f);
    const float boxY = std::floor(anchorScreenY - layout.height + 0.5f);
    const struct nk_rect box = nk_rect(boxX, boxY, layout.width, layout.height);

    nk_fill_rect(canvas, box, style.cornerRadius, nk_rgba(16, 18, 28, 220));
    nk_stroke_rect(canvas, box, style.cornerRadius, 1.0f, nk_rgba(120, 135, 170, 220));

    const struct nk_color transparent = nk_rgba(0, 0, 0, 0);
    const struct nk_color textColor = nk_rgba(235, 240, 250, 255);
    const float lineHeight = userFont->height + style.lineSpacing;
    float textY = boxY + style.padding;
    for (const std::string& line : lines) {
        const int lineLen = static_cast<int>(line.size());
        const float lineWidth = MeasureTextWidth(userFont, line.c_str(), lineLen);
        const float textX = std::floor(boxX + (layout.width - lineWidth) * 0.5f + 0.5f);
        const struct nk_rect textRect = nk_rect(textX, textY, lineWidth, userFont->height);
        nk_draw_text(canvas, textRect, line.c_str(), lineLen, userFont, transparent, textColor);
        textY += lineHeight;
    }

    const float pointerHalfWidth = 7.0f;
    const float pointerHeight = 6.0f;
    const struct nk_vec2 pointer[3] = {
        nk_vec2(screenX - pointerHalfWidth, boxY + layout.height),
        nk_vec2(screenX + pointerHalfWidth, boxY + layout.height),
        nk_vec2(screenX, boxY + layout.height + pointerHeight),
    };
    nk_fill_triangle(canvas, pointer[0].x, pointer[0].y, pointer[1].x, pointer[1].y, pointer[2].x,
                     pointer[2].y, nk_rgba(16, 18, 28, 220));
    nk_stroke_line(canvas, pointer[0].x, pointer[0].y, pointer[1].x, pointer[1].y, 1.0f,
                   nk_rgba(120, 135, 170, 220));
    nk_stroke_line(canvas, pointer[1].x, pointer[1].y, pointer[2].x, pointer[2].y, 1.0f,
                   nk_rgba(120, 135, 170, 220));
    nk_stroke_line(canvas, pointer[2].x, pointer[2].y, pointer[0].x, pointer[0].y, 1.0f,
                   nk_rgba(120, 135, 170, 220));
}

} // namespace UI
} // namespace Game
