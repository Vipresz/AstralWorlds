#pragma once

#include "../remesh/Remeshing.h"  // For GridKey and PointCloudPoint
#include <map>
#include <set>
#include <vector>
#include <tuple>

namespace Engine {
namespace Editors {
namespace CameraSystem {
namespace GridBased {

// Per-cell statistics for persistence and variance tracking
struct CellStatistics {
    int frameCount;        // Number of frames where cell has valid measurement
    float depthSum;        // Sum of depth values
    float depthSumSq;      // Sum of squared depth values
    int lastSeenFrame;     // Last frame where this cell was seen (for decay)
    
    CellStatistics() : frameCount(0), depthSum(0.0f), depthSumSq(0.0f), lastSeenFrame(0) {}
};

// Algorithm parameters
struct OccupancyParams {
    // Neighbor Consistency
    bool neighborConsistencyEnabled;
    float neighborRadius;
    float residualThreshold;
    
    // Persistence
    bool persistenceEnabled;
    float persistenceGamma;
    static constexpr int PERSISTENCE_WINDOW_SIZE = 100;
    
    // Variance-based
    bool varianceEnabled;
    float varianceThreshold;
    
    // Eviction-based
    bool evictionEnabled;
    bool evictionUseStrategy2;  // If true, use Strategy 2 (removal count), else use Strategy 1 (original)
    float evictionDecay;
    float evictionIncrement;
    float evictionMinStartValue;  // Minimum start value added to increment/decrement deltas
    int evictionRemovalThreshold;  // For Strategy 2: remove cell when count reaches this (default: 5)
    
    // Eviction-based-2
    bool eviction2Enabled;
    float eviction2Radius;
    int eviction2MinNeighbors;
    
    OccupancyParams()
        : neighborConsistencyEnabled(false)
        , neighborRadius(2.0f)
        , residualThreshold(1.5f)
        , persistenceEnabled(false)
        , persistenceGamma(2.0f)
        , varianceEnabled(false)
        , varianceThreshold(1.0f)
        , evictionEnabled(false)
        , evictionUseStrategy2(true)  // Always use Strategy 2 (removal count algorithm)
        , evictionDecay(2.0f)  // Decay value used in (decay^count)/100
        , evictionIncrement(1.5f)  // Multiply weight by this when occupied
        , evictionMinStartValue(0.1f)
        , evictionRemovalThreshold(5)  // Remove when count reaches this (hardcoded)
        , eviction2Enabled(false)
        , eviction2Radius(2.0f)
        , eviction2MinNeighbors(3)
    {}
};

// Algorithm state (persistent across frames)
struct OccupancyState {
    // Global frame counter
    int totalFrames;
    
    // Per-cell statistics
    std::map<GridKey, CellStatistics> cellStatistics;
    
    // Eviction-based state
    std::map<GridKey, float> evictionWeightsOld;
    std::map<GridKey, float> evictionWeightsNew;
    std::set<GridKey> evictionKeysOld;
    std::set<GridKey> evictionKeysNew;
    
    // Strategy 2 state: removal count tracking
    std::map<GridKey, int> evictionRemovalCounts;  // Track removal count for each grid cell
    
    // Eviction-based-2 state
    std::map<GridKey, PointCloudPoint> eviction2Map1;
    std::map<GridKey, PointCloudPoint> eviction2Map2;
    std::vector<std::tuple<int, int, int>> eviction2NeighborOffsets;
    float eviction2CachedRadius;
    
    OccupancyState()
        : totalFrames(0)
        , eviction2CachedRadius(-1.0f)
    {}
};

// Helper function: Update cell statistics for persistence and variance algorithms
void UpdateCellStatistics(std::map<GridKey, PointCloudPoint>& pointCloudGrid,
                          OccupancyState& state);

// Helper function: Compute neighbor offsets for eviction-based-2 (cached)
void ComputeEviction2NeighborOffsets(float radius, 
                                     std::vector<std::tuple<int, int, int>>& neighborOffsets,
                                     float& cachedRadius);

// Algorithm 1: Neighbor Consistency
void ApplyNeighborConsistency(std::map<GridKey, PointCloudPoint>& pointCloudGrid,
                               const OccupancyParams& params);

// Algorithm 2: Persistence
void ApplyPersistence(std::map<GridKey, PointCloudPoint>& pointCloudGrid,
                      const OccupancyParams& params,
                      const OccupancyState& state);

// Algorithm 3: Variance-based
void ApplyVariance(std::map<GridKey, PointCloudPoint>& pointCloudGrid,
                   const OccupancyParams& params,
                   const OccupancyState& state);

// Algorithm 4: Eviction-based
void ApplyEviction(std::map<GridKey, PointCloudPoint>& pointCloudGrid,
                   const OccupancyParams& params,
                   OccupancyState& state);

// Algorithm 5: Eviction-based-2 (density-based weighting)
void ApplyEviction2(std::map<GridKey, PointCloudPoint>& pointCloudGrid,
                    const OccupancyParams& params,
                    OccupancyState& state);

// Apply all enabled occupancy algorithms to gridded point cloud
// NOTE: All occupancy algorithms ONLY operate on gridded data
void ApplyOccupancyAlgorithms(std::map<GridKey, PointCloudPoint>& pointCloudGrid,
                               const OccupancyParams& params,
                               OccupancyState& state);

} // namespace GridBased
} // namespace CameraSystem
} // namespace Editors
} // namespace Engine

