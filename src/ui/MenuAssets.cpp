#include "ui/MenuAssets.h"

#include <fstream>

namespace Game {
namespace UI {

bool MenuAssets::Exists(const char* relativePath) {
    if (!relativePath || relativePath[0] == '\0') {
        return false;
    }
    std::ifstream file(relativePath, std::ios::binary);
    return file.good();
}

MenuAssets::ButtonStates MenuAssets::DefaultButtons() {
    return ButtonStates{
        ButtonNormal(),
        ButtonHover(),
        ButtonActive(),
        ButtonDisabled(),
    };
}

} // namespace UI
} // namespace Game
