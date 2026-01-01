#include "UIHelpers.h"
#include <cmath>
#include <sstream>
#include <iomanip>

namespace Engine {
namespace Editors {
namespace CameraSystem {
namespace UI {

bool LogScaleSliderFloat(struct nk_context* ctx, const char* label, float& value,
                         float minValue, float maxValue, float labelWidth) {
    // Create label with value on same line
    nk_layout_row_template_begin(ctx, 25);
    nk_layout_row_template_push_static(ctx, labelWidth);
    nk_layout_row_template_push_dynamic(ctx);
    nk_layout_row_template_end(ctx);
    
    char labelText[128];
    std::snprintf(labelText, sizeof(labelText), "%s: %.3f", label, value);
    nk_label(ctx, labelText, NK_TEXT_LEFT);
    
    // Create slider
    nk_layout_row_dynamic(ctx, 25, 1);
    
    // Convert value to log scale slider position (0-100)
    float logMin = std::log10(minValue);
    float logMax = std::log10(maxValue);
    float currentLog = std::log10(value);
    float normalized = (currentLog - logMin) / (logMax - logMin);
    int sliderValue = static_cast<int>(normalized * 100.0f);
    sliderValue = glm::clamp(sliderValue, 0, 100);
    
    nk_slider_int(ctx, 0, &sliderValue, 100, 1);
    
    // Convert slider back to log scale value
    normalized = sliderValue / 100.0f;
    float newLog = logMin + normalized * (logMax - logMin);
    float newValue = std::pow(10.0f, newLog);
    
    if (std::abs(newValue - value) > 0.001f) {
        value = newValue;
        return true;
    }
    return false;
}

bool LogScaleSliderInt(struct nk_context* ctx, const char* label, int& value,
                       int minValue, int maxValue, float labelWidth) {
    float floatValue = static_cast<float>(value);
    bool changed = LogScaleSliderFloat(ctx, label, floatValue, 
                                        static_cast<float>(minValue), 
                                        static_cast<float>(maxValue), 
                                        labelWidth);
    if (changed) {
        value = static_cast<int>(floatValue + 0.5f);
    }
    return changed;
}

bool InvertedCheckbox(struct nk_context* ctx, const char* label, bool& value) {
    nk_bool checkboxState = value ? nk_false : nk_true; // Inverted logic
    if (nk_checkbox_label(ctx, label, &checkboxState)) {
        value = (checkboxState == nk_false);
        return true;
    }
    return false;
}

bool NormalCheckbox(struct nk_context* ctx, const char* label, bool& value) {
    nk_bool checkboxState = value ? nk_true : nk_false; // Normal logic
    if (nk_checkbox_label(ctx, label, &checkboxState)) {
        value = (checkboxState == nk_true);
        return true;
    }
    return false;
}

void LabelWithValue(struct nk_context* ctx, const char* label, const char* value,
                    float labelWidth, float rowHeight) {
    nk_layout_row_template_begin(ctx, rowHeight);
    nk_layout_row_template_push_static(ctx, labelWidth);
    nk_layout_row_template_push_dynamic(ctx);
    nk_layout_row_template_end(ctx);
    
    std::string fullLabel = std::string(label) + ": " + value;
    nk_label(ctx, fullLabel.c_str(), NK_TEXT_LEFT);
}

bool FloatSliderWithLabel(struct nk_context* ctx, const char* label, float& value,
                          float minVal, float maxVal, float step, float labelWidth) {
    // Label with value
    nk_layout_row_template_begin(ctx, 25);
    nk_layout_row_template_push_static(ctx, labelWidth);
    nk_layout_row_template_push_dynamic(ctx);
    nk_layout_row_template_end(ctx);
    
    char labelText[128];
    std::snprintf(labelText, sizeof(labelText), "%s: %.3f", label, value);
    nk_label(ctx, labelText, NK_TEXT_LEFT);
    
    // Slider
    nk_layout_row_dynamic(ctx, 25, 1);
    float oldValue = value;
    nk_slider_float(ctx, minVal, &value, maxVal, step);
    
    return std::abs(value - oldValue) > 0.001f;
}

bool IntSliderWithLabel(struct nk_context* ctx, const char* label, int& value,
                         int minVal, int maxVal, int step, float labelWidth) {
    // Label with value
    nk_layout_row_template_begin(ctx, 25);
    nk_layout_row_template_push_static(ctx, labelWidth);
    nk_layout_row_template_push_dynamic(ctx);
    nk_layout_row_template_end(ctx);
    
    char labelText[128];
    std::snprintf(labelText, sizeof(labelText), "%s: %d", label, value);
    nk_label(ctx, labelText, NK_TEXT_LEFT);
    
    // Slider
    nk_layout_row_dynamic(ctx, 25, 1);
    int oldValue = value;
    nk_slider_int(ctx, minVal, &value, maxVal, step);
    
    return value != oldValue;
}

std::string FormatVector3(const glm::vec3& vec, int precision) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(precision);
    oss << "(" << vec.x << ", " << vec.y << ", " << vec.z << ")";
    return oss.str();
}

std::string FormatFloat(float value, int precision) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(precision);
    oss << value;
    return oss.str();
}

} // namespace UI
} // namespace CameraSystem
} // namespace Editors
} // namespace Engine







