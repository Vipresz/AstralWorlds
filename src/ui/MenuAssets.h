#pragma once

#include <string>

namespace Game {
namespace UI {

// Runtime paths for menu textures (relative to game executable directory).
// Source files live in repo: menu/
// See menu/manifest.json and menu/README.md for slot descriptions.
struct MenuAssets {
    static constexpr const char* Root() { return "menu"; }
    static constexpr const char* Manifest() { return "menu/manifest.json"; }

    // backgrounds/
    static constexpr const char* PanelBackground() { return "menu/backgrounds/panel.png"; }
    static constexpr const char* OverlayDim() { return "menu/backgrounds/overlay_dim.png"; }

    // buttons/
    static constexpr const char* ButtonNormal() { return "menu/buttons/normal.png"; }
    static constexpr const char* ButtonHover() { return "menu/buttons/hover.png"; }
    static constexpr const char* ButtonActive() { return "menu/buttons/active.png"; }
    static constexpr const char* ButtonDisabled() { return "menu/buttons/disabled.png"; }

    // frames/
    static constexpr const char* WindowBorder() { return "menu/frames/window_border.png"; }
    static constexpr const char* TitleBar() { return "menu/frames/title_bar.png"; }

    // fonts/ (see menu/fonts/LICENSE.txt)
    static constexpr const char* FontsRoot() { return "menu/fonts"; }
    static constexpr const char* InterRegular() { return "menu/fonts/Inter-Regular.ttf"; }
    static constexpr const char* InterBold() { return "menu/fonts/Inter-Bold.ttf"; }
    static constexpr const char* JetBrainsMonoRegular() { return "menu/fonts/JetBrainsMono-Regular.ttf"; }

    // Returns true if a file exists at the given runtime path (from exe CWD).
    static bool Exists(const char* relativePath);

    // Convenience: all core button state paths in draw order.
    struct ButtonStates {
        const char* normal;
        const char* hover;
        const char* active;
        const char* disabled;
    };

    static ButtonStates DefaultButtons();
};

} // namespace UI
} // namespace Game
