#include "TOFProcessing.h"
#include <cmath>
#include <algorithm>
#include <set>

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
    
    glm::vec3 output;
    const float b0 = m_lowPassCoefficients.b0;
    const float b1 = m_lowPassCoefficients.b1;
    const float b2 = m_lowPassCoefficients.b2;
    const float a1 = m_lowPassCoefficients.a1;
    const float a2 = m_lowPassCoefficients.a2;
    
    output.x = b0 * inputPos.x + b1 * state.x1.x + b2 * state.x2.x - a1 * state.y1.x - a2 * state.y2.x;
    output.y = b0 * inputPos.y + b1 * state.x1.y + b2 * state.x2.y - a1 * state.y1.y - a2 * state.y2.y;
    output.z = b0 * inputPos.z + b1 * state.x1.z + b2 * state.x2.z - a1 * state.y1.z - a2 * state.y2.z;
    
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
    
    glm::vec3 output;
    const float b0 = m_lowPassCoefficients.b0;
    const float b1 = m_lowPassCoefficients.b1;
    const float b2 = m_lowPassCoefficients.b2;
    const float a1 = m_lowPassCoefficients.a1;
    const float a2 = m_lowPassCoefficients.a2;
    
    output.x = b0 * inputPos.x + b1 * state.x1.x + b2 * state.x2.x - a1 * state.y1.x - a2 * state.y2.x;
    output.y = b0 * inputPos.y + b1 * state.x1.y + b2 * state.x2.y - a1 * state.y1.y - a2 * state.y2.y;
    output.z = b0 * inputPos.z + b1 * state.x1.z + b2 * state.x2.z - a1 * state.y1.z - a2 * state.y2.z;
    
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
    
    glm::vec3 rayDir = pointPos - cameraPos;
    const float rayLengthSq = glm::dot(rayDir, rayDir);
    if (rayLengthSq < 1e-8f) {
        return;
    }
    const float rayLengthInv = 1.0f / std::sqrt(rayLengthSq);
    rayDir *= rayLengthInv;
    const float surfaceDist = rayLengthSq * rayLengthInv;
    
    glm::vec3 toVoxel = voxelCenter - cameraPos;
    const float projectedDist = glm::dot(toVoxel, rayDir);
    float signedDist = projectedDist - surfaceDist;
    
    const float truncation = truncationDistance * gridSize;
    signedDist = std::clamp(signedDist, -truncation, truncation);
    
    // Get or create TSDF voxel
    TSDFVoxel& voxel = m_tsdfGrid[key];
    
    const float newWeight = 1.0f;
    const float oldWeight = voxel.weight * weightDecay;
    const float totalWeight = oldWeight + newWeight;
    
    if (totalWeight > 0.001f) {
        const float invTotalWeight = 1.0f / totalWeight;
        voxel.distance = (voxel.distance * oldWeight + signedDist) * invTotalWeight;
        voxel.weight = std::min(totalWeight, 10.0f);
    } else {
        voxel.distance = signedDist;
        voxel.weight = newWeight;
    }
    
    if (storeColor) {
        const float oldColorWeight = voxel.colorWeight * weightDecay;
        const float totalColorWeight = oldColorWeight + newWeight;
        
        if (totalColorWeight > 0.001f) {
            const float invTotalColorWeight = 1.0f / totalColorWeight;
            voxel.color = (voxel.color * oldColorWeight + color) * invTotalColorWeight;
            voxel.colorWeight = std::min(totalColorWeight, 10.0f);
        } else {
            voxel.color = color;
            voxel.colorWeight = newWeight;
        }
    }
}

