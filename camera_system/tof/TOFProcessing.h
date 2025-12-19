#pragma once

#include "../remesh/Remeshing.h"  // For GridKey and PointCloudPoint
#include <glm/glm.hpp>
#include <map>
#include <vector>

namespace Engine {
namespace Editors {
namespace CameraSystem {

// Use GridKey from Remeshing.h namespace
using GridKey = Engine::Editors::CameraSystem::GridKey;

// Grid mode for TOF point cloud processing
enum class TOFGridMode {
    Average,    // Average all points that snap to the same grid coordinate
    FirstCome,  // Save only the first point that maps to a grid cell
    GridValue   // Use the snapped grid position (center of grid cell)
};

// Grid accumulator for averaging multiple points in the same grid cell
struct GridAccumulator {
    glm::vec3 positionSum;
    glm::vec3 colorSum;
    int count;
    
    GridAccumulator() : positionSum(0.0f), colorSum(0.0f), count(0) {}
};

// Second-order low-pass filter coefficients
struct LowPassFilterCoefficients {
    float b0, b1, b2;  // Numerator coefficients
    float a1, a2;      // Denominator coefficients (a0 normalized to 1)
    bool valid;        // Whether coefficients are valid
    
    LowPassFilterCoefficients() : b0(0.0f), b1(0.0f), b2(0.0f), a1(0.0f), a2(0.0f), valid(false) {}
};

// Second-order low-pass filter state (per-position filtering)
struct LowPassFilterState {
    glm::vec3 x1;  // Previous input (x, y, z)
    glm::vec3 x2;  // Input two samples ago (x, y, z)
    glm::vec3 y1;  // Previous output (x, y, z)
    glm::vec3 y2;  // Output two samples ago (x, y, z)
    bool initialized;
    
    LowPassFilterState() : x1(0.0f), x2(0.0f), y1(0.0f), y2(0.0f), initialized(false) {}
};

// TSDF (Truncated Signed Distance Function) data structure
struct TSDFVoxel {
    float distance;  // Signed distance value (truncated)
    float weight;    // Integration weight (for averaging)
    glm::vec3 color; // Color value (weighted average)
    float colorWeight; // Weight for color averaging (separate from distance weight)
    
    TSDFVoxel() : distance(0.0f), weight(0.0f), color(0.0f), colorWeight(0.0f) {}
};

// TOF Processing class - handles TSDF, low-pass filtering, and gridding
class TOFProcessing {
public:
    TOFProcessing();
    ~TOFProcessing();
    
    // Low-pass filter functions
    void ComputeLowPassFilterCoefficients(float cutoffHz, float sampleRate);
    glm::vec3 ApplyLowPassFilterPosition(const GridKey& key, const glm::vec3& inputPos);
    glm::vec3 ApplyLowPassFilterPosition(size_t positionHash, const glm::vec3& inputPos);
    
    // TSDF functions
    void IntegrateTSDF(const GridKey& key, const glm::vec3& pointPos, const glm::vec3& cameraPos, float gridSize, float truncationDistance, float weightDecay, const glm::vec3& color = glm::vec3(0.0f), bool storeColor = false);
    void ExtractTSDFPointCloud(std::vector<PointCloudPoint>& output, float gridSize, bool useStoredColors = false);
    void ClearTSDF();
    void CleanupTSDF(float minWeightThreshold = 0.05f, float weightDecay = 0.95f);  // Remove voxels with weight below threshold (applies decay to all voxels)
    
    // Gridding functions
    void ProcessGriddedPoint(const GridKey& key, const glm::vec3& position, const glm::vec3& color, 
                            TOFGridMode mode, float gridSize,
                            std::map<GridKey, PointCloudPoint>& pointCloudGrid,
                            std::map<GridKey, GridAccumulator>& gridAccumulators);
    
    void ExtractGriddedPointCloud(const std::map<GridKey, PointCloudPoint>& pointCloudGrid,
                                  const std::map<GridKey, GridAccumulator>& gridAccumulators,
                                  TOFGridMode mode,
                                  std::vector<PointCloudPoint>& output,
                                  bool lowPassEnabled);
    
    // Access to internal state
    LowPassFilterCoefficients& GetLowPassCoefficients() { return m_lowPassCoefficients; }
    const LowPassFilterCoefficients& GetLowPassCoefficients() const { return m_lowPassCoefficients; }
    
    std::map<GridKey, LowPassFilterState>& GetLowPassFilterStateGrid() { return m_lowPassFilterStateGrid; }
    std::map<size_t, LowPassFilterState>& GetLowPassFilterStateNonGrid() { return m_lowPassFilterStateNonGrid; }
    
    std::map<GridKey, TSDFVoxel>& GetTSDFGrid() { return m_tsdfGrid; }
    
    void ClearLowPassFilterState();
    void ClearAll();
    
private:
    // Low-pass filter
    LowPassFilterCoefficients m_lowPassCoefficients;
    std::map<GridKey, LowPassFilterState> m_lowPassFilterStateGrid;
    std::map<size_t, LowPassFilterState> m_lowPassFilterStateNonGrid;
    
    // TSDF
    std::map<GridKey, TSDFVoxel> m_tsdfGrid;
};

} // namespace CameraSystem
} // namespace Editors
} // namespace Engine

