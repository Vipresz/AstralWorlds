#include "TOFProcessing.h"
#include <cmath>
#include <algorithm>

namespace Engine {
namespace Editors {
namespace CameraSystem {

TOFProcessing::TOFProcessing() {
}

TOFProcessing::~TOFProcessing() {
}

// Compute second-order Butterworth low-pass filter coefficients
void TOFProcessing::ComputeLowPassFilterCoefficients(float cutoffHz, float sampleRate) {
    if (cutoffHz <= 0.0f || sampleRate <= 0.0f) {
        m_lowPassCoefficients.valid = false;
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
    m_lowPassCoefficients.a1 = 2.0f * (k2 - 1.0f) / a0;
    m_lowPassCoefficients.a2 = (1.0f - sqrt2 * k + k2) / a0;
    
    // Numerator coefficients (all equal for Butterworth)
    m_lowPassCoefficients.b0 = k2 / a0;
    m_lowPassCoefficients.b1 = 2.0f * k2 / a0;
    m_lowPassCoefficients.b2 = k2 / a0;
    
    m_lowPassCoefficients.valid = true;
}

// Second-order low-pass filter implementation (Butterworth design) for 3D positions
// Applied to gridded points using GridKey
glm::vec3 TOFProcessing::ApplyLowPassFilterPosition(const GridKey& key, const glm::vec3& inputPos) {
    if (!m_lowPassCoefficients.valid) {
        return inputPos;  // No filtering if coefficients are invalid
    }
    
    // Get or create filter state for this grid key
    LowPassFilterState& state = m_lowPassFilterStateGrid[key];
    
    // Initialize state if needed
    if (!state.initialized) {
        state.x1 = inputPos;
        state.x2 = inputPos;
        state.y1 = inputPos;
        state.y2 = inputPos;
        state.initialized = true;
    }
    
    // Apply filter to each component: y[n] = b0*x[n] + b1*x[n-1] + b2*x[n-2] - a1*y[n-1] - a2*y[n-2]
    glm::vec3 output;
    output.x = m_lowPassCoefficients.b0 * inputPos.x + m_lowPassCoefficients.b1 * state.x1.x + m_lowPassCoefficients.b2 * state.x2.x - m_lowPassCoefficients.a1 * state.y1.x - m_lowPassCoefficients.a2 * state.y2.x;
    output.y = m_lowPassCoefficients.b0 * inputPos.y + m_lowPassCoefficients.b1 * state.x1.y + m_lowPassCoefficients.b2 * state.x2.y - m_lowPassCoefficients.a1 * state.y1.y - m_lowPassCoefficients.a2 * state.y2.y;
    output.z = m_lowPassCoefficients.b0 * inputPos.z + m_lowPassCoefficients.b1 * state.x1.z + m_lowPassCoefficients.b2 * state.x2.z - m_lowPassCoefficients.a1 * state.y1.z - m_lowPassCoefficients.a2 * state.y2.z;
    
    // Update state
    state.x2 = state.x1;
    state.x1 = inputPos;
    state.y2 = state.y1;
    state.y1 = output;
    
    return output;
}

// Second-order low-pass filter implementation for non-gridded points using position hash
glm::vec3 TOFProcessing::ApplyLowPassFilterPosition(size_t positionHash, const glm::vec3& inputPos) {
    if (!m_lowPassCoefficients.valid) {
        return inputPos;  // No filtering if coefficients are invalid
    }
    
    // Get or create filter state for this position hash
    LowPassFilterState& state = m_lowPassFilterStateNonGrid[positionHash];
    
    // Initialize state if needed
    if (!state.initialized) {
        state.x1 = inputPos;
        state.x2 = inputPos;
        state.y1 = inputPos;
        state.y2 = inputPos;
        state.initialized = true;
    }
    
    // Apply filter to each component: y[n] = b0*x[n] + b1*x[n-1] + b2*x[n-2] - a1*y[n-1] - a2*y[n-2]
    glm::vec3 output;
    output.x = m_lowPassCoefficients.b0 * inputPos.x + m_lowPassCoefficients.b1 * state.x1.x + m_lowPassCoefficients.b2 * state.x2.x - m_lowPassCoefficients.a1 * state.y1.x - m_lowPassCoefficients.a2 * state.y2.x;
    output.y = m_lowPassCoefficients.b0 * inputPos.y + m_lowPassCoefficients.b1 * state.x1.y + m_lowPassCoefficients.b2 * state.x2.y - m_lowPassCoefficients.a1 * state.y1.y - m_lowPassCoefficients.a2 * state.y2.y;
    output.z = m_lowPassCoefficients.b0 * inputPos.z + m_lowPassCoefficients.b1 * state.x1.z + m_lowPassCoefficients.b2 * state.x2.z - m_lowPassCoefficients.a1 * state.y1.z - m_lowPassCoefficients.a2 * state.y2.z;
    
    // Update state
    state.x2 = state.x1;
    state.x1 = inputPos;
    state.y2 = state.y1;
    state.y1 = output;
    
    return output;
}

// TSDF Integration: Add a point measurement to the TSDF grid
void TOFProcessing::IntegrateTSDF(const GridKey& key, const glm::vec3& pointPos, const glm::vec3& cameraPos, 
                                  float gridSize, float truncationDistance, float weightDecay, 
                                  const glm::vec3& color, bool storeColor) {
    // Get voxel center position
    glm::vec3 voxelCenter(
        key.x * gridSize,
        key.y * gridSize,
        key.z * gridSize
    );
    
    // Calculate ray direction from camera to point
    glm::vec3 rayDir = pointPos - cameraPos;
    float rayLength = glm::length(rayDir);
    if (rayLength < 0.0001f) {
        return;  // Invalid ray
    }
    rayDir = rayDir / rayLength;
    
    // Calculate vector from camera to voxel center
    glm::vec3 toVoxel = voxelCenter - cameraPos;
    
    // Project voxel center onto the ray
    float projectedDist = glm::dot(toVoxel, rayDir);
    
    // Distance from camera to surface point
    float surfaceDist = rayLength;
    
    // Signed distance: distance from voxel center to surface along the ray
    // Positive = voxel is behind surface (further from camera)
    // Negative = voxel is in front of surface (closer to camera)
    float signedDist = projectedDist - surfaceDist;
    
    // Truncate the distance to truncation distance
    float truncation = truncationDistance * gridSize;
    signedDist = std::clamp(signedDist, -truncation, truncation);
    
    // Get or create TSDF voxel
    TSDFVoxel& voxel = m_tsdfGrid[key];
    
    // Weight for this measurement (constant for now, could be based on distance/angle)
    float newWeight = 1.0f;
    
    // Apply weight decay to existing weight
    float oldWeight = voxel.weight * weightDecay;
    float totalWeight = oldWeight + newWeight;
    
    if (totalWeight > 0.001f) {
        // Weighted average: integrate new measurement
        voxel.distance = (voxel.distance * oldWeight + signedDist * newWeight) / totalWeight;
        voxel.weight = totalWeight;
        
        // Cap weight to prevent unbounded growth
        voxel.weight = std::min(voxel.weight, 10.0f);
    } else {
        // First measurement for this voxel
        voxel.distance = signedDist;
        voxel.weight = newWeight;
    }
    
    // Store color if enabled
    if (storeColor) {
        float oldColorWeight = voxel.colorWeight * weightDecay;
        float newColorWeight = newWeight;  // Use same weight as distance
        float totalColorWeight = oldColorWeight + newColorWeight;
        
        if (totalColorWeight > 0.001f) {
            // Weighted average: integrate new color measurement
            voxel.color = (voxel.color * oldColorWeight + color * newColorWeight) / totalColorWeight;
            voxel.colorWeight = totalColorWeight;
            
            // Cap color weight to prevent unbounded growth
            voxel.colorWeight = std::min(voxel.colorWeight, 10.0f);
        } else {
            // First color measurement for this voxel
            voxel.color = color;
            voxel.colorWeight = newColorWeight;
        }
    }
}

// Extract point cloud from TSDF by finding zero-crossings (surface locations)
void TOFProcessing::ExtractTSDFPointCloud(std::vector<PointCloudPoint>& output, float gridSize, bool useStoredColors) {
    output.clear();
    output.reserve(m_tsdfGrid.size());
    
    for (const auto& pair : m_tsdfGrid) {
        const GridKey& key = pair.first;
        const TSDFVoxel& voxel = pair.second;
        
        // Only extract points near the surface (distance close to zero)
        // and with sufficient weight (observed multiple times)
        if (std::abs(voxel.distance) < gridSize * 0.5f && voxel.weight > 0.5f) {
            PointCloudPoint point;
            point.position = glm::vec3(
                key.x * gridSize,
                key.y * gridSize,
                key.z * gridSize
            );
            
            // Adjust position based on signed distance (move towards surface)
            // This provides sub-voxel accuracy
            glm::vec3 normal = glm::normalize(point.position);  // Approximate normal
            point.position += normal * voxel.distance * 0.5f;  // Partial correction
            
            // Use stored color if available and enabled, otherwise use default
            if (useStoredColors && voxel.colorWeight > 0.001f) {
                point.color = voxel.color;
            } else {
                point.color = glm::vec3(0.8f, 0.8f, 0.8f);  // Default gray color
            }
            
            output.push_back(point);
        }
    }
}

// Clear TSDF data
void TOFProcessing::ClearTSDF() {
    m_tsdfGrid.clear();
}

// Cleanup TSDF by removing voxels with weight below threshold
// This prevents unbounded growth by removing old/unobserved voxels
void TOFProcessing::CleanupTSDF(float minWeightThreshold, float weightDecay) {
    // Use iterator-based removal to safely remove elements while iterating
    for (auto it = m_tsdfGrid.begin(); it != m_tsdfGrid.end();) {
        TSDFVoxel& voxel = it->second;
        
        // Apply weight decay to ALL voxels (even if not updated this frame)
        // This ensures unobserved voxels decay over time
        // Use the same weightDecay parameter as IntegrateTSDF for consistency
        voxel.weight *= weightDecay;
        
        // Remove voxels with very low weight (not observed recently)
        // OR voxels that are far from the surface (large absolute distance)
        // Large distance means the voxel is not near any surface
        const float maxDistanceFromSurface = 2.0f;  // Remove voxels more than 2 grid units from surface
        if (voxel.weight < minWeightThreshold || std::abs(voxel.distance) > maxDistanceFromSurface) {
            it = m_tsdfGrid.erase(it);
        } else {
            ++it;
        }
    }
}

// Process a gridded point (add to grid accumulators or point cloud grid)
void TOFProcessing::ProcessGriddedPoint(const GridKey& key, const glm::vec3& position, const glm::vec3& color,
                                        TOFGridMode mode, float gridSize,
                                        std::map<GridKey, PointCloudPoint>& pointCloudGrid,
                                        std::map<GridKey, GridAccumulator>& gridAccumulators) {
    if (mode == TOFGridMode::Average) {
        if (gridAccumulators.find(key) == gridAccumulators.end()) {
            GridAccumulator acc;
            acc.positionSum = position;
            acc.colorSum = color;
            acc.count = 1;
            gridAccumulators[key] = acc;
        } else {
            gridAccumulators[key].positionSum += position;
            gridAccumulators[key].colorSum += color;
            gridAccumulators[key].count++;
        }
    } else if (mode == TOFGridMode::FirstCome) {
        if (pointCloudGrid.find(key) == pointCloudGrid.end()) {
            PointCloudPoint point;
            point.position = position;
            point.color = color;
            pointCloudGrid[key] = point;
        }
    } else {  // GridValue
        glm::vec3 snappedPos(
            key.x * gridSize,
            key.y * gridSize,
            key.z * gridSize
        );
        
        PointCloudPoint point;
        point.position = snappedPos;
        point.color = color;
        pointCloudGrid[key] = point;
    }
}

// Extract point cloud from gridded data
void TOFProcessing::ExtractGriddedPointCloud(const std::map<GridKey, PointCloudPoint>& pointCloudGrid,
                                              const std::map<GridKey, GridAccumulator>& gridAccumulators,
                                              TOFGridMode mode,
                                              std::vector<PointCloudPoint>& output,
                                              bool lowPassEnabled) {
    output.clear();
    
    if (mode == TOFGridMode::Average) {
        output.reserve(gridAccumulators.size());
        for (const auto& pair : gridAccumulators) {
            const GridAccumulator& acc = pair.second;
            PointCloudPoint point;
            glm::vec3 avgPosition = acc.positionSum / static_cast<float>(acc.count);
            
            // Apply low-pass filter to gridded points AFTER gridding
            if (lowPassEnabled && m_lowPassCoefficients.valid) {
                avgPosition = ApplyLowPassFilterPosition(pair.first, avgPosition);
            }
            
            point.position = avgPosition;
            point.color = acc.colorSum / static_cast<float>(acc.count);
            output.push_back(point);
        }
    } else {
        output.reserve(pointCloudGrid.size());
        for (const auto& pair : pointCloudGrid) {
            PointCloudPoint point = pair.second;
            
            // Apply low-pass filter to gridded points AFTER gridding
            if (lowPassEnabled && m_lowPassCoefficients.valid) {
                point.position = ApplyLowPassFilterPosition(pair.first, point.position);
            }
            
            output.push_back(point);
        }
    }
}

// Clear low-pass filter state
void TOFProcessing::ClearLowPassFilterState() {
    m_lowPassFilterStateGrid.clear();
    m_lowPassFilterStateNonGrid.clear();
}

// Clear all TOF processing data
void TOFProcessing::ClearAll() {
    ClearTSDF();
    ClearLowPassFilterState();
}

} // namespace CameraSystem
} // namespace Editors
} // namespace Engine

