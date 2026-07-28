# Menu assets

Drop your UI textures here. The game copies this folder next to `game.exe` on build.

## Folder layout

| Folder | Purpose |
|--------|---------|
| `backgrounds/` | Panel and full-screen menu backgrounds |
| `buttons/` | Button skins for each interaction state |
| `frames/` | Optional borders and title bars |
| `fonts/` | TTF fonts for menus and in-game HUD (see below) |
| `icons/` | Optional icons (settings, close, etc.) |

## Fonts

Bundled fonts (SIL Open Font License — see `fonts/LICENSE.txt`):

| File | Engine role | Use for |
|------|-------------|---------|
| `fonts/Inter-Regular.ttf` | `UiRegular` | Menu body, default labels |
| `fonts/Inter-Bold.ttf` | `UiBold`, `Hud` | Titles, buttons, HUD headings |
| `fonts/JetBrainsMono-Regular.ttf` | `Mono` | Debug text, counters, coordinates |

Sizes are set in `MenuStyle.metrics` (`fontSize`, `monoFontSize`, `hudFontSize`).

Re-download fonts:

```powershell
python menu\tools\download_fonts.py
```

In code:

```cpp
#include "ui/MenuAssets.h"
#include "ui/FontLibrary.h"

Game::UI::MenuAssets::InterRegular();
uiSystem->GetFontLibrary().GetFont(Engine::Platform::UI::FontRole::Hud);
```

For in-game HUD text, use `Game::UI::HudText` or Nuklear `nk_push_font` with `FontRole::Hud` / `Mono`.

## Button states

| File | When to use |
|------|-------------|
| `buttons/normal.png` | Default, unclicked |
| `buttons/hover.png` | Mouse over the button |
| `buttons/active.png` | Mouse held down / clicked |
| `buttons/disabled.png` | Button cannot be used |

## Backgrounds

| File | When to use |
|------|-------------|
| `backgrounds/panel.png` | Main menu, settings, inventory panels |
| `backgrounds/overlay_dim.png` | Dark layer behind pause / modal UI |

## Paths in code

Use `Game::UI::MenuAssets` in `src/ui/MenuAssets.h` — paths match `manifest.json`.

Example:

```cpp
#include "ui/MenuAssets.h"

const char* bg = Game::UI::MenuAssets::PanelBackground();
const char* btn = Game::UI::MenuAssets::ButtonNormal();
```

## Replacing placeholders

1. Keep the same filename (e.g. swap in your own `normal.png`).
2. Prefer PNG with alpha for rounded buttons and panels.
3. Rebuild or copy the `menu/` folder to `build/bin/Debug/menu/`.
4. Suggested sizes: panel 512×512+, buttons 256×64 (or 9-slice-friendly power-of-two).

## Manifest

`manifest.json` lists every slot and its runtime path. Edit it if you add new asset categories.
