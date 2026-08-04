#pragma once

#ifdef PLATFORM_HAS_NUKLEAR

#include "Menu.h"
#include "render/Texture.h"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

struct nk_context;

namespace Game {
namespace UI {

// Top-left character HUD frame with bar slots laid out relative to the HUD rect.
class CharacterHud {
public:
    struct ScreenBounds {
        float x = 0.0f;
        float y = 0.0f;
        float w = 0.0f;
        float h = 0.0f;
    };

    bool Initialize(const CharacterHudConfig& config);
    void Shutdown();

    void SetEnabled(bool enabled) { m_enabled = enabled; }
    bool IsEnabled() const { return m_enabled; }

    // Fill amount in [0, 1] for a bar id from the manifest (e.g. "health", "mana").
    void SetBarFill(const std::string& barId, float fill01);

    void SetTitleLabel(const std::string& title) { m_titleLabel = title; }
    void ClearTitleLabel() { m_titleLabel.clear(); }

    void SetPositionOverride(float x, float y) {
        m_usePositionOverride = true;
        m_overrideX = x;
        m_overrideY = y;
    }
    void ClearPositionOverride() { m_usePositionOverride = false; }

    void SetOverlayWindowName(const std::string& name) { m_overlayWindowName = name; }

    ScreenBounds ComputeScreenBounds(int screenWidth, int screenHeight) const;

    // Includes the title label area above the frame when a title is set.
    ScreenBounds ComputeInteractiveBounds(int screenWidth, int screenHeight) const;

    // Hot-reloads character_hud layout from menus/manifest.json when the file changes
    // (after save + sync_menu_manifests). Texture is reloaded only if the path changes.
    void Draw(struct nk_context* ctx, int screenWidth, int screenHeight);

private:
    void MaybeReloadLayoutFromManifest();
    void ResolveScreenBounds(int screenWidth,
                             int screenHeight,
                             float& outX,
                             float& outY,
                             float& outW,
                             float& outH) const;
    void DrawAt(struct nk_context* ctx,
                int screenWidth,
                int screenHeight,
                float hudX,
                float hudY,
                float hudW,
                float hudH);

    CharacterHudConfig m_config;
    std::unique_ptr<Engine::Platform::Render::Texture> m_texture;
    std::unordered_map<std::string, float> m_barFills;
    std::string m_titleLabel;
    bool m_usePositionOverride = false;
    float m_overrideX = 0.0f;
    float m_overrideY = 0.0f;
    std::string m_overlayWindowName = "CharacterHud";
    std::uint64_t m_lastManifestWriteTime = 0;
    bool m_hasManifestWriteTime = false;
    bool m_enabled = true;
};

} // namespace UI
} // namespace Game

#endif // PLATFORM_HAS_NUKLEAR
