#pragma once

#ifdef PLATFORM_HAS_NUKLEAR

#include "ui/MenuStyle.h"
#include "ui/UISystem.h"
#include "render/Texture.h"
#include "MenuLayout.h"
#include "character/CharacterPaperdollRenderer.h"
#include "core/Types.h"

#include <chrono>
#include <memory>
#include <string>
#include <vector>

struct nk_context;

namespace Engine {
namespace ECS {
class Registry;
}
namespace Platform {
namespace Render {
class Shader;
}
} // namespace Platform
} // namespace Engine

namespace Game {
namespace Character {
class CharacterInstance;
} // namespace Character

namespace UI {

struct BarButtonConfig {
    std::string id;
    std::string darkTexturePath;
    std::string lightTexturePath;
    float xFraction = -1.0f;
    float yFraction = -1.0f;
    float heightFraction = -1.0f;
};

struct MinimapHudConfig {
    std::string borderTexturePath = "menus/map/map.png";
    float sizePixels = 234.0f;
    float marginPixels = 21.0f;
    float borderInsetFraction = 0.13f;
};

// One bar slot inside the character HUD frame. Fractions are relative to the HUD rect
// (0,0 = HUD top-left, 1,1 = HUD bottom-right), not the screen.
struct CharacterHudBarConfig {
    std::string id;
    float xFraction = 0.0f;
    float yFraction = 0.0f;
    float widthFraction = 1.0f;
    float heightFraction = 1.0f;
};

struct CharacterHudConfig {
    std::string texturePath = "menus/hud/hud.png";
    // Design-space size in pixels (scaled by UI scale vs 1280x720 reference).
    // Authored in src/menus/manifest.json as source_width / source_height.
    int sourceWidth = 400;
    int sourceHeight = 103;
    float xFraction = 0.015f;
    float yFraction = 0.02f;
    std::string anchor = "top_left";
    std::vector<CharacterHudBarConfig> bars;
};

struct OverlayPanelConfig {
    std::string buttonId;
    std::string texturePath;
    float widthFraction = 0.25f;
    float xFraction = 0.18f;
    float yFraction = 0.42f;
    bool hasCloseButtonPlacement = false;
    float closeSizeFraction = 0.12f;
    float closeXAnchorFraction = 1.0f;
    float closeYAnchorFraction = 0.0f;
    float closeMarginFraction = 0.0f;
    bool closeCenterOnCorner = true;
    bool hasCharacterPreview = false;
    float characterPreviewSizeFraction = 0.5f;
    float characterPreviewXFraction = 0.5f;
    float characterPreviewYFraction = 0.4f;
};

struct CloseButtonConfig {
    std::string darkTexturePath = "menus/close/close_dark.png";
    std::string lightTexturePath = "menus/close/close_light.png";
    float sizeFraction = 0.12f;
    float xAnchorFraction = 1.0f;
    float yAnchorFraction = 0.0f;
    float marginFraction = 0.0f;
    bool centerOnCorner = true;
};

struct PaperdollRotateButtonConfig {
    std::string leftTexturePath = "menus/paperdoll/arrows/rotate_left_side.png";
    std::string rightTexturePath = "menus/paperdoll/arrows/rotate_right_side.png";
    std::string leftLightTexturePath = "menus/paperdoll/arrows/rotate_left_side_light.png";
    std::string rightLightTexturePath = "menus/paperdoll/arrows/rotate_right_side_light.png";
    // Vertical center of the buttons, measured up from the panel bottom (0.20 = 20% above bottom).
    float yFromBottomFraction = 0.20f;
    float sizeFraction = 0.10f;
    // Button width / height. Height is derived as width / aspectRatio.
    float aspectRatio = 1.0f;
    // Horizontal centers as fractions of panel width (0 = left edge, 1 = right edge).
    float leftXFraction = 0.18f;
    float rightXFraction = 0.82f;
    // Continuous spin speed while a rotate arrow is held.
    float degreesPerSecond = 120.0f;
};

struct CharacterMenuConfig {
    static constexpr const char* kManifestPath = "menus/manifest.json";
    static constexpr const char* kRoot = "menus";

