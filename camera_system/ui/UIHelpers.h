#pragma once

#include "../../../game_libs/Nuklear/nuklear.h"
#include <glm/glm.hpp>
#include <string>

namespace Engine {
namespace Editors {
namespace CameraSystem {
namespace UI {

// Helper functions for common UI patterns that can be reused across projects

/**
 * Create a log scale slider for Nuklear
 * Maps a linear slider value (0-100) to a logarithmic range (min-max)
 * 
 * @param ctx Nuklear context
 * @param label Label text to display
 * @param value Current value (will be updated if slider changes)
 * @param minValue Minimum value in the log scale range
 * @param maxValue Maximum value in the log scale range
 * @param labelWidth Width of the label column
 * @return true if the value was changed
 */
bool LogScaleSliderFloat(struct nk_context* ctx, const char* label, float& value, 
                         float minValue, float maxValue, float labelWidth = 150.0f);

/**
 * Create a log scale slider for integers
 */
bool LogScaleSliderInt(struct nk_context* ctx, const char* label, int& value,
                       int minValue, int maxValue, float labelWidth = 150.0f);

/**
 * Create a checkbox with inverted logic (common pattern in this codebase)
 * @param ctx Nuklear context
 * @param label Checkbox label
 * @param value Current boolean value (true = checked, false = unchecked)
 * @return true if the checkbox was clicked
 */
bool InvertedCheckbox(struct nk_context* ctx, const char* label, bool& value);

/**
 * Create a normal checkbox (non-inverted)
 */
bool NormalCheckbox(struct nk_context* ctx, const char* label, bool& value);

/**
 * Create a label with value on the same line using template layout
 */
void LabelWithValue(struct nk_context* ctx, const char* label, const char* value, 
                    float labelWidth = 150.0f, float rowHeight = 25.0f);

/**
 * Create a float slider with label and value display
 * @return true if value changed
 */
bool FloatSliderWithLabel(struct nk_context* ctx, const char* label, float& value,
                          float minVal, float maxVal, float step, float labelWidth = 150.0f);

/**
 * Create an integer slider with label and value display
 * @return true if value changed
 */
bool IntSliderWithLabel(struct nk_context* ctx, const char* label, int& value,
                        int minVal, int maxVal, int step, float labelWidth = 150.0f);

/**
 * Format a vector3 as a string for display
 */
std::string FormatVector3(const glm::vec3& vec, int precision = 2);

/**
 * Format a float value with specified precision
 */
std::string FormatFloat(float value, int precision = 1);

} // namespace UI
} // namespace CameraSystem
} // namespace Editors
} // namespace Engine







