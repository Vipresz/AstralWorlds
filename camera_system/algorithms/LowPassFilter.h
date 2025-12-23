#pragma once

#include "../remesh/Remeshing.h"  // For GridKey and PointCloudPoint
#include <glm/glm.hpp>
#include <map>

namespace Engine {
namespace Editors {
namespace CameraSystem {
namespace Algorithms {

// Second-order low-pass filter coefficients
struct LowPassFilterCoefficients {
    float b0, b1, b2;  // Numerator coefficients
    float a1, a2;      // Denominator coefficients (a0 normalized to 1)
    bool valid;        // Whether coefficients are valid
    
    LowPassFilterCoefficients() : b0(0.0f), b1(0.0f), b2(0.0f), a1(0.0f), a2(0.0f), valid(false) {}
};

// Second-order low-pass filter state (per-position and weight filtering)
struct LowPassFilterState {
    // Position filter state
    glm::vec3 x1;  // Previous input (x, y, z)
    glm::vec3 x2;  // Input two samples ago (x, y, z)
    glm::vec3 y1;  // Previous output (x, y, z)
    glm::vec3 y2;  // Output two samples ago (x, y, z)
    
    // Weight filter state (second-order Butterworth)
    float x1_weight;  // Previous weight input
    float x2_weight;  // Weight input two samples ago
    float y1_weight;  // Previous weight output
    float y2_weight;  // Weight output two samples ago
    
    bool initialized;
    
    LowPassFilterState() 
        : x1(0.0f), x2(0.0f), y1(0.0f), y2(0.0f)
        , x1_weight(0.0f), x2_weight(0.0f), y1_weight(0.0f), y2_weight(0.0f)
        , initialized(false) {}
};

// Low-pass filter parameters
struct LowPassFilterParams {
    float cutoffHz;      // Cutoff frequency in Hz
    float sampleRate;    // Sampling rate in Hz
    bool enabled;       // Whether filtering is enabled
    
    LowPassFilterParams() : cutoffHz(10.0f), sampleRate(30.0f), enabled(false) {}
};

// Low-pass filter state container
struct LowPassFilterStateContainer {
    std::map<GridKey, LowPassFilterState> gridState;
    
    void Clear() {
        gridState.clear();
    }
};

// Compute second-order Butterworth low-pass filter coefficients
void ComputeLowPassFilterCoefficients(float cutoffHz, float sampleRate, LowPassFilterCoefficients& coefficients);

// Apply weight-based low-pass filter to a gridded point cloud
// The filter output is weighted by the point's weight value
void ApplyWeightBasedLowPassFilter(std::map<GridKey, PointCloudPoint>& pointCloudGrid,
                                   const LowPassFilterCoefficients& coefficients,
                                   LowPassFilterStateContainer& state);

// Clear filter state
void ClearLowPassFilterState(LowPassFilterStateContainer& state);

} // namespace Algorithms
} // namespace CameraSystem
} // namespace Editors
} // namespace Engine