    std::string bottomBarTexturePath = "menus/char_screen/bottom_menu.png";
    float bottomBarWidthFraction = 0.6f;
    float barButtonHeightFraction = 22.0f / 51.0f;
    float barButtonXSpacingFraction = 0.05350877192982456140350877192982f;
    float barButtonFirstXFraction = 0.4485308173773625628217642848642f;
    float barButtonYFraction = 0.5f;
    std::vector<BarButtonConfig> barButtons;
    MinimapHudConfig minimap;
    CharacterHudConfig characterHud;
    std::vector<OverlayPanelConfig> panels;
    CloseButtonConfig closeButton;
    PaperdollRotateButtonConfig paperdollRotate;
    int minScreenWidth = 1280;
    int minScreenHeight = 720;

    MenuReferenceScreen ReferenceScreen() const {
        return MenuReferenceScreen{minScreenWidth, minScreenHeight};
    }
};

// Bottom HUD overlay on the character screen (after main menu PLAY).
class CharacterMenu {
public:
    static bool LoadConfig(CharacterMenuConfig& out, const char* manifestPath = CharacterMenuConfig::kManifestPath);

    bool Initialize(const CharacterMenuConfig* config = nullptr);
    void Shutdown();

    void Show() { m_visible = true; }
    void Hide() { m_visible = false; }
    bool IsVisible() const { return m_visible; }
    bool BlocksGameInput() const { return m_visible && (m_pointerOverBarUi || m_pointerOverPanelUi); }

    const CharacterMenuConfig& GetConfig() const { return m_config; }

    void SetPaperdollCharacter(Engine::ECS::Registry* registry,
                               Engine::EntityID characterEntity,
                               Engine::Platform::Render::Shader* shader,
                               Game::Character::CharacterInstance* characterInstance);

    void Build(struct nk_context* ctx,
               const Engine::Platform::UI::MenuStyle& style,
               Engine::Platform::UI::UISystem& uiSystem,
               int screenWidth,
               int screenHeight);

private:
    struct LoadedBarButton {
        std::string id;
        std::unique_ptr<Engine::Platform::Render::Texture> darkTexture;
        std::unique_ptr<Engine::Platform::Render::Texture> lightTexture;
        float xFraction = -1.0f;
        float yFraction = -1.0f;
        float heightFraction = -1.0f;
        bool pressed = false;
    };

    LoadedBarButton* FindBarButton(const std::string& id);
    CloseButtonConfig ResolvePanelCloseButton(const OverlayPanelConfig& panel) const;

    struct LoadedOverlayPanel {
        OverlayPanelConfig config;
        std::unique_ptr<Engine::Platform::Render::Texture> texture;
        bool positionInitialized = false;
        bool dragging = false;
        float centerX = 0.0f;
        float centerY = 0.0f;
        float dragGrabX = 0.0f;
        float dragGrabY = 0.0f;
    };

    CharacterMenuConfig m_config;
    std::unique_ptr<Engine::Platform::Render::Texture> m_bottomBarTexture;
    std::unique_ptr<Engine::Platform::Render::Texture> m_closeDarkTexture;
    std::unique_ptr<Engine::Platform::Render::Texture> m_closeLightTexture;
    std::unique_ptr<Engine::Platform::Render::Texture> m_paperdollRotateLeftTexture;
    std::unique_ptr<Engine::Platform::Render::Texture> m_paperdollRotateRightTexture;
    std::unique_ptr<Engine::Platform::Render::Texture> m_paperdollRotateLeftLightTexture;
    std::unique_ptr<Engine::Platform::Render::Texture> m_paperdollRotateRightLightTexture;
    std::vector<LoadedBarButton> m_barButtons;
    std::vector<LoadedOverlayPanel> m_overlayPanels;
    std::unique_ptr<Game::Character::CharacterPaperdollRenderer> m_paperdollRenderer;
    Engine::ECS::Registry* m_paperdollRegistry = nullptr;
    Engine::EntityID m_paperdollCharacterEntity = Engine::INVALID_ENTITY;
    Engine::Platform::Render::Shader* m_paperdollShader = nullptr;
    Game::Character::CharacterInstance* m_paperdollCharacterInstance = nullptr;
    float m_paperdollYawDegrees = 0.0f;
    bool m_hasPaperdollRotateClock = false;
    std::chrono::steady_clock::time_point m_lastPaperdollRotateTime{};
    bool m_visible = false;
    mutable bool m_pointerOverBarUi = false;
    mutable bool m_pointerOverPanelUi = false;
};

} // namespace UI
} // namespace Game

#endif // PLATFORM_HAS_NUKLEAR
