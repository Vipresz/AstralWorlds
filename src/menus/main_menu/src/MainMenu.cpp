#include "MainMenu.h"

#include "Logger.h"

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#include "nuklear.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>

using EngineLogging::Logger;

namespace Game {
namespace UI {

namespace {

struct nk_image ToNkImage(const Engine::Platform::Render::Texture& texture) {
    return nk_image_id(static_cast<int>(texture.GetID()));
}

std::string ReadTextFile(const char* path) {
    std::ifstream file(path);
    if (!file) {
        return {};
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

const char* SkipJsonWhitespace(const char* cursor) {
    while (cursor && *cursor && std::isspace(static_cast<unsigned char>(*cursor))) {
        ++cursor;
    }
    return cursor;
}

bool JsonTryGetFloat(const std::string& json, const char* key, float& out) {
    const std::string needle = std::string("\"") + key + "\"";
    const size_t keyPos = json.find(needle);
    if (keyPos == std::string::npos) {
        return false;
    }

    const size_t colonPos = json.find(':', keyPos + needle.size());
    if (colonPos == std::string::npos) {
        return false;
    }

    const char* start = SkipJsonWhitespace(json.c_str() + colonPos + 1);
    char* end = nullptr;
    const float value = std::strtof(start, &end);
    if (end == start) {
        return false;
    }

    out = value;
    return true;
}

bool JsonTryGetInt(const std::string& json, const char* key, int& out) {
    float value = 0.0f;
    if (!JsonTryGetFloat(json, key, value)) {
        return false;
    }
    out = static_cast<int>(value);
    return true;
}

bool JsonTryGetString(const std::string& json, const char* key, std::string& out) {
    const std::string needle = std::string("\"") + key + "\"";
    const size_t keyPos = json.find(needle);
    if (keyPos == std::string::npos) {
        return false;
    }

    const size_t colonPos = json.find(':', keyPos + needle.size());
    if (colonPos == std::string::npos) {
        return false;
    }

    const size_t quoteStart = json.find('"', colonPos + 1);
    if (quoteStart == std::string::npos) {
        return false;
    }

    const size_t quoteEnd = json.find('"', quoteStart + 1);
    if (quoteEnd == std::string::npos || quoteEnd <= quoteStart + 1) {
        return false;
    }

    out.assign(json, quoteStart + 1, quoteEnd - quoteStart - 1);
    return true;
}

std::string JsonExtractObject(const std::string& json, const char* objectKey) {
    const std::string needle = std::string("\"") + objectKey + "\"";
    const size_t keyPos = json.find(needle);
    if (keyPos == std::string::npos) {
        return {};
    }

    const size_t braceStart = json.find('{', keyPos);
    if (braceStart == std::string::npos) {
        return {};
    }

    int depth = 0;
    for (size_t i = braceStart; i < json.size(); ++i) {
        if (json[i] == '{') {
            ++depth;
        } else if (json[i] == '}') {
            --depth;
            if (depth == 0) {
                return json.substr(braceStart, i - braceStart + 1);
            }
        }
    }

    return {};
}

bool JsonTryGetStringInObject(const std::string& json,
                              const char* objectKey,
                              const char* fieldKey,
                              std::string& out) {
    const std::string objectJson = JsonExtractObject(json, objectKey);
    if (objectJson.empty()) {
        return false;
    }
    return JsonTryGetString(objectJson, fieldKey, out);
}

std::string JoinPath(const char* root, const std::string& relative) {
    if (relative.empty()) {
        return root ? root : "";
    }
    if (!root || root[0] == '\0') {
        return relative;
    }

    std::string joined = root;
    if (joined.back() != '/' && joined.back() != '\\') {
        joined.push_back('/');
    }
    if (!relative.empty() && (relative.front() == '/' || relative.front() == '\\')) {
        joined += relative.substr(1);
    } else {
        joined += relative;
    }
    return joined;
}

void DrawFullscreenImage(struct nk_command_buffer* canvas,
                         const Engine::Platform::Render::Texture& texture,
                         float screenWidth,
                         float screenHeight) {
    if (!canvas || screenWidth <= 0.0f || screenHeight <= 0.0f) {
        return;
    }

    const struct nk_rect bounds = nk_rect(0.0f, 0.0f, screenWidth, screenHeight);
    const struct nk_image image = ToNkImage(texture);
    nk_draw_image(canvas, bounds, &image, nk_rgba(255, 255, 255, 255));
}

void DrawPlayLabel(struct nk_command_buffer* canvas,
                   const struct nk_rect& buttonBounds,
                   const struct nk_user_font* font,
                   const std::string& text,
                   bool pressed,
                   float uiScale) {
    if (!canvas || !font || text.empty()) {
        return;
    }

    const int textLen = static_cast<int>(text.size());
    const float textW = font->width(font->userdata, font->height, text.c_str(), textLen);
    const float textH = font->height;
    const float textX = buttonBounds.x + (buttonBounds.w - textW) * 0.5f;
    const float textY = buttonBounds.y + (buttonBounds.h - textH) * 0.5f;
    const struct nk_rect textRect = nk_rect(textX, textY, textW, textH);

    const struct nk_color transparent = nk_rgba(0, 0, 0, 0);
    const struct nk_color white = nk_rgba(255, 255, 255, 255);
    const struct nk_color grey = nk_rgba(150, 150, 150, 255);
    const struct nk_color shadow = nk_rgba(0, 0, 0, 200);
    const float shadowStep = 2.0f * uiScale;

    if (pressed) {
        const float offsets[][2] = {
            {-shadowStep, 0.0f}, {shadowStep, 0.0f}, {0.0f, -shadowStep}, {0.0f, shadowStep},
            {-shadowStep, -shadowStep}, {shadowStep, -shadowStep}, {-shadowStep, shadowStep}, {shadowStep, shadowStep},
        };
        for (const auto& offset : offsets) {
            struct nk_rect shadowRect = textRect;
            shadowRect.x += offset[0];
            shadowRect.y += offset[1];
            nk_draw_text(canvas, shadowRect, text.c_str(), textLen, font, transparent, shadow);
        }
        nk_draw_text(canvas, textRect, text.c_str(), textLen, font, transparent, grey);
    } else {
        nk_draw_text(canvas, textRect, text.c_str(), textLen, font, transparent, white);
    }
}

} // namespace

bool MainMenu::LoadConfig(MainMenuConfig& out, const char* manifestPath) {
    const char* path = manifestPath ? manifestPath : MainMenuConfig::kManifestPath;
    const std::string json = ReadTextFile(path);
    if (json.empty()) {
        LOG_WARNING_FMT("MainMenu", "Manifest not found at %s — using defaults", path);
        return false;
    }

    std::string backgroundRelative;
    if (JsonTryGetStringInObject(json, "background", "texture", backgroundRelative)) {
        out.backgroundTexturePath = JoinPath(MainMenuConfig::kRoot, backgroundRelative);
    }

    std::string buttonRelative;
    if (JsonTryGetStringInObject(json, "play_button", "texture", buttonRelative)) {
        out.buttonTexturePath = JoinPath(MainMenuConfig::kRoot, buttonRelative);
    }

    const std::string playButtonJson = JsonExtractObject(json, "play_button");
    if (!playButtonJson.empty()) {
        JsonTryGetString(playButtonJson, "label", out.labelText);
        JsonTryGetFloat(playButtonJson, "text_size", out.textSize);

        const std::string layoutJson = JsonExtractObject(playButtonJson, "layout");
        if (!layoutJson.empty()) {
            JsonTryGetFloat(layoutJson, "max_width_fraction", out.maxWidthFraction);
            JsonTryGetFloat(layoutJson, "y_fraction", out.yFraction);
            JsonTryGetFloat(layoutJson, "fallback_width", out.fallbackButtonWidth);
            JsonTryGetFloat(layoutJson, "fallback_height", out.fallbackButtonHeight);
        }
    }

    const std::string screenJson = JsonExtractObject(json, "screen");
    if (!screenJson.empty()) {
        JsonTryGetInt(screenJson, "min_width", out.minScreenWidth);
        JsonTryGetInt(screenJson, "min_height", out.minScreenHeight);
    }

    LOG_INFO_FMT("MainMenu",
                 "Manifest loaded: background=%s button=%s label=\"%s\" text_size=%.0f ref=%dx%d",
                 out.backgroundTexturePath.c_str(),
                 out.buttonTexturePath.c_str(),
                 out.labelText.c_str(),
                 out.textSize,
                 out.minScreenWidth,
                 out.minScreenHeight);
    return true;
}

bool MainMenu::Initialize(const MainMenuConfig* config) {
    m_config = config ? *config : MainMenuConfig{};
    if (!config) {
        LoadConfig(m_config);
    }

    m_backgroundTexture = std::make_unique<Engine::Platform::Render::Texture>(m_config.backgroundTexturePath.c_str());
    if (!m_backgroundTexture->LoadTextureUI(1920)) {
        LOG_WARNING_FMT("MainMenu", "Background not loaded (%s) — using solid black", m_config.backgroundTexturePath.c_str());
        m_backgroundTexture.reset();
    }

    m_buttonTexture = std::make_unique<Engine::Platform::Render::Texture>(m_config.buttonTexturePath.c_str());

    if (!m_buttonTexture->LoadTextureUI(512)) {
        LOG_ERROR_FMT("MainMenu", "Failed to load %s", m_config.buttonTexturePath.c_str());
        m_buttonTexture.reset();
        return false;
    }

    LOG_INFO("MainMenu", "Main menu button loaded");
    return true;
}

void MainMenu::Shutdown() {
    if (m_backgroundTexture) {
        m_backgroundTexture->ClearTexture();
        m_backgroundTexture.reset();
    }
    if (m_buttonTexture) {
        m_buttonTexture->ClearTexture();
        m_buttonTexture.reset();
    }
}

void MainMenu::Build(struct nk_context* ctx,
                     const Engine::Platform::UI::MenuStyle& /*style*/,
                     Engine::Platform::UI::UISystem& uiSystem,
                     int screenWidth,
                     int screenHeight) {
    if (!ctx || m_gameStarted) {
        return;
    }

    if (screenWidth <= 0 || screenHeight <= 0) {
        return;
    }

    const bool haveBackground = m_backgroundTexture && m_backgroundTexture->IsLoaded();
    const bool haveTexture = m_buttonTexture && m_buttonTexture->IsLoaded();
    const MenuScreenLayout layout = MenuScreenLayout::From(screenWidth, screenHeight, m_config.ReferenceScreen());

    if (nk_begin(ctx, "MainMenu",
                 nk_rect(0.0f, 0.0f, layout.screenWidth, layout.screenHeight),
                 NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BACKGROUND)) {
        struct nk_command_buffer* canvas = nk_window_get_canvas(ctx);
        if (haveBackground) {
            DrawFullscreenImage(canvas, *m_backgroundTexture, layout.screenWidth, layout.screenHeight);
        } else {
            nk_fill_rect(canvas, nk_rect(0.0f, 0.0f, layout.screenWidth, layout.screenHeight),
                         0.0f, nk_rgb(0, 0, 0));
        }

        if (!haveTexture) {
            const float buttonW = layout.Scale(m_config.fallbackButtonWidth);
            const float buttonH = layout.Scale(m_config.fallbackButtonHeight);
            const float x = layout.CenteredX(buttonW);
            const float y = layout.YFromFraction(buttonH, m_config.yFraction);
            nk_layout_space_begin(ctx, NK_STATIC, buttonH, screenWidth);
            nk_layout_space_push(ctx, nk_rect(x, y, buttonW, buttonH));
            if (nk_button_label(ctx, m_config.labelText.c_str())) {
                m_gameStarted = true;
                LOG_INFO("MainMenu", "PLAY clicked — starting game");
            }
            nk_layout_space_end(ctx);
            nk_end(ctx);
            return;
        }

        const int texW = m_buttonTexture->GetWidth();
        const int texH = m_buttonTexture->GetHeight();
        if (texW <= 0 || texH <= 0) {
            nk_end(ctx);
            return;
        }

        const float designMaxWidth =
            static_cast<float>(m_config.minScreenWidth) * m_config.maxWidthFraction;
        const float maxWidth = layout.Scale(designMaxWidth);
        const float scale = maxWidth / static_cast<float>(texW);
        const float drawW = static_cast<float>(texW) * scale;
        const float drawH = static_cast<float>(texH) * scale;
        const float x = layout.CenteredX(drawW);
        const float y = layout.YFromFraction(drawH, m_config.yFraction);
        const struct nk_rect buttonBounds = nk_rect(x, y, drawW, drawH);

        const bool hovered = nk_input_is_mouse_hovering_rect(&ctx->input, buttonBounds);
        const bool pressed = hovered && nk_input_is_mouse_down(&ctx->input, NK_BUTTON_LEFT);
        const bool activated = nk_input_is_mouse_click_in_rect(&ctx->input, NK_BUTTON_LEFT, buttonBounds);

        const struct nk_image buttonImage = ToNkImage(*m_buttonTexture);
        nk_draw_image(canvas, buttonBounds, &buttonImage, nk_rgba(255, 255, 255, 255));

        struct nk_font* labelFont = uiSystem.GetFontLibrary().GetFont(Engine::Platform::UI::FontRole::Hud);
        if (!labelFont) {
            labelFont = uiSystem.GetFontLibrary().GetFont(Engine::Platform::UI::FontRole::UiBold);
        }
        if (labelFont) {
            DrawPlayLabel(canvas, buttonBounds, &labelFont->handle, m_config.labelText, pressed, layout.uiScale);
        }

        if (activated) {
            m_gameStarted = true;
            LOG_INFO("MainMenu", "PLAY clicked — starting game");
        }
    }
    nk_end(ctx);
}

} // namespace UI
} // namespace Game
