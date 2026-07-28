#pragma once

#ifdef PLATFORM_HAS_NUKLEAR

#include "ui/MenuStyle.h"
#include "ui/UISystem.h"
#include "render/Texture.h"
#include "MenuLayout.h"

#include <memory>
#include <string>

struct nk_context;
struct nk_command_buffer;

namespace Game {
namespace UI {

struct MainMenuConfig {
    static constexpr const char* kManifestPath = "menus/main_menu/manifest.json";
    static constexpr const char* kRoot = "menus/main_menu";

    std::string backgroundTexturePath = "menus/main_menu/backgrounds/background.png";
    std::string buttonTexturePath = "menus/main_menu/buttons/clicked.png";
    std::string loadingEmptyTexturePath = "menus/main_menu/loading/loadingEmpty.png";
    std::string loadingFullTexturePath = "menus/main_menu/loading/loadingFull.png";
    std::string enterLoadingBackgroundPath = "menus/main_menu/loading_images/void_waste_old.png";
    std::string enterReadyBackgroundPath = "menus/main_menu/loading_images/void_waste.png";
    std::string labelText = "PLAY";
    std::string loadingLabelText = "Loading...";
    std::string enterLoadingLabelText = "Entering world...";
    float textSize = 36.0f;
    float maxWidthFraction = 0.15f;
    float yFraction = 0.9f;
    float loadingWidthFraction = 0.45f;
    float loadingYFraction = 0.88f;
    float enterHoldSeconds = 0.5f;
    float enterFadeSeconds = 0.75f;
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

// Boot: loading bar → PLAY.
// After PLAY: void_waste_old + bar → void_waste hold → fade out → in-game.
class MainMenu {
public:
    enum class Phase {
        BootLoading,
        Ready,
        EnterLoading,
        EnterHold,
        EnterFade,
        InGame
    };

    static bool LoadConfig(MainMenuConfig& out, const char* manifestPath = MainMenuConfig::kManifestPath);

    bool Initialize(const MainMenuConfig* config = nullptr);
    void Shutdown();

    // Boot progress in [0, 1]. Call from staged startup before SetReady().
    void SetLoadProgress(float progress01);
    void SetLoadStatus(const std::string& status);
    void SetReady();

    // After PLAY: drive join/character load, then reveal splash.
    void BeginEnterWorld();
    void SetEnterLoadProgress(float progress01);
    void SetEnterLoadStatus(const std::string& status);
    void NotifyEnterWorldReady();
    void Update(float deltaTime);

    Phase GetPhase() const { return m_phase; }
    bool IsLoading() const { return m_phase == Phase::BootLoading; }
    bool IsReady() const { return m_phase == Phase::Ready; }
    bool IsEnterWorldRequested() const {
        return m_phase == Phase::EnterLoading || m_phase == Phase::EnterHold ||
               m_phase == Phase::EnterFade || m_phase == Phase::InGame;
    }
    bool IsEnterWorldLoading() const { return m_phase == Phase::EnterLoading; }
    // Overlay still covers the screen (including hold/fade).
    bool IsVisible() const { return m_phase != Phase::InGame; }
    // True only after the enter splash has fully faded — show HUD / character menu.
    bool IsGameStarted() const { return m_phase == Phase::InGame; }
    bool BlocksGameInput() const { return m_phase != Phase::InGame; }

    float GetLoadProgress() const { return m_loadProgress; }
    const MainMenuConfig& GetConfig() const { return m_config; }

    void Build(struct nk_context* ctx,
               const Engine::Platform::UI::MenuStyle& style,
               Engine::Platform::UI::UISystem& uiSystem,
               int screenWidth,
               int screenHeight);

private:
    void DrawLoadingBar(struct nk_context* ctx,
                        struct nk_command_buffer* canvas,
                        Engine::Platform::UI::UISystem& uiSystem,
                        const MenuScreenLayout& layout,
                        unsigned char alpha) const;
    void DrawPlayButton(struct nk_context* ctx,
                        struct nk_command_buffer* canvas,
                        Engine::Platform::UI::UISystem& uiSystem,
                        const MenuScreenLayout& layout);

    MainMenuConfig m_config;
    std::unique_ptr<Engine::Platform::Render::Texture> m_backgroundTexture;
    std::unique_ptr<Engine::Platform::Render::Texture> m_buttonTexture;
    std::unique_ptr<Engine::Platform::Render::Texture> m_loadingEmptyTexture;
    std::unique_ptr<Engine::Platform::Render::Texture> m_loadingFullTexture;
    std::unique_ptr<Engine::Platform::Render::Texture> m_enterLoadingBackground;
    std::unique_ptr<Engine::Platform::Render::Texture> m_enterReadyBackground;
    std::string m_loadStatus = "Loading...";
    float m_loadProgress = 0.0f;
    float m_phaseTimer = 0.0f;
    float m_fadeAlpha = 1.0f;
    Phase m_phase = Phase::BootLoading;
};

} // namespace UI
} // namespace Game

#endif // PLATFORM_HAS_NUKLEAR
