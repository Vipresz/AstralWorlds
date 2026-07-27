#pragma once

#ifdef PLATFORM_HAS_NUKLEAR

#include "ui/MenuStyle.h"
#include "ui/UISystem.h"
#include "render/Texture.h"
#include "MenuLayout.h"
#include "character/CharacterPaperdollRenderer.h"
#include "core/Types.h"

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
    std::vector<OverlayPanelConfig> panels;
    CloseButtonConfig closeButton;
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
    std::vector<LoadedBarButton> m_barButtons;
    std::vector<LoadedOverlayPanel> m_overlayPanels;
    std::unique_ptr<Game::Character::CharacterPaperdollRenderer> m_paperdollRenderer;
    Engine::ECS::Registry* m_paperdollRegistry = nullptr;
    Engine::EntityID m_paperdollCharacterEntity = Engine::INVALID_ENTITY;
    Engine::Platform::Render::Shader* m_paperdollShader = nullptr;
    Game::Character::CharacterInstance* m_paperdollCharacterInstance = nullptr;
    bool m_visible = false;
    mutable bool m_pointerOverBarUi = false;
    mutable bool m_pointerOverPanelUi = false;
};

} // namespace UI
} // namespace Game

#endif // PLATFORM_HAS_NUKLEAR
