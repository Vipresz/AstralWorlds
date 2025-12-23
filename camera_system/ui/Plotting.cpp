#include "Plotting.h"
#include "../CameraSystemEditor.h"
#include "../GlobalConstants.h"
#include "../../../game_libs/Nuklear/nuklear.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>

using namespace Engine::Editors::CameraSystem::Constants;

namespace Engine {
namespace Editors {
namespace CameraSystem {
namespace Plotting {
namespace Detail {

// Extract weights from PointCloudPoint vector
std::vector<float> ExtractNumericValues(const std::vector<PointCloudPoint>& data) {
    std::vector<float> result;
    result.reserve(data.size());
    for (const auto& point : data) {
        result.push_back(point.weight);
    }
    return result;
}

// Extract weights from std::map<GridKey, PointCloudPoint>
std::vector<float> ExtractNumericValues(const std::map<GridKey, PointCloudPoint>& data) {
    std::vector<float> result;
    result.reserve(data.size());
    for (const auto& pair : data) {
        result.push_back(pair.second.weight);
    }
    return result;
}

// Extract x, y, z components from glm::vec3 vector
std::vector<std::pair<std::string, std::vector<float>>> ExtractNumericValues(const std::vector<glm::vec3>& data) {
    std::vector<std::pair<std::string, std::vector<float>>> result;
    if (data.empty()) return result;
    
    std::vector<float> xVals, yVals, zVals;
    xVals.reserve(data.size());
    yVals.reserve(data.size());
    zVals.reserve(data.size());
    
    for (const auto& vec : data) {
        xVals.push_back(vec.x);
        yVals.push_back(vec.y);
        zVals.push_back(vec.z);
    }
    
    result.push_back({"x", std::move(xVals)});
    result.push_back({"y", std::move(yVals)});
    result.push_back({"z", std::move(zVals)});
    return result;
}

// Extract components from std::map<GridKey, PointCloudPoint>
std::vector<std::pair<std::string, std::vector<float>>> ExtractNumericValues(const std::map<GridKey, PointCloudPoint>& data, bool extractComponents) {
    std::vector<std::pair<std::string, std::vector<float>>> result;
    if (data.empty()) return result;
    
    if (extractComponents) {
        // Extract weight, position.x, position.y, position.z, color.r, color.g, color.b
        std::vector<float> weights, posX, posY, posZ, colorR, colorG, colorB;
        weights.reserve(data.size());
        posX.reserve(data.size());
        posY.reserve(data.size());
        posZ.reserve(data.size());
        colorR.reserve(data.size());
        colorG.reserve(data.size());
        colorB.reserve(data.size());
        
        for (const auto& pair : data) {
            weights.push_back(pair.second.weight);
            posX.push_back(pair.second.position.x);
            posY.push_back(pair.second.position.y);
            posZ.push_back(pair.second.position.z);
            colorR.push_back(pair.second.color.r);
            colorG.push_back(pair.second.color.g);
            colorB.push_back(pair.second.color.b);
        }
        
        result.push_back({"weight", std::move(weights)});
        result.push_back({"position.x", std::move(posX)});
        result.push_back({"position.y", std::move(posY)});
        result.push_back({"position.z", std::move(posZ)});
        result.push_back({"color.r", std::move(colorR)});
        result.push_back({"color.g", std::move(colorG)});
        result.push_back({"color.b", std::move(colorB)});
    }
    
    return result;
}

// Extract x, y, z components from GridKey vector
std::vector<std::pair<std::string, std::vector<float>>> ExtractNumericValues(const std::vector<GridKey>& data) {
    std::vector<std::pair<std::string, std::vector<float>>> result;
    if (data.empty()) return result;
    
    std::vector<float> xVals, yVals, zVals;
    xVals.reserve(data.size());
    yVals.reserve(data.size());
    zVals.reserve(data.size());
    
    for (const auto& key : data) {
        xVals.push_back(static_cast<float>(key.x));
        yVals.push_back(static_cast<float>(key.y));
        zVals.push_back(static_cast<float>(key.z));
    }
    
    result.push_back({"x", std::move(xVals)});
    result.push_back({"y", std::move(yVals)});
    result.push_back({"z", std::move(zVals)});
    return result;
}

} // namespace Detail

// Initialize histogram from float vector
HistogramData InitializeHistogram(const std::vector<float>& values,
                                   const std::string& title,
                                   const std::string& xAxisLabel,
                                   int numBins,
                                   float minValue,
                                   float maxValue) {
    HistogramData data;
    data.title = title;
    data.xAxisLabel = xAxisLabel;
    data.numBins = numBins;
    data.totalCount = values.size();
    
    if (values.empty()) {
        data.bins.resize(numBins, 0);
        return data;
    }
    
    // Calculate min/max if not provided
    if (std::isnan(minValue)) {
        data.minValue = *std::min_element(values.begin(), values.end());
    } else {
        data.minValue = minValue;
    }
    
    if (std::isnan(maxValue)) {
        data.maxValue = *std::max_element(values.begin(), values.end());
    } else {
        data.maxValue = maxValue;
    }
    
    // Handle edge case where all values are the same
    if (data.maxValue == data.minValue) {
        data.maxValue = data.minValue + 1.0f;
    }
    
    // Calculate mean
    float sum = std::accumulate(values.begin(), values.end(), 0.0f);
    data.meanValue = sum / static_cast<float>(values.size());
    
    // Initialize bins
    data.bins.resize(numBins, 0);
    const float binWidth = (data.maxValue - data.minValue) / static_cast<float>(numBins);
    
    // Fill bins
    for (float value : values) {
        // Clamp value to [minValue, maxValue]
        value = std::max(data.minValue, std::min(data.maxValue, value));
        int binIndex = static_cast<int>((value - data.minValue) / binWidth);
        binIndex = std::max(0, std::min(binIndex, numBins - 1));
        data.bins[binIndex]++;
    }
    
    return data;
}

// Implementation for PointCloudPoint vector
std::vector<std::pair<std::string, HistogramData>> InitializeHistograms(const std::vector<PointCloudPoint>& data,
                                                                          const std::string& baseTitle) {
    std::vector<std::pair<std::string, HistogramData>> result;
    
    if (data.empty()) return result;
    
    // Extract weights
    std::vector<float> weights = Detail::ExtractNumericValues(data);
    if (!weights.empty()) {
        result.push_back({baseTitle + " - Weight", InitializeHistogram(weights, baseTitle + " - Weight", "Weight", 20, 0.0f, 1.0f)});
    }
    
    // Extract position components
    std::vector<glm::vec3> positions;
    positions.reserve(data.size());
    for (const auto& point : data) {
        positions.push_back(point.position);
    }
    auto posComponents = Detail::ExtractNumericValues(positions);
    for (const auto& comp : posComponents) {
        result.push_back({baseTitle + " - Position." + comp.first, 
                         InitializeHistogram(comp.second, baseTitle + " - Position." + comp.first, comp.first)});
    }
    
    // Extract color components
    std::vector<glm::vec3> colors;
    colors.reserve(data.size());
    for (const auto& point : data) {
        colors.push_back(point.color);
    }
    auto colorComponents = Detail::ExtractNumericValues(colors);
    for (const auto& comp : colorComponents) {
        result.push_back({baseTitle + " - Color." + comp.first, 
                         InitializeHistogram(comp.second, baseTitle + " - Color." + comp.first, comp.first, 20, 0.0f, 1.0f)});
    }
    
    return result;
}

// Implementation for std::map<GridKey, PointCloudPoint>
std::vector<std::pair<std::string, HistogramData>> InitializeHistograms(const std::map<GridKey, PointCloudPoint>& data,
                                                                          const std::string& baseTitle) {
    std::vector<std::pair<std::string, HistogramData>> result;
    
    if (data.empty()) return result;
    
    // Extract components
    auto components = Detail::ExtractNumericValues(data, true);
    for (const auto& comp : components) {
        float minVal = std::numeric_limits<float>::quiet_NaN();
        float maxVal = std::numeric_limits<float>::quiet_NaN();
        
        // Set known ranges for certain components
        if (comp.first == "weight" || comp.first.find("color") != std::string::npos) {
            minVal = 0.0f;
            maxVal = 1.0f;
        }
        
        result.push_back({baseTitle + " - " + comp.first, 
                         InitializeHistogram(comp.second, baseTitle + " - " + comp.first, comp.first, 20, minVal, maxVal)});
    }
    
    return result;
}

// Render a single histogram
void RenderHistogram(struct nk_context* ctx, const HistogramData& data) {
    if (data.totalCount == 0) {
        nk_layout_row_dynamic(ctx, UI_ROW_HEIGHT_STANDARD, 1);
        nk_label(ctx, "No data available", NK_TEXT_LEFT);
        return;
    }
    
    // Display title
    nk_layout_row_dynamic(ctx, UI_ROW_HEIGHT_STANDARD, 1);
    nk_label(ctx, data.title.c_str(), NK_TEXT_LEFT);
    
    // Display statistics
    int maxCount = *std::max_element(data.bins.begin(), data.bins.end());
    char statsText[256];
    std::snprintf(statsText, sizeof(statsText), "Total: %zu | Max Count: %d | Min: %.3f | Max: %.3f | Mean: %.3f",
                 data.totalCount, maxCount, data.minValue, data.maxValue, data.meanValue);
    nk_layout_row_dynamic(ctx, UI_ROW_HEIGHT_MEDIUM, 1);
    nk_label(ctx, statsText, NK_TEXT_LEFT);
    
    nk_layout_row_dynamic(ctx, UI_SPACING_SMALL, 1);
    nk_spacing(ctx, 1);
    
    // Draw histogram
    nk_layout_row_dynamic(ctx, UI_HISTOGRAM_HEIGHT, 1);
    
    struct nk_rect bounds = nk_widget_bounds(ctx);
    if (bounds.w > 0 && bounds.h > 0) {
        struct nk_command_buffer* canvas = nk_window_get_canvas(ctx);
        
        // Draw background
        nk_fill_rect(canvas, bounds, 0.0f, nk_rgb(40, 40, 40));
        
        // Draw grid lines
        struct nk_color gridColor = nk_rgb(60, 60, 60);
        for (int i = 0; i <= 4; i++) {
            float y = bounds.y + (bounds.h * i / 4.0f);
            nk_stroke_line(canvas, bounds.x, y, bounds.x + bounds.w, y, 1.0f, gridColor);
        }
        
        // Draw histogram bars
        const float barSpacing = 2.0f;
        const float totalBarWidth = bounds.w - (barSpacing * (data.numBins - 1));
        const float barWidth = totalBarWidth / static_cast<float>(data.numBins);
        
        struct nk_color barColor = nk_rgb(100, 150, 255);
        struct nk_color barColorHigh = nk_rgb(255, 100, 100);
        const float bottomMargin = 10.0f;
        const float availableHeight = bounds.h - bottomMargin;
        
        for (int i = 0; i < data.numBins; i++) {
            float normalizedHeight = static_cast<float>(data.bins[i]) / static_cast<float>(maxCount);
            float barHeight = normalizedHeight * availableHeight;
            
            if (barHeight > 0.5f) {
                float x = bounds.x + i * (barWidth + barSpacing);
                struct nk_rect bar;
                bar.x = x;
                bar.y = bounds.y + bounds.h - barHeight - bottomMargin;
                bar.w = barWidth;
                bar.h = barHeight;
                
                struct nk_color color = (normalizedHeight > 0.8f) ? barColorHigh : barColor;
                nk_fill_rect(canvas, bar, 2.0f, color);
            }
        }
    }
    
    // Draw axis label
    char axisLabel[128];
    std::snprintf(axisLabel, sizeof(axisLabel), "%s Range: %.3f - %.3f", data.xAxisLabel.c_str(), data.minValue, data.maxValue);
    nk_layout_row_dynamic(ctx, UI_ROW_HEIGHT_SMALL, 1);
    nk_label(ctx, axisLabel, NK_TEXT_CENTERED);
}

// Render multiple histograms
void RenderHistograms(struct nk_context* ctx, const std::vector<std::pair<std::string, HistogramData>>& histograms) {
    for (const auto& pair : histograms) {
        RenderHistogram(ctx, pair.second);
        nk_layout_row_dynamic(ctx, UI_SPACING_MEDIUM, 1);
        nk_spacing(ctx, 1);
    }
}

} // namespace Plotting
} // namespace CameraSystem
} // namespace Editors
} // namespace Engine
