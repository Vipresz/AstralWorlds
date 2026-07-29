#include "Menu.h"

#include "Logger.h"
#include "character/CharacterPaperdollRenderer.h"
#include "systems/inventory/InventoryBagLog.h"
#include "render/Shader.h"
#include "render/Window.h"
#include "systems/inventory/InventoryComponent.h"
#include "ui/OverlayWindow.h"
#include "ui/ItemTooltip.h"

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_FONT_BAKING
#include "nuklear.h"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <initializer_list>

using EngineLogging::Logger;

namespace Game {
namespace UI {

namespace {

std::string ReadTextFile(const char* path) {
    std::ifstream file(path);
    if (!file) {
        return {};
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

const char* SkipJsonWhitespace(const char* cursor) {
    while (cursor && *cursor && std::isspace(static_cast<unsigned char>(*cursor))) {
        ++cursor;
    }
    return cursor;
}

bool JsonTryGetFloat(const std::string& json, const char* key, float& out) {
    const std::string needle = std::string("\"") + key + "\"";
    const size_t keyPos = json.find(needle);
    if (keyPos == std::string::npos) {
        return false;
    }

    const size_t colonPos = json.find(':', keyPos + needle.size());
    if (colonPos == std::string::npos) {
        return false;
    }

    const char* start = SkipJsonWhitespace(json.c_str() + colonPos + 1);
    char* end = nullptr;
    const float value = std::strtof(start, &end);
    if (end == start) {
        return false;
    }

    out = value;
    return true;
}

bool JsonTryGetString(const std::string& json, const char* key, std::string& out) {
    const std::string needle = std::string("\"") + key + "\"";
    const size_t keyPos = json.find(needle);
    if (keyPos == std::string::npos) {
        return false;
    }

    const size_t colonPos = json.find(':', keyPos + needle.size());
    if (colonPos == std::string::npos) {
        return false;
    }

    const size_t quoteStart = json.find('"', colonPos + 1);
    if (quoteStart == std::string::npos) {
        return false;
    }

    const size_t quoteEnd = json.find('"', quoteStart + 1);
    if (quoteEnd == std::string::npos || quoteEnd <= quoteStart + 1) {
        return false;
    }

    out.assign(json, quoteStart + 1, quoteEnd - quoteStart - 1);
    return true;
}

bool JsonTryParseFractionString(const std::string& value, float& out) {
    const size_t slash = value.find('/');
    if (slash == std::string::npos) {
        return false;
    }

    char* end = nullptr;
    const float numerator = std::strtof(value.c_str(), &end);
    if (end == value.c_str() || end != value.c_str() + static_cast<std::ptrdiff_t>(slash)) {
        return false;
    }

    const float denominator = std::strtof(value.c_str() + slash + 1, &end);
    if (denominator <= 0.0f) {
        return false;
    }

    out = numerator / denominator;
    return true;
}

bool JsonTryGetFraction(const std::string& json, const char* key, float& out) {
    if (JsonTryGetFloat(json, key, out)) {
        return true;
    }

    std::string fractionText;
    if (!JsonTryGetString(json, key, fractionText)) {
        return false;
    }

    return JsonTryParseFractionString(fractionText, out);
}

bool JsonTryGetFractionAlias(const std::string& json,
                             std::initializer_list<const char*> keys,
                             float& out) {
    for (const char* key : keys) {
        if (JsonTryGetFraction(json, key, out)) {
            return true;
        }
    }
    return false;
}

bool JsonTryGetInt(const std::string& json, const char* key, int& out) {
    float value = 0.0f;
    if (!JsonTryGetFloat(json, key, value)) {
        return false;
    }
    out = static_cast<int>(value);
    return true;
}

std::string JsonExtractObject(const std::string& json, const char* objectKey) {
    const std::string needle = std::string("\"") + objectKey + "\"";
    const size_t keyPos = json.find(needle);
    if (keyPos == std::string::npos) {
        return {};
    }

    const size_t braceStart = json.find('{', keyPos);
    if (braceStart == std::string::npos) {
        return {};
    }

    int depth = 0;
    for (size_t i = braceStart; i < json.size(); ++i) {
        if (json[i] == '{') {
            ++depth;
        } else if (json[i] == '}') {
            --depth;
            if (depth == 0) {
                return json.substr(braceStart, i - braceStart + 1);
            }
        }
    }

    return {};
}

bool JsonTryGetStringInObject(const std::string& json,
                              const char* objectKey,
                              const char* fieldKey,
                              std::string& out) {
    const std::string objectJson = JsonExtractObject(json, objectKey);
    if (objectJson.empty()) {
        return false;
    }
    return JsonTryGetString(objectJson, fieldKey, out);
}

bool JsonTryGetStringArray(const std::string& objectJson, const char* key, std::vector<std::string>& out) {
    const std::string needle = std::string("\"") + key + "\"";
    const size_t keyPos = objectJson.find(needle);
    if (keyPos == std::string::npos) {
        return false;
    }

    const size_t bracketStart = objectJson.find('[', keyPos);
    if (bracketStart == std::string::npos) {
        return false;
    }

    size_t cursor = bracketStart + 1;
    bool found = false;
    while (cursor < objectJson.size()) {
        cursor = objectJson.find('"', cursor);
        if (cursor == std::string::npos) {
            break;
        }

        const size_t quoteEnd = objectJson.find('"', cursor + 1);
        if (quoteEnd == std::string::npos) {
            break;
        }

        out.emplace_back(objectJson, cursor + 1, quoteEnd - cursor - 1);
        found = true;
        cursor = quoteEnd + 1;

        const size_t nextBracket = objectJson.find(']', cursor);
        const size_t nextComma = objectJson.find(',', cursor);
        if (nextBracket != std::string::npos &&
            (nextComma == std::string::npos || nextBracket < nextComma)) {
            break;
        }
    }

    return found;
}

void AssignBarButtonLayout(CharacterMenuConfig& config) {
    const size_t buttonCount = config.barButtons.size();
    if (buttonCount == 0) {
        return;
    }

    for (size_t index = 0; index < buttonCount; ++index) {
        if (config.barButtons[index].xFraction < 0.0f) {
            config.barButtons[index].xFraction =
                config.barButtonFirstXFraction +
                static_cast<float>(index) * config.barButtonXSpacingFraction;
        }
    }
}

std::vector<std::string> JsonExtractArrayObjects(const std::string& json, const char* arrayKey) {
    std::vector<std::string> objects;
    const std::string needle = std::string("\"") + arrayKey + "\"";
    const size_t keyPos = json.find(needle);
    if (keyPos == std::string::npos) {
        return objects;
    }

    const size_t bracketStart = json.find('[', keyPos);
    if (bracketStart == std::string::npos) {
        return objects;
    }

    size_t cursor = bracketStart + 1;
    while (cursor < json.size()) {
        while (cursor < json.size() && json[cursor] != '{' && json[cursor] != ']') {
            ++cursor;
        }
        if (cursor >= json.size() || json[cursor] == ']') {
            break;
        }

        const size_t objectStart = cursor;
        int depth = 0;
        for (; cursor < json.size(); ++cursor) {
            if (json[cursor] == '{') {
                ++depth;
            } else if (json[cursor] == '}') {
                --depth;
                if (depth == 0) {
                    objects.push_back(json.substr(objectStart, cursor - objectStart + 1));
                    ++cursor;
                    break;
                }
            }
        }
    }

    return objects;
}

std::string JoinPath(const char* root, const std::string& relative) {
    if (relative.empty()) {
        return root ? root : "";
    }
    if (!root || root[0] == '\0') {
        return relative;
    }

    std::string joined = root;
    if (joined.back() != '/' && joined.back() != '\\') {
        joined.push_back('/');
    }
    if (!relative.empty() && (relative.front() == '/' || relative.front() == '\\')) {
        joined += relative.substr(1);
    } else {
        joined += relative;
    }
    return joined;
}

bool LoadBarButtonFromObject(const std::string& objectJson,
                             const char* root,
                             BarButtonConfig& out) {
    JsonTryGetString(objectJson, "id", out.id);

    std::string darkRelative;
    std::string lightRelative;
    const std::string texturesJson = JsonExtractObject(objectJson, "textures");
    if (!texturesJson.empty()) {
        JsonTryGetString(texturesJson, "dark", darkRelative);
        JsonTryGetString(texturesJson, "light", lightRelative);
    } else {
        JsonTryGetString(objectJson, "dark", darkRelative);
        JsonTryGetString(objectJson, "light", lightRelative);
    }

    if (darkRelative.empty() || lightRelative.empty()) {
        return false;
    }

    out.darkTexturePath = JoinPath(root, darkRelative);
    out.lightTexturePath = JoinPath(root, lightRelative);

    JsonTryGetFractionAlias(
        objectJson,
        {"icon_height_fraction", "height_fraction", "icon_size_fraction"},
        out.heightFraction);
    JsonTryGetFloat(objectJson, "y_fraction", out.yFraction);
    JsonTryGetFloat(objectJson, "x_fraction", out.xFraction);

    return true;
}

bool LoadCloseButtonPlacementFromObject(const std::string& objectJson,
                                        float& sizeFraction,
                                        float& xAnchorFraction,
                                        float& yAnchorFraction,
                                        float& marginFraction,
                                        bool& centerOnCorner) {
    bool found = false;
    if (JsonTryGetFloat(objectJson, "size_fraction", sizeFraction)) {
        found = true;
    }
    if (JsonTryGetFloat(objectJson, "x_anchor_fraction", xAnchorFraction)) {
        found = true;
    }
    if (JsonTryGetFloat(objectJson, "y_anchor_fraction", yAnchorFraction)) {
        found = true;
    }
    if (JsonTryGetFloat(objectJson, "margin_fraction", marginFraction)) {
        found = true;
    }
    std::string placement;
    if (JsonTryGetString(objectJson, "placement", placement)) {
        centerOnCorner = placement == "center_on_corner";
        found = true;
    }
    return found;
}

bool LoadOverlayPanelFromObject(const std::string& objectJson,
                              const char* root,
                              OverlayPanelConfig& out) {
    JsonTryGetString(objectJson, "button_id", out.buttonId);
    if (out.buttonId.empty()) {
        JsonTryGetString(objectJson, "id", out.buttonId);
    }

    std::string textureRelative;
    if (!JsonTryGetString(objectJson, "texture", textureRelative)) {
        return false;
    }

    out.texturePath = JoinPath(root, textureRelative);
    JsonTryGetFloat(objectJson, "width_fraction", out.widthFraction);
    JsonTryGetFloat(objectJson, "x_fraction", out.xFraction);
    JsonTryGetFloat(objectJson, "y_fraction", out.yFraction);

    const std::string closeButtonJson = JsonExtractObject(objectJson, "close_button");
    if (!closeButtonJson.empty()) {
        out.hasCloseButtonPlacement = LoadCloseButtonPlacementFromObject(
            closeButtonJson,
            out.closeSizeFraction,
            out.closeXAnchorFraction,
            out.closeYAnchorFraction,
            out.closeMarginFraction,
            out.closeCenterOnCorner);
    }

    const std::string characterPreviewJson = JsonExtractObject(objectJson, "character_preview");
    if (!characterPreviewJson.empty()) {
        out.hasCharacterPreview = true;
        JsonTryGetFloat(characterPreviewJson, "size_fraction", out.characterPreviewSizeFraction);
        JsonTryGetFloat(characterPreviewJson, "x_fraction", out.characterPreviewXFraction);
        JsonTryGetFloat(characterPreviewJson, "y_fraction", out.characterPreviewYFraction);
    } else if (out.buttonId == "char") {
        out.hasCharacterPreview = true;
    }

    const std::string itemGridJson = JsonExtractObject(objectJson, "item_grid");
    if (!itemGridJson.empty()) {
        out.hasItemGrid = true;
        std::string slotTextureRelative;
        if (JsonTryGetString(itemGridJson, "slot_texture", slotTextureRelative)) {
            out.itemGrid.slotTexturePath = JoinPath(root, slotTextureRelative);
        }
        JsonTryGetInt(itemGridJson, "columns", out.itemGrid.columns);
        JsonTryGetInt(itemGridJson, "rows", out.itemGrid.rows);
        JsonTryGetFloat(itemGridJson, "width_fraction", out.itemGrid.widthFraction);
        JsonTryGetFloat(itemGridJson, "height_fraction", out.itemGrid.heightFraction);
        JsonTryGetFloat(itemGridJson, "gap_x_fraction", out.itemGrid.gapXFraction);
        JsonTryGetFloat(itemGridJson, "gap_y_fraction", out.itemGrid.gapYFraction);
        JsonTryGetFloat(itemGridJson, "slot_alpha", out.itemGrid.slotAlpha);
        JsonTryGetFloat(itemGridJson, "drop_snap_fraction", out.itemGrid.dropSnapFraction);
        out.dragFromEdgesOnly = true;
    }

    JsonTryGetFloat(objectJson, "drag_edge_fraction", out.dragEdgeFraction);
    std::string dragFromEdgesOnly;
    if (JsonTryGetString(objectJson, "drag_from_edges_only", dragFromEdgesOnly)) {
        out.dragFromEdgesOnly =
            dragFromEdgesOnly == "true" || dragFromEdgesOnly == "1" || dragFromEdgesOnly == "yes";
    }

    return !out.buttonId.empty();
}

} // namespace

bool CharacterMenu::LoadConfig(CharacterMenuConfig& out, const char* manifestPath) {
    const char* path = manifestPath ? manifestPath : CharacterMenuConfig::kManifestPath;
    const std::string json = ReadTextFile(path);
    if (json.empty()) {
        LOG_WARNING_FMT("CharacterMenu", "Manifest not found at %s — using defaults", path);
        return false;
    }

    std::string bottomBarRelative;
    if (JsonTryGetStringInObject(json, "bottom_bar", "texture", bottomBarRelative)) {
        out.bottomBarTexturePath = JoinPath(CharacterMenuConfig::kRoot, bottomBarRelative);
    }

    const std::string bottomBarJson = JsonExtractObject(json, "bottom_bar");
    if (!bottomBarJson.empty()) {
        JsonTryGetFloat(bottomBarJson, "width_fraction", out.bottomBarWidthFraction);
    }

    const std::string barButtonsJson = JsonExtractObject(json, "bar_buttons");
    if (!barButtonsJson.empty()) {
        JsonTryGetFractionAlias(
            barButtonsJson,
            {"icon_height_fraction", "height_fraction", "icon_size_fraction"},
            out.barButtonHeightFraction);
        JsonTryGetFloat(barButtonsJson, "x_spacing_fraction", out.barButtonXSpacingFraction);
        JsonTryGetFloat(barButtonsJson, "first_x_fraction", out.barButtonFirstXFraction);
        JsonTryGetFloat(barButtonsJson, "y_fraction", out.barButtonYFraction);

        out.barButtons.clear();
        for (const std::string& buttonJson : JsonExtractArrayObjects(barButtonsJson, "buttons")) {
            BarButtonConfig button;
            if (!LoadBarButtonFromObject(buttonJson, CharacterMenuConfig::kRoot, button)) {
                continue;
            }
            out.barButtons.push_back(std::move(button));
        }
        AssignBarButtonLayout(out);
    } else {
        const std::string barIconsJson = JsonExtractObject(json, "bar_icons");
        if (!barIconsJson.empty()) {
            JsonTryGetFractionAlias(
                barIconsJson,
                {"icon_height_fraction", "height_fraction", "icon_size_fraction"},
                out.barButtonHeightFraction);
            JsonTryGetFloat(barIconsJson, "x_spacing_fraction", out.barButtonXSpacingFraction);
            JsonTryGetFloat(barIconsJson, "first_x_fraction", out.barButtonFirstXFraction);
            JsonTryGetFloat(barIconsJson, "y_fraction", out.barButtonYFraction);

            std::vector<std::string> iconPaths;
            if (JsonTryGetStringArray(barIconsJson, "icons", iconPaths)) {
                out.barButtons.clear();
                out.barButtons.reserve(iconPaths.size());
                for (const std::string& relativePath : iconPaths) {
                    BarButtonConfig button;
                    button.darkTexturePath = JoinPath(CharacterMenuConfig::kRoot, relativePath);
                    button.lightTexturePath = button.darkTexturePath;
                    out.barButtons.push_back(std::move(button));
                }
                AssignBarButtonLayout(out);
            }
        } else {
            std::string characterButtonRelative;
            if (JsonTryGetStringInObject(json, "character_button", "texture", characterButtonRelative)) {
                BarButtonConfig button;
                button.id = "character";
                button.darkTexturePath = JoinPath(CharacterMenuConfig::kRoot, characterButtonRelative);
                button.lightTexturePath = button.darkTexturePath;
                const std::string characterButtonJson = JsonExtractObject(json, "character_button");
                if (!characterButtonJson.empty()) {
                    JsonTryGetFloat(characterButtonJson, "x_fraction", out.barButtonFirstXFraction);
                    JsonTryGetFloat(characterButtonJson, "y_fraction", out.barButtonYFraction);
                    JsonTryGetFractionAlias(
                        characterButtonJson,
                        {"icon_height_fraction", "height_fraction", "icon_size_fraction"},
                        out.barButtonHeightFraction);
                }
                button.xFraction = out.barButtonFirstXFraction;
                button.yFraction = out.barButtonYFraction;
                out.barButtons = {button};
            }
        }
    }

    const std::string screenJson = JsonExtractObject(json, "screen");
    if (!screenJson.empty()) {
        JsonTryGetInt(screenJson, "min_width", out.minScreenWidth);
        JsonTryGetInt(screenJson, "min_height", out.minScreenHeight);
    }

    const std::string minimapJson = JsonExtractObject(json, "minimap");
    if (!minimapJson.empty()) {
        std::string borderRelative;
        if (JsonTryGetString(minimapJson, "border_texture", borderRelative)) {
            out.minimap.borderTexturePath = JoinPath(CharacterMenuConfig::kRoot, borderRelative);
        }
        JsonTryGetFloat(minimapJson, "size_pixels", out.minimap.sizePixels);
        JsonTryGetFloat(minimapJson, "margin_pixels", out.minimap.marginPixels);
        JsonTryGetFloat(minimapJson, "border_inset_fraction", out.minimap.borderInsetFraction);
    }

    const std::string characterHudJson = JsonExtractObject(json, "character_hud");
    if (!characterHudJson.empty()) {
        std::string hudRelative;
        if (JsonTryGetString(characterHudJson, "texture", hudRelative)) {
            out.characterHud.texturePath = JoinPath(CharacterMenuConfig::kRoot, hudRelative);
        }
        JsonTryGetInt(characterHudJson, "source_width", out.characterHud.sourceWidth);
        JsonTryGetInt(characterHudJson, "source_height", out.characterHud.sourceHeight);
        JsonTryGetFloat(characterHudJson, "x_fraction", out.characterHud.xFraction);
        JsonTryGetFloat(characterHudJson, "y_fraction", out.characterHud.yFraction);
        JsonTryGetString(characterHudJson, "anchor", out.characterHud.anchor);

        out.characterHud.bars.clear();
        for (const std::string& barJson : JsonExtractArrayObjects(characterHudJson, "bars")) {
            CharacterHudBarConfig bar;
            JsonTryGetString(barJson, "id", bar.id);
            JsonTryGetFloat(barJson, "x_fraction", bar.xFraction);
            JsonTryGetFloat(barJson, "y_fraction", bar.yFraction);
            JsonTryGetFloat(barJson, "width_fraction", bar.widthFraction);
            JsonTryGetFloat(barJson, "height_fraction", bar.heightFraction);
            if (!bar.id.empty()) {
                out.characterHud.bars.push_back(std::move(bar));
            }
        }
    }

    const std::string chatJson = JsonExtractObject(json, "chat");
    if (!chatJson.empty()) {
        std::string chatRelative;
        if (JsonTryGetString(chatJson, "texture", chatRelative)) {
            out.chat.texturePath = JoinPath(CharacterMenuConfig::kRoot, chatRelative);
        }
        JsonTryGetInt(chatJson, "width", out.chat.width);
        JsonTryGetInt(chatJson, "height", out.chat.height);
        JsonTryGetFloat(chatJson, "x_fraction", out.chat.xFraction);
        JsonTryGetFloat(chatJson, "y_fraction", out.chat.yFraction);
        JsonTryGetString(chatJson, "anchor", out.chat.anchor);
        JsonTryGetFloat(chatJson, "padding_pixels", out.chat.paddingPixels);
        JsonTryGetFloat(chatJson, "input_row_height", out.chat.inputRowHeight);
        JsonTryGetFloat(chatJson, "log_row_height", out.chat.logRowHeight);
        JsonTryGetFloat(chatJson, "scroll_lines_per_wheel", out.chat.scrollLinesPerWheel);
        JsonTryGetFloat(chatJson, "idle_alpha", out.chat.idleAlpha);
        const std::string contentJson = JsonExtractObject(chatJson, "content");
        if (!contentJson.empty()) {
            JsonTryGetFloat(contentJson, "x_fraction", out.chat.content.xFraction);
            JsonTryGetFloat(contentJson, "y_fraction", out.chat.content.yFraction);
            JsonTryGetFloat(contentJson, "width_fraction", out.chat.content.widthFraction);
            JsonTryGetFloat(contentJson, "height_fraction", out.chat.content.heightFraction);
            JsonTryGetFloat(contentJson, "input_height_fraction", out.chat.content.inputHeightFraction);
        }
    }

    const std::string paperdollJson = JsonExtractObject(json, "paperdoll");
    out.panels.clear();
    for (const std::string& panelJson : JsonExtractArrayObjects(json, "panels")) {
        OverlayPanelConfig panel;
        if (LoadOverlayPanelFromObject(panelJson, CharacterMenuConfig::kRoot, panel)) {
            out.panels.push_back(std::move(panel));
        }
    }
    if (out.panels.empty() && !paperdollJson.empty()) {
        OverlayPanelConfig panel;
        panel.buttonId = "char";
        std::string paperdollRelative;
        if (JsonTryGetString(paperdollJson, "texture", paperdollRelative)) {
            panel.texturePath = JoinPath(CharacterMenuConfig::kRoot, paperdollRelative);
        }
        JsonTryGetFloat(paperdollJson, "width_fraction", panel.widthFraction);
        JsonTryGetFloat(paperdollJson, "x_fraction", panel.xFraction);
        JsonTryGetFloat(paperdollJson, "y_fraction", panel.yFraction);
        if (!panel.texturePath.empty()) {
            out.panels.push_back(std::move(panel));
        }
    }

    const std::string closeButtonJson = JsonExtractObject(json, "close_button");
    if (!closeButtonJson.empty()) {
        const std::string texturesJson = JsonExtractObject(closeButtonJson, "textures");
        std::string darkRelative;
        std::string lightRelative;
        if (!texturesJson.empty()) {
            JsonTryGetString(texturesJson, "dark", darkRelative);
            JsonTryGetString(texturesJson, "light", lightRelative);
        } else {
            JsonTryGetString(closeButtonJson, "dark", darkRelative);
            JsonTryGetString(closeButtonJson, "light", lightRelative);
        }
        if (!darkRelative.empty()) {
            out.closeButton.darkTexturePath = JoinPath(CharacterMenuConfig::kRoot, darkRelative);
        }
        if (!lightRelative.empty()) {
            out.closeButton.lightTexturePath = JoinPath(CharacterMenuConfig::kRoot, lightRelative);
        }
        LoadCloseButtonPlacementFromObject(
            closeButtonJson,
            out.closeButton.sizeFraction,
            out.closeButton.xAnchorFraction,
            out.closeButton.yAnchorFraction,
            out.closeButton.marginFraction,
            out.closeButton.centerOnCorner);
    }

    const std::string paperdollRotateJson = JsonExtractObject(json, "paperdoll_rotate");
    if (!paperdollRotateJson.empty()) {
        JsonTryGetFloat(
            paperdollRotateJson, "y_from_bottom_fraction", out.paperdollRotate.yFromBottomFraction);
        JsonTryGetFloat(paperdollRotateJson, "size_fraction", out.paperdollRotate.sizeFraction);
        JsonTryGetFloat(paperdollRotateJson, "aspect_ratio", out.paperdollRotate.aspectRatio);
        // Prefer degrees_per_second; accept degrees_per_click as a legacy alias.
        if (!JsonTryGetFloat(
                paperdollRotateJson, "degrees_per_second", out.paperdollRotate.degreesPerSecond)) {
            JsonTryGetFloat(
                paperdollRotateJson, "degrees_per_click", out.paperdollRotate.degreesPerSecond);
        }

        // Backward-compatible alias: side_inset_fraction mirrors left/right from the edges.
        float sideInset = -1.0f;
        if (JsonTryGetFloat(paperdollRotateJson, "side_inset_fraction", sideInset) &&
            sideInset >= 0.0f) {
            out.paperdollRotate.leftXFraction = sideInset;
            out.paperdollRotate.rightXFraction = 1.0f - sideInset;
        }

        const std::string leftJson = JsonExtractObject(paperdollRotateJson, "left");
        if (!leftJson.empty()) {
            JsonTryGetFloat(leftJson, "x_fraction", out.paperdollRotate.leftXFraction);
            const std::string leftTextures = JsonExtractObject(leftJson, "textures");
            std::string darkRelative;
            std::string lightRelative;
            if (!leftTextures.empty()) {
                JsonTryGetString(leftTextures, "dark", darkRelative);
                JsonTryGetString(leftTextures, "light", lightRelative);
            } else {
                JsonTryGetString(leftJson, "dark", darkRelative);
                JsonTryGetString(leftJson, "light", lightRelative);
            }
            if (!darkRelative.empty()) {
                out.paperdollRotate.leftTexturePath =
                    JoinPath(CharacterMenuConfig::kRoot, darkRelative);
            }
            if (!lightRelative.empty()) {
                out.paperdollRotate.leftLightTexturePath =
                    JoinPath(CharacterMenuConfig::kRoot, lightRelative);
            }
        }

        const std::string rightJson = JsonExtractObject(paperdollRotateJson, "right");
        if (!rightJson.empty()) {
            JsonTryGetFloat(rightJson, "x_fraction", out.paperdollRotate.rightXFraction);
            const std::string rightTextures = JsonExtractObject(rightJson, "textures");
            std::string darkRelative;
            std::string lightRelative;
            if (!rightTextures.empty()) {
                JsonTryGetString(rightTextures, "dark", darkRelative);
                JsonTryGetString(rightTextures, "light", lightRelative);
            } else {
                JsonTryGetString(rightJson, "dark", darkRelative);
                JsonTryGetString(rightJson, "light", lightRelative);
            }
            if (!darkRelative.empty()) {
                out.paperdollRotate.rightTexturePath =
                    JoinPath(CharacterMenuConfig::kRoot, darkRelative);
            }
            if (!lightRelative.empty()) {
                out.paperdollRotate.rightLightTexturePath =
                    JoinPath(CharacterMenuConfig::kRoot, lightRelative);
            }
        }
    }

    LOG_INFO_FMT("CharacterMenu", "Manifest loaded: bottom_bar=%s", out.bottomBarTexturePath.c_str());
    return true;
}

bool CharacterMenu::Initialize(const CharacterMenuConfig* config) {
    m_config = config ? *config : CharacterMenuConfig{};
    if (!config) {
        LoadConfig(m_config);
    }

    m_bottomBarTexture =
        std::make_unique<Engine::Platform::Render::Texture>(m_config.bottomBarTexturePath.c_str());
    if (!m_bottomBarTexture->LoadTextureUI(1024)) {
        LOG_ERROR_FMT("CharacterMenu", "Failed to load %s", m_config.bottomBarTexturePath.c_str());
        m_bottomBarTexture.reset();
        return false;
    }

    LOG_INFO("CharacterMenu", "Character menu bottom bar loaded");

    m_overlayPanels.clear();
    m_overlayPanels.reserve(m_config.panels.size());
    for (const OverlayPanelConfig& panelConfig : m_config.panels) {
        LoadedOverlayPanel loaded;
        loaded.config = panelConfig;
        loaded.texture =
            std::make_unique<Engine::Platform::Render::Texture>(panelConfig.texturePath.c_str());
        if (!loaded.texture->LoadTextureUI(1024)) {
            LOG_WARNING_FMT("CharacterMenu",
                            "Overlay panel not loaded for '%s' (%s)",
                            panelConfig.buttonId.c_str(),
                            panelConfig.texturePath.c_str());
            continue;
        }
        if (panelConfig.hasItemGrid && !panelConfig.itemGrid.slotTexturePath.empty()) {
            loaded.slotTexture = std::make_unique<Engine::Platform::Render::Texture>(
                panelConfig.itemGrid.slotTexturePath.c_str());
            if (!loaded.slotTexture->LoadTextureUI(512)) {
                LOG_WARNING_FMT("CharacterMenu",
                                "Bag slot texture not loaded for '%s' (%s)",
                                panelConfig.buttonId.c_str(),
                                panelConfig.itemGrid.slotTexturePath.c_str());
                loaded.slotTexture.reset();
            }
        }
        m_overlayPanels.push_back(std::move(loaded));
    }

    m_closeDarkTexture =
        std::make_unique<Engine::Platform::Render::Texture>(m_config.closeButton.darkTexturePath.c_str());
    if (!m_closeDarkTexture->LoadTextureUI(128)) {
        LOG_WARNING_FMT("CharacterMenu",
                        "Close button dark texture not loaded (%s)",
                        m_config.closeButton.darkTexturePath.c_str());
        m_closeDarkTexture.reset();
    }

    m_closeLightTexture =
        std::make_unique<Engine::Platform::Render::Texture>(m_config.closeButton.lightTexturePath.c_str());
    if (!m_closeLightTexture->LoadTextureUI(128)) {
        LOG_WARNING_FMT("CharacterMenu",
                        "Close button light texture not loaded (%s)",
                        m_config.closeButton.lightTexturePath.c_str());
        m_closeLightTexture.reset();
    }

    m_paperdollRotateLeftTexture = std::make_unique<Engine::Platform::Render::Texture>(
        m_config.paperdollRotate.leftTexturePath.c_str());
    if (!m_paperdollRotateLeftTexture->LoadTextureUI(256)) {
        LOG_WARNING_FMT("CharacterMenu",
                        "Paperdoll rotate-left texture not loaded (%s)",
                        m_config.paperdollRotate.leftTexturePath.c_str());
        m_paperdollRotateLeftTexture.reset();
    }

    m_paperdollRotateRightTexture = std::make_unique<Engine::Platform::Render::Texture>(
        m_config.paperdollRotate.rightTexturePath.c_str());
    if (!m_paperdollRotateRightTexture->LoadTextureUI(256)) {
        LOG_WARNING_FMT("CharacterMenu",
                        "Paperdoll rotate-right texture not loaded (%s)",
                        m_config.paperdollRotate.rightTexturePath.c_str());
        m_paperdollRotateRightTexture.reset();
    }

    m_paperdollRotateLeftLightTexture = std::make_unique<Engine::Platform::Render::Texture>(
        m_config.paperdollRotate.leftLightTexturePath.c_str());
    if (!m_paperdollRotateLeftLightTexture->LoadTextureUI(256)) {
        LOG_WARNING_FMT("CharacterMenu",
                        "Paperdoll rotate-left light texture not loaded (%s)",
                        m_config.paperdollRotate.leftLightTexturePath.c_str());
        m_paperdollRotateLeftLightTexture.reset();
    }

    m_paperdollRotateRightLightTexture = std::make_unique<Engine::Platform::Render::Texture>(
        m_config.paperdollRotate.rightLightTexturePath.c_str());
    if (!m_paperdollRotateRightLightTexture->LoadTextureUI(256)) {
        LOG_WARNING_FMT("CharacterMenu",
                        "Paperdoll rotate-right light texture not loaded (%s)",
                        m_config.paperdollRotate.rightLightTexturePath.c_str());
        m_paperdollRotateRightLightTexture.reset();
    }

    m_barButtons.clear();
    m_barButtons.reserve(m_config.barButtons.size());
    for (const BarButtonConfig& buttonConfig : m_config.barButtons) {
        LoadedBarButton loaded;
        loaded.id = buttonConfig.id;
        loaded.xFraction = buttonConfig.xFraction;
        loaded.yFraction = buttonConfig.yFraction;
        loaded.heightFraction = buttonConfig.heightFraction;

        loaded.darkTexture = std::make_unique<Engine::Platform::Render::Texture>(
            buttonConfig.darkTexturePath.c_str());
        if (!loaded.darkTexture->LoadTextureUI(256)) {
            LOG_WARNING_FMT("CharacterMenu",
                            "Bar button dark icon not loaded (%s)",
                            buttonConfig.darkTexturePath.c_str());
            loaded.darkTexture.reset();
        }

        loaded.lightTexture = std::make_unique<Engine::Platform::Render::Texture>(
            buttonConfig.lightTexturePath.c_str());
        if (!loaded.lightTexture->LoadTextureUI(256)) {
            LOG_WARNING_FMT("CharacterMenu",
                            "Bar button light icon not loaded (%s)",
                            buttonConfig.lightTexturePath.c_str());
            loaded.lightTexture.reset();
        }

        if (!loaded.darkTexture && !loaded.lightTexture) {
            continue;
        }

        m_barButtons.push_back(std::move(loaded));
    }

    return true;
}

void CharacterMenu::Shutdown() {
    if (m_bottomBarTexture) {
        m_bottomBarTexture->ClearTexture();
        m_bottomBarTexture.reset();
    }
    for (LoadedOverlayPanel& panel : m_overlayPanels) {
        if (panel.texture) {
            panel.texture->ClearTexture();
            panel.texture.reset();
        }
        if (panel.slotTexture) {
            panel.slotTexture->ClearTexture();
            panel.slotTexture.reset();
        }
    }
    m_overlayPanels.clear();
    if (m_closeDarkTexture) {
        m_closeDarkTexture->ClearTexture();
        m_closeDarkTexture.reset();
    }
    if (m_closeLightTexture) {
        m_closeLightTexture->ClearTexture();
        m_closeLightTexture.reset();
    }
    if (m_paperdollRotateLeftTexture) {
        m_paperdollRotateLeftTexture->ClearTexture();
        m_paperdollRotateLeftTexture.reset();
    }
    if (m_paperdollRotateRightTexture) {
        m_paperdollRotateRightTexture->ClearTexture();
        m_paperdollRotateRightTexture.reset();
    }
    if (m_paperdollRotateLeftLightTexture) {
        m_paperdollRotateLeftLightTexture->ClearTexture();
        m_paperdollRotateLeftLightTexture.reset();
    }
    if (m_paperdollRotateRightLightTexture) {
        m_paperdollRotateRightLightTexture->ClearTexture();
        m_paperdollRotateRightLightTexture.reset();
    }
    for (LoadedBarButton& button : m_barButtons) {
        if (button.darkTexture) {
            button.darkTexture->ClearTexture();
            button.darkTexture.reset();
        }
        if (button.lightTexture) {
            button.lightTexture->ClearTexture();
            button.lightTexture.reset();
        }
    }
    m_barButtons.clear();
    for (auto& iconEntry : m_itemIconTextures) {
        if (iconEntry.second) {
            iconEntry.second->ClearTexture();
        }
    }
    m_itemIconTextures.clear();
    m_gameItemCatalog = nullptr;
    m_overlayPanels.clear();
    if (m_paperdollRenderer) {
        m_paperdollRenderer->Shutdown();
        m_paperdollRenderer.reset();
    }
    m_paperdollRegistry = nullptr;
    m_paperdollCharacterEntity = Engine::INVALID_ENTITY;
    m_paperdollShader = nullptr;
    m_paperdollCharacterInstance = nullptr;
    m_paperdollYawDegrees = 0.0f;
    m_hasPaperdollRotateClock = false;
}

namespace {

struct OverlayPanelLayout {
    float panelW = 0.0f;
    float panelH = 0.0f;
};

OverlayPanelLayout ComputeOverlayPanelLayout(const MenuScreenLayout& layout,
                                             int panelTexW,
                                             int panelTexH,
                                             float widthFraction) {
    OverlayPanelLayout result;
    if (panelTexW <= 0 || panelTexH <= 0) {
        return result;
    }

    const float clampedWidthFraction = std::clamp(widthFraction, 0.1f, 0.95f);
    result.panelW = layout.ScaledReferenceWidth(clampedWidthFraction);
    const float aspect = static_cast<float>(panelTexH) / static_cast<float>(panelTexW);
    result.panelH = result.panelW * aspect;

    const float maxPanelH = layout.ScaledReferenceHeight(0.82f);
    if (result.panelH > maxPanelH) {
        result.panelH = maxPanelH;
        result.panelW = result.panelH / aspect;
    }

    return result;
}

struct nk_rect PanelBoundsFromCenter(float centerX, float centerY, float panelW, float panelH) {
    return nk_rect(centerX - panelW * 0.5f, centerY - panelH * 0.5f, panelW, panelH);
}

bool IsMouseOnPanelDragEdge(const struct nk_rect& bounds,
                            float mouseX,
                            float mouseY,
                            float edgeFraction) {
    if (mouseX < bounds.x || mouseX > bounds.x + bounds.w || mouseY < bounds.y ||
        mouseY > bounds.y + bounds.h) {
        return false;
    }

    const float edgeThickness =
        std::min(bounds.w, bounds.h) * std::clamp(edgeFraction, 0.03f, 0.45f);
    const bool onLeft = mouseX - bounds.x < edgeThickness;
    const bool onRight = bounds.x + bounds.w - mouseX < edgeThickness;
    const bool onTop = mouseY - bounds.y < edgeThickness;
    const bool onBottom = bounds.y + bounds.h - mouseY < edgeThickness;
    return onLeft || onRight || onTop || onBottom;
}

struct nk_rect CharacterPreviewBoundsFromPanel(const struct nk_rect& panelBounds,
                                               const OverlayPanelConfig& config) {
    const float sizeFraction = std::clamp(config.characterPreviewSizeFraction, 0.1f, 1.0f);
    const float previewW = panelBounds.w * sizeFraction * 0.72f;
    const float previewH = panelBounds.h * sizeFraction * 0.88f;
    const float centerX =
        panelBounds.x + panelBounds.w * std::clamp(config.characterPreviewXFraction, 0.0f, 1.0f);
    const float centerY =
        panelBounds.y + panelBounds.h * std::clamp(config.characterPreviewYFraction, 0.0f, 1.0f);
    return nk_rect(centerX - previewW * 0.5f, centerY - previewH * 0.5f, previewW, previewH);
}

bool DrawCharacterPreviewOnPanel(struct nk_command_buffer* canvas,
                                 const struct nk_rect& previewBounds,
                                 Game::Character::CharacterPaperdollRenderer& renderer,
                                 Engine::ECS::Registry& registry,
                                 Engine::EntityID characterEntity,
                                 Game::Character::CharacterInstance& characterInstance,
                                 Engine::Platform::Render::Shader& shader) {
    if (!canvas || previewBounds.w <= 0.0f || previewBounds.h <= 0.0f) {
        return false;
    }

    const int pixelWidth = std::max(64, static_cast<int>(std::round(previewBounds.w)));
    const int pixelHeight = std::max(64, static_cast<int>(std::round(previewBounds.h)));
    if (!renderer.RenderToTexture(
            registry, characterEntity, characterInstance, shader, pixelWidth, pixelHeight)) {
        return false;
    }

    if (renderer.GetColorTexture() == 0) {
        return false;
    }

    const struct nk_image previewImage =
        nk_image_id(static_cast<int>(renderer.GetColorTexture()));
    nk_draw_image(canvas, previewBounds, &previewImage, nk_rgba(255, 255, 255, 255));
    return true;
}

void ClampPanelCenter(float& centerX,
                      float& centerY,
                      float panelW,
                      float panelH,
                      float screenWidth,
                      float screenHeight) {
    const float halfW = panelW * 0.5f;
    const float halfH = panelH * 0.5f;
    centerX = std::clamp(centerX, halfW, std::max(halfW, screenWidth - halfW));
    centerY = std::clamp(centerY, halfH, std::max(halfH, screenHeight - halfH));
}

struct nk_rect CloseButtonBoundsFromPanel(const struct nk_rect& panelBounds,
                                          const CloseButtonConfig& config) {
    const float clampedFraction = std::clamp(config.sizeFraction, 0.04f, 0.35f);
    const float closeSize = std::min(panelBounds.w, panelBounds.h) * clampedFraction;

    const float marginX = panelBounds.w * std::clamp(config.marginFraction, 0.0f, 0.25f);
    const float marginY = panelBounds.h * std::clamp(config.marginFraction, 0.0f, 0.25f);

    const float anchorX = panelBounds.x +
                          panelBounds.w * std::clamp(config.xAnchorFraction, 0.0f, 1.0f) - marginX;
    const float anchorY = panelBounds.y +
                          panelBounds.h * std::clamp(config.yAnchorFraction, 0.0f, 1.0f) + marginY;

    if (config.centerOnCorner) {
        return nk_rect(anchorX - closeSize * 0.5f, anchorY - closeSize * 0.5f, closeSize, closeSize);
    }

    return nk_rect(anchorX - closeSize, anchorY, closeSize, closeSize);
}

bool DrawPanelCloseButton(struct nk_context* ctx,
                          struct nk_command_buffer* canvas,
                          const struct nk_rect& closeBounds,
                          const Engine::Platform::Render::Texture* darkTexture,
                          const Engine::Platform::Render::Texture* lightTexture,
                          bool& pointerOverPanelUi) {
    if (!ctx || !canvas) {
        return false;
    }

    const bool hovering = nk_input_is_mouse_hovering_rect(&ctx->input, closeBounds);
    if (hovering) {
        pointerOverPanelUi = true;
    }

    const Engine::Platform::Render::Texture* texture = hovering
                                                             ? (lightTexture && lightTexture->IsLoaded()
                                                                    ? lightTexture
                                                                    : darkTexture)
                                                             : (darkTexture && darkTexture->IsLoaded()
                                                                    ? darkTexture
                                                                    : lightTexture);
    if (!texture || !texture->IsLoaded()) {
        return false;
    }

    Engine::Platform::UI::DrawTexture(
        canvas,
        *texture,
        closeBounds,
        nk_rgba(255, 255, 255, 255));

    return nk_input_is_mouse_click_in_rect(&ctx->input, NK_BUTTON_LEFT, closeBounds);
}

struct PaperdollRotateButtonBounds {
    struct nk_rect left{};
    struct nk_rect right{};
};

PaperdollRotateButtonBounds PaperdollRotateBoundsFromPanel(
    const struct nk_rect& panelBounds,
    const PaperdollRotateButtonConfig& config) {
    PaperdollRotateButtonBounds result;
    const float width =
        std::min(panelBounds.w, panelBounds.h) * std::clamp(config.sizeFraction, 0.04f, 0.25f);
    const float aspectRatio = std::max(0.05f, config.aspectRatio);
    const float height = width / aspectRatio;
    const float yFromBottom = std::clamp(config.yFromBottomFraction, 0.05f, 0.45f);
    const float leftX = std::clamp(config.leftXFraction, 0.0f, 1.0f);
    const float rightX = std::clamp(config.rightXFraction, 0.0f, 1.0f);
    const float centerY = panelBounds.y + panelBounds.h * (1.0f - yFromBottom);

    result.left = nk_rect(
        panelBounds.x + panelBounds.w * leftX - width * 0.5f,
        centerY - height * 0.5f,
        width,
        height);
    result.right = nk_rect(
        panelBounds.x + panelBounds.w * rightX - width * 0.5f,
        centerY - height * 0.5f,
        width,
        height);
    return result;
}

bool DrawPaperdollRotateButton(struct nk_context* ctx,
                               struct nk_command_buffer* canvas,
                               const struct nk_rect& bounds,
                               const Engine::Platform::Render::Texture* darkTexture,
                               const Engine::Platform::Render::Texture* lightTexture,
                               bool& pointerOverPanelUi) {
    if (!ctx || !canvas) {
        return false;
    }

    const bool hovering = nk_input_is_mouse_hovering_rect(&ctx->input, bounds);
    if (hovering) {
        pointerOverPanelUi = true;
    }

    const bool pressed =
        hovering && ctx->input.mouse.buttons[NK_BUTTON_LEFT].down;
    const Engine::Platform::Render::Texture* texture =
        pressed ? (lightTexture && lightTexture->IsLoaded() ? lightTexture : darkTexture)
                : (darkTexture && darkTexture->IsLoaded() ? darkTexture : lightTexture);
    if (!texture || !texture->IsLoaded()) {
        return false;
    }

    Engine::Platform::UI::DrawTexture(canvas, *texture, bounds, nk_rgba(255, 255, 255, 255));
    return pressed;
}

float NormalizeYawDegrees(float yaw) {
    while (yaw > 180.0f) {
        yaw -= 360.0f;
    }
    while (yaw < -180.0f) {
        yaw += 360.0f;
    }
    return yaw;
}

struct BagGridLayout {
    int columns = 0;
    int rows = 0;
    int slotCount = 0;
    float slotSize = 0.0f;
    struct nk_rect contentBounds{};
    std::vector<struct nk_rect> slotBounds;
};

BagGridLayout ComputeBagGridLayout(const struct nk_rect& panelBounds, const BagItemGridConfig& grid) {
    BagGridLayout layout;
    layout.columns = std::max(1, grid.columns);
    layout.rows = std::max(1, grid.rows);
    layout.slotCount = layout.columns * layout.rows;

    const float widthFraction = std::clamp(grid.widthFraction, 0.05f, 1.0f);
    const float heightFraction = std::clamp(grid.heightFraction, 0.05f, 1.0f);
    const float areaW = panelBounds.w * widthFraction;
    const float areaH = panelBounds.h * heightFraction;
    layout.contentBounds =
        nk_rect(panelBounds.x + (panelBounds.w - areaW) * 0.5f,
                panelBounds.y + (panelBounds.h - areaH) * 0.5f,
                areaW,
                areaH);

    const float gapX =
        layout.contentBounds.w * std::clamp(grid.gapXFraction, 0.0f, 0.2f);
    const float gapY =
        layout.contentBounds.h * std::clamp(grid.gapYFraction, 0.0f, 0.2f);
    layout.slotSize = std::max(
        8.0f,
        std::min((layout.contentBounds.w - gapX * static_cast<float>(layout.columns - 1)) /
                     static_cast<float>(layout.columns),
                 (layout.contentBounds.h - gapY * static_cast<float>(layout.rows - 1)) /
                     static_cast<float>(layout.rows)));
    const float gridW =
        static_cast<float>(layout.columns) * layout.slotSize +
        gapX * static_cast<float>(layout.columns - 1);
    const float gridH =
        static_cast<float>(layout.rows) * layout.slotSize +
        gapY * static_cast<float>(layout.rows - 1);
    const float gridOriginX = layout.contentBounds.x + (layout.contentBounds.w - gridW) * 0.5f;
    const float gridOriginY = layout.contentBounds.y + (layout.contentBounds.h - gridH) * 0.5f;

    layout.slotBounds.resize(static_cast<size_t>(layout.slotCount));
    for (int slotIndex = 0; slotIndex < layout.slotCount; ++slotIndex) {
        const int column = slotIndex % layout.columns;
        const int row = slotIndex / layout.columns;
        layout.slotBounds[static_cast<size_t>(slotIndex)] =
            nk_rect(gridOriginX + static_cast<float>(column) * (layout.slotSize + gapX),
                    gridOriginY + static_cast<float>(row) * (layout.slotSize + gapY),
                    layout.slotSize,
                    layout.slotSize);
    }

    return layout;
}

bool PointInRect(const struct nk_rect& rect, float x, float y) {
    return x >= rect.x && x <= rect.x + rect.w && y >= rect.y && y <= rect.y + rect.h;
}

int FindNearestBagSlot(float mouseX,
                       float mouseY,
                       const std::vector<struct nk_rect>& slotBounds,
                       float maxCenterDistance) {
    int bestSlot = -1;
    float bestDistance = maxCenterDistance;
    for (size_t slotIndex = 0; slotIndex < slotBounds.size(); ++slotIndex) {
        const struct nk_rect& bounds = slotBounds[slotIndex];
        const float centerX = bounds.x + bounds.w * 0.5f;
        const float centerY = bounds.y + bounds.h * 0.5f;
        const float dx = mouseX - centerX;
        const float dy = mouseY - centerY;
        const float distance = std::sqrt(dx * dx + dy * dy);
        if (distance <= bestDistance) {
            bestDistance = distance;
            bestSlot = static_cast<int>(slotIndex);
        }
    }
    return bestSlot;
}

bool InventoryIsEmpty(const Engine::Systems::Inventory::InventoryComponent* inventory) {
    if (!inventory) {
        return true;
    }
    for (const Engine::Systems::Inventory::ItemSlot& slot : inventory->slots) {
        if (!slot.isEmpty) {
            return false;
        }
    }
    return true;
}

void DrawBagItemGrid(struct nk_context* ctx,
                     struct nk_command_buffer* canvas,
                     const struct nk_rect& panelBounds,
                     const BagItemGridConfig& grid,
                     const Engine::Platform::Render::Texture* slotTexture,
                     Engine::ECS::Registry* inventoryRegistry,
                     Engine::EntityID inventoryEntity,
                     CharacterMenu& menu,
                     const CharacterMenu::BagDragState& dragState,
                     int screenWidth,
                     int screenHeight) {
    if (!ctx || !canvas) {
        return;
    }

    const BagGridLayout layout = ComputeBagGridLayout(panelBounds, grid);

    const Engine::Systems::Inventory::InventoryComponent* inventory = nullptr;
    if (inventoryRegistry && inventoryEntity != Engine::INVALID_ENTITY) {
        inventory = inventoryRegistry->GetComponent<Engine::Systems::Inventory::InventoryComponent>(
            inventoryEntity);
    }

    const struct nk_color slotFill = nk_rgba(18, 22, 34, 170);
    const struct nk_color slotBorder = nk_rgba(95, 110, 145, 200);
    const struct nk_color dropHighlight = nk_rgba(120, 200, 255, 220);
    const struct nk_color textColor = nk_rgba(225, 232, 245, 255);
    const struct nk_color qtyColor = nk_rgba(170, 190, 230, 255);
    const struct nk_color transparent = nk_rgba(0, 0, 0, 0);
    const struct nk_user_font* font = ctx->style.font;
    const bool drawSlotTexture = slotTexture && slotTexture->IsLoaded();
    const float slotAlpha = std::clamp(grid.slotAlpha, 0.0f, 1.0f);
    const unsigned char slotAlphaByte =
        static_cast<unsigned char>(std::lround(slotAlpha * 255.0f));

    for (int slotIndex = 0; slotIndex < layout.slotCount; ++slotIndex) {
        const struct nk_rect slotBounds = layout.slotBounds[static_cast<size_t>(slotIndex)];

        if (drawSlotTexture && slotAlphaByte > 0) {
            Engine::Platform::UI::DrawTexture(
                canvas, *slotTexture, slotBounds, nk_rgba(255, 255, 255, slotAlphaByte));
        } else {
            nk_fill_rect(canvas, slotBounds, 4.0f, slotFill);
            nk_stroke_rect(canvas, slotBounds, 4.0f, 1.0f, slotBorder);
        }

        if (dragState.active && dragState.hoverDropSlot == slotIndex) {
            nk_stroke_rect(canvas, slotBounds, 4.0f, 2.5f, dropHighlight);
        }

        if (!inventory || slotIndex >= static_cast<int>(inventory->slots.size()) ||
            inventory->slots[static_cast<size_t>(slotIndex)].isEmpty) {
            continue;
        }

        if (dragState.active && dragState.sourceSlot == slotIndex) {
            continue;
        }

        const Engine::Systems::Inventory::Item& item =
            inventory->slots[static_cast<size_t>(slotIndex)].item;

        const Engine::Platform::Render::Texture* iconTexture =
            menu.GetItemIconTexture(item.id);

        if (iconTexture) {
            const float iconPad = slotBounds.w * 0.10f;
            const struct nk_rect iconBounds = nk_rect(
                slotBounds.x + iconPad,
                slotBounds.y + iconPad,
                slotBounds.w - iconPad * 2.0f,
                slotBounds.h - iconPad * 2.0f);
            Engine::Platform::UI::DrawTexture(
                canvas, *iconTexture, iconBounds, nk_rgba(255, 255, 255, 255));
        }

        if (!font) {
            continue;
        }

        if (!iconTexture) {
            const std::string& label = item.name.empty() ? "Item" : item.name;
            const int labelLen = static_cast<int>(std::min(label.size(), size_t{24}));
            const float textW =
                font->width(font->userdata, font->height, label.c_str(), labelLen);
            const float textX =
                slotBounds.x + std::max(2.0f, (slotBounds.w - textW) * 0.5f);
            const float textY = slotBounds.y + slotBounds.h * 0.58f;
            nk_draw_text(canvas,
                         nk_rect(textX, textY, textW, font->height),
                         label.c_str(),
                         labelLen,
                         font,
                         transparent,
                         textColor);
        }

        if (item.quantity >= 1) {
            const std::string qty = std::to_string(item.quantity);
            const int qtyLen = static_cast<int>(qty.size());
            const float qtyW = font->width(font->userdata, font->height, qty.c_str(), qtyLen);
            nk_draw_text(canvas,
                         nk_rect(slotBounds.x + slotBounds.w - qtyW - 4.0f,
                                 slotBounds.y + slotBounds.h - font->height - 4.0f,
                                 qtyW,
                                 font->height),
                         qty.c_str(),
                         qtyLen,
                         font,
                         transparent,
                         qtyColor);
        }
    }

    if (dragState.active && dragState.item.quantity > 0) {
        const float ghostSize = layout.slotSize * 0.82f;
        const struct nk_rect ghostBounds =
            nk_rect(ctx->input.mouse.pos.x - ghostSize * 0.5f,
                    ctx->input.mouse.pos.y - ghostSize * 0.5f,
                    ghostSize,
                    ghostSize);
        const Engine::Platform::Render::Texture* dragTexture =
            menu.GetItemIconTexture(dragState.item.id);
        if (dragTexture) {
            Engine::Platform::UI::DrawTexture(
                canvas, *dragTexture, ghostBounds, nk_rgba(255, 255, 255, 230));
        }

        if (font && dragState.item.quantity > 1) {
            const std::string qty = std::to_string(dragState.item.quantity);
            const int qtyLen = static_cast<int>(qty.size());
            const float qtyW = font->width(font->userdata, font->height, qty.c_str(), qtyLen);
            nk_draw_text(canvas,
                         nk_rect(ghostBounds.x + ghostBounds.w - qtyW - 3.0f,
                                 ghostBounds.y + ghostBounds.h - font->height - 3.0f,
                                 qtyW,
                                 font->height),
                         qty.c_str(),
                         qtyLen,
                         font,
                         transparent,
                         qtyColor);
        }
    }

    if (!dragState.active && inventory && font && screenWidth > 0 && screenHeight > 0) {
        const float mouseX = ctx->input.mouse.pos.x;
        const float mouseY = ctx->input.mouse.pos.y;
        for (int slotIndex = 0; slotIndex < layout.slotCount; ++slotIndex) {
            if (slotIndex >= static_cast<int>(inventory->slots.size()) ||
                inventory->slots[static_cast<size_t>(slotIndex)].isEmpty) {
                continue;
            }

            const struct nk_rect& slotBounds = layout.slotBounds[static_cast<size_t>(slotIndex)];
            if (!PointInRect(slotBounds, mouseX, mouseY)) {
                continue;
            }

            const Engine::Systems::Inventory::Item& hoveredItem =
                inventory->slots[static_cast<size_t>(slotIndex)].item;
            const Engine::Platform::UI::ItemTooltipContent tooltipContent =
                menu.BuildItemTooltipContent(hoveredItem);
            Engine::Platform::UI::PresentItemTooltip(
                ctx,
                canvas,
                slotBounds,
                screenWidth,
                screenHeight,
                tooltipContent);
            break;
        }
    }
}

} // namespace

CharacterMenu::LoadedBarButton* CharacterMenu::FindBarButton(const std::string& id) {
    for (LoadedBarButton& button : m_barButtons) {
        if (button.id == id) {
            return &button;
        }
    }
    return nullptr;
}

void CharacterMenu::SetInventoryEntity(Engine::ECS::Registry* registry, Engine::EntityID entity) {
    m_inventoryRegistry = registry;
    m_inventoryEntity = entity;
    m_lastBagLogSnapshot.clear();
    CancelBagDrag();
}

void CharacterMenu::SetInventorySyncCallback(std::function<void()> callback) {
    m_inventorySyncCallback = std::move(callback);
}

void CharacterMenu::SetInventoryMutationCallback(std::function<void()> callback) {
    m_inventoryMutationCallback = std::move(callback);
}

void CharacterMenu::RequestInventorySync() {
    m_inventorySyncPending = true;
}

void CharacterMenu::CancelBagDrag() {
    if (!m_bagDrag.active) {
        return;
    }

    if (m_inventoryRegistry && m_inventoryEntity != Engine::INVALID_ENTITY &&
        m_bagDrag.sourceSlot >= 0 && m_bagDrag.item.quantity > 0) {
        auto* inventory =
            m_inventoryRegistry->GetComponent<Engine::Systems::Inventory::InventoryComponent>(
                m_inventoryEntity);
        if (inventory) {
            inventory->PlaceItemAtSlot(static_cast<size_t>(m_bagDrag.sourceSlot),
                                         m_bagDrag.item,
                                         static_cast<size_t>(m_bagDrag.sourceSlot));
        }
    }

    m_bagDrag = BagDragState{};
}

bool CharacterMenu::UpdateBagItemDrag(struct nk_context* ctx,
                                      float panelX,
                                      float panelY,
                                      float panelW,
                                      float panelH,
                                      const BagItemGridConfig& grid) {
    if (!ctx || !m_inventoryRegistry || m_inventoryEntity == Engine::INVALID_ENTITY) {
        return false;
    }

    const struct nk_rect panelBounds = nk_rect(panelX, panelY, panelW, panelH);

    auto* inventory =
        m_inventoryRegistry->GetComponent<Engine::Systems::Inventory::InventoryComponent>(
            m_inventoryEntity);
    if (!inventory) {
        return false;
    }

    const BagGridLayout layout = ComputeBagGridLayout(panelBounds, grid);
    const float mouseX = ctx->input.mouse.pos.x;
    const float mouseY = ctx->input.mouse.pos.y;
    const bool mousePressed = nk_input_is_mouse_pressed(&ctx->input, NK_BUTTON_LEFT);
    const bool mouseReleased = nk_input_is_mouse_released(&ctx->input, NK_BUTTON_LEFT);
    const float snapRadius =
        layout.slotSize * std::clamp(grid.dropSnapFraction, 0.15f, 1.0f);

    bool pointerOverGrid = PointInRect(layout.contentBounds, mouseX, mouseY);
    if (!pointerOverGrid) {
        for (const struct nk_rect& slotBounds : layout.slotBounds) {
            if (PointInRect(slotBounds, mouseX, mouseY)) {
                pointerOverGrid = true;
                break;
            }
        }
    }

    if (m_bagDrag.active) {
        m_bagDrag.hoverDropSlot =
            FindNearestBagSlot(mouseX, mouseY, layout.slotBounds, snapRadius);

        if (mouseReleased) {
            if (m_bagDrag.hoverDropSlot >= 0) {
                inventory->PlaceItemAtSlot(static_cast<size_t>(m_bagDrag.hoverDropSlot),
                                           m_bagDrag.item,
                                           static_cast<size_t>(m_bagDrag.sourceSlot));
            } else if (m_bagDrag.sourceSlot >= 0) {
                inventory->PlaceItemAtSlot(static_cast<size_t>(m_bagDrag.sourceSlot),
                                           m_bagDrag.item,
                                           static_cast<size_t>(m_bagDrag.sourceSlot));
            }
            m_bagDrag = BagDragState{};
            m_lastBagLogSnapshot.clear();
            if (m_inventoryMutationCallback) {
                m_inventoryMutationCallback();
            }
        }

        return true;
    }

    if (mousePressed) {
        for (size_t slotIndex = 0; slotIndex < layout.slotBounds.size(); ++slotIndex) {
            if (!PointInRect(layout.slotBounds[slotIndex], mouseX, mouseY)) {
                continue;
            }
            if (slotIndex >= inventory->slots.size() || inventory->slots[slotIndex].isEmpty) {
                continue;
            }

            if (inventory->TakeItemFromSlot(slotIndex, m_bagDrag.item)) {
                m_bagDrag.active = true;
                m_bagDrag.sourceSlot = static_cast<int>(slotIndex);
                m_bagDrag.hoverDropSlot = -1;
                return true;
            }
        }
    }

    return pointerOverGrid;
}

void CharacterMenu::SetGameItemCatalog(const Game::Character::GameItemCatalog* catalog) {
    m_gameItemCatalog = catalog;
    m_itemIconTextures.clear();
    if (!catalog) {
        return;
    }

    for (const auto& entry : catalog->Items()) {
        const Game::Character::GameItemDefinition& definition = entry.second;
        if (definition.iconPath.empty()) {
            continue;
        }

        auto texture = std::make_unique<Engine::Platform::Render::Texture>(definition.iconPath.c_str());
        if (!texture->LoadTextureUI(512)) {
            LOG_WARNING_FMT(
                "CharacterMenu",
                "Item icon not loaded (%s) for '%s'",
                definition.iconPath.c_str(),
                definition.id.c_str());
            continue;
        }
        m_itemIconTextures.emplace(definition.numericId, std::move(texture));
    }

    LOG_INFO_FMT(
        "CharacterMenu",
        "Loaded %zu/%zu item icon texture(s) for bag rendering",
        m_itemIconTextures.size(),
        catalog->Items().size());
}

Engine::Platform::UI::ItemTooltipContent CharacterMenu::BuildItemTooltipContent(
    const Engine::Systems::Inventory::Item& item) const {
    Engine::Platform::UI::ItemTooltipContent content;
    if (m_gameItemCatalog) {
        const Game::Character::GameItemTooltipInfo catalogInfo =
            m_gameItemCatalog->BuildTooltipInfo(item.id);
        content.name = catalogInfo.name.empty() ? item.name : catalogInfo.name;
        content.typeLabel =
            catalogInfo.typeLabel.empty()
                ? Game::Character::GameItemCatalog::ItemTypeDisplayName(item.type)
                : catalogInfo.typeLabel;
        content.description = catalogInfo.description;
    } else {
        content.name = item.name.empty() ? "Item" : item.name;
        content.typeLabel = Game::Character::GameItemCatalog::ItemTypeDisplayName(item.type);
    }

    if (content.name.empty()) {
        content.name = "Item";
    }
    return content;
}

const Engine::Platform::Render::Texture* CharacterMenu::GetItemIconTexture(uint32_t numericItemId) {
    const auto cached = m_itemIconTextures.find(numericItemId);
    if (cached != m_itemIconTextures.end() && cached->second && cached->second->IsLoaded()) {
        return cached->second.get();
    }

    if (!m_gameItemCatalog) {
        return nullptr;
    }

    const Game::Character::GameItemDefinition* definition =
        m_gameItemCatalog->FindByNumericId(numericItemId);
    if (!definition || definition->iconPath.empty()) {
        return nullptr;
    }

    auto texture =
        std::make_unique<Engine::Platform::Render::Texture>(definition->iconPath.c_str());
    if (!texture->LoadTextureUI(512)) {
        LOG_WARNING_FMT(
            "CharacterMenu",
            "Lazy item icon load failed (%s) for '%s' numeric_id=%u",
            definition->iconPath.c_str(),
            definition->id.c_str(),
            numericItemId);
        return nullptr;
    }

    Engine::Platform::Render::Texture* loadedTexture = texture.get();
    m_itemIconTextures.emplace(numericItemId, std::move(texture));
    return loadedTexture;
}

void CharacterMenu::LogBagContentsIfChanged() {
    if (!m_inventoryRegistry || m_inventoryEntity == Engine::INVALID_ENTITY) {
        return;
    }

    const auto* inventory =
        m_inventoryRegistry->GetComponent<Engine::Systems::Inventory::InventoryComponent>(
            m_inventoryEntity);
    if (!inventory) {
        return;
    }

    std::ostringstream snapshot;
    for (size_t slotIndex = 0; slotIndex < inventory->slots.size(); ++slotIndex) {
        const auto& slot = inventory->slots[slotIndex];
        if (slot.isEmpty) {
            continue;
        }
        snapshot << Game::Inventory::DescribeInventorySlot(
            slotIndex, slot.item, m_gameItemCatalog);
        snapshot << " | ";
    }

    const std::string snapshotText = snapshot.str();
    if (snapshotText == m_lastBagLogSnapshot) {
        return;
    }

    m_lastBagLogSnapshot = snapshotText;
    Game::Inventory::LogInventoryContents(*inventory, m_gameItemCatalog, "bag-ui");
}

bool CharacterMenu::KeyPressed(Engine::Platform::Window& window, int key, bool& wasDown) const {
    GLFWwindow* glfwWindow = window.GetGLFWWindow();
    if (!glfwWindow || key < 0 || key >= 512) {
        return false;
    }
    const bool down = glfwGetKey(glfwWindow, key) == GLFW_PRESS;
    const bool pressed = down && !wasDown;
    wasDown = down;
    return pressed;
}

void CharacterMenu::ToggleBarButton(const std::string& id) {
    LoadedBarButton* button = FindBarButton(id);
    if (!button) {
        return;
    }
    button->pressed = !button->pressed;
    LOG_INFO_FMT("CharacterMenu",
                 "Bar button '%s' %s (keyboard)",
                 id.c_str(),
                 button->pressed ? "pressed" : "released");
}

void CharacterMenu::Update(Engine::Platform::Window& window) {
    if (!m_visible) {
        return;
    }
    if (KeyPressed(window, GLFW_KEY_B, m_bKeyWasDown)) {
        ToggleBarButton("bag");
    }
}

void CharacterMenu::SetPaperdollCharacter(Engine::ECS::Registry* registry,
                                          Engine::EntityID characterEntity,
                                          Engine::Platform::Render::Shader* shader,
                                          Game::Character::CharacterInstance* characterInstance) {
    m_paperdollRegistry = registry;
    m_paperdollCharacterEntity = characterEntity;
    m_paperdollShader = shader;
    m_paperdollCharacterInstance = characterInstance;
    if (!m_paperdollRenderer) {
        m_paperdollRenderer = std::make_unique<Game::Character::CharacterPaperdollRenderer>();
    }
}

CloseButtonConfig CharacterMenu::ResolvePanelCloseButton(const OverlayPanelConfig& panel) const {
    CloseButtonConfig resolved = m_config.closeButton;
    if (!panel.hasCloseButtonPlacement) {
        return resolved;
    }

    resolved.sizeFraction = panel.closeSizeFraction;
    resolved.xAnchorFraction = panel.closeXAnchorFraction;
    resolved.yAnchorFraction = panel.closeYAnchorFraction;
    resolved.marginFraction = panel.closeMarginFraction;
    resolved.centerOnCorner = panel.closeCenterOnCorner;
    return resolved;
}

void CharacterMenu::Build(struct nk_context* ctx,
                          const Engine::Platform::UI::MenuStyle& /*style*/,
                          Engine::Platform::UI::UISystem& /*uiSystem*/,
                          int screenWidth,
                          int screenHeight) {
    if (!ctx || !m_visible) {
        return;
    }

    if (screenWidth <= 0 || screenHeight <= 0) {
        return;
    }

    if (!m_bottomBarTexture || !m_bottomBarTexture->IsLoaded()) {
        return;
    }

    const int texW = m_bottomBarTexture->GetWidth();
    const int texH = m_bottomBarTexture->GetHeight();
    if (texW <= 0 || texH <= 0) {
        return;
    }

    const float widthFraction = std::clamp(m_config.bottomBarWidthFraction, 0.1f, 1.0f);
    const MenuScreenLayout layout =
        MenuScreenLayout::From(screenWidth, screenHeight, m_config.ReferenceScreen());
    const float drawW = layout.ScaledReferenceWidth(widthFraction);
    const float drawH = drawW * (static_cast<float>(texH) / static_cast<float>(texW));
    const float x = layout.CenteredX(drawW);
    const float y = layout.screenHeight - drawH;
    const struct nk_rect barBounds = nk_rect(x, y, drawW, drawH);

    m_pointerOverBarUi = false;
    m_pointerOverPanelUi = false;

    Engine::Platform::UI::OverlayScope overlay(
        ctx,
        "CharacterMenu",
        Engine::Platform::UI::OverlayScope::FullScreen(screenWidth, screenHeight));
    if (!overlay.IsOpen() || !overlay.Canvas()) {
        return;
    }

    struct nk_command_buffer* canvas = overlay.Canvas();

    for (LoadedOverlayPanel& panel : m_overlayPanels) {
        if (!panel.texture || !panel.texture->IsLoaded()) {
            continue;
        }

        LoadedBarButton* barButton = FindBarButton(panel.config.buttonId);
        if (!barButton || !barButton->pressed) {
            panel.dragging = false;
            continue;
        }

        const int panelTexW = panel.texture->GetWidth();
        const int panelTexH = panel.texture->GetHeight();
        const OverlayPanelLayout panelLayout = ComputeOverlayPanelLayout(
            layout, panelTexW, panelTexH, panel.config.widthFraction);
        if (panelLayout.panelW <= 0.0f || panelLayout.panelH <= 0.0f) {
            continue;
        }

        if (!panel.positionInitialized) {
            panel.centerX =
                layout.DesignXToScreen(layout.referenceWidth * panel.config.xFraction);
            panel.centerY =
                layout.DesignYToScreen(layout.referenceHeight * panel.config.yFraction);
            panel.positionInitialized = true;
        }

        ClampPanelCenter(panel.centerX,
                         panel.centerY,
                         panelLayout.panelW,
                         panelLayout.panelH,
                         layout.screenWidth,
                         layout.screenHeight);

        struct nk_rect panelBounds = PanelBoundsFromCenter(
            panel.centerX,
            panel.centerY,
            panelLayout.panelW,
            panelLayout.panelH);

        const CloseButtonConfig panelCloseButton = ResolvePanelCloseButton(panel.config);
        const PaperdollRotateButtonBounds rotateBounds =
            panel.config.hasCharacterPreview
                ? PaperdollRotateBoundsFromPanel(panelBounds, m_config.paperdollRotate)
                : PaperdollRotateButtonBounds{};

        if (panel.dragging) {
            if (ctx->input.mouse.buttons[NK_BUTTON_LEFT].down) {
                const float left = ctx->input.mouse.pos.x - panel.dragGrabX;
                const float top = ctx->input.mouse.pos.y - panel.dragGrabY;
                panel.centerX = left + panelLayout.panelW * 0.5f;
                panel.centerY = top + panelLayout.panelH * 0.5f;
                ClampPanelCenter(panel.centerX,
                                 panel.centerY,
                                 panelLayout.panelW,
                                 panelLayout.panelH,
                                 layout.screenWidth,
                                 layout.screenHeight);
                panelBounds = PanelBoundsFromCenter(
                    panel.centerX,
                    panel.centerY,
                    panelLayout.panelW,
                    panelLayout.panelH);
                m_pointerOverPanelUi = true;
            } else {
                panel.dragging = false;
            }
        } else if (nk_input_is_mouse_click_down_in_rect(
                       &ctx->input, NK_BUTTON_LEFT, panelBounds, nk_true)) {
            if (m_bagDrag.active) {
                m_pointerOverPanelUi = true;
            } else {
            panel.dragGrabX = ctx->input.mouse.pos.x - panelBounds.x;
            panel.dragGrabY = ctx->input.mouse.pos.y - panelBounds.y;

            const struct nk_rect closeBounds =
                CloseButtonBoundsFromPanel(panelBounds, panelCloseButton);
            const bool overClose = nk_input_is_mouse_hovering_rect(&ctx->input, closeBounds);
            const bool overRotateLeft =
                panel.config.hasCharacterPreview &&
                nk_input_is_mouse_hovering_rect(&ctx->input, rotateBounds.left);
            const bool overRotateRight =
                panel.config.hasCharacterPreview &&
                nk_input_is_mouse_hovering_rect(&ctx->input, rotateBounds.right);
            bool canStartPanelDrag = !overClose && !overRotateLeft && !overRotateRight;
            if (panel.config.hasItemGrid) {
                const BagGridLayout gridLayout =
                    ComputeBagGridLayout(panelBounds, panel.config.itemGrid);
                if (PointInRect(
                        gridLayout.contentBounds,
                        ctx->input.mouse.pos.x,
                        ctx->input.mouse.pos.y)) {
                    canStartPanelDrag = false;
                }
            } else if (panel.config.dragFromEdgesOnly) {
                canStartPanelDrag =
                    canStartPanelDrag &&
                    IsMouseOnPanelDragEdge(
                        panelBounds,
                        ctx->input.mouse.pos.x,
                        ctx->input.mouse.pos.y,
                        panel.config.dragEdgeFraction);
            }
            if (canStartPanelDrag) {
                panel.dragging = true;
                m_pointerOverPanelUi = true;
            }
            }
        }

        Engine::Platform::UI::DrawTexture(
            canvas,
            *panel.texture,
            panelBounds,
            nk_rgba(255, 255, 255, 255));

        if (panel.config.hasItemGrid) {
            const auto* inventory =
                m_inventoryRegistry
                    ? m_inventoryRegistry->GetComponent<
                          Engine::Systems::Inventory::InventoryComponent>(m_inventoryEntity)
                    : nullptr;
            if (m_inventorySyncPending && m_inventorySyncCallback) {
                CancelBagDrag();
                m_inventorySyncCallback();
                m_inventorySyncPending = false;
            }

            if (this->UpdateBagItemDrag(ctx,
                                        panelBounds.x,
                                        panelBounds.y,
                                        panelBounds.w,
                                        panelBounds.h,
                                        panel.config.itemGrid) ||
                m_bagDrag.active) {
                m_pointerOverPanelUi = true;
            }

            LogBagContentsIfChanged();
            DrawBagItemGrid(ctx,
                            canvas,
                            panelBounds,
                            panel.config.itemGrid,
                            panel.slotTexture.get(),
                            m_inventoryRegistry,
                            m_inventoryEntity,
                            *this,
                            m_bagDrag,
                            screenWidth,
                            screenHeight);
        }

        if (panel.config.hasCharacterPreview && m_paperdollRenderer && m_paperdollRegistry &&
            m_paperdollShader && m_paperdollCharacterInstance &&
            m_paperdollCharacterEntity != Engine::INVALID_ENTITY) {
            m_paperdollRenderer->SetYawDegrees(m_paperdollYawDegrees);
            const struct nk_rect previewBounds =
                CharacterPreviewBoundsFromPanel(panelBounds, panel.config);
            DrawCharacterPreviewOnPanel(canvas,
                                        previewBounds,
                                        *m_paperdollRenderer,
                                        *m_paperdollRegistry,
                                        m_paperdollCharacterEntity,
                                        *m_paperdollCharacterInstance,
                                        *m_paperdollShader);

            const PaperdollRotateButtonBounds liveRotateBounds =
                PaperdollRotateBoundsFromPanel(panelBounds, m_config.paperdollRotate);

            const auto now = std::chrono::steady_clock::now();
            float deltaSeconds = 1.0f / 60.0f;
            if (m_hasPaperdollRotateClock) {
                deltaSeconds = std::chrono::duration<float>(now - m_lastPaperdollRotateTime).count();
                deltaSeconds = std::clamp(deltaSeconds, 0.0f, 0.05f);
            }
            m_lastPaperdollRotateTime = now;
            m_hasPaperdollRotateClock = true;

            const float speed = std::max(0.0f, m_config.paperdollRotate.degreesPerSecond);
            const float step = speed * deltaSeconds;
            // Left arrow: clockwise. Right arrow: anti-clockwise. Hold for smooth spin.
            if (DrawPaperdollRotateButton(ctx,
                                          canvas,
                                          liveRotateBounds.left,
                                          m_paperdollRotateLeftTexture.get(),
                                          m_paperdollRotateLeftLightTexture.get(),
                                          m_pointerOverPanelUi)) {
                m_paperdollYawDegrees = NormalizeYawDegrees(m_paperdollYawDegrees - step);
                m_paperdollRenderer->SetYawDegrees(m_paperdollYawDegrees);
            }
            if (DrawPaperdollRotateButton(ctx,
                                          canvas,
                                          liveRotateBounds.right,
                                          m_paperdollRotateRightTexture.get(),
                                          m_paperdollRotateRightLightTexture.get(),
                                          m_pointerOverPanelUi)) {
                m_paperdollYawDegrees = NormalizeYawDegrees(m_paperdollYawDegrees + step);
                m_paperdollRenderer->SetYawDegrees(m_paperdollYawDegrees);
            }
        }

        if (nk_input_is_mouse_hovering_rect(&ctx->input, panelBounds)) {
            m_pointerOverPanelUi = true;
        }

        const struct nk_rect closeBounds =
            CloseButtonBoundsFromPanel(panelBounds, panelCloseButton);
        if (DrawPanelCloseButton(ctx,
                                 canvas,
                                 closeBounds,
                                 m_closeDarkTexture.get(),
                                 m_closeLightTexture.get(),
                                 m_pointerOverPanelUi)) {
            barButton->pressed = false;
            panel.dragging = false;
            LOG_INFO_FMT("CharacterMenu", "Panel '%s' closed", panel.config.buttonId.c_str());
        }
    }

    Engine::Platform::UI::DrawTexture(
        canvas,
        *m_bottomBarTexture,
        barBounds,
        nk_rgba(255, 255, 255, 255));

    if (nk_input_is_mouse_hovering_rect(&ctx->input, barBounds)) {
        m_pointerOverBarUi = true;
    }

    for (LoadedBarButton& button : m_barButtons) {
        const Engine::Platform::Render::Texture* texture = nullptr;
        if (button.pressed) {
            texture = button.lightTexture && button.lightTexture->IsLoaded()
                          ? button.lightTexture.get()
                          : button.darkTexture.get();
        } else {
            texture = button.darkTexture && button.darkTexture->IsLoaded()
                          ? button.darkTexture.get()
                          : button.lightTexture.get();
        }

        if (!texture || !texture->IsLoaded()) {
            continue;
        }

        const float heightFraction =
            button.heightFraction >= 0.0f ? button.heightFraction : m_config.barButtonHeightFraction;
        const float iconHeight = drawH * std::clamp(heightFraction, 0.05f, 1.0f);
        const int texW = texture->GetWidth();
        const int texH = texture->GetHeight();
        const float aspect = texH > 0 ? static_cast<float>(texW) / static_cast<float>(texH) : 1.0f;
        const float iconWidth = iconHeight * aspect;
        const float yFraction =
            button.yFraction >= 0.0f ? button.yFraction : m_config.barButtonYFraction;
        const float iconCenterY = y + drawH * std::clamp(yFraction, 0.0f, 1.0f);
        const float iconTop = iconCenterY - iconHeight * 0.5f;
        const float iconCenterX = x + drawW * button.xFraction;
        const float iconLeft = iconCenterX - iconWidth * 0.5f;
        const struct nk_rect buttonBounds = nk_rect(iconLeft, iconTop, iconWidth, iconHeight);

        if (nk_input_is_mouse_hovering_rect(&ctx->input, buttonBounds)) {
            m_pointerOverBarUi = true;
        }

        const bool activated =
            nk_input_is_mouse_click_in_rect(&ctx->input, NK_BUTTON_LEFT, buttonBounds);
        if (activated) {
            button.pressed = !button.pressed;
            LOG_INFO_FMT("CharacterMenu",
                         "Bar button '%s' %s",
                         button.id.c_str(),
                         button.pressed ? "pressed" : "released");
        }

        Engine::Platform::UI::DrawTexture(
            canvas,
            *texture,
            buttonBounds,
            nk_rgba(255, 255, 255, 255));
    }
}

} // namespace UI
} // namespace Game
