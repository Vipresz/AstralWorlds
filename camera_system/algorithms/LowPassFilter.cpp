#include "LowPassFilter.h"
#include <cmath>
#include <algorithm>

namespace Engine {
namespace Editors {
namespace CameraSystem {
namespace Algorithms {

void ComputeLowPassFilterCoefficients(float cutoffHz, float sampleRate, LowPassFilterCoefficients& coefficients) {
    if (cutoffHz <= 0.0f || sampleRate <= 0.0f) {
        coefficients.valid = false;
        return;
    }
    
    // Normalized cutoff frequency (0 to 0.5, where 0.5 is Nyquist)
    float normalizedCutoff = cutoffHz / sampleRate;
    if (normalizedCutoff >= 0.5f) {
        normalizedCutoff = 0.499f;  // Clamp to just below Nyquist
    }
    
    // Pre-warp for bilinear transform: k = tan(π * fc / fs)
    // This ensures the digital filter matches the analog cutoff frequency
    const float PI = 3.14159265359f;
    float k = std::tan(PI * normalizedCutoff);
    float k2 = k * k;
    const float sqrt2 = 1.41421356237f;  // sqrt(2) - Butterworth Q factor
    
    // Second-order Butterworth low-pass filter coefficients
    // Analog transfer function: H(s) = ωc² / (s² + √2·ωc·s + ωc²)
    // After bilinear transform to z-domain:
    float a0 = 1.0f + sqrt2 * k + k2;
    coefficients.a1 = 2.0f * (k2 - 1.0f) / a0;
    coefficients.a2 = (1.0f - sqrt2 * k + k2) / a0;
    
    // Numerator coefficients (all equal for Butterworth)
    coefficients.b0 = k2 / a0;
    coefficients.b1 = 2.0f * k2 / a0;
    coefficients.b2 = k2 / a0;
    
    coefficients.valid = true;
}

void ApplyWeightBasedLowPassFilter(std::map<GridKey, PointCloudPoint>& pointCloudGrid,
                                   const LowPassFilterCoefficients& coefficients,
                                   LowPassFilterStateContainer& state) {
    if (!coefficients.valid || pointCloudGrid.empty()) {
        return;
    }
    
    const float b0 = coefficients.b0;
    const float b1 = coefficients.b1;
    const float b2 = coefficients.b2;
    const float a1 = coefficients.a1;
    const float a2 = coefficients.a2;
    
    // Apply filter to each point, using weight to blend filtered and unfiltered positions
    for (auto& pair : pointCloudGrid) {
        const GridKey& key = pair.first;
        PointCloudPoint& point = pair.second;
        
        // Get or create filter state for this grid key
        LowPassFilterState& filterState = state.gridState[key];
        
        // Initialize state if needed
        if (!filterState.initialized) {
            filterState.x1 = point.position;
            filterState.x2 = point.position;
            filterState.y1 = point.position;
            filterState.y2 = point.position;
            filterState.x1_weight = point.weight;
            filterState.x2_weight = point.weight;
            filterState.y1_weight = point.weight;
            filterState.y2_weight = point.weight;
            filterState.initialized = true;
        }
        
        // Apply second-order Butterworth filter to position
        glm::vec3 filteredPos;
        filteredPos.x = b0 * point.position.x + b1 * filterState.x1.x + b2 * filterState.x2.x 
                       - a1 * filterState.y1.x - a2 * filterState.y2.x;
        filteredPos.y = b0 * point.position.y + b1 * filterState.x1.y + b2 * filterState.x2.y 
                       - a1 * filterState.y1.y - a2 * filterState.y2.y;
        filteredPos.z = b0 * point.position.z + b1 * filterState.x1.z + b2 * filterState.x2.z 
                       - a1 * filterState.y1.z - a2 * filterState.y2.z;
        
        // Apply second-order Butterworth filter to weight
        float filteredWeight = b0 * point.weight + b1 * filterState.x1_weight + b2 * filterState.x2_weight
                             - a1 * filterState.y1_weight - a2 * filterState.y2_weight;
        
        // Clamp filtered weight to [0, 1] range
        filteredWeight = std::max(0.0f, std::min(1.0f, filteredWeight));
        
        // Update filter state for position
        filterState.x2 = filterState.x1;
        filterState.x1 = point.position;
        filterState.y2 = filterState.y1;
        filterState.y1 = filteredPos;
        
        // Update filter state for weight
        filterState.x2_weight = filterState.x1_weight;
        filterState.x1_weight = point.weight;
        filterState.y2_weight = filterState.y1_weight;
        filterState.y1_weight = filteredWeight;
        
        // Weight-based blending: blend filtered and unfiltered positions based on original point weight
        // Higher weight = more trust in filtered position
        // Lower weight = more trust in original position (less filtering)
        float blendFactor = std::max(0.0f, std::min(1.0f, point.weight));
        point.position = blendFactor * filteredPos + (1.0f - blendFactor) * point.position;
        
        // Store filtered weight directly (no blending needed for weight itself)
        point.weight = filteredWeight;
    }
}

void ClearLowPassFilterState(LowPassFilterStateContainer& state) {
    state.Clear();
}

} // namespace Algorithms
} // namespace CameraSystem
} // namespace Editors
} // namespace Engine



