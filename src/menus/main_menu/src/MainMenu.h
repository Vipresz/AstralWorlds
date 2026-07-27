#pragma once

#ifdef PLATFORM_HAS_NUKLEAR

#include "ui/MenuStyle.h"
#include "ui/UISystem.h"
#include "render/Texture.h"
#include "MenuLayout.h"

#include <memory>
#include <string>

struct nk_context;

namespace Game {
namespace UI {

struct MainMenuConfig {
    static constexpr const char* kManifestPath = "menus/main_menu/manifest.json";
    static constexpr const char* kRoot = "menus/main_menu";

    std::string backgroundTexturePath = "menus/main_menu/backgrounds/background.png";
    std::string buttonTexturePath = "menus/main_menu/buttons/clicked.png";
    std::string labelText = "PLAY";
    float textSize = 36.0f;
    float maxWidthFraction = 0.15f;
    float yFraction = 0.9f;
    int minScreenWidth = 1280;
    int minScreenHeight = 720;
    float fallbackButtonWidth = 280.0f;
    float fallbackButtonHeight = 80.0f;

    MenuReferenceScreen ReferenceScreen() const {
        return MenuReferenceScreen{minScreenWidth, minScreenHeight};
    }

    float ComputeUiScale(int screenWidth, int screenHeight) const {
        return MenuScreenLayout::From(screenWidth, screenHeight, ReferenceScreen()).uiScale;
    }
};

// Full-screen main menu with a centered PLAY image button.
class MainMenu {
public:
    // Load layout values from manifest.json (falls back to MainMenuConfig defaults).
    static bool LoadConfig(MainMenuConfig& out, const char* manifestPath = MainMenuConfig::kManifestPath);

    bool Initialize(const MainMenuConfig* config = nullptr);
    void Shutdown();

    bool IsVisible() const { return !m_gameStarted; }
    bool IsGameStarted() const { return m_gameStarted; }
    bool BlocksGameInput() const { return !m_gameStarted; }

    const MainMenuConfig& GetConfig() const { return m_config; }

    void Build(struct nk_context* ctx,
               const Engine::Platform::UI::MenuStyle& style,
               Engine::Platform::UI::UISystem& uiSystem,
               int screenWidth,
               int screenHeight);

private:
    MainMenuConfig m_config;
    std::unique_ptr<Engine::Platform::Render::Texture> m_backgroundTexture;
    std::unique_ptr<Engine::Platform::Render::Texture> m_buttonTexture;
    bool m_gameStarted = false;
};

} // namespace UI
} // namespace Game

#endif // PLATFORM_HAS_NUKLEAR
