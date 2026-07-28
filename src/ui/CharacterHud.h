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
    bool Initialize(const CharacterHudConfig& config);
    void Shutdown();

    void SetEnabled(bool enabled) { m_enabled = enabled; }
    bool IsEnabled() const { return m_enabled; }

    // Fill amount in [0, 1] for a bar id from the manifest (e.g. "health", "mana").
    void SetBarFill(const std::string& barId, float fill01);

    // Hot-reloads character_hud layout from menus/manifest.json when the file changes
    // (after save + sync_menu_manifests). Texture is reloaded only if the path changes.
    void Draw(struct nk_context* ctx, int screenWidth, int screenHeight);

private:
    void MaybeReloadLayoutFromManifest();

    CharacterHudConfig m_config;
    std::unique_ptr<Engine::Platform::Render::Texture> m_texture;
    std::unordered_map<std::string, float> m_barFills;
    std::uint64_t m_lastManifestWriteTime = 0;
    bool m_hasManifestWriteTime = false;
    bool m_enabled = true;
};

} // namespace UI
} // namespace Game

#endif // PLATFORM_HAS_NUKLEAR
