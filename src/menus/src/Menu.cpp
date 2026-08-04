#include "Menu.h"

#include "Logger.h"
#include "ManifestTradeConfig.h"
#include "character/CharacterPaperdollRenderer.h"
#include "core/ecs/components/PlayerTradeComponent.h"
#include "core/network/TradeSession.h"
#include "render/Shader.h"
#include "render/Window.h"
#include "systems/inventory/InventoryComponent.h"
#include "systems/inventory/InventoryBagLog.h"
#include "ui/OverlayWindow.h"
#include "ui/ItemTooltip.h"

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_FONT_BAKING
#include "nuklear.h"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cctype>
#include <cstring>
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

void LoadBagItemGridFromObject(const std::string& itemGridJson,
                               const char* root,
                               BagItemGridConfig& out) {
    std::string slotTextureRelative;
    if (JsonTryGetString(itemGridJson, "slot_texture", slotTextureRelative)) {
        out.slotTexturePath = JoinPath(root, slotTextureRelative);
    }
    JsonTryGetInt(itemGridJson, "columns", out.columns);
    JsonTryGetInt(itemGridJson, "rows", out.rows);
    JsonTryGetFloat(itemGridJson, "width_fraction", out.widthFraction);
    JsonTryGetFloat(itemGridJson, "height_fraction", out.heightFraction);
    JsonTryGetFloat(itemGridJson, "center_x_fraction", out.centerXFraction);
    JsonTryGetFloat(itemGridJson, "center_y_fraction", out.centerYFraction);
    JsonTryGetFloat(itemGridJson, "gap_x_fraction", out.gapXFraction);
    JsonTryGetFloat(itemGridJson, "gap_y_fraction", out.gapYFraction);
    JsonTryGetFloat(itemGridJson, "slot_alpha", out.slotAlpha);
    JsonTryGetFloat(itemGridJson, "drop_snap_fraction", out.dropSnapFraction);
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
        LoadBagItemGridFromObject(itemGridJson, root, out.itemGrid);
        out.dragFromEdgesOnly = true;
    }

    const std::string selfTradeGridJson = JsonExtractObject(objectJson, "self_item_grid");
    const std::string partnerTradeGridJson = JsonExtractObject(objectJson, "partner_item_grid");
    if (!selfTradeGridJson.empty() && !partnerTradeGridJson.empty()) {
        out.hasTradeGrids = true;
        LoadBagItemGridFromObject(selfTradeGridJson, root, out.selfTradeGrid);
        LoadBagItemGridFromObject(partnerTradeGridJson, root, out.partnerTradeGrid);
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

    ManifestTradeConfig tradeConfig;
    if (LoadManifestTradeConfig(tradeConfig, path)) {
        out.tradeMaxDistance = tradeConfig.maxDistance;
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
        } else if (panelConfig.hasTradeGrids &&
                   !panelConfig.selfTradeGrid.slotTexturePath.empty()) {
            loaded.slotTexture = std::make_unique<Engine::Platform::Render::Texture>(
                panelConfig.selfTradeGrid.slotTexturePath.c_str());
            if (!loaded.slotTexture->LoadTextureUI(512)) {
                LOG_WARNING_FMT("CharacterMenu",
                                "Trade slot texture not loaded for '%s' (%s)",
                                panelConfig.buttonId.c_str(),
                                panelConfig.selfTradeGrid.slotTexturePath.c_str());
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
    const float centerXFraction = std::clamp(grid.centerXFraction, 0.0f, 1.0f);
    const float centerYFraction = std::clamp(grid.centerYFraction, 0.0f, 1.0f);
    const float areaW = panelBounds.w * widthFraction;
    const float areaH = panelBounds.h * heightFraction;
    layout.contentBounds =
        nk_rect(panelBounds.x + panelBounds.w * centerXFraction - areaW * 0.5f,
                panelBounds.y + panelBounds.h * centerYFraction - areaH * 0.5f,
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

struct TradeChromeLayout {
    struct nk_rect selfAcceptButton{};
    struct nk_rect partnerAcceptButton{};
};

TradeChromeLayout ComputeTradeChromeLayout(const struct nk_rect& panelBounds,
                                           Engine::ECS::TradePhase phase) {
    TradeChromeLayout chrome{};
    if (phase != Engine::ECS::TradePhase::Open) {
        return chrome;
    }

    // Align with the built-in accept buttons on trade_window_circle.png (one per side).
    const float buttonW = std::clamp(panelBounds.w * 0.11f, 48.0f, 90.0f);
    const float buttonH = std::clamp(panelBounds.h * 0.055f, 22.0f, 32.0f);
    const float buttonY = panelBounds.y + panelBounds.h - buttonH - panelBounds.h * 0.025f;

    const float selfCenterX = panelBounds.x + panelBounds.w * 0.365f;
    chrome.selfAcceptButton =
        nk_rect(selfCenterX - buttonW * 0.5f, buttonY, buttonW, buttonH);

    const float partnerCenterX = panelBounds.x + panelBounds.w * 0.635f;
    chrome.partnerAcceptButton =
        nk_rect(partnerCenterX - buttonW * 0.5f, buttonY, buttonW, buttonH);
    return chrome;
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
    const struct nk_color useSelectedHighlight = nk_rgba(255, 204, 77, 255);
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

        if (!dragState.active && menu.IsBagUseSlotSelected(slotIndex)) {
            nk_stroke_rect(canvas, slotBounds, 4.0f, 3.0f, useSelectedHighlight);
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

    if (!dragState.active && inventory && font && screenWidth > 0 && screenHeight > 0 &&
        !menu.IsBagItemActionPanelOpen()) {
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

CharacterMenu::LoadedOverlayPanel* CharacterMenu::FindOverlayPanel(const std::string& buttonId) {
    for (LoadedOverlayPanel& panel : m_overlayPanels) {
        if (panel.config.buttonId == buttonId) {
            return &panel;
        }
    }
    return nullptr;
}

void CharacterMenu::SetTradeCommandCallback(
    std::function<void(const Engine::Net::TradeCommand&)> callback) {
    m_tradeCommandCallback = std::move(callback);
}

void CharacterMenu::OpenTradePanel() {
    m_tradePanelDismissedByUser = false;
    m_tradePanelOpen = true;
    OpenBagPanel();
    LoadedOverlayPanel* tradePanel = FindOverlayPanel("trade");
    if (tradePanel && !tradePanel->positionInitialized) {
        tradePanel->positionInitialized = false;
    }
}

void CharacterMenu::CloseTradePanel() {
    m_tradePanelOpen = false;
    CloseTradeQuantityPanel();
}

void CharacterMenu::DeclineAndCloseTradePanel() {
    m_tradePanelDismissedByUser = true;
    if (m_tradeCommandCallback && m_tradeRegistry && m_tradeEntity != Engine::INVALID_ENTITY) {
        const auto* trade =
            m_tradeRegistry->GetComponent<Engine::ECS::PlayerTradeComponent>(m_tradeEntity);
        if (trade && trade->IsActive()) {
            Engine::Net::TradeCommand command;
            command.targetUserId = trade->partnerUserId;
            command.action = Engine::Net::TradeAction::Cancel;
            m_tradeCommandCallback(command);
        }
    }
    m_bagActionPanelOpen = false;
    m_bagActionPanelSlot = -1;
    CloseTradePanel();
}

void CharacterMenu::OpenBagPanel() {
    if (LoadedBarButton* bagButton = FindBarButton("bag")) {
        bagButton->pressed = true;
    }
}

void CharacterMenu::SyncTradeState(Engine::ECS::Registry* registry, Engine::EntityID entity) {
    m_tradeRegistry = registry;
    m_tradeEntity = entity;
    if (!registry || entity == Engine::INVALID_ENTITY) {
        return;
    }

    const auto* trade = registry->GetComponent<Engine::ECS::PlayerTradeComponent>(entity);
    if (trade && trade->phase == Engine::ECS::TradePhase::Open) {
        if (!m_tradePanelOpen && !m_tradePanelDismissedByUser) {
            OpenTradePanel();
        }
    } else {
        if (m_tradePanelOpen) {
            CloseTradePanel();
        }
        if (!trade || !trade->IsActive()) {
            m_tradePanelDismissedByUser = false;
            m_tradeSelfDisplayName.clear();
            m_tradePartnerDisplayName.clear();
        }
    }
}

void CharacterMenu::SetTradeDisplayNames(const std::string& selfName,
                                         const std::string& partnerName) {
    m_tradeSelfDisplayName = selfName;
    m_tradePartnerDisplayName = partnerName;
}

void CharacterMenu::SendTradeOfferUpdate(
    const Engine::ECS::PlayerTradeComponent* trade,
    std::vector<Engine::ECS::TradeOfferItem> offer) {
    if (!m_tradeCommandCallback || !trade) {
        return;
    }

    if (m_tradeRegistry && m_tradeEntity != Engine::INVALID_ENTITY) {
        if (auto* localTrade =
                m_tradeRegistry->GetComponent<Engine::ECS::PlayerTradeComponent>(m_tradeEntity)) {
            localTrade->selfOffer = offer;
            localTrade->selfLocked = false;
            localTrade->partnerLocked = false;
        }
    }

    Engine::Net::TradeCommand command;
    command.action = Engine::Net::TradeAction::SetOffer;
    command.targetUserId = trade->partnerUserId;
    command.offer = std::move(offer);
    m_tradeCommandCallback(command);
    m_inventorySyncPending = true;
}

bool CharacterMenu::CanModifyTradeOffer() const {
    if (!m_tradePanelOpen || !m_tradeRegistry || m_tradeEntity == Engine::INVALID_ENTITY) {
        return false;
    }
    const auto* trade =
        m_tradeRegistry->GetComponent<Engine::ECS::PlayerTradeComponent>(m_tradeEntity);
    return trade && trade->phase == Engine::ECS::TradePhase::Open && !trade->selfLocked;
}

bool CharacterMenu::CanShowTradeItemActions() const {
    if (!m_tradePanelOpen) {
        return false;
    }
    if (!m_tradeRegistry || m_tradeEntity == Engine::INVALID_ENTITY) {
        return true;
    }
    const auto* trade =
        m_tradeRegistry->GetComponent<Engine::ECS::PlayerTradeComponent>(m_tradeEntity);
    if (!trade || !trade->IsActive()) {
        return true;
    }
    return !trade->selfLocked;
}

void CharacterMenu::OpenTradeQuantityPanel(int bagSlotIndex) {
    if (!CanModifyTradeOffer() || !m_inventoryRegistry ||
        m_inventoryEntity == Engine::INVALID_ENTITY || bagSlotIndex < 0) {
        return;
    }

    const auto* inventory =
        m_inventoryRegistry->GetComponent<Engine::Systems::Inventory::InventoryComponent>(
            m_inventoryEntity);
    if (!inventory || bagSlotIndex >= static_cast<int>(inventory->slots.size()) ||
        inventory->slots[static_cast<size_t>(bagSlotIndex)].isEmpty) {
        return;
    }

    const uint32_t ownedQty =
        std::max(1u, inventory->slots[static_cast<size_t>(bagSlotIndex)].item.quantity);
    m_tradeQuantityPanelOpen = true;
    m_tradeQuantitySlot = bagSlotIndex;
    m_tradeQuantityMax = static_cast<int>(ownedQty);
    m_tradeQuantityValue = m_tradeQuantityMax;
    m_tradeQuantityAnchorX = m_bagActionPanelAnchorX;
    m_tradeQuantityAnchorY = m_bagActionPanelAnchorY;
    m_tradeQuantityAnchorW = m_bagActionPanelAnchorW;
    m_tradeQuantityAnchorH = m_bagActionPanelAnchorH;
    m_bagActionPanelOpen = false;
    m_bagActionPanelSlot = -1;
}

void CharacterMenu::CloseTradeQuantityPanel() {
    m_tradeQuantityPanelOpen = false;
    m_tradeQuantitySlot = -1;
    m_tradeQuantityValue = 1;
    m_tradeQuantityMax = 1;
}

bool CharacterMenu::TryAddBagItemToTradeOffer(int bagSlotIndex, uint32_t quantity) {
    if (!m_tradePanelOpen || bagSlotIndex < 0 || !m_tradeCommandCallback || !m_gameItemCatalog ||
        !m_inventoryRegistry || m_inventoryEntity == Engine::INVALID_ENTITY) {
        return false;
    }

    const auto* inventory =
        m_inventoryRegistry->GetComponent<Engine::Systems::Inventory::InventoryComponent>(
            m_inventoryEntity);
    const auto* trade =
        m_tradeRegistry && m_tradeEntity != Engine::INVALID_ENTITY
            ? m_tradeRegistry->GetComponent<Engine::ECS::PlayerTradeComponent>(m_tradeEntity)
            : nullptr;
    if (!inventory || !trade || trade->phase != Engine::ECS::TradePhase::Open || trade->selfLocked) {
        return false;
    }

    if (bagSlotIndex >= static_cast<int>(inventory->slots.size()) ||
        inventory->slots[static_cast<size_t>(bagSlotIndex)].isEmpty) {
        return false;
    }

    const Engine::Systems::Inventory::Item& bagItem =
        inventory->slots[static_cast<size_t>(bagSlotIndex)].item;
    const std::string* itemId = m_gameItemCatalog->FindItemIdByNumericId(bagItem.id);
    if (!itemId || itemId->empty()) {
        return false;
    }

    if (trade->selfOffer.size() >= Engine::ECS::kMaxTradeOfferSlots) {
        return false;
    }

    const uint16_t bagSlot = static_cast<uint16_t>(bagSlotIndex);
    for (const Engine::ECS::TradeOfferItem& existing : trade->selfOffer) {
        if (existing.bagSlotIndex == bagSlot) {
            return false;
        }
    }

    const uint32_t ownedQty = std::max(1u, bagItem.quantity);
    uint32_t offerQty = quantity > 0 ? quantity : ownedQty;
    offerQty = std::clamp(offerQty, 1u, ownedQty);

    std::vector<Engine::ECS::TradeOfferItem> offer = trade->selfOffer;
    Engine::ECS::TradeOfferItem entry;
    entry.itemId = *itemId;
    entry.quantity = offerQty;
    entry.instanceId = 0;
    entry.bagSlotIndex = static_cast<uint16_t>(bagSlotIndex);
    offer.push_back(entry);
    SendTradeOfferUpdate(trade, std::move(offer));
    return true;
}

bool CharacterMenu::TryRemoveTradeOfferAtSlot(int tradeSlotIndex) {
    if (!m_tradePanelOpen || tradeSlotIndex < 0 || !m_tradeCommandCallback) {
        return false;
    }

    const auto* trade =
        m_tradeRegistry && m_tradeEntity != Engine::INVALID_ENTITY
            ? m_tradeRegistry->GetComponent<Engine::ECS::PlayerTradeComponent>(m_tradeEntity)
            : nullptr;
    if (!trade || trade->phase != Engine::ECS::TradePhase::Open || trade->selfLocked) {
        return false;
    }

    if (tradeSlotIndex >= static_cast<int>(trade->selfOffer.size())) {
        return false;
    }

    std::vector<Engine::ECS::TradeOfferItem> offer = trade->selfOffer;
    offer.erase(offer.begin() + tradeSlotIndex);
    SendTradeOfferUpdate(trade, std::move(offer));
    return true;
}

void CharacterMenu::UpdateTradeInteractions(
    struct nk_context* ctx,
    const struct nk_rect& panelBounds,
    const BagItemGridConfig& selfGrid,
    const Engine::ECS::PlayerTradeComponent* trade) {
    if (!ctx || !trade) {
        return;
    }

    const BagGridLayout selfLayout = ComputeBagGridLayout(panelBounds, selfGrid);
    m_selfTradeSlotBounds.clear();
    m_selfTradeSlotBounds.reserve(selfLayout.slotBounds.size());
    for (const struct nk_rect& slotBounds : selfLayout.slotBounds) {
        m_selfTradeSlotBounds.push_back(
            TradeSlotRect{slotBounds.x, slotBounds.y, slotBounds.w, slotBounds.h});
    }

    if (trade->phase != Engine::ECS::TradePhase::Open || trade->selfLocked) {
        // Still allow trade action buttons below.
    }

    const float mouseX = ctx->input.mouse.pos.x;
    const float mouseY = ctx->input.mouse.pos.y;
    const bool mouseReleased = nk_input_is_mouse_released(&ctx->input, NK_BUTTON_LEFT);
    const TradeChromeLayout chrome = ComputeTradeChromeLayout(panelBounds, trade->phase);

    const auto releasedIn = [&](const struct nk_rect& bounds) {
        return mouseReleased && PointInRect(bounds, mouseX, mouseY);
    };

    if (!m_tradeCommandCallback) {
        return;
    }

    if (mouseReleased) {
        if (trade->phase == Engine::ECS::TradePhase::Open) {
            if (!trade->selfLocked && releasedIn(chrome.selfAcceptButton)) {
                Engine::Net::TradeCommand command;
                command.action = Engine::Net::TradeAction::Lock;
                command.targetUserId = trade->partnerUserId;
                m_tradeCommandCallback(command);
                return;
            }

            if (!trade->selfLocked) {
                for (size_t slotIndex = 0; slotIndex < selfLayout.slotBounds.size(); ++slotIndex) {
                    if (!PointInRect(selfLayout.slotBounds[slotIndex], mouseX, mouseY)) {
                        continue;
                    }
                    if (slotIndex < trade->selfOffer.size()) {
                        TryRemoveTradeOfferAtSlot(static_cast<int>(slotIndex));
                    }
                    return;
                }
            }
        }
    }
}

void CharacterMenu::DrawTradeGrids(
    struct nk_context* ctx,
    struct nk_command_buffer* canvas,
    const struct nk_rect& panelBounds,
    const BagItemGridConfig& selfGrid,
    const BagItemGridConfig& partnerGrid,
    const Engine::Platform::Render::Texture* slotTexture,
    const Engine::ECS::PlayerTradeComponent* trade,
    int screenWidth,
    int screenHeight) {
    if (!ctx || !canvas || !trade) {
        return;
    }

    const BagGridLayout selfLayout = ComputeBagGridLayout(panelBounds, selfGrid);
    const BagGridLayout partnerLayout = ComputeBagGridLayout(panelBounds, partnerGrid);
    const bool drawSlotTexture = slotTexture && slotTexture->IsLoaded();
    const float slotAlpha = std::clamp(selfGrid.slotAlpha, 0.0f, 1.0f);
    const unsigned char slotAlphaByte =
        static_cast<unsigned char>(std::lround(slotAlpha * 255.0f));
    const struct nk_color slotFill = nk_rgba(18, 22, 34, 170);
    const struct nk_color slotBorder = nk_rgba(95, 110, 145, 200);
    const struct nk_color textColor = nk_rgba(225, 232, 245, 255);
    const struct nk_color qtyColor = nk_rgba(170, 190, 230, 255);
    const struct nk_color transparent = nk_rgba(0, 0, 0, 0);
    const struct nk_user_font* font = ctx->style.font;

    auto drawOfferGrid = [&](const BagGridLayout& layout,
                             const std::vector<Engine::ECS::TradeOfferItem>& offer,
                             bool interactive) {
        for (int slotIndex = 0; slotIndex < layout.slotCount; ++slotIndex) {
            const struct nk_rect slotBounds = layout.slotBounds[static_cast<size_t>(slotIndex)];
            if (drawSlotTexture && slotAlphaByte > 0) {
                Engine::Platform::UI::DrawTexture(
                    canvas, *slotTexture, slotBounds, nk_rgba(255, 255, 255, slotAlphaByte));
            } else {
                nk_fill_rect(canvas, slotBounds, 4.0f, slotFill);
                nk_stroke_rect(canvas, slotBounds, 4.0f, 1.0f, slotBorder);
            }

            if (slotIndex >= static_cast<int>(offer.size()) || !font || !m_gameItemCatalog) {
                continue;
            }

            const Engine::ECS::TradeOfferItem& offerItem =
                offer[static_cast<size_t>(slotIndex)];
            const Game::Character::GameItemDefinition* definition =
                m_gameItemCatalog->Find(offerItem.itemId);
            if (!definition) {
                continue;
            }

            const Engine::Platform::Render::Texture* iconTexture =
                GetItemIconTexture(definition->numericId);
            if (iconTexture && iconTexture->IsLoaded()) {
                const float iconPadding = slotBounds.w * 0.12f;
                const struct nk_rect iconBounds = nk_rect(slotBounds.x + iconPadding,
                                                          slotBounds.y + iconPadding,
                                                          slotBounds.w - iconPadding * 2.0f,
                                                          slotBounds.h - iconPadding * 2.0f);
                Engine::Platform::UI::DrawTexture(
                    canvas, *iconTexture, iconBounds, nk_rgba(255, 255, 255, 255));
            }

            if (offerItem.quantity > 1) {
                const std::string qty = std::to_string(offerItem.quantity);
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

            if (interactive &&
                PointInRect(slotBounds, ctx->input.mouse.pos.x, ctx->input.mouse.pos.y) &&
                font) {
                const std::string& label = definition->displayName;
                const int labelLen = static_cast<int>(std::min(label.size(), size_t{24}));
                const float textW =
                    font->width(font->userdata, font->height, label.c_str(), labelLen);
                Engine::Platform::UI::PresentItemTooltip(
                    ctx,
                    canvas,
                    slotBounds,
                    screenWidth,
                    screenHeight,
                    BuildItemTooltipContent(
                        m_gameItemCatalog->CreateInventoryItem(definition->numericId, 1)));
            }
            (void)textColor;
        }
    };

    drawOfferGrid(selfLayout, trade->selfOffer, true);
    drawOfferGrid(partnerLayout, trade->partnerOffer, false);

    if (!font) {
        return;
    }

    const TradeChromeLayout chrome = ComputeTradeChromeLayout(panelBounds, trade->phase);

    const struct nk_color buttonText = nk_rgba(230, 236, 248, 255);
    const struct nk_color buttonTextPressed = nk_rgba(150, 220, 168, 255);
    const struct nk_color nameColor = nk_rgba(225, 232, 245, 255);
    const struct nk_color nameAcceptedColor = nk_rgba(150, 220, 168, 255);

    auto drawCenteredLabel = [&](const struct nk_rect& bounds,
                                 const std::string& text,
                                 const struct nk_color& color) {
        if (text.empty()) {
            return;
        }
        const int labelLen = static_cast<int>(std::min(text.size(), size_t{28}));
        const float textW =
            font->width(font->userdata, font->height, text.c_str(), labelLen);
        nk_draw_text(canvas,
                     nk_rect(bounds.x + (bounds.w - textW) * 0.5f,
                             bounds.y + (bounds.h - font->height) * 0.5f,
                             textW,
                             font->height),
                     text.c_str(),
                     labelLen,
                     font,
                     transparent,
                     color);
    };

    auto drawLeftLabel = [&](const struct nk_rect& bounds,
                             const std::string& text,
                             const struct nk_color& color) {
        if (text.empty()) {
            return;
        }
        const int labelLen = static_cast<int>(std::min(text.size(), size_t{28}));
        nk_draw_text(canvas,
                     nk_rect(bounds.x,
                             bounds.y + (bounds.h - font->height) * 0.5f,
                             bounds.w,
                             font->height),
                     text.c_str(),
                     labelLen,
                     font,
                     transparent,
                     color);
    };

    auto drawRightLabel = [&](const struct nk_rect& bounds,
                              const std::string& text,
                              const struct nk_color& color) {
        if (text.empty()) {
            return;
        }
        const int labelLen = static_cast<int>(std::min(text.size(), size_t{28}));
        const float textW =
            font->width(font->userdata, font->height, text.c_str(), labelLen);
        nk_draw_text(canvas,
                     nk_rect(bounds.x + bounds.w - textW,
                             bounds.y + (bounds.h - font->height) * 0.5f,
                             textW,
                             font->height),
                     text.c_str(),
                     labelLen,
                     font,
                     transparent,
                     color);
    };

    auto drawTradeButtonLabel = [&](const struct nk_rect& bounds, bool pressed, const char* label) {
        const float inset = pressed ? 2.0f : 0.0f;
        const struct nk_rect drawBounds =
            nk_rect(bounds.x + inset, bounds.y + inset, bounds.w - inset, bounds.h - inset);
        drawCenteredLabel(drawBounds, label, pressed ? buttonTextPressed : buttonText);
    };

    const std::string& selfName =
        m_tradeSelfDisplayName.empty() ? std::string("You") : m_tradeSelfDisplayName;
    const std::string& partnerName = m_tradePartnerDisplayName.empty()
                                         ? std::string("Partner")
                                         : m_tradePartnerDisplayName;

    const float nameLabelH = static_cast<float>(font->height) + 6.0f;
    const float nameTopY = panelBounds.y + 10.0f;
    const float namePad = 14.0f;
    const float nameHalfW = (panelBounds.w - namePad * 2.0f) * 0.5f;
    const struct nk_rect selfNameBounds =
        nk_rect(panelBounds.x + namePad, nameTopY, nameHalfW, nameLabelH);
    const struct nk_rect partnerNameBounds =
        nk_rect(panelBounds.x + namePad + nameHalfW, nameTopY, nameHalfW, nameLabelH);
    drawLeftLabel(selfNameBounds,
                  selfName,
                  trade->selfLocked ? nameAcceptedColor : nameColor);
    drawRightLabel(partnerNameBounds,
                   partnerName,
                   trade->partnerLocked ? nameAcceptedColor : nameColor);

    if (trade->phase == Engine::ECS::TradePhase::Open) {
        drawTradeButtonLabel(chrome.selfAcceptButton, trade->selfLocked, "Accept");
        drawTradeButtonLabel(chrome.partnerAcceptButton, trade->partnerLocked, "Accept");
    }
    (void)slotBorder;
    (void)textColor;
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

void CharacterMenu::SetInventoryActionCallback(
    std::function<void(BagItemAction action, int slotIndex)> callback) {
    m_inventoryActionCallback = std::move(callback);
}

void CharacterMenu::SetInventoryCombinedUseCallback(
    std::function<void(int primarySlot, int secondarySlot)> callback) {
    m_inventoryCombinedUseCallback = std::move(callback);
}

void CharacterMenu::RequestInventorySync() {
    m_inventorySyncPending = true;
}

void CharacterMenu::CancelBagDrag() {
    if (m_bagDrag.active && m_inventoryRegistry && m_inventoryEntity != Engine::INVALID_ENTITY &&
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
    m_bagClickPending = false;
    m_bagPendingClickSlot = -1;
    ClearBagUseSelection();
    m_bagActionPanelOpen = false;
    m_bagActionPanelSlot = -1;
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

    if (m_bagActionPanelOpen || m_tradeQuantityPanelOpen) {
        return true;
    }

    const struct nk_rect panelBounds = nk_rect(panelX, panelY, panelW, panelH);

    auto* inventory =
        m_inventoryRegistry->GetComponent<Engine::Systems::Inventory::InventoryComponent>(
            m_inventoryEntity);
    if (!inventory) {
        return false;
    }

    if (m_bagSelectedUseSlot >= 0 &&
        !CanUseItemAtSlot(static_cast<size_t>(m_bagSelectedUseSlot))) {
        ClearBagUseSelection();
    }

    const BagGridLayout layout = ComputeBagGridLayout(panelBounds, grid);
    const float mouseX = ctx->input.mouse.pos.x;
    const float mouseY = ctx->input.mouse.pos.y;
    const bool mousePressed = nk_input_is_mouse_pressed(&ctx->input, NK_BUTTON_LEFT);
    const bool mouseReleased = nk_input_is_mouse_released(&ctx->input, NK_BUTTON_LEFT);
    const bool rightReleased = nk_input_is_mouse_released(&ctx->input, NK_BUTTON_RIGHT);
    const float snapRadius =
        layout.slotSize * std::clamp(grid.dropSnapFraction, 0.15f, 1.0f);
    constexpr float kDragStartPixels = 6.0f;

    bool pointerOverGrid = PointInRect(layout.contentBounds, mouseX, mouseY);
    if (!pointerOverGrid) {
        for (const struct nk_rect& slotBounds : layout.slotBounds) {
            if (PointInRect(slotBounds, mouseX, mouseY)) {
                pointerOverGrid = true;
                break;
            }
        }
    }

    if (rightReleased && pointerOverGrid) {
        bool openedPanel = false;
        for (size_t slotIndex = 0; slotIndex < layout.slotBounds.size(); ++slotIndex) {
            if (!PointInRect(layout.slotBounds[slotIndex], mouseX, mouseY)) {
                continue;
            }
            if (slotIndex >= inventory->slots.size() || inventory->slots[slotIndex].isEmpty) {
                continue;
            }
            m_bagActionPanelSlot = static_cast<int>(slotIndex);
            m_bagActionPanelAnchorX = layout.slotBounds[slotIndex].x;
            m_bagActionPanelAnchorY = layout.slotBounds[slotIndex].y;
            m_bagActionPanelAnchorW = layout.slotBounds[slotIndex].w;
            m_bagActionPanelAnchorH = layout.slotBounds[slotIndex].h;
            m_bagActionPanelOpen = true;
            ClearBagUseSelection();
            openedPanel = true;
            break;
        }
        if (!openedPanel) {
            m_bagActionPanelOpen = false;
            m_bagActionPanelSlot = -1;
        }
        return true;
    }

    if (m_bagDrag.active) {
        m_bagDrag.hoverDropSlot =
            FindNearestBagSlot(mouseX, mouseY, layout.slotBounds, snapRadius);

        if (mouseReleased) {
            bool placedInTrade = false;
            if (m_bagDrag.hoverDropSlot < 0 && m_tradePanelOpen) {
                placedInTrade = TryAddBagItemToTradeOffer(m_bagDrag.sourceSlot);
            }
            if (!placedInTrade) {
                if (m_bagDrag.hoverDropSlot >= 0) {
                    inventory->PlaceItemAtSlot(static_cast<size_t>(m_bagDrag.hoverDropSlot),
                                               m_bagDrag.item,
                                               static_cast<size_t>(m_bagDrag.sourceSlot));
                } else if (m_bagDrag.sourceSlot >= 0) {
                    inventory->PlaceItemAtSlot(static_cast<size_t>(m_bagDrag.sourceSlot),
                                               m_bagDrag.item,
                                               static_cast<size_t>(m_bagDrag.sourceSlot));
                }
                if (m_inventoryMutationCallback) {
                    m_inventoryMutationCallback();
                }
            } else if (m_bagDrag.sourceSlot >= 0) {
                inventory->PlaceItemAtSlot(static_cast<size_t>(m_bagDrag.sourceSlot),
                                           m_bagDrag.item,
                                           static_cast<size_t>(m_bagDrag.sourceSlot));
            }
            m_bagDrag = BagDragState{};
            m_lastBagLogSnapshot.clear();
        }

        return true;
    }

    if (m_bagClickPending && nk_input_is_mouse_down(&ctx->input, NK_BUTTON_LEFT)) {
        const float dragDx = mouseX - m_bagPendingClickMouseX;
        const float dragDy = mouseY - m_bagPendingClickMouseY;
        if (dragDx * dragDx + dragDy * dragDy >= kDragStartPixels * kDragStartPixels &&
            m_bagPendingClickSlot >= 0) {
            const size_t slotIndex = static_cast<size_t>(m_bagPendingClickSlot);
            if (slotIndex < inventory->slots.size() && !inventory->slots[slotIndex].isEmpty &&
                inventory->TakeItemFromSlot(slotIndex, m_bagDrag.item)) {
                m_bagDrag.active = true;
                m_bagDrag.sourceSlot = m_bagPendingClickSlot;
                m_bagDrag.hoverDropSlot = -1;
                ClearBagUseSelection();
            }
            m_bagClickPending = false;
            m_bagPendingClickSlot = -1;
            return true;
        }
    }

    if (mousePressed && pointerOverGrid) {
        m_bagClickPending = true;
        m_bagPendingClickSlot = -1;
        m_bagPendingClickMouseX = mouseX;
        m_bagPendingClickMouseY = mouseY;

        for (size_t slotIndex = 0; slotIndex < layout.slotBounds.size(); ++slotIndex) {
            if (!PointInRect(layout.slotBounds[slotIndex], mouseX, mouseY)) {
                continue;
            }
            if (slotIndex < inventory->slots.size() && !inventory->slots[slotIndex].isEmpty) {
                m_bagPendingClickSlot = static_cast<int>(slotIndex);
            }
            return true;
        }
    }

    if (mouseReleased && m_bagClickPending) {
        m_bagClickPending = false;
        if (m_bagPendingClickSlot >= 0) {
            if (!(CanModifyTradeOffer() &&
                  TryAddBagItemToTradeOffer(m_bagPendingClickSlot))) {
                HandleBagUseSlotClick(m_bagPendingClickSlot);
            }
        } else {
            ClearBagUseSelection();
        }
        m_bagPendingClickSlot = -1;
        return true;
    }

    return pointerOverGrid;
}

bool CharacterMenu::CanUseItemAtSlot(size_t slotIndex) const {
    if (!m_inventoryRegistry || m_inventoryEntity == Engine::INVALID_ENTITY || !m_gameItemCatalog) {
        return false;
    }

    const auto* inventory =
        m_inventoryRegistry->GetComponent<Engine::Systems::Inventory::InventoryComponent>(
            m_inventoryEntity);
    if (!inventory || slotIndex >= inventory->slots.size() || inventory->slots[slotIndex].isEmpty) {
        return false;
    }

    return m_gameItemCatalog->CanUseItem(inventory->slots[slotIndex].item.id);
}

void CharacterMenu::ClearBagUseSelection() {
    m_bagSelectedUseSlot = -1;
}

void CharacterMenu::HandleBagUseSlotClick(int slotIndex) {
    if (slotIndex < 0 || !CanUseItemAtSlot(static_cast<size_t>(slotIndex))) {
        ClearBagUseSelection();
        return;
    }

    if (m_bagSelectedUseSlot == slotIndex) {
        ClearBagUseSelection();
        return;
    }

    if (m_bagSelectedUseSlot >= 0) {
        const int primarySlot = m_bagSelectedUseSlot;
        const int secondarySlot = slotIndex;
        ClearBagUseSelection();
        PerformCombinedBagItemUse(primarySlot, secondarySlot);
        return;
    }

    m_bagSelectedUseSlot = slotIndex;
}

void CharacterMenu::PerformCombinedBagItemUse(int primarySlot, int secondarySlot) {
    if (primarySlot < 0 || secondarySlot < 0 || !m_inventoryCombinedUseCallback) {
        return;
    }

    CancelBagDrag();
    m_inventoryCombinedUseCallback(primarySlot, secondarySlot);
    m_lastBagLogSnapshot.clear();
}

void CharacterMenu::PerformBagItemAction(BagItemAction action, int slotIndex) {
    if (slotIndex < 0 || !m_inventoryActionCallback) {
        return;
    }

    CancelBagDrag();
    m_inventoryActionCallback(action, slotIndex);
    m_lastBagLogSnapshot.clear();
    if (action == BagItemAction::Drop) {
        ClearBagUseSelection();
    }
}

bool CharacterMenu::BuildBagActionPanelLayout(
    int screenWidth,
    int screenHeight,
    const struct nk_user_font* font,
    Engine::Platform::UI::ItemActionPanelLayout& outLayout,
    Engine::Platform::UI::ItemTooltipContent& outContent,
    bool& outShowUse,
    bool& outShowTrade,
    bool& outShowTradeX) const {
    outLayout = {};
    outContent = {};
    outShowUse = false;
    outShowTrade = false;
    outShowTradeX = false;

    if (!m_bagActionPanelOpen || m_bagActionPanelSlot < 0 || !m_inventoryRegistry ||
        m_inventoryEntity == Engine::INVALID_ENTITY) {
        return false;
    }

    const auto* inventory =
        m_inventoryRegistry->GetComponent<Engine::Systems::Inventory::InventoryComponent>(
            m_inventoryEntity);
    if (!inventory) {
        return false;
    }

    const size_t slotIndex = static_cast<size_t>(m_bagActionPanelSlot);
    if (slotIndex >= inventory->slots.size() || inventory->slots[slotIndex].isEmpty) {
        return false;
    }

    outContent = BuildItemTooltipContent(inventory->slots[slotIndex].item);
    if (outContent.name.empty()) {
        return false;
    }

    if (!font) {
        return false;
    }

    outShowUse = CanUseItemAtSlot(slotIndex);
    outShowTrade = CanShowTradeItemActions();
    outShowTradeX = outShowTrade;

    const struct nk_rect anchor = nk_rect(m_bagActionPanelAnchorX,
                                          m_bagActionPanelAnchorY,
                                          m_bagActionPanelAnchorW,
                                          m_bagActionPanelAnchorH);
    outLayout = Engine::Platform::UI::LayoutItemActionPanel(
        anchor,
        screenWidth,
        screenHeight,
        outContent,
        outShowUse,
        outShowTrade,
        outShowTradeX,
        font);
    return outLayout.panelBounds.w > 0.0f && outLayout.panelBounds.h > 0.0f;
}

bool CharacterMenu::UpdateBagItemActionPanel(struct nk_context* ctx,
                                           int screenWidth,
                                           int screenHeight) {
    if (!ctx || !m_bagActionPanelOpen || m_bagActionPanelSlot < 0) {
        return false;
    }

    const struct nk_user_font* font = ctx->style.font;
    if (!font) {
        return false;
    }

    Engine::Platform::UI::ItemActionPanelLayout panelLayout;
    Engine::Platform::UI::ItemTooltipContent content;
    bool showUse = false;
    bool showTrade = false;
    bool showTradeX = false;
    if (!BuildBagActionPanelLayout(
            screenWidth, screenHeight, font, panelLayout, content, showUse, showTrade, showTradeX)) {
        m_bagActionPanelOpen = false;
        m_bagActionPanelSlot = -1;
        return false;
    }

    const struct nk_rect panelBoundsNk =
        nk_rect(panelLayout.panelBounds.x,
                panelLayout.panelBounds.y,
                panelLayout.panelBounds.w,
                panelLayout.panelBounds.h);
    const struct nk_rect useButtonBoundsNk =
        nk_rect(panelLayout.useButtonBounds.x,
                panelLayout.useButtonBounds.y,
                panelLayout.useButtonBounds.w,
                panelLayout.useButtonBounds.h);
    const struct nk_rect dropButtonBoundsNk =
        nk_rect(panelLayout.dropButtonBounds.x,
                panelLayout.dropButtonBounds.y,
                panelLayout.dropButtonBounds.w,
                panelLayout.dropButtonBounds.h);
    const struct nk_rect tradeButtonBoundsNk =
        nk_rect(panelLayout.tradeButtonBounds.x,
                panelLayout.tradeButtonBounds.y,
                panelLayout.tradeButtonBounds.w,
                panelLayout.tradeButtonBounds.h);
    const struct nk_rect tradeXButtonBoundsNk =
        nk_rect(panelLayout.tradeXButtonBounds.x,
                panelLayout.tradeXButtonBounds.y,
                panelLayout.tradeXButtonBounds.w,
                panelLayout.tradeXButtonBounds.h);

    const float mouseX = ctx->input.mouse.pos.x;
    const float mouseY = ctx->input.mouse.pos.y;
    if (PointInRect(panelBoundsNk, mouseX, mouseY) ||
        nk_input_is_mouse_hovering_rect(&ctx->input, panelBoundsNk)) {
        // Consume pointer while the action menu is open.
    }

    if (!nk_input_is_mouse_released(&ctx->input, NK_BUTTON_LEFT)) {
        return PointInRect(panelBoundsNk, mouseX, mouseY);
    }

    const size_t slotIndex = static_cast<size_t>(m_bagActionPanelSlot);
    if (showUse && PointInRect(useButtonBoundsNk, mouseX, mouseY)) {
        m_bagActionPanelOpen = false;
        m_bagActionPanelSlot = -1;
        HandleBagUseSlotClick(static_cast<int>(slotIndex));
        return true;
    }
    if (showTrade && PointInRect(tradeButtonBoundsNk, mouseX, mouseY)) {
        m_bagActionPanelOpen = false;
        m_bagActionPanelSlot = -1;
        if (!TryAddBagItemToTradeOffer(static_cast<int>(slotIndex)) && !CanModifyTradeOffer()) {
            LOG_INFO("CharacterMenu", "Accept the trade before adding items");
        }
        return true;
    }
    if (showTradeX && PointInRect(tradeXButtonBoundsNk, mouseX, mouseY)) {
        if (CanModifyTradeOffer()) {
            OpenTradeQuantityPanel(static_cast<int>(slotIndex));
        } else {
            LOG_INFO("CharacterMenu", "Accept the trade before adding items");
            m_bagActionPanelOpen = false;
            m_bagActionPanelSlot = -1;
        }
        return true;
    }
    if (PointInRect(dropButtonBoundsNk, mouseX, mouseY)) {
        m_bagActionPanelOpen = false;
        m_bagActionPanelSlot = -1;
        PerformBagItemAction(BagItemAction::Drop, static_cast<int>(slotIndex));
        return true;
    }
    if (!PointInRect(panelBoundsNk, mouseX, mouseY)) {
        m_bagActionPanelOpen = false;
        m_bagActionPanelSlot = -1;
    }
    return PointInRect(panelBoundsNk, mouseX, mouseY);
}

void CharacterMenu::DrawBagItemActionPanel(struct nk_context* ctx,
                                           struct nk_command_buffer* canvas,
                                           int screenWidth,
                                           int screenHeight) {
    if (!ctx || !canvas || !m_bagActionPanelOpen || m_bagActionPanelSlot < 0) {
        return;
    }

    const struct nk_user_font* font = ctx->style.font;
    if (!font) {
        return;
    }

    Engine::Platform::UI::ItemActionPanelLayout panelLayout;
    Engine::Platform::UI::ItemTooltipContent content;
    bool showUse = false;
    bool showTrade = false;
    bool showTradeX = false;
    if (!BuildBagActionPanelLayout(
            screenWidth, screenHeight, font, panelLayout, content, showUse, showTrade, showTradeX)) {
        return;
    }

    const struct nk_rect useButtonBoundsNk =
        nk_rect(panelLayout.useButtonBounds.x,
                panelLayout.useButtonBounds.y,
                panelLayout.useButtonBounds.w,
                panelLayout.useButtonBounds.h);
    const struct nk_rect dropButtonBoundsNk =
        nk_rect(panelLayout.dropButtonBounds.x,
                panelLayout.dropButtonBounds.y,
                panelLayout.dropButtonBounds.w,
                panelLayout.dropButtonBounds.h);
    const struct nk_rect tradeButtonBoundsNk =
        nk_rect(panelLayout.tradeButtonBounds.x,
                panelLayout.tradeButtonBounds.y,
                panelLayout.tradeButtonBounds.w,
                panelLayout.tradeButtonBounds.h);
    const struct nk_rect tradeXButtonBoundsNk =
        nk_rect(panelLayout.tradeXButtonBounds.x,
                panelLayout.tradeXButtonBounds.y,
                panelLayout.tradeXButtonBounds.w,
                panelLayout.tradeXButtonBounds.h);

    const bool hoverUse =
        showUse && nk_input_is_mouse_hovering_rect(&ctx->input, useButtonBoundsNk);
    const bool hoverTrade =
        showTrade && nk_input_is_mouse_hovering_rect(&ctx->input, tradeButtonBoundsNk);
    const bool hoverTradeX =
        showTradeX && nk_input_is_mouse_hovering_rect(&ctx->input, tradeXButtonBoundsNk);
    const bool hoverDrop =
        nk_input_is_mouse_hovering_rect(&ctx->input, dropButtonBoundsNk);

    Engine::Platform::UI::DrawItemActionPanel(
        canvas,
        panelLayout,
        content,
        showUse,
        showTrade,
        showTradeX,
        font,
        hoverUse,
        hoverTrade,
        hoverTradeX,
        hoverDrop);
}

bool CharacterMenu::UpdateTradeQuantityPanel(struct nk_context* ctx,
                                             int screenWidth,
                                             int screenHeight) {
    if (!ctx || !m_tradeQuantityPanelOpen || m_tradeQuantitySlot < 0 ||
        !m_inventoryRegistry || m_inventoryEntity == Engine::INVALID_ENTITY) {
        return false;
    }

    const auto* inventory =
        m_inventoryRegistry->GetComponent<Engine::Systems::Inventory::InventoryComponent>(
            m_inventoryEntity);
    if (!inventory) {
        CloseTradeQuantityPanel();
        return false;
    }

    const size_t slotIndex = static_cast<size_t>(m_tradeQuantitySlot);
    if (slotIndex >= inventory->slots.size() || inventory->slots[slotIndex].isEmpty) {
        CloseTradeQuantityPanel();
        return false;
    }

    const struct nk_user_font* font = ctx->style.font;
    if (!font) {
        return false;
    }

    const int ownedQty = std::max(1, static_cast<int>(inventory->slots[slotIndex].item.quantity));
    m_tradeQuantityMax = ownedQty;
    m_tradeQuantityValue = std::clamp(m_tradeQuantityValue, 1, m_tradeQuantityMax);

    const float panelW = 220.0f;
    const float rowH = static_cast<float>(font->height) + 12.0f;
    const float panelH = rowH * 4.0f + 28.0f;
    float panelX = m_tradeQuantityAnchorX + m_tradeQuantityAnchorW * 0.5f - panelW * 0.5f;
    float panelY = m_tradeQuantityAnchorY - panelH - 8.0f;
    if (panelY < 8.0f) {
        panelY = m_tradeQuantityAnchorY + m_tradeQuantityAnchorH + 8.0f;
    }
    panelX = std::clamp(panelX, 8.0f, static_cast<float>(screenWidth) - panelW - 8.0f);
    panelY = std::clamp(panelY, 8.0f, static_cast<float>(screenHeight) - panelH - 8.0f);

    const struct nk_rect panelBounds = nk_rect(panelX, panelY, panelW, panelH);
    const float controlY = panelBounds.y + rowH + 8.0f;
    const float buttonW = 28.0f;
    const float qtyFieldW = 72.0f;
    const float controlGap = 8.0f;
    const float controlsTotalW = buttonW + controlGap + qtyFieldW + controlGap + buttonW;
    const float controlsX = panelBounds.x + (panelBounds.w - controlsTotalW) * 0.5f;

    const struct nk_rect minusBounds = nk_rect(controlsX, controlY, buttonW, rowH);
    const struct nk_rect qtyBounds =
        nk_rect(controlsX + buttonW + controlGap, controlY, qtyFieldW, rowH);
    const struct nk_rect plusBounds =
        nk_rect(controlsX + buttonW + controlGap + qtyFieldW + controlGap, controlY, buttonW, rowH);

    const float actionY = panelBounds.y + panelH - rowH - 10.0f;
    const float actionW = (panelBounds.w - 30.0f) * 0.5f;
    const struct nk_rect confirmBounds = nk_rect(panelBounds.x + 10.0f, actionY, actionW, rowH);
    const struct nk_rect cancelBounds =
        nk_rect(panelBounds.x + panelBounds.w - actionW - 10.0f, actionY, actionW, rowH);

    const float mouseX = ctx->input.mouse.pos.x;
    const float mouseY = ctx->input.mouse.pos.y;
    if (!nk_input_is_mouse_released(&ctx->input, NK_BUTTON_LEFT)) {
        return PointInRect(panelBounds, mouseX, mouseY);
    }

    if (PointInRect(minusBounds, mouseX, mouseY)) {
        m_tradeQuantityValue = std::max(1, m_tradeQuantityValue - 1);
        return true;
    }
    if (PointInRect(plusBounds, mouseX, mouseY)) {
        m_tradeQuantityValue = std::min(m_tradeQuantityMax, m_tradeQuantityValue + 1);
        return true;
    }
    if (PointInRect(qtyBounds, mouseX, mouseY)) {
        m_tradeQuantityValue = std::min(m_tradeQuantityMax, m_tradeQuantityValue + 1);
        if (m_tradeQuantityValue >= m_tradeQuantityMax) {
            m_tradeQuantityValue = 1;
        }
        return true;
    }
    if (PointInRect(confirmBounds, mouseX, mouseY)) {
        const int slot = m_tradeQuantitySlot;
        const uint32_t qty = static_cast<uint32_t>(
            std::clamp(m_tradeQuantityValue, 1, m_tradeQuantityMax));
        CloseTradeQuantityPanel();
        TryAddBagItemToTradeOffer(slot, qty);
        return true;
    }
    if (PointInRect(cancelBounds, mouseX, mouseY) ||
        !PointInRect(panelBounds, mouseX, mouseY)) {
        CloseTradeQuantityPanel();
        return true;
    }
    return PointInRect(panelBounds, mouseX, mouseY);
}

void CharacterMenu::DrawTradeQuantityPanel(struct nk_context* ctx,
                                           struct nk_command_buffer* canvas,
                                           int screenWidth,
                                           int screenHeight) {
    if (!ctx || !canvas || !m_tradeQuantityPanelOpen || m_tradeQuantitySlot < 0 ||
        !m_inventoryRegistry || m_inventoryEntity == Engine::INVALID_ENTITY) {
        return;
    }

    const auto* inventory =
        m_inventoryRegistry->GetComponent<Engine::Systems::Inventory::InventoryComponent>(
            m_inventoryEntity);
    if (!inventory) {
        CloseTradeQuantityPanel();
        return;
    }

    const size_t slotIndex = static_cast<size_t>(m_tradeQuantitySlot);
    if (slotIndex >= inventory->slots.size() || inventory->slots[slotIndex].isEmpty) {
        CloseTradeQuantityPanel();
        return;
    }

    const Engine::Systems::Inventory::Item& item = inventory->slots[slotIndex].item;
    const Engine::Platform::UI::ItemTooltipContent content = BuildItemTooltipContent(item);
    const struct nk_user_font* font = ctx->style.font;
    if (!font || content.name.empty()) {
        CloseTradeQuantityPanel();
        return;
    }

    const int ownedQty = std::max(1, static_cast<int>(item.quantity));
    m_tradeQuantityMax = ownedQty;
    m_tradeQuantityValue = std::clamp(m_tradeQuantityValue, 1, m_tradeQuantityMax);

    const float panelW = 220.0f;
    const float rowH = static_cast<float>(font->height) + 12.0f;
    const float panelH = rowH * 4.0f + 28.0f;
    float panelX = m_tradeQuantityAnchorX + m_tradeQuantityAnchorW * 0.5f - panelW * 0.5f;
    float panelY = m_tradeQuantityAnchorY - panelH - 8.0f;
    if (panelY < 8.0f) {
        panelY = m_tradeQuantityAnchorY + m_tradeQuantityAnchorH + 8.0f;
    }
    panelX = std::clamp(panelX, 8.0f, static_cast<float>(screenWidth) - panelW - 8.0f);
    panelY = std::clamp(panelY, 8.0f, static_cast<float>(screenHeight) - panelH - 8.0f);

    const struct nk_rect panelBounds = nk_rect(panelX, panelY, panelW, panelH);
    const struct nk_color background = nk_rgba(16, 20, 32, 230);
    const struct nk_color border = nk_rgba(120, 140, 180, 200);
    const struct nk_color buttonFill = nk_rgba(34, 42, 58, 230);
    const struct nk_color buttonHoverFill = nk_rgba(52, 66, 92, 240);
    const struct nk_color buttonText = nk_rgba(225, 232, 245, 255);
    const struct nk_color titleColor = nk_rgba(235, 240, 250, 255);
    const struct nk_color hintColor = nk_rgba(150, 175, 210, 255);
    const struct nk_color transparent = nk_rgba(0, 0, 0, 0);

    nk_fill_rect(canvas, panelBounds, 6.0f, background);
    nk_stroke_rect(canvas, panelBounds, 6.0f, 1.0f, border);

    const std::string title = "Trade " + content.name;
    const int titleLen = static_cast<int>(std::min(title.size(), size_t{32}));
    nk_draw_text(canvas,
                 nk_rect(panelBounds.x + 10.0f, panelBounds.y + 8.0f, panelBounds.w - 20.0f, rowH),
                 title.c_str(),
                 titleLen,
                 font,
                 transparent,
                 titleColor);

    const float controlY = panelBounds.y + rowH + 8.0f;
    const float buttonW = 28.0f;
    const float qtyFieldW = 72.0f;
    const float controlGap = 8.0f;
    const float controlsTotalW = buttonW + controlGap + qtyFieldW + controlGap + buttonW;
    const float controlsX = panelBounds.x + (panelBounds.w - controlsTotalW) * 0.5f;

    const struct nk_rect minusBounds = nk_rect(controlsX, controlY, buttonW, rowH);
    const struct nk_rect qtyBounds =
        nk_rect(controlsX + buttonW + controlGap, controlY, qtyFieldW, rowH);
    const struct nk_rect plusBounds =
        nk_rect(controlsX + buttonW + controlGap + qtyFieldW + controlGap, controlY, buttonW, rowH);

    const bool hoverMinus = nk_input_is_mouse_hovering_rect(&ctx->input, minusBounds);
    const bool hoverPlus = nk_input_is_mouse_hovering_rect(&ctx->input, plusBounds);

    const auto drawButton = [&](const struct nk_rect& bounds, const char* label, bool hover) {
        nk_fill_rect(canvas, bounds, 5.0f, hover ? buttonHoverFill : buttonFill);
        nk_stroke_rect(canvas, bounds, 5.0f, 1.0f, border);
        const int labelLen = static_cast<int>(std::strlen(label));
        const float textW = font->width(font->userdata, font->height, label, labelLen);
        nk_draw_text(canvas,
                     nk_rect(bounds.x + (bounds.w - textW) * 0.5f,
                             bounds.y + (bounds.h - font->height) * 0.5f,
                             textW,
                             font->height),
                     label,
                     labelLen,
                     font,
                     transparent,
                     buttonText);
    };

    drawButton(minusBounds, "-", hoverMinus);
    drawButton(plusBounds, "+", hoverPlus);

    nk_fill_rect(canvas, qtyBounds, 5.0f, nk_rgba(24, 30, 42, 240));
    nk_stroke_rect(canvas, qtyBounds, 5.0f, 1.0f, border);
    const std::string qtyText = std::to_string(m_tradeQuantityValue);
    const int qtyLen = static_cast<int>(qtyText.size());
    const float qtyTextW = font->width(font->userdata, font->height, qtyText.c_str(), qtyLen);
    nk_draw_text(canvas,
                 nk_rect(qtyBounds.x + (qtyBounds.w - qtyTextW) * 0.5f,
                         qtyBounds.y + (qtyBounds.h - font->height) * 0.5f,
                         qtyTextW,
                         font->height),
                 qtyText.c_str(),
                 qtyLen,
                 font,
                 transparent,
                 buttonText);

    const std::string maxHint = "Owned: " + std::to_string(m_tradeQuantityMax);
    const int maxHintLen = static_cast<int>(maxHint.size());
    const float maxHintW = font->width(font->userdata, font->height, maxHint.c_str(), maxHintLen);
    nk_draw_text(canvas,
                 nk_rect(panelBounds.x + (panelBounds.w - maxHintW) * 0.5f,
                         controlY + rowH + 4.0f,
                         maxHintW,
                         font->height),
                 maxHint.c_str(),
                 maxHintLen,
                 font,
                 transparent,
                 hintColor);

    const float actionY = panelBounds.y + panelH - rowH - 10.0f;
    const float actionW = (panelBounds.w - 30.0f) * 0.5f;
    const struct nk_rect confirmBounds = nk_rect(panelBounds.x + 10.0f, actionY, actionW, rowH);
    const struct nk_rect cancelBounds =
        nk_rect(panelBounds.x + panelBounds.w - actionW - 10.0f, actionY, actionW, rowH);
    const bool hoverConfirm = nk_input_is_mouse_hovering_rect(&ctx->input, confirmBounds);
    const bool hoverCancel = nk_input_is_mouse_hovering_rect(&ctx->input, cancelBounds);
    drawButton(confirmBounds, "Trade", hoverConfirm);
    drawButton(cancelBounds, "Cancel", hoverCancel);
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

void CharacterMenu::OpenInspectPaperdoll(Engine::ECS::Registry* registry,
                                         Engine::EntityID characterEntity,
                                         Engine::Platform::Render::Shader* shader,
                                         Game::Character::CharacterInstance* characterInstance,
                                         const std::string& displayName) {
    m_inspectPaperdollRegistry = registry;
    m_inspectCharacterEntity = characterEntity;
    m_inspectShader = shader;
    m_inspectCharacterInstance = characterInstance;
    m_inspectDisplayName = displayName;
    m_inspectPaperdollOpen = true;
    if (!m_inspectPaperdollRenderer) {
        m_inspectPaperdollRenderer = std::make_unique<Game::Character::CharacterPaperdollRenderer>();
    }
    if (LoadedOverlayPanel* inspectPanel = FindOverlayPanel("inspect")) {
        inspectPanel->positionInitialized = false;
    }
    LOG_INFO_FMT("CharacterMenu", "Inspect paperdoll opened for '%s'", displayName.c_str());
}

void CharacterMenu::CloseInspectPaperdoll() {
    m_inspectPaperdollOpen = false;
    m_inspectDisplayName.clear();
    m_inspectCharacterEntity = Engine::INVALID_ENTITY;
    m_inspectCharacterInstance = nullptr;
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
                          const Engine::Platform::UI::MenuStyle& style,
                          Engine::Platform::UI::UISystem& uiSystem,
                          int screenWidth,
                          int screenHeight) {
    BuildHudChrome(ctx, screenWidth, screenHeight);
    BuildOpenPanels(ctx, style, uiSystem, screenWidth, screenHeight);
}

bool CharacterMenu::TryGetBottomBarLayout(int screenWidth,
                                          int screenHeight,
                                          MenuScreenLayout& outLayout,
                                          struct nk_rect& outBarBounds,
                                          float& outDrawW,
                                          float& outDrawH,
                                          float& outBarX,
                                          float& outBarY) const {
    if (screenWidth <= 0 || screenHeight <= 0 || !m_bottomBarTexture ||
        !m_bottomBarTexture->IsLoaded()) {
        return false;
    }

    const int texW = m_bottomBarTexture->GetWidth();
    const int texH = m_bottomBarTexture->GetHeight();
    if (texW <= 0 || texH <= 0) {
        return false;
    }

    const float widthFraction = std::clamp(m_config.bottomBarWidthFraction, 0.1f, 1.0f);
    outLayout = MenuScreenLayout::From(screenWidth, screenHeight, m_config.ReferenceScreen());
    outDrawW = outLayout.ScaledReferenceWidth(widthFraction);
    outDrawH = outDrawW * (static_cast<float>(texH) / static_cast<float>(texW));
    outBarX = outLayout.CenteredX(outDrawW);
    outBarY = outLayout.screenHeight - outDrawH;
    outBarBounds = nk_rect(outBarX, outBarY, outDrawW, outDrawH);
    return true;
}

void CharacterMenu::BuildHudChrome(struct nk_context* ctx, int screenWidth, int screenHeight) {
    if (!ctx || !m_visible) {
        return;
    }

    MenuScreenLayout layout;
    struct nk_rect barBounds{};
    float drawW = 0.0f;
    float drawH = 0.0f;
    float barX = 0.0f;
    float barY = 0.0f;
    if (!TryGetBottomBarLayout(screenWidth, screenHeight, layout, barBounds, drawW, drawH, barX, barY)) {
        return;
    }

    m_pointerOverBarUi = false;

    Engine::Platform::UI::OverlayScope overlay(
        ctx,
        "CharacterMenuHud",
        Engine::Platform::UI::OverlayScope::FullScreen(screenWidth, screenHeight));
    if (!overlay.IsOpen() || !overlay.Canvas()) {
        return;
    }

    struct nk_command_buffer* canvas = overlay.Canvas();
    Engine::Platform::UI::DrawTexture(
        canvas, *m_bottomBarTexture, barBounds, nk_rgba(255, 255, 255, 255));

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
        const int iconTexW = texture->GetWidth();
        const int iconTexH = texture->GetHeight();
        const float aspect =
            iconTexH > 0 ? static_cast<float>(iconTexW) / static_cast<float>(iconTexH) : 1.0f;
        const float iconWidth = iconHeight * aspect;
        const float yFraction =
            button.yFraction >= 0.0f ? button.yFraction : m_config.barButtonYFraction;
        const float iconCenterY = barY + drawH * std::clamp(yFraction, 0.0f, 1.0f);
        const float iconTop = iconCenterY - iconHeight * 0.5f;
        const float iconCenterX = barX + drawW * button.xFraction;
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
            canvas, *texture, buttonBounds, nk_rgba(255, 255, 255, 255));
    }
}

void CharacterMenu::BuildOpenPanels(struct nk_context* ctx,
                                    const Engine::Platform::UI::MenuStyle& style,
                                    Engine::Platform::UI::UISystem& uiSystem,
                                    int screenWidth,
                                    int screenHeight) {
    if (!ctx || !m_visible || screenWidth <= 0 || screenHeight <= 0) {
        return;
    }

    const MenuScreenLayout layout =
        MenuScreenLayout::From(screenWidth, screenHeight, m_config.ReferenceScreen());

    m_pointerOverPanelUi = false;

    Engine::Platform::UI::OverlayScope overlay(
        ctx,
        "CharacterMenuPanels",
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
        const bool tradePanelVisible =
            panel.config.buttonId == "trade" && m_tradePanelOpen;
        const bool inspectPanelVisible =
            panel.config.buttonId == "inspect" && m_inspectPaperdollOpen;
        const bool barPanelVisible = barButton && barButton->pressed;
        if (!tradePanelVisible && !inspectPanelVisible && !barPanelVisible) {
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
            } else if (panel.config.hasTradeGrids) {
                const BagGridLayout selfGridLayout =
                    ComputeBagGridLayout(panelBounds, panel.config.selfTradeGrid);
                const BagGridLayout partnerGridLayout =
                    ComputeBagGridLayout(panelBounds, panel.config.partnerTradeGrid);
                if (PointInRect(selfGridLayout.contentBounds,
                                ctx->input.mouse.pos.x,
                                ctx->input.mouse.pos.y) ||
                    PointInRect(partnerGridLayout.contentBounds,
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
                m_bagDrag.active ||
                m_bagActionPanelOpen ||
                m_tradeQuantityPanelOpen) {
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

        if (panel.config.hasTradeGrids && m_tradePanelOpen) {
            const Engine::ECS::PlayerTradeComponent* trade =
                m_tradeRegistry && m_tradeEntity != Engine::INVALID_ENTITY
                    ? m_tradeRegistry->GetComponent<Engine::ECS::PlayerTradeComponent>(
                          m_tradeEntity)
                    : nullptr;
            m_tradePanelX = panelBounds.x;
            m_tradePanelY = panelBounds.y;
            m_tradePanelW = panelBounds.w;
            m_tradePanelH = panelBounds.h;

            if (trade) {
                UpdateTradeInteractions(ctx, panelBounds, panel.config.selfTradeGrid, trade);
                if (m_bagDrag.active) {
                    m_pointerOverPanelUi = true;
                }
                DrawTradeGrids(ctx,
                               canvas,
                               panelBounds,
                               panel.config.selfTradeGrid,
                               panel.config.partnerTradeGrid,
                               panel.slotTexture.get(),
                               trade,
                               screenWidth,
                               screenHeight);
            }
        }

        if (panel.config.hasCharacterPreview) {
            const bool isInspectPanel = panel.config.buttonId == "inspect";
            Game::Character::CharacterPaperdollRenderer* paperdollRenderer = nullptr;
            Engine::ECS::Registry* paperdollRegistry = nullptr;
            Engine::EntityID paperdollEntity = Engine::INVALID_ENTITY;
            Game::Character::CharacterInstance* paperdollInstance = nullptr;
            Engine::Platform::Render::Shader* paperdollShader = nullptr;
            float* paperdollYawDegrees = nullptr;
            bool* hasRotateClock = nullptr;
            std::chrono::steady_clock::time_point* lastRotateTime = nullptr;

            if (isInspectPanel) {
                paperdollRenderer = m_inspectPaperdollRenderer.get();
                paperdollRegistry = m_inspectPaperdollRegistry;
                paperdollEntity = m_inspectCharacterEntity;
                paperdollInstance = m_inspectCharacterInstance;
                paperdollShader = m_inspectShader;
                paperdollYawDegrees = &m_inspectPaperdollYawDegrees;
                hasRotateClock = &m_hasInspectPaperdollRotateClock;
                lastRotateTime = &m_lastInspectPaperdollRotateTime;
            } else {
                paperdollRenderer = m_paperdollRenderer.get();
                paperdollRegistry = m_paperdollRegistry;
                paperdollEntity = m_paperdollCharacterEntity;
                paperdollInstance = m_paperdollCharacterInstance;
                paperdollShader = m_paperdollShader;
                paperdollYawDegrees = &m_paperdollYawDegrees;
                hasRotateClock = &m_hasPaperdollRotateClock;
                lastRotateTime = &m_lastPaperdollRotateTime;
            }

            if (isInspectPanel && !m_inspectDisplayName.empty()) {
                const struct nk_user_font* font = ctx->style.font;
                if (font) {
                    const struct nk_color transparent = nk_rgba(0, 0, 0, 0);
                    const struct nk_color nameColor = nk_rgba(225, 232, 245, 255);
                    const std::string& name = m_inspectDisplayName;
                    const int nameLen = static_cast<int>(std::min(name.size(), size_t{32}));
                    const float textW =
                        font->width(font->userdata, font->height, name.c_str(), nameLen);
                    const float nameTopY = panelBounds.y + 10.0f;
                    nk_draw_text(canvas,
                                 nk_rect(panelBounds.x + (panelBounds.w - textW) * 0.5f,
                                         nameTopY,
                                         textW,
                                         static_cast<float>(font->height)),
                                 name.c_str(),
                                 nameLen,
                                 font,
                                 transparent,
                                 nameColor);
                }
            }

            if (paperdollRenderer && paperdollRegistry && paperdollShader && paperdollInstance &&
                paperdollEntity != Engine::INVALID_ENTITY) {
                paperdollRenderer->SetYawDegrees(*paperdollYawDegrees);
                const struct nk_rect previewBounds =
                    CharacterPreviewBoundsFromPanel(panelBounds, panel.config);
                DrawCharacterPreviewOnPanel(canvas,
                                            previewBounds,
                                            *paperdollRenderer,
                                            *paperdollRegistry,
                                            paperdollEntity,
                                            *paperdollInstance,
                                            *paperdollShader);

                const PaperdollRotateButtonBounds liveRotateBounds =
                    PaperdollRotateBoundsFromPanel(panelBounds, m_config.paperdollRotate);

                const auto now = std::chrono::steady_clock::now();
                float deltaSeconds = 1.0f / 60.0f;
                if (*hasRotateClock) {
                    deltaSeconds =
                        std::chrono::duration<float>(now - *lastRotateTime).count();
                    deltaSeconds = std::clamp(deltaSeconds, 0.0f, 0.05f);
                }
                *lastRotateTime = now;
                *hasRotateClock = true;

                const float speed = std::max(0.0f, m_config.paperdollRotate.degreesPerSecond);
                const float step = speed * deltaSeconds;
                if (DrawPaperdollRotateButton(ctx,
                                              canvas,
                                              liveRotateBounds.left,
                                              m_paperdollRotateLeftTexture.get(),
                                              m_paperdollRotateLeftLightTexture.get(),
                                              m_pointerOverPanelUi)) {
                    *paperdollYawDegrees = NormalizeYawDegrees(*paperdollYawDegrees - step);
                    paperdollRenderer->SetYawDegrees(*paperdollYawDegrees);
                }
                if (DrawPaperdollRotateButton(ctx,
                                              canvas,
                                              liveRotateBounds.right,
                                              m_paperdollRotateRightTexture.get(),
                                              m_paperdollRotateRightLightTexture.get(),
                                              m_pointerOverPanelUi)) {
                    *paperdollYawDegrees = NormalizeYawDegrees(*paperdollYawDegrees + step);
                    paperdollRenderer->SetYawDegrees(*paperdollYawDegrees);
                }
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
            if (barButton) {
                barButton->pressed = false;
            }
            if (panel.config.buttonId == "trade") {
                DeclineAndCloseTradePanel();
            }
            if (panel.config.buttonId == "inspect") {
                CloseInspectPaperdoll();
            }
            panel.dragging = false;
            LOG_INFO_FMT("CharacterMenu", "Panel '%s' closed", panel.config.buttonId.c_str());
        }
    }

    DrawSettingsMenu(ctx, style, uiSystem, screenWidth, screenHeight);
}

void CharacterMenu::BuildPopups(struct nk_context* ctx, int screenWidth, int screenHeight) {
    if (!ctx || !m_visible) {
        return;
    }

    if (m_bagActionPanelOpen) {
        UpdateBagItemActionPanel(ctx, screenWidth, screenHeight);
    }
    if (m_tradeQuantityPanelOpen) {
        UpdateTradeQuantityPanel(ctx, screenWidth, screenHeight);
    }
    if (m_bagActionPanelOpen || m_tradeQuantityPanelOpen) {
        m_pointerOverPanelUi = true;
    }

    if (!m_bagActionPanelOpen && !m_tradeQuantityPanelOpen) {
        return;
    }

    Engine::Platform::UI::OverlayScope overlay(
        ctx,
        "CharacterMenuPopups",
        Engine::Platform::UI::OverlayScope::FullScreen(screenWidth, screenHeight));
    if (!overlay.IsOpen() || !overlay.Canvas()) {
        return;
    }

    struct nk_command_buffer* canvas = overlay.Canvas();
    DrawBagItemActionPanel(ctx, canvas, screenWidth, screenHeight);
    DrawTradeQuantityPanel(ctx, canvas, screenWidth, screenHeight);
}

void CharacterMenu::DrawSettingsMenu(struct nk_context* ctx,
                                     const Engine::Platform::UI::MenuStyle& style,
                                     Engine::Platform::UI::UISystem& uiSystem,
                                     int screenWidth,
                                     int screenHeight) {
    if (!ctx || screenWidth <= 0 || screenHeight <= 0) {
        return;
    }

    LoadedBarButton* settingsButton = FindBarButton("settings");
    if (!settingsButton || !settingsButton->pressed) {
        return;
    }

    const float panelW = style.metrics.menuButtonWidth + style.metrics.windowPaddingX * 2.0f + 80.0f;
    const float panelH = style.metrics.menuButtonHeight * 5.0f + style.metrics.itemSpacing * 4.0f +
                         style.metrics.titleHeight + 48.0f;
    const float x = std::max(16.0f, static_cast<float>(screenWidth) * 0.5f - panelW * 0.5f);
    const float y = std::max(16.0f, static_cast<float>(screenHeight) * 0.5f - panelH * 0.5f);

    if (nk_begin(
            ctx,
            "Settings",
            nk_rect(x, y, panelW, panelH),
            NK_WINDOW_BORDER | NK_WINDOW_TITLE | NK_WINDOW_MOVABLE | NK_WINDOW_CLOSABLE |
                NK_WINDOW_SCALABLE)) {
        m_pointerOverPanelUi = true;

        nk_layout_row_dynamic(ctx, style.metrics.menuButtonHeight, 1);
        static int showFps = 1;
        nk_checkbox_label(ctx, "Show FPS", &showFps);

        nk_layout_row_dynamic(ctx, style.metrics.itemSpacing, 1);
        nk_spacing(ctx, 1);

        static int masterVolumeEnabled = 1;
        nk_checkbox_label(ctx, "Master volume", &masterVolumeEnabled);

        nk_layout_row_dynamic(ctx, style.metrics.menuButtonHeight, 1);
        static float masterVolume = 0.75f;
        nk_slider_float(ctx, 0.0f, &masterVolume, 1.0f, 0.01f);

        nk_layout_row_dynamic(ctx, style.metrics.itemSpacing, 1);
        nk_spacing(ctx, 1);

        static int qualityIndex = 1;
        static const char* qualityOptions[] = {"Low", "Medium", "High"};
        nk_combo(ctx, qualityOptions, 3, qualityIndex, 24, nk_vec2(180, 200));

        nk_layout_row_dynamic(ctx, style.metrics.itemSpacing, 1);
        nk_spacing(ctx, 1);

        if (uiSystem.MenuButton(ctx, "Apply")) {
            LOG_INFO("CharacterMenu", "Settings apply clicked (placeholder)");
        }
    } else {
        settingsButton->pressed = false;
        LOG_INFO("CharacterMenu", "Settings menu closed");
    }
    nk_end(ctx);
}

} // namespace UI
} // namespace Game
