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
    
    // Eviction-based-2 (Consecutive Frame Persistence)
    // Points must be seen N consecutive frames to be kept, removed after M consecutive frames unseen
    bool eviction2Enabled;
    int eviction2AppearanceThreshold;  // N: min consecutive frames a point must be seen to be kept
    int eviction2RemovalThreshold;     // M: max consecutive frames unseen before removal
    
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
        , eviction2AppearanceThreshold(3)  // Must be seen 3 consecutive frames to be kept
        , eviction2RemovalThreshold(5)     // Removed after 5 consecutive frames unseen
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
    
    // Eviction-based-2 state (Consecutive Frame Persistence)
    std::map<GridKey, int> eviction2ConsecutiveSeenCount;    // How many consecutive frames this cell has been seen
    std::map<GridKey, int> eviction2ConsecutiveUnseenCount;  // How many consecutive frames this cell has been unseen
    std::map<GridKey, PointCloudPoint> eviction2ConfirmedPoints;  // Points that have passed the appearance threshold
    
    OccupancyState()
        : totalFrames(0)
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

// Algorithm 5: Eviction-based-2 (Consecutive Frame Persistence)
// Points must be seen N consecutive frames to be confirmed, removed after M frames unseen
void ApplyConsecutiveFramePersistence(std::map<GridKey, PointCloudPoint>& pointCloudGrid,
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

