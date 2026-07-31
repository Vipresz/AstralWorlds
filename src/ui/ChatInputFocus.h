#pragma once

namespace Game {
namespace UI {

// Returns true on the frame Ctrl+L is first pressed (edge-triggered).
inline bool IsChatFocusChord(bool ctrlDown, bool lKeyDown, bool lWasDown) {
    return ctrlDown && lKeyDown && !lWasDown;
}

} // namespace UI
} // namespace Game
