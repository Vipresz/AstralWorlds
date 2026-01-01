#include "OccupancyAlgorithms.h"
#include <algorithm>
#include <cmath>

namespace Engine {
namespace Editors {
namespace CameraSystem {
namespace GridBased {

/**
 * Updates per-cell statistics for persistence and variance algorithms.
 * Tracks frame counts, depth sums, and maintains a sliding window by removing
 * cells that haven't been seen recently to prevent unbounded memory growth.
 */
void UpdateCellStatistics(std::map<GridKey, PointCloudPoint>& pointCloudGrid,
                          OccupancyState& state) {
    state.totalFrames++;
    
    // Clean up old statistics using sliding window approach
    if (state.totalFrames > OccupancyParams::PERSISTENCE_WINDOW_SIZE * 2) {
        auto it = state.cellStatistics.begin();
        while (it != state.cellStatistics.end()) {
            if (state.totalFrames - it->second.lastSeenFrame > OccupancyParams::PERSISTENCE_WINDOW_SIZE * 2) {
                it = state.cellStatistics.erase(it);
            } else {
                ++it;
            }
        }
    }
    
    // Update statistics for all current cells
    for (const auto& pair : pointCloudGrid) {
        CellStatistics& stats = state.cellStatistics[pair.first];
        stats.frameCount++;
        stats.lastSeenFrame = state.totalFrames;
        
        float depth = pair.second.position.z;
        stats.depthSum += depth;
        stats.depthSumSq += depth * depth;
    }
}

/**
 * Computes 3D neighbor offsets within a given radius for eviction-based-2 algorithm.
 * Results are cached and only recomputed when the radius changes.
 */
void ComputeEviction2NeighborOffsets(float radius, 
                                     std::vector<std::tuple<int, int, int>>& neighborOffsets,
                                     float& cachedRadius) {
    if (std::abs(radius - cachedRadius) < 0.001f && !neighborOffsets.empty()) {
        return;
    }
    
    neighborOffsets.clear();
    cachedRadius = radius;
    
    int radiusCells = static_cast<int>(std::ceil(radius));
    
    // Generate all 3D neighbor offsets within radius
    for (int dx = -radiusCells; dx <= radiusCells; ++dx) {
        for (int dy = -radiusCells; dy <= radiusCells; ++dy) {
            for (int dz = -radiusCells; dz <= radiusCells; ++dz) {
                if (dx == 0 && dy == 0 && dz == 0) continue;
                
                float dist = std::sqrt(static_cast<float>(dx*dx + dy*dy + dz*dz));
                if (dist <= radius) {
                    neighborOffsets.push_back({dx, dy, dz});
                }
            }
        }
    }
}

/**
 * Algorithm 1: Neighbor Consistency
 * 
 * Computes weights based on how consistent each point's depth is with its 2D XY neighbors.
 * Uses a Huber-style weight function: w = 1 / (1 + (r/r0)^2) where r is the residual
 * from the local median depth. Points that are outliers relative to their neighbors
 * receive lower weights.
 */
void ApplyNeighborConsistency(std::map<GridKey, PointCloudPoint>& pointCloudGrid,
                               const OccupancyParams& params) {
    const int radius = static_cast<int>(params.neighborRadius);
    std::vector<std::pair<int, int>> neighborOffsets;
    for (int dx = -radius; dx <= radius; ++dx) {
        for (int dy = -radius; dy <= radius; ++dy) {
            if (dx == 0 && dy == 0) continue;
            neighborOffsets.push_back({dx, dy});
        }
    }
    
    for (auto& pair : pointCloudGrid) {
        const GridKey& key = pair.first;
        
        // Collect neighbor depths at same Z level
        std::vector<float> neighborDepths;
        for (const auto& offset : neighborOffsets) {
            GridKey neighborKey(key.x + offset.first, key.y + offset.second, key.z);
            auto it = pointCloudGrid.find(neighborKey);
            if (it != pointCloudGrid.end()) {
                neighborDepths.push_back(it->second.position.z);
            }
        }
        
        if (neighborDepths.size() >= 3) {
            std::sort(neighborDepths.begin(), neighborDepths.end());
            float median = neighborDepths[neighborDepths.size() / 2];
            float residual = std::abs(pair.second.position.z - median);
            
            // Huber-style weight: penalize large residuals
            float r0 = params.residualThreshold;
            float weight = 1.0f / (1.0f + (residual / r0) * (residual / r0));
            pair.second.weight *= weight;
        }
    }
}

/**
 * Algorithm 2: Persistence
 * 
 * Rewards points that appear consistently across frames. Computes persistence as the
 * ratio of frames where a cell appears to total frames (using a sliding window).
 * Applies gamma sharpening: w = p^γ to emphasize high-persistence points.
 * New cells (not yet in statistics) receive moderate weight (0.5).
 */
void ApplyPersistence(std::map<GridKey, PointCloudPoint>& pointCloudGrid,
                      const OccupancyParams& params,
                      const OccupancyState& state) {
    if (state.totalFrames <= 0) {
        return;
    }
    
    for (auto& pair : pointCloudGrid) {
        const GridKey& key = pair.first;
        auto statsIt = state.cellStatistics.find(key);
        
        if (statsIt != state.cellStatistics.end()) {
            const CellStatistics& stats = statsIt->second;
            float p;
            
            // Compute persistence using sliding window approach
            if (state.totalFrames <= OccupancyParams::PERSISTENCE_WINDOW_SIZE) {
                p = static_cast<float>(stats.frameCount) / static_cast<float>(state.totalFrames);
            } else {
                int framesSinceLastSeen = state.totalFrames - stats.lastSeenFrame;
                if (framesSinceLastSeen < OccupancyParams::PERSISTENCE_WINDOW_SIZE) {
                    float appearanceRate = static_cast<float>(stats.frameCount) / static_cast<float>(state.totalFrames);
                    p = appearanceRate;
                } else {
                    p = 0.1f;  // Low persistence for cells not seen recently
                }
            }
            
            p = std::max(0.0f, std::min(1.0f, p));
            float weight = std::pow(p, params.persistenceGamma);
            pair.second.weight *= weight;
        } else {
            pair.second.weight *= 0.5f;  // New cells get moderate weight
        }
    }
}

/**
 * Algorithm 3: Variance-based
 * 
 * Penalizes points with high depth variance across frames. Stable points (low variance)
 * receive higher weights. Uses exponential decay: w = exp(-(σ/σ0)^2) where σ is the
 * standard deviation of depth measurements and σ0 is the threshold.
 */
void ApplyVariance(std::map<GridKey, PointCloudPoint>& pointCloudGrid,
                   const OccupancyParams& params,
                   const OccupancyState& state) {
    for (auto& pair : pointCloudGrid) {
        const GridKey& key = pair.first;
        auto statsIt = state.cellStatistics.find(key);
        
        if (statsIt != state.cellStatistics.end() && statsIt->second.frameCount > 1) {
            const CellStatistics& stats = statsIt->second;
            
            float meanDepth = stats.depthSum / static_cast<float>(stats.frameCount);
            float meanDepthSq = stats.depthSumSq / static_cast<float>(stats.frameCount);
            float variance = meanDepthSq - (meanDepth * meanDepth);
            float stddev = std::sqrt(std::max(0.0f, variance));
            
            float sigma0 = params.varianceThreshold;
            if (sigma0 > 0.0001f) {
                float weight = std::exp(-(stddev / sigma0) * (stddev / sigma0));
                pair.second.weight *= weight;
            }
        }
    }
}

/**
 * Algorithm 4: Eviction-based (temporal weight tracking)
 * 
 * Strategy 1 (Original): Tracks weights over time using double-buffering. Persisting cells gradually increase
 * weight (up to 1.0), while disappeared cells decay. New cells start with low weight.
 * Uses squared increment/decrement formulas to ensure smooth weight transitions.
 * 
 * Strategy 2 (New): Tracks removal count for each grid cell. If grid is occupied, count resets to 0 and
 * weight is multiplied by increment. If grid was previously occupied but isn't on current frame, increment count.
 * If count reaches threshold, remove the grid value from the map. If grid gets unoccupied, update weight by
 * subtracting ((decay value) ^ (decay count))/100. New cells start with minimum start value.
 */
void ApplyEviction(std::map<GridKey, PointCloudPoint>& pointCloudGrid,
                   const OccupancyParams& params,
                   OccupancyState& state) {
    if (params.evictionUseStrategy2) {
        // Strategy 2: Removal count-based eviction
        
        // Track which cells are currently occupied
        std::set<GridKey> currentKeys;
        for (const auto& pair : pointCloudGrid) {
            currentKeys.insert(pair.first);
        }
        
        // Process cells that were previously occupied but aren't now (unoccupied)
        std::vector<GridKey> keysToRemove;
        for (const auto& oldPair : state.evictionWeightsOld) {
            const GridKey& key = oldPair.first;
            if (currentKeys.find(key) == currentKeys.end()) {
                // Cell was previously occupied but isn't now
                int& removedCount = state.evictionRemovalCounts[key];
                removedCount++;
                
                if (removedCount >= params.evictionRemovalThreshold) {
                    // Remove from map when count reaches threshold
                    keysToRemove.push_back(key);
                } else {
                    // Update weight by subtracting ((decay^count)/100)
                    float currentWeight = oldPair.second;
                    float decayAmount = std::pow(params.evictionDecay, static_cast<float>(removedCount)) / 100.0f;
                    float newWeight = std::max(0.0f, currentWeight - decayAmount);
                    
                    // Ensure weight doesn't go below minimum start value (unless being removed)
                    if (newWeight > 0.0f) {
                        newWeight = std::max(params.evictionMinStartValue, newWeight);
                    }
                    
                    state.evictionWeightsNew[key] = newWeight;
                }
            }
        }
        
        // Remove cells that exceeded removal threshold
        for (const GridKey& key : keysToRemove) {
            state.evictionWeightsOld.erase(key);
            state.evictionWeightsNew.erase(key);
            state.evictionRemovalCounts.erase(key);
        }
        
        // Process currently occupied cells
        for (auto& pair : pointCloudGrid) {
            const GridKey& key = pair.first;
            
            // Reset count when occupied
            state.evictionRemovalCounts[key] = 0;
            
            float newWeight;
            if (state.evictionWeightsOld.find(key) == state.evictionWeightsOld.end()) {
                // New cell: start with minimum start value
                newWeight = params.evictionMinStartValue;
            } else {
                // Persisting cell: multiply weight by increment
                float oldWeight = state.evictionWeightsOld[key];
                newWeight = oldWeight * params.evictionIncrement;
                // Clamp to [min, 1.0]
                newWeight = std::min(1.0f, std::max(params.evictionMinStartValue, newWeight));
            }
            
            state.evictionWeightsNew[key] = newWeight;
            // Apply weight directly to point cloud
            pair.second.weight = newWeight;
        }
        
        // Update state for next frame
        std::swap(state.evictionWeightsOld, state.evictionWeightsNew);
        state.evictionWeightsNew.clear();
        
    } else {
        // Strategy 1: Original eviction algorithm
        std::swap(state.evictionWeightsOld, state.evictionWeightsNew);
        std::swap(state.evictionKeysOld, state.evictionKeysNew);
        state.evictionWeightsNew.clear();
        state.evictionKeysNew.clear();
        
        for (const auto& pair : pointCloudGrid) {
            state.evictionKeysNew.insert(pair.first);
        }
        
        // Decay weights for disappeared cells
        for (const auto& oldPair : state.evictionWeightsOld) {
            const GridKey& key = oldPair.first;
            if (state.evictionKeysNew.find(key) == state.evictionKeysNew.end()) {
                float currentWeight = oldPair.second;
                float decrementDelta = std::pow(params.evictionMinStartValue + (1.0f - currentWeight) * params.evictionDecay, 2.0f);
                float newWeight = currentWeight - decrementDelta;
                
                if (newWeight > 0.0f) {
                    state.evictionWeightsNew[key] = newWeight;
                    state.evictionKeysNew.insert(key);
                }
            }
        }
        
        // Process current cells: increment persisting, initialize new
        for (const auto& pair : pointCloudGrid) {
            const GridKey& key = pair.first;
            float newWeight;
            
            if (state.evictionKeysOld.find(key) == state.evictionKeysOld.end()) {
                // New or reappearing cell
                auto decayedIt = state.evictionWeightsOld.find(key);
                newWeight = (decayedIt != state.evictionWeightsOld.end()) 
                    ? decayedIt->second
                    : params.evictionIncrement;
            } else {
                // Persisting cell: increment weight
                float oldWeight = 0.0f;
                auto oldIt = state.evictionWeightsOld.find(key);
                if (oldIt != state.evictionWeightsOld.end()) {
                    oldWeight = oldIt->second;
                }
                
                float incrementDelta = std::pow(params.evictionMinStartValue + oldWeight * params.evictionIncrement, 2.0f);
                newWeight = std::min(1.0f, oldWeight + incrementDelta);
            }
            
            state.evictionWeightsNew[key] = newWeight;
        }
        
        // Apply eviction weights to point cloud weights
        for (auto& pair : pointCloudGrid) {
            const GridKey& key = pair.first;
            auto evictionIt = state.evictionWeightsNew.find(key);
            if (evictionIt != state.evictionWeightsNew.end()) {
                pair.second.weight *= evictionIt->second;
            } else {
                pair.second.weight *= params.evictionIncrement;
            }
        }
    }
}

/**
 * Algorithm 5: Eviction-based-2 (density-based weighting)
 * 
 * Rewards points in dense regions by checking 3D neighbor occupancy. Points with
 * sufficient neighbors (>= minNeighbors) keep full weight. Isolated points are
 * penalized proportionally to their neighbor count.
 */
void ApplyEviction2(std::map<GridKey, PointCloudPoint>& pointCloudGrid,
                    const OccupancyParams& params,
                    OccupancyState& state) {
    ComputeEviction2NeighborOffsets(params.eviction2Radius, 
                                    state.eviction2NeighborOffsets,
                                    state.eviction2CachedRadius);
    
    for (auto& pair : pointCloudGrid) {
        const GridKey& key = pair.first;
        
        // Count occupied neighbors in 3D space
        int neighborCount = 0;
        for (const auto& offset : state.eviction2NeighborOffsets) {
            GridKey neighborKey(
                key.x + std::get<0>(offset),
                key.y + std::get<1>(offset),
                key.z + std::get<2>(offset)
            );
            
            if (pointCloudGrid.find(neighborKey) != pointCloudGrid.end()) {
                neighborCount++;
            }
        }
        
        // Penalize isolated points, reward dense regions
        if (neighborCount < params.eviction2MinNeighbors) {
            float neighborRatio = static_cast<float>(neighborCount) / static_cast<float>(params.eviction2MinNeighbors);
            neighborRatio = std::max(0.0f, std::min(1.0f, neighborRatio));
            pair.second.weight *= neighborRatio;
        }
    }
}

/**
 * Applies all enabled occupancy algorithms to the gridded point cloud.
 * 
 * Algorithms are applied multiplicatively - each enabled algorithm modifies weights
 * by multiplying with existing values. Final weights are clamped to [0, 1] range.
 * All algorithms operate only on gridded (snapped) data, not raw point positions.
 */
void ApplyOccupancyAlgorithms(std::map<GridKey, PointCloudPoint>& pointCloudGrid,
                               const OccupancyParams& params,
                               OccupancyState& state) {
    if (pointCloudGrid.empty()) {
        return;
    }
    
    if (params.persistenceEnabled || params.varianceEnabled) {
        UpdateCellStatistics(pointCloudGrid, state);
    }
    
    // Apply enabled algorithms multiplicatively - each algorithm modifies weights directly
    if (params.neighborConsistencyEnabled) {
        ApplyNeighborConsistency(pointCloudGrid, params);
    }
    if (params.persistenceEnabled) {
        ApplyPersistence(pointCloudGrid, params, state);
    }
    if (params.varianceEnabled) {
        ApplyVariance(pointCloudGrid, params, state);
    }
    if (params.evictionEnabled) {
        ApplyEviction(pointCloudGrid, params, state);
    }
    if (params.eviction2Enabled) {
        ApplyEviction2(pointCloudGrid, params, state);
    }
    
    // Clamp final weights to [0, 1] range
    for (auto& pair : pointCloudGrid) {
        pair.second.weight = std::max(0.0f, std::min(1.0f, pair.second.weight));
    }
}

} // namespace GridBased
} // namespace CameraSystem
} // namespace Editors
} // namespace Engine
