#pragma once

#include <algorithm>

namespace Game {
namespace UI {

// Layout reference resolution — UI element sizes are authored for this minimum screen.
struct MenuReferenceScreen {
    int width = 1280;
    int height = 720;
};

// Runtime layout derived from the actual viewport and a reference screen.
struct MenuScreenLayout {
    float screenWidth = 0.0f;
    float screenHeight = 0.0f;
    float referenceWidth = 1280.0f;
    float referenceHeight = 720.0f;
    float uiScale = 1.0f;
    float offsetX = 0.0f;
    float offsetY = 0.0f;

    static MenuScreenLayout From(int screenWidth, int screenHeight, const MenuReferenceScreen& reference) {
        MenuScreenLayout layout;
        layout.screenWidth = static_cast<float>(screenWidth);
        layout.screenHeight = static_cast<float>(screenHeight);
        layout.referenceWidth = static_cast<float>(reference.width);
        layout.referenceHeight = static_cast<float>(reference.height);

        if (screenWidth > 0 && screenHeight > 0 && reference.width > 0 && reference.height > 0) {
            const float scaleX = layout.screenWidth / layout.referenceWidth;
            const float scaleY = layout.screenHeight / layout.referenceHeight;
            layout.uiScale = std::min(scaleX, scaleY);
            layout.offsetX = (layout.screenWidth - layout.referenceWidth * layout.uiScale) * 0.5f;
            layout.offsetY = (layout.screenHeight - layout.referenceHeight * layout.uiScale) * 0.5f;
        }

        return layout;
    }

    float Scale(float designPixels) const {
        return designPixels * uiScale;
    }

    float ScaledReferenceWidth(float widthFraction) const {
        return Scale(referenceWidth * widthFraction);
    }

    float ScaledReferenceHeight(float heightFraction) const {
        return Scale(referenceHeight * heightFraction);
    }

    float DesignXToScreen(float designX) const {
        return offsetX + Scale(designX);
    }

    float DesignYToScreen(float designY) const {
        return offsetY + Scale(designY);
    }

    float CenteredX(float elementWidth) const {
        return (screenWidth - elementWidth) * 0.5f;
    }

    // Position on the actual screen using a 0–1 fraction (0 = top, 1 = bottom).
    float YFromFraction(float elementHeight, float yFraction) const {
        return (screenHeight - elementHeight) * yFraction;
    }
};

} // namespace UI
} // namespace Game
