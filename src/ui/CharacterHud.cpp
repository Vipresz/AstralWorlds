#include "ui/CharacterHud.h"

#ifdef PLATFORM_HAS_NUKLEAR

#include "Logger.h"
#include "MenuLayout.h"
#include "ui/OverlayWindow.h"

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#include "nuklear.h"

#include <algorithm>
#include <cmath>
#include <filesystem>

using Engine::Platform::Render::Texture;
using Engine::Platform::UI::DrawTexture;
using Engine::Platform::UI::OverlayScope;
using EngineLogging::Logger;

namespace Game {
namespace UI {

namespace {

struct nk_color BarColorForId(const std::string& id, float alpha) {
    const unsigned char a = static_cast<unsigned char>(std::clamp(alpha, 0.0f, 1.0f) * 255.0f);
    if (id == "mana") {
        return nk_rgba(70, 140, 255, a);
    }
    // Default / health
    return nk_rgba(210, 55, 55, a);
}

std::uint64_t FileWriteTimeToken(const char* path) {
    namespace fs = std::filesystem;
    std::error_code ec;
    const auto writeTime = fs::last_write_time(path, ec);
    if (ec) {
        return 0;
    }
    return static_cast<std::uint64_t>(writeTime.time_since_epoch().count());
}

// Prefer the editable src manifest when launching from build/bin/<Config>.
constexpr const char* kRuntimeManifestPath = "menus/manifest.json";
constexpr const char* kDevSrcManifestPath = "../../../src/menus/manifest.json";

struct ManifestWatch {
    const char* path = kRuntimeManifestPath;
    std::uint64_t writeTime = 0;
};

ManifestWatch ResolveHudManifest() {
    const std::uint64_t srcWriteTime = FileWriteTimeToken(kDevSrcManifestPath);
    if (srcWriteTime != 0) {
        return ManifestWatch{kDevSrcManifestPath, srcWriteTime};
    }
    return ManifestWatch{kRuntimeManifestPath, FileWriteTimeToken(kRuntimeManifestPath)};
}

bool ApplyManifestLayout(CharacterHudConfig& config, const char* manifestPath) {
    CharacterMenuConfig loaded;
    if (!CharacterMenu::LoadConfig(loaded, manifestPath)) {
        return false;
    }
    config = loaded.characterHud;
    return true;
}

} // namespace

bool CharacterHud::Initialize(const CharacterHudConfig& config) {
    Shutdown();
    m_config = config;

    // Size/layout always come from src/menus/manifest.json when present.
    const ManifestWatch watch = ResolveHudManifest();
    if (watch.writeTime != 0) {
        ApplyManifestLayout(m_config, watch.path);
    }
    m_lastManifestWriteTime = watch.writeTime;
    m_hasManifestWriteTime = watch.writeTime != 0;

    if (m_config.texturePath.empty()) {
        LOG_WARNING("CharacterHud", "No HUD texture path configured");
        return false;
    }

    m_texture = std::make_unique<Texture>(m_config.texturePath.c_str());
    if (!m_texture->LoadTextureUI(1024)) {
        LOG_ERROR_FMT("CharacterHud", "Failed to load HUD texture: %s", m_config.texturePath.c_str());
        m_texture.reset();
        return false;
    }

    for (const CharacterHudBarConfig& bar : m_config.bars) {
        m_barFills[bar.id] = 1.0f;
    }

    LOG_INFO_FMT("CharacterHud",
                 "Loaded %s from %s (%d bars) size=%dx%d",
                 m_config.texturePath.c_str(),
                 watch.path,
                 static_cast<int>(m_config.bars.size()),
                 m_config.sourceWidth,
                 m_config.sourceHeight);
    return true;
}

void CharacterHud::Shutdown() {
    m_texture.reset();
    m_barFills.clear();
    m_lastManifestWriteTime = 0;
    m_hasManifestWriteTime = false;
    m_enabled = true;
}

void CharacterHud::SetBarFill(const std::string& barId, float fill01) {
    m_barFills[barId] = std::clamp(fill01, 0.0f, 1.0f);
}

void CharacterHud::ComputeAnchoredPosition(int screenWidth,
                                           int screenHeight,
                                           float hudW,
                                           float hudH,
                                           float& outX,
                                           float& outY) const {
    outX = static_cast<float>(screenWidth) * m_config.xFraction;
    outY = static_cast<float>(screenHeight) * m_config.yFraction;
    if (m_config.anchor == "top_right") {
        outX = static_cast<float>(screenWidth) * (1.0f - m_config.xFraction) - hudW;
    } else if (m_config.anchor == "bottom_left") {
        outY = static_cast<float>(screenHeight) * (1.0f - m_config.yFraction) - hudH;
    } else if (m_config.anchor == "bottom_right") {
        outX = static_cast<float>(screenWidth) * (1.0f - m_config.xFraction) - hudW;
        outY = static_cast<float>(screenHeight) * (1.0f - m_config.yFraction) - hudH;
    }
}

void CharacterHud::ClampPanelPosition(
    float& x, float& y, float width, float height, int screenWidth, int screenHeight) {
    const float maxX = std::max(0.0f, static_cast<float>(screenWidth) - width);
    const float maxY = std::max(0.0f, static_cast<float>(screenHeight) - height);
    x = std::clamp(x, 0.0f, maxX);
    y = std::clamp(y, 0.0f, maxY);
}

void CharacterHud::EnsurePositionInitialized(int screenWidth, int screenHeight) {
    if (m_positionInitialized || m_usePositionOverride || !m_config.movable || !m_allowDrag) {
        return;
    }

    const MenuScreenLayout layout =
        MenuScreenLayout::From(screenWidth, screenHeight, MenuReferenceScreen{});
    const float hudW = layout.Scale(static_cast<float>(std::max(1, m_config.sourceWidth)));
    const float hudH = layout.Scale(static_cast<float>(std::max(1, m_config.sourceHeight)));
    ComputeAnchoredPosition(screenWidth, screenHeight, hudW, hudH, m_panelX, m_panelY);
    ClampPanelPosition(m_panelX, m_panelY, hudW, hudH, screenWidth, screenHeight);
    m_positionInitialized = true;
}

void CharacterHud::ResolveScreenBounds(int screenWidth,
                                       int screenHeight,
                                       float& outX,
                                       float& outY,
                                       float& outW,
                                       float& outH) const {
    const MenuScreenLayout layout =
        MenuScreenLayout::From(screenWidth, screenHeight, MenuReferenceScreen{});
    outW = layout.Scale(static_cast<float>(std::max(1, m_config.sourceWidth)));
    outH = layout.Scale(static_cast<float>(std::max(1, m_config.sourceHeight)));

    if (m_usePositionOverride) {
        outX = m_overrideX;
        outY = m_overrideY;
        return;
    }

    if (m_config.movable && m_allowDrag && m_positionInitialized) {
        outX = m_panelX;
        outY = m_panelY;
        return;
    }

    ComputeAnchoredPosition(screenWidth, screenHeight, outW, outH, outX, outY);
}

CharacterHud::ScreenBounds CharacterHud::ComputeScreenBounds(int screenWidth,
                                                             int screenHeight) const {
    ScreenBounds bounds;
    ResolveScreenBounds(screenWidth, screenHeight, bounds.x, bounds.y, bounds.w, bounds.h);
    return bounds;
}

CharacterHud::ScreenBounds CharacterHud::ComputeInteractiveBounds(int screenWidth,
                                                                  int screenHeight) const {
    ScreenBounds bounds = ComputeScreenBounds(screenWidth, screenHeight);
    if (!m_titleLabel.empty()) {
        const MenuScreenLayout layout =
            MenuScreenLayout::From(screenWidth, screenHeight, MenuReferenceScreen{});
        const float titleHeight = layout.Scale(16.0f) + 4.0f;
        bounds.y -= titleHeight;
        bounds.h += titleHeight;
    }
    return bounds;
}

void CharacterHud::MaybeReloadLayoutFromManifest() {
    const ManifestWatch watch = ResolveHudManifest();
    if (watch.writeTime == 0) {
        return;
    }
    if (m_hasManifestWriteTime && watch.writeTime == m_lastManifestWriteTime) {
        return;
    }
    m_lastManifestWriteTime = watch.writeTime;
    m_hasManifestWriteTime = true;

    const std::string previousTexture = m_config.texturePath;
    const auto previousFills = m_barFills;
    if (!ApplyManifestLayout(m_config, watch.path)) {
        return;
    }

    if (m_config.texturePath != previousTexture || !m_texture || !m_texture->IsLoaded()) {
        m_texture = std::make_unique<Texture>(m_config.texturePath.c_str());
        if (!m_texture->LoadTextureUI(1024)) {
            LOG_ERROR_FMT("CharacterHud",
                          "Failed to reload HUD texture: %s",
                          m_config.texturePath.c_str());
            m_texture.reset();
        }
    }

    m_barFills = previousFills;
    for (const CharacterHudBarConfig& bar : m_config.bars) {
        if (m_barFills.find(bar.id) == m_barFills.end()) {
            m_barFills[bar.id] = 1.0f;
        }
    }

    LOG_INFO_FMT("CharacterHud",
                 "Reloaded layout from %s (size=%dx%d, %d bars)",
                 watch.path,
                 m_config.sourceWidth,
                 m_config.sourceHeight,
                 static_cast<int>(m_config.bars.size()));
}

void CharacterHud::UpdatePointer(struct nk_context* ctx, int screenWidth, int screenHeight) {
    if (!ctx || screenWidth <= 0 || screenHeight <= 0 || !m_config.movable || !m_allowDrag ||
        m_usePositionOverride) {
        m_dragging = false;
        m_pointerDown = false;
        return;
    }

    EnsurePositionInitialized(screenWidth, screenHeight);

    const ScreenBounds interactive = ComputeInteractiveBounds(screenWidth, screenHeight);
    const struct nk_rect panelBounds =
        nk_rect(interactive.x, interactive.y, interactive.w, interactive.h);

    const bool leftDown = ctx->input.mouse.buttons[NK_BUTTON_LEFT].down;
    const bool leftPressed =
        nk_input_is_mouse_click_down_in_rect(&ctx->input, NK_BUTTON_LEFT, panelBounds, nk_true);

    if (m_dragging) {
        if (leftDown) {
            m_panelX = ctx->input.mouse.pos.x - m_dragGrabX;
            m_panelY = ctx->input.mouse.pos.y - m_dragGrabY;
            ClampPanelPosition(
                m_panelX, m_panelY, interactive.w, interactive.h, screenWidth, screenHeight);
        } else {
            m_dragging = false;
        }
        m_pointerDown = false;
        return;
    }

    if (leftPressed) {
        m_pointerDown = true;
        m_pointerDownX = ctx->input.mouse.pos.x;
        m_pointerDownY = ctx->input.mouse.pos.y;
        m_dragGrabX = ctx->input.mouse.pos.x - interactive.x;
        m_dragGrabY = ctx->input.mouse.pos.y - interactive.y;
        return;
    }

    if (m_pointerDown && leftDown) {
        const float dx = ctx->input.mouse.pos.x - m_pointerDownX;
        const float dy = ctx->input.mouse.pos.y - m_pointerDownY;
        if ((dx * dx + dy * dy) > (kDragThresholdPixels * kDragThresholdPixels)) {
            m_dragging = true;
            m_pointerDown = false;
        }
        return;
    }

    if (m_pointerDown && !leftDown) {
        m_pointerDown = false;
    }
}

void CharacterHud::Draw(struct nk_context* ctx, int screenWidth, int screenHeight) {
    MaybeReloadLayoutFromManifest();

    if (!m_enabled || !ctx || !m_texture || !m_texture->IsLoaded() ||
        screenWidth <= 0 || screenHeight <= 0) {
        return;
    }

    UpdatePointer(ctx, screenWidth, screenHeight);
    EnsurePositionInitialized(screenWidth, screenHeight);

    float hudX = 0.0f;
    float hudY = 0.0f;
    float hudW = 0.0f;
    float hudH = 0.0f;
    ResolveScreenBounds(screenWidth, screenHeight, hudX, hudY, hudW, hudH);
    DrawAt(ctx, screenWidth, screenHeight, hudX, hudY, hudW, hudH);
}

void CharacterHud::DrawAt(struct nk_context* ctx,
                          int screenWidth,
                          int screenHeight,
                          float hudX,
                          float hudY,
                          float hudW,
                          float hudH) {
    (void)screenWidth;
    (void)screenHeight;
    OverlayScope overlay(
        ctx,
        m_overlayWindowName.c_str(),
        OverlayScope::FullScreen(screenWidth, screenHeight),
        NK_WINDOW_NO_INPUT);
    if (!overlay.IsOpen() || !overlay.Canvas()) {
        return;
    }

    struct nk_command_buffer* canvas = overlay.Canvas();
    const struct nk_rect hudBounds = nk_rect(hudX, hudY, hudW, hudH);

    if (!m_titleLabel.empty() && ctx->style.font) {
        const struct nk_user_font* font = ctx->style.font;
        const struct nk_color transparent = nk_rgba(0, 0, 0, 0);
        const struct nk_color titleColor = nk_rgba(225, 232, 245, 255);
        const int titleLen = static_cast<int>(std::min(m_titleLabel.size(), size_t{32}));
        const float textW =
            font->width(font->userdata, font->height, m_titleLabel.c_str(), titleLen);
        const float titleY = hudY - static_cast<float>(font->height) - 4.0f;
        nk_draw_text(canvas,
                     nk_rect(hudX + (hudW - textW) * 0.5f,
                             titleY,
                             textW,
                             static_cast<float>(font->height)),
                     m_titleLabel.c_str(),
                     titleLen,
                     font,
                     transparent,
                     titleColor);
    }

    // Fills behind the HUD frame so the ornate border/slots sit on top.
    for (const CharacterHudBarConfig& bar : m_config.bars) {
        float fill = 1.0f;
        const auto it = m_barFills.find(bar.id);
        if (it != m_barFills.end()) {
            fill = it->second;
        }
        if (fill <= 0.001f) {
            continue;
        }

        const float slotX = hudX + hudW * bar.xFraction;
        const float slotY = hudY + hudH * bar.yFraction;
        const float slotW = hudW * bar.widthFraction;
        const float slotH = hudH * bar.heightFraction;
        const struct nk_rect fillRect = nk_rect(slotX, slotY, slotW * fill, slotH);
        nk_fill_rect(canvas, fillRect, 0.0f, BarColorForId(bar.id, 0.85f));
    }

    DrawTexture(canvas, *m_texture, hudBounds, nk_rgba(255, 255, 255, 255));
}

} // namespace UI
} // namespace Game

#endif // PLATFORM_HAS_NUKLEAR
