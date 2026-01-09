#pragma once

#include "../remesh/Remeshing.h"  // For GridKey and PointCloudPoint
#include <glm/glm.hpp>
#include <map>
#include <vector>
#include <array>

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

// TSDF (Truncated Signed Distance Function) data structure
struct TSDFVoxel {
    float distance;  // Signed distance value (truncated)
    float weight;    // Integration weight (for averaging)
    glm::vec3 color; // Color value (weighted average)
    float colorWeight; // Weight for color averaging (separate from distance weight)
    
    TSDFVoxel() : distance(0.0f), weight(0.0f), color(0.0f), colorWeight(0.0f) {}
};

// Surface flow history - stores 3 consecutive frames for temporal analysis
// Buffer 0 = current frame, Buffer 1 = previous frame, Buffer 2 = 2 frames ago
struct SurfaceFlowHistory {
    std::array<std::map<GridKey, PointCloudPoint>, 3> frameBuffers;
    int frameCount;  // Number of frames accumulated (0-3)
    
    SurfaceFlowHistory() : frameCount(0) {}
    
    void Clear() {
        for (auto& buffer : frameBuffers) {
            buffer.clear();
        }
        frameCount = 0;
    }
};

// TOF Processing class - handles TSDF and gridding
// NOTE: Low-pass filtering is handled by algorithms/LowPassFilter.h
class TOFProcessing {
public:
    TOFProcessing();
    ~TOFProcessing();
    
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
                                  std::vector<PointCloudPoint>& output);
    
    // Main gridding function - processes entire point cloud into grid
    void ApplyGridding(const std::vector<PointCloudPoint>& inputPoints,
                      std::map<GridKey, PointCloudPoint>& outputGrid,
                      TOFGridMode mode,
                      float gridSize);
    
    // Helper function to convert world position to grid key
    static GridKey WorldToGridKey(const glm::vec3& position, float gridSize);
    
    // Access to internal state
    std::map<GridKey, TSDFVoxel>& GetTSDFGrid() { return m_tsdfGrid; }
    
    void ClearAll();
    
    // Surface flow functions - temporal filtering using 3-frame history
    // Call UpdateSurfaceFlowHistory once per frame AFTER segmentation IDs are assigned
    void UpdateSurfaceFlowHistory(const std::map<GridKey, PointCloudPoint>& currentFrame);
    
    // Apply surface flow filtering - removes points that don't match local flow patterns
    // Returns filtered point cloud with outliers removed
    // neighborRadius: radius for finding local neighbors (in world units)
    // flowThreshold: maximum allowed deviation from local mean flow (in world units)
    void ApplySurfaceFlowFilter(std::map<GridKey, PointCloudPoint>& pointCloud,
                                float neighborRadius,
                                float flowThreshold,
                                float gridSize);
    
    // Clear surface flow history
    void ClearSurfaceFlowHistory();
    
    // Get current frame count in history (0-3)
    int GetSurfaceFlowFrameCount() const { return m_surfaceFlowHistory.frameCount; }
    
private:
    // TSDF
    std::map<GridKey, TSDFVoxel> m_tsdfGrid;
    
    // Surface flow history for temporal filtering
    SurfaceFlowHistory m_surfaceFlowHistory;
};

} // namespace CameraSystem
} // namespace Editors
} // namespace Engine

