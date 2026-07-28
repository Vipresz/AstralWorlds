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
                         float screenHeight,
                         unsigned char alpha = 255) {
    if (!canvas || screenWidth <= 0.0f || screenHeight <= 0.0f || alpha == 0) {
        return;
    }

    const struct nk_rect bounds = nk_rect(0.0f, 0.0f, screenWidth, screenHeight);
    const struct nk_image image = ToNkImage(texture);
    nk_draw_image(canvas, bounds, &image, nk_rgba(255, 255, 255, alpha));
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

    const std::string loadingBarJson = JsonExtractObject(json, "loading_bar");
    if (!loadingBarJson.empty()) {
        std::string emptyRelative;
        std::string fullRelative;
        if (JsonTryGetString(loadingBarJson, "empty", emptyRelative)) {
            out.loadingEmptyTexturePath = JoinPath(MainMenuConfig::kRoot, emptyRelative);
        }
        if (JsonTryGetString(loadingBarJson, "full", fullRelative)) {
            out.loadingFullTexturePath = JoinPath(MainMenuConfig::kRoot, fullRelative);
        }
        JsonTryGetString(loadingBarJson, "label", out.loadingLabelText);

        const std::string loadingLayoutJson = JsonExtractObject(loadingBarJson, "layout");
        if (!loadingLayoutJson.empty()) {
            JsonTryGetFloat(loadingLayoutJson, "width_fraction", out.loadingWidthFraction);
            JsonTryGetFloat(loadingLayoutJson, "y_fraction", out.loadingYFraction);
        }
    }

    const std::string enterWorldJson = JsonExtractObject(json, "enter_world");
    if (!enterWorldJson.empty()) {
        std::string loadingBg;
        std::string readyBg;
        if (JsonTryGetString(enterWorldJson, "loading_background", loadingBg)) {
            out.enterLoadingBackgroundPath = JoinPath(MainMenuConfig::kRoot, loadingBg);
        }
        if (JsonTryGetString(enterWorldJson, "ready_background", readyBg)) {
            out.enterReadyBackgroundPath = JoinPath(MainMenuConfig::kRoot, readyBg);
        }
        JsonTryGetString(enterWorldJson, "label", out.enterLoadingLabelText);
        JsonTryGetFloat(enterWorldJson, "hold_seconds", out.enterHoldSeconds);
        JsonTryGetFloat(enterWorldJson, "fade_seconds", out.enterFadeSeconds);
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

    m_loadProgress = 0.0f;
    m_loadStatus = m_config.loadingLabelText;
    m_phaseTimer = 0.0f;
    m_fadeAlpha = 1.0f;
    m_phase = Phase::BootLoading;

    auto loadUiTexture = [](const std::string& path, int maxSize, const char* label) {
        auto texture = std::make_unique<Engine::Platform::Render::Texture>(path.c_str());
        if (!texture->LoadTextureUI(maxSize)) {
            LOG_WARNING_FMT("MainMenu", "%s not loaded (%s)", label, path.c_str());
            return std::unique_ptr<Engine::Platform::Render::Texture>{};
        }
        return texture;
    };

    m_backgroundTexture = loadUiTexture(m_config.backgroundTexturePath, 1920, "Background");
    m_loadingEmptyTexture = loadUiTexture(m_config.loadingEmptyTexturePath, 1024, "Loading empty bar");
    m_loadingFullTexture = loadUiTexture(m_config.loadingFullTexturePath, 1024, "Loading full bar");
    m_enterLoadingBackground =
        loadUiTexture(m_config.enterLoadingBackgroundPath, 1920, "Enter loading background");
    m_enterReadyBackground =
        loadUiTexture(m_config.enterReadyBackgroundPath, 1920, "Enter ready background");
    m_buttonTexture = loadUiTexture(m_config.buttonTexturePath, 512, "PLAY button");
    if (m_buttonTexture) {
        LOG_INFO("MainMenu", "Main menu button loaded");
    }

    return true;
}

void MainMenu::Shutdown() {
    auto clear = [](std::unique_ptr<Engine::Platform::Render::Texture>& texture) {
        if (texture) {
            texture->ClearTexture();
            texture.reset();
        }
    };
    clear(m_backgroundTexture);
    clear(m_buttonTexture);
    clear(m_loadingEmptyTexture);
    clear(m_loadingFullTexture);
    clear(m_enterLoadingBackground);
    clear(m_enterReadyBackground);
}

void MainMenu::SetLoadProgress(float progress01) {
    if (m_phase == Phase::BootLoading) {
        m_loadProgress = std::clamp(progress01, 0.0f, 1.0f);
    }
}

void MainMenu::SetLoadStatus(const std::string& status) {
    if (m_phase == Phase::BootLoading) {
        m_loadStatus = status.empty() ? m_config.loadingLabelText : status;
    }
}

void MainMenu::SetReady() {
    m_loadProgress = 1.0f;
    m_phase = Phase::Ready;
    LOG_INFO("MainMenu", "Boot load complete — PLAY ready");
}

void MainMenu::BeginEnterWorld() {
    m_phase = Phase::EnterLoading;
    m_loadProgress = 0.0f;
    m_loadStatus = m_config.enterLoadingLabelText;
    m_phaseTimer = 0.0f;
    m_fadeAlpha = 1.0f;
    LOG_INFO("MainMenu", "PLAY clicked — enter-world loading");
}

void MainMenu::SetEnterLoadProgress(float progress01) {
    if (m_phase == Phase::EnterLoading) {
        m_loadProgress = std::clamp(progress01, 0.0f, 1.0f);
    }
}

void MainMenu::SetEnterLoadStatus(const std::string& status) {
    if (m_phase == Phase::EnterLoading) {
        m_loadStatus = status.empty() ? m_config.enterLoadingLabelText : status;
    }
}

void MainMenu::NotifyEnterWorldReady() {
    if (m_phase != Phase::EnterLoading) {
        return;
    }
    m_loadProgress = 1.0f;
    m_phase = Phase::EnterHold;
    m_phaseTimer = 0.0f;
    m_fadeAlpha = 1.0f;
    LOG_INFO("MainMenu", "Enter-world load complete — holding splash");
}

void MainMenu::Update(float deltaTime) {
    const float dt = std::max(0.0f, deltaTime);
    if (m_phase == Phase::EnterHold) {
        m_phaseTimer += dt;
        const float hold = std::max(0.0f, m_config.enterHoldSeconds);
        if (m_phaseTimer >= hold) {
            m_phase = Phase::EnterFade;
            m_phaseTimer = 0.0f;
            m_fadeAlpha = 1.0f;
            LOG_INFO("MainMenu", "Enter splash fade started");
        }
        return;
    }

    if (m_phase == Phase::EnterFade) {
        m_phaseTimer += dt;
        const float fade = std::max(0.01f, m_config.enterFadeSeconds);
        m_fadeAlpha = 1.0f - std::clamp(m_phaseTimer / fade, 0.0f, 1.0f);
        if (m_phaseTimer >= fade) {
            m_fadeAlpha = 0.0f;
            m_phase = Phase::InGame;
            LOG_INFO("MainMenu", "Enter splash complete — in game");
        }
    }
}

void MainMenu::DrawLoadingBar(struct nk_context* ctx,
                              struct nk_command_buffer* canvas,
                              Engine::Platform::UI::UISystem& uiSystem,
                              const MenuScreenLayout& layout,
                              unsigned char alpha) const {
    (void)ctx;
    if (alpha == 0) {
        return;
    }

    const bool haveEmpty = m_loadingEmptyTexture && m_loadingEmptyTexture->IsLoaded();
    const bool haveFull = m_loadingFullTexture && m_loadingFullTexture->IsLoaded();

    const float designWidth =
        static_cast<float>(m_config.minScreenWidth) * std::clamp(m_config.loadingWidthFraction, 0.1f, 1.0f);
    float barW = layout.Scale(designWidth);
    float barH = layout.Scale(36.0f);

    if (haveEmpty) {
        const int texW = std::max(1, m_loadingEmptyTexture->GetWidth());
        const int texH = std::max(1, m_loadingEmptyTexture->GetHeight());
        barH = barW * (static_cast<float>(texH) / static_cast<float>(texW));
    }

    const float x = layout.CenteredX(barW);
    const float y = layout.YFromFraction(barH, m_config.loadingYFraction);
    const struct nk_rect barBounds = nk_rect(x, y, barW, barH);

    if (haveEmpty) {
        const struct nk_image emptyImage = ToNkImage(*m_loadingEmptyTexture);
        nk_draw_image(canvas, barBounds, &emptyImage, nk_rgba(255, 255, 255, alpha));
    } else {
        nk_fill_rect(canvas, barBounds, 4.0f * layout.uiScale, nk_rgba(40, 40, 48, alpha));
        nk_stroke_rect(canvas, barBounds, 4.0f * layout.uiScale, 2.0f * layout.uiScale,
                       nk_rgba(180, 180, 190, alpha));
    }

    const float fill = std::clamp(m_loadProgress, 0.0f, 1.0f);
    if (fill > 0.001f) {
        const float fillW = barW * fill;
        if (haveFull) {
            const struct nk_image fullImage = ToNkImage(*m_loadingFullTexture);
            nk_push_scissor(canvas, nk_rect(x, y, fillW, barH));
            nk_draw_image(canvas, barBounds, &fullImage, nk_rgba(255, 255, 255, alpha));
            nk_push_scissor(canvas, nk_rect(-8192.0f, -8192.0f, 16384.0f, 16384.0f));
        } else {
            nk_fill_rect(canvas, nk_rect(x, y, fillW, barH), 4.0f * layout.uiScale,
                         nk_rgba(70, 160, 255, alpha));
        }
    }

    struct nk_font* labelFont = uiSystem.GetFontLibrary().GetFont(Engine::Platform::UI::FontRole::UiRegular);
    if (!labelFont) {
        labelFont = uiSystem.GetFontLibrary().GetFont(Engine::Platform::UI::FontRole::UiBold);
    }
    if (labelFont && !m_loadStatus.empty()) {
        const struct nk_user_font* font = &labelFont->handle;
        const int textLen = static_cast<int>(m_loadStatus.size());
        const float textW = font->width(font->userdata, font->height, m_loadStatus.c_str(), textLen);
        const float textH = font->height;
        const float textX = layout.CenteredX(textW);
        const float textY = y - textH - 8.0f * layout.uiScale;
        nk_draw_text(canvas,
                     nk_rect(textX, textY, textW, textH),
                     m_loadStatus.c_str(),
                     textLen,
                     font,
                     nk_rgba(0, 0, 0, 0),
                     nk_rgba(255, 255, 255, alpha));
    }
}

void MainMenu::DrawPlayButton(struct nk_context* ctx,
                              struct nk_command_buffer* canvas,
                              Engine::Platform::UI::UISystem& uiSystem,
                              const MenuScreenLayout& layout) {
    const bool haveTexture = m_buttonTexture && m_buttonTexture->IsLoaded();
    if (!haveTexture) {
        const float buttonW = layout.Scale(m_config.fallbackButtonWidth);
        const float buttonH = layout.Scale(m_config.fallbackButtonHeight);
        const float x = layout.CenteredX(buttonW);
        const float y = layout.YFromFraction(buttonH, m_config.yFraction);
        nk_layout_space_begin(ctx, NK_STATIC, buttonH, static_cast<int>(layout.screenWidth));
        nk_layout_space_push(ctx, nk_rect(x, y, buttonW, buttonH));
        if (nk_button_label(ctx, m_config.labelText.c_str())) {
            BeginEnterWorld();
        }
        nk_layout_space_end(ctx);
        return;
    }

    const int texW = m_buttonTexture->GetWidth();
    const int texH = m_buttonTexture->GetHeight();
    if (texW <= 0 || texH <= 0) {
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
        BeginEnterWorld();
    }
}

void MainMenu::Build(struct nk_context* ctx,
                     const Engine::Platform::UI::MenuStyle& /*style*/,
                     Engine::Platform::UI::UISystem& uiSystem,
                     int screenWidth,
                     int screenHeight) {
    if (!ctx || m_phase == Phase::InGame) {
        return;
    }

    if (screenWidth <= 0 || screenHeight <= 0) {
        return;
    }

    const MenuScreenLayout layout = MenuScreenLayout::From(screenWidth, screenHeight, m_config.ReferenceScreen());
    const unsigned char fadeAlpha = static_cast<unsigned char>(std::clamp(m_fadeAlpha, 0.0f, 1.0f) * 255.0f);

    // During fade, skip the opaque window fill so the 3D world shows through.
    const nk_flags windowFlags = (m_phase == Phase::EnterFade)
        ? (NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_NO_INPUT)
        : (NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BACKGROUND);

    if (nk_begin(ctx, "MainMenu",
                 nk_rect(0.0f, 0.0f, layout.screenWidth, layout.screenHeight),
                 windowFlags)) {
        struct nk_command_buffer* canvas = nk_window_get_canvas(ctx);

        if (m_phase == Phase::BootLoading || m_phase == Phase::Ready) {
            if (m_backgroundTexture && m_backgroundTexture->IsLoaded()) {
                DrawFullscreenImage(canvas, *m_backgroundTexture, layout.screenWidth, layout.screenHeight);
            } else {
                nk_fill_rect(canvas, nk_rect(0.0f, 0.0f, layout.screenWidth, layout.screenHeight),
                             0.0f, nk_rgb(0, 0, 0));
            }
            if (m_phase == Phase::BootLoading) {
                DrawLoadingBar(ctx, canvas, uiSystem, layout, 255);
            } else {
                DrawPlayButton(ctx, canvas, uiSystem, layout);
            }
        } else if (m_phase == Phase::EnterLoading) {
            if (m_enterLoadingBackground && m_enterLoadingBackground->IsLoaded()) {
                DrawFullscreenImage(
                    canvas, *m_enterLoadingBackground, layout.screenWidth, layout.screenHeight);
            } else if (m_backgroundTexture && m_backgroundTexture->IsLoaded()) {
                DrawFullscreenImage(canvas, *m_backgroundTexture, layout.screenWidth, layout.screenHeight);
            } else {
                nk_fill_rect(canvas, nk_rect(0.0f, 0.0f, layout.screenWidth, layout.screenHeight),
                             0.0f, nk_rgb(0, 0, 0));
            }
            DrawLoadingBar(ctx, canvas, uiSystem, layout, 255);
        } else if (m_phase == Phase::EnterHold || m_phase == Phase::EnterFade) {
            if (m_enterReadyBackground && m_enterReadyBackground->IsLoaded()) {
                DrawFullscreenImage(canvas,
                                    *m_enterReadyBackground,
                                    layout.screenWidth,
                                    layout.screenHeight,
                                    fadeAlpha);
            } else if (m_enterLoadingBackground && m_enterLoadingBackground->IsLoaded()) {
                DrawFullscreenImage(canvas,
                                    *m_enterLoadingBackground,
                                    layout.screenWidth,
                                    layout.screenHeight,
                                    fadeAlpha);
            } else {
                nk_fill_rect(canvas,
                             nk_rect(0.0f, 0.0f, layout.screenWidth, layout.screenHeight),
                             0.0f,
                             nk_rgba(0, 0, 0, fadeAlpha));
            }
        }
    }
    nk_end(ctx);
}

} // namespace UI
} // namespace Game