void TOFProcessing::ExtractTSDFPointCloud(std::vector<PointCloudPoint>& output, float gridSize, bool useStoredColors) {
    output.clear();
    output.reserve(m_tsdfGrid.size());
    
    const float halfGridSize = gridSize * 0.5f;
    const float minWeight = 0.5f;
    
    for (const auto& pair : m_tsdfGrid) {
        const GridKey& key = pair.first;
        const TSDFVoxel& voxel = pair.second;
        
        if (std::abs(voxel.distance) < halfGridSize && voxel.weight > minWeight) {
            PointCloudPoint point;
            const float posX = key.x * gridSize;
            const float posY = key.y * gridSize;
            const float posZ = key.z * gridSize;
            
            const float distCorrection = voxel.distance * 0.5f;
            const float lenSq = posX * posX + posY * posY + posZ * posZ;
            if (lenSq > 1e-8f) {
                const float lenInv = 1.0f / std::sqrt(lenSq);
                point.position.x = posX + posX * lenInv * distCorrection;
                point.position.y = posY + posY * lenInv * distCorrection;
                point.position.z = posZ + posZ * lenInv * distCorrection;
            } else {
                point.position.x = posX;
                point.position.y = posY;
                point.position.z = posZ;
            }
            
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
            const float invCount = 1.0f / static_cast<float>(acc.count);
            glm::vec3 avgPosition = acc.positionSum * invCount;
            
            if (lowPassEnabled && m_lowPassCoefficients.valid) {
                avgPosition = ApplyLowPassFilterPosition(pair.first, avgPosition);
            }
            
            point.position = avgPosition;
            point.color = acc.colorSum * invCount;
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

// Main gridding function - processes entire point cloud into grid
void TOFProcessing::ApplyGridding(const std::vector<PointCloudPoint>& inputPoints,
                                 std::map<GridKey, PointCloudPoint>& outputGrid,
                                 TOFGridMode mode,
                                 float gridSize) {
    outputGrid.clear();
    const float invGridSize = 1.0f / gridSize;
    
    if (mode == TOFGridMode::Average) {
        // Collect all values per grid coordinate in lists
        std::map<GridKey, std::vector<PointCloudPoint>> gridLists;
        
        for (const auto& point : inputPoints) {
            GridKey gridKey = WorldToGridKey(point.position, gridSize);
            gridLists[gridKey].push_back(point);
        }
        
        // Average values per grid coordinate
        for (const auto& pair : gridLists) {
            const GridKey& gridKey = pair.first;
            const std::vector<PointCloudPoint>& points = pair.second;
            
            glm::vec3 posSum(0.0f);
            glm::vec3 colorSum(0.0f);
            for (const auto& p : points) {
                posSum += p.position;
                colorSum += p.color;
            }
            
            PointCloudPoint averagedPoint;
            averagedPoint.position = posSum / static_cast<float>(points.size());
            averagedPoint.color = colorSum / static_cast<float>(points.size());
            // Normalize weight by number of points: points/100, capped at 1.0
            averagedPoint.weight = std::min(1.0f, static_cast<float>(points.size()) / 100.0f);
            
            outputGrid[gridKey] = averagedPoint;
        }
    } else if (mode == TOFGridMode::FirstCome) {
        // Save first value, ignore others
        for (const auto& point : inputPoints) {
            GridKey gridKey = WorldToGridKey(point.position, gridSize);
            
            if (outputGrid.find(gridKey) == outputGrid.end()) {
                PointCloudPoint gridPoint = point;
                gridPoint.weight = 1.0f;
                outputGrid[gridKey] = gridPoint;
            }
        }
    } else {  // TOFGridMode::GridValue
        // Use grid coordinate itself (snap to grid center)
        std::set<GridKey> gridKeys;
        
        for (const auto& point : inputPoints) {
            GridKey gridKey = WorldToGridKey(point.position, gridSize);
            gridKeys.insert(gridKey);
        }
        
        // Create points at grid centers
        for (const GridKey& gridKey : gridKeys) {
            glm::vec3 snappedPos(
                gridKey.x * gridSize,
                gridKey.y * gridSize,
                gridKey.z * gridSize
            );
            
            PointCloudPoint gridPoint;
            gridPoint.position = snappedPos;
            gridPoint.color = glm::vec3(1.0f);
            gridPoint.weight = 1.0f;
            
            outputGrid[gridKey] = gridPoint;
        }
    }
}

// Helper function to convert world position to grid key
GridKey TOFProcessing::WorldToGridKey(const glm::vec3& position, float gridSize) {
    const float invGridSize = 1.0f / gridSize;
    GridKey key;
    key.x = static_cast<int>(std::round(position.x * invGridSize));
    key.y = static_cast<int>(std::round(position.y * invGridSize));
    key.z = static_cast<int>(std::round(position.z * invGridSize));
    return key;
}

} // namespace CameraSystem
} // namespace Editors
} // namespace Engine

