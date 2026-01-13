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
// NOTE: Low-pass filtering should be applied separately using algorithms/LowPassFilter.h
void TOFProcessing::ExtractGriddedPointCloud(const std::map<GridKey, PointCloudPoint>& pointCloudGrid,
                                              const std::map<GridKey, GridAccumulator>& gridAccumulators,
                                              TOFGridMode mode,
                                              std::vector<PointCloudPoint>& output) {
    output.clear();
    
    if (mode == TOFGridMode::Average) {
        output.reserve(gridAccumulators.size());
        for (const auto& pair : gridAccumulators) {
            const GridAccumulator& acc = pair.second;
            PointCloudPoint point;
            const float invCount = 1.0f / static_cast<float>(acc.count);
            point.position = acc.positionSum * invCount;
            point.color = acc.colorSum * invCount;
            output.push_back(point);
        }
    } else {
        output.reserve(pointCloudGrid.size());
        for (const auto& pair : pointCloudGrid) {
            output.push_back(pair.second);
        }
    }
}

// Clear all TOF processing data
void TOFProcessing::ClearAll() {
    ClearTSDF();
}

// Main gridding function - processes entire point cloud into grid
void TOFProcessing::ApplyGridding(const std::vector<PointCloudPoint>& inputPoints,
                                 std::map<GridKey, PointCloudPoint>& outputGrid,
                                 TOFGridMode mode,
                                 float gridSize) {
    outputGrid.clear();
    
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
            std::map<int32_t, int> segmentVotes;  // Majority voting for segment ID
            std::map<int32_t, int> cameraVotes;   // Majority voting for camera ID
            
            for (const auto& p : points) {
                posSum += p.position;
                colorSum += p.color;
                if (p.segmentId >= 0) {
                    segmentVotes[p.segmentId]++;
                }
                if (p.cameraId >= 0) {
                    cameraVotes[p.cameraId]++;
                }
            }
            
            PointCloudPoint averagedPoint;
            averagedPoint.position = posSum / static_cast<float>(points.size());
            averagedPoint.color = colorSum / static_cast<float>(points.size());
            // Normalize weight by number of points: points/100, capped at 1.0
            averagedPoint.weight = std::min(1.0f, static_cast<float>(points.size()) / 100.0f);
            
            // Use majority voting for segment ID
            int32_t bestSegId = -1;
            int bestSegCount = 0;
            for (const auto& vote : segmentVotes) {
                if (vote.second > bestSegCount) {
                    bestSegCount = vote.second;
                    bestSegId = vote.first;
                }
            }
            averagedPoint.segmentId = bestSegId;
            
            // Use majority voting for camera ID
            int32_t bestCamId = -1;
            int bestCamCount = 0;
            for (const auto& vote : cameraVotes) {
                if (vote.second > bestCamCount) {
                    bestCamCount = vote.second;
                    bestCamId = vote.first;
                }
            }
            averagedPoint.cameraId = bestCamId;
            
            outputGrid[gridKey] = averagedPoint;
        }
    } else if (mode == TOFGridMode::FirstCome) {
        // Save first value, ignore others (preserves segment ID from first point)
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
        // Collect segment IDs and camera IDs for majority voting, and average colors
        std::map<GridKey, std::map<int32_t, int>> gridSegmentVotes;
        std::map<GridKey, std::map<int32_t, int>> gridCameraVotes;
        std::map<GridKey, glm::vec3> gridColorSum;
        std::map<GridKey, int> gridColorCount;
        
        for (const auto& point : inputPoints) {
            GridKey gridKey = WorldToGridKey(point.position, gridSize);
            if (point.segmentId >= 0) {
                gridSegmentVotes[gridKey][point.segmentId]++;
            }
            if (point.cameraId >= 0) {
                gridCameraVotes[gridKey][point.cameraId]++;
            }
            if (gridColorSum.count(gridKey) == 0) {
                gridColorSum[gridKey] = point.color;
                gridColorCount[gridKey] = 1;
            } else {
                gridColorSum[gridKey] += point.color;
                gridColorCount[gridKey]++;
            }
        }
        
        // Create points at grid centers
        for (const auto& pair : gridColorSum) {
            const GridKey& gridKey = pair.first;
            
            glm::vec3 snappedPos(
                gridKey.x * gridSize,
                gridKey.y * gridSize,
                gridKey.z * gridSize
            );
            
            PointCloudPoint gridPoint;
            gridPoint.position = snappedPos;
            gridPoint.color = pair.second / static_cast<float>(gridColorCount[gridKey]);
            gridPoint.weight = 1.0f;
            
            // Use majority voting for segment ID
            if (gridSegmentVotes.count(gridKey)) {
                int32_t bestSegId = -1;
                int bestCount = 0;
                for (const auto& vote : gridSegmentVotes[gridKey]) {
                    if (vote.second > bestCount) {
                        bestCount = vote.second;
                        bestSegId = vote.first;
                    }
                }
                gridPoint.segmentId = bestSegId;
            }
            
            // Use majority voting for camera ID
            if (gridCameraVotes.count(gridKey)) {
                int32_t bestCamId = -1;
                int bestCount = 0;
                for (const auto& vote : gridCameraVotes[gridKey]) {
                    if (vote.second > bestCount) {
                        bestCount = vote.second;
                        bestCamId = vote.first;
                    }
                }
                gridPoint.cameraId = bestCamId;
            }
            
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

// Update surface flow history with current frame data
// Shifts buffers: buffer[2] = buffer[1], buffer[1] = buffer[0], buffer[0] = currentFrame
void TOFProcessing::UpdateSurfaceFlowHistory(const std::map<GridKey, PointCloudPoint>& currentFrame) {
    // Shift buffers - oldest frame is discarded
    m_surfaceFlowHistory.frameBuffers[2] = std::move(m_surfaceFlowHistory.frameBuffers[1]);
    m_surfaceFlowHistory.frameBuffers[1] = std::move(m_surfaceFlowHistory.frameBuffers[0]);
    m_surfaceFlowHistory.frameBuffers[0] = currentFrame;
    
    // Increment frame count up to maximum of 3
    if (m_surfaceFlowHistory.frameCount < 3) {
        m_surfaceFlowHistory.frameCount++;
    }
}

// Clear surface flow history
void TOFProcessing::ClearSurfaceFlowHistory() {
    m_surfaceFlowHistory.Clear();
}

// Apply surface flow filtering to remove outlier points that don't match local motion patterns
// Algorithm:
// 1. For each point in current frame (buffer 0), find corresponding points in previous frames
//    by searching within a neighborhood radius (points may have moved)
// 2. Estimate local flow vectors by finding nearest neighbors in previous frames
// 3. Compute mean flow for the local neighborhood
// 4. Remove points whose motion deviates significantly from the local mean flow
void TOFProcessing::ApplySurfaceFlowFilter(std::map<GridKey, PointCloudPoint>& pointCloud,
                                           float neighborRadius,
                                           float flowThreshold,
                                           float gridSize) {
    // Need at least 2 frames of history to compute flow
    if (m_surfaceFlowHistory.frameCount < 2) {
        return;
    }
    
    const auto& prevFrame = m_surfaceFlowHistory.frameBuffers[1];
    if (prevFrame.empty()) {
        return;
    }
    
    // Build spatial index for previous frame (simple grid-based)
    // Key: grid cell at coarser resolution for neighbor search
    const float searchGridSize = neighborRadius;
    std::map<GridKey, std::vector<const PointCloudPoint*>> prevSpatialIndex;
    
    for (const auto& pair : prevFrame) {
        GridKey searchKey = WorldToGridKey(pair.second.position, searchGridSize);
        prevSpatialIndex[searchKey].push_back(&pair.second);
    }
    
    // For each point in current frame, compute flow and check consistency
    std::vector<GridKey> keysToRemove;
    
    for (auto& pair : pointCloud) {
        const GridKey& key = pair.first;
        PointCloudPoint& point = pair.second;
        
        // Only process points with valid segment IDs
        if (point.segmentId < 0) {
            continue;
        }
        
        // Find neighbors in previous frame within radius
        GridKey searchKey = WorldToGridKey(point.position, searchGridSize);
        std::vector<glm::vec3> localFlows;
        
        // Search in 3x3x3 neighborhood of search grid cells
        for (int dx = -1; dx <= 1; dx++) {
            for (int dy = -1; dy <= 1; dy++) {
                for (int dz = -1; dz <= 1; dz++) {
                    GridKey neighborSearchKey(searchKey.x + dx, searchKey.y + dy, searchKey.z + dz);
                    
                    auto it = prevSpatialIndex.find(neighborSearchKey);
                    if (it == prevSpatialIndex.end()) continue;
                    
                    for (const PointCloudPoint* prevPoint : it->second) {
                        // Only match points from same segment
                        if (prevPoint->segmentId != point.segmentId) {
                            continue;
                        }
                        
                        float dist = glm::length(point.position - prevPoint->position);
                        if (dist < neighborRadius) {
                            // Flow vector from previous to current
                            glm::vec3 flow = point.position - prevPoint->position;
                            localFlows.push_back(flow);
                        }
                    }
                }
            }
        }
        
        // Need enough flow samples for reliable estimation
        if (localFlows.size() < 3) {
            // Not enough data - keep point but mark as uncertain
            continue;
        }
        
        // Compute mean flow
        glm::vec3 meanFlow(0.0f);
        for (const auto& flow : localFlows) {
            meanFlow += flow;
        }
        meanFlow /= static_cast<float>(localFlows.size());
        
        // Compute this point's flow (use nearest neighbor in prev frame)
        float minDist = std::numeric_limits<float>::max();
        glm::vec3 pointFlow(0.0f);
        bool foundMatch = false;
        
        for (int dx = -1; dx <= 1; dx++) {
            for (int dy = -1; dy <= 1; dy++) {
                for (int dz = -1; dz <= 1; dz++) {
                    GridKey neighborSearchKey(searchKey.x + dx, searchKey.y + dy, searchKey.z + dz);
                    
                    auto it = prevSpatialIndex.find(neighborSearchKey);
                    if (it == prevSpatialIndex.end()) continue;
                    
                    for (const PointCloudPoint* prevPoint : it->second) {
                        if (prevPoint->segmentId != point.segmentId) {
                            continue;
                        }
                        
                        float dist = glm::length(point.position - prevPoint->position);
                        if (dist < minDist && dist < neighborRadius) {
                            minDist = dist;
                            pointFlow = point.position - prevPoint->position;
                            foundMatch = true;
                        }
                    }
                }
            }
        }
        
        if (!foundMatch) {
            // No match in previous frame - this could be a new point or an outlier
            // Keep it for now (don't remove new valid points)
            continue;
        }
        
        // Check if point's flow deviates from local mean
        float flowDeviation = glm::length(pointFlow - meanFlow);
        
        if (flowDeviation > flowThreshold) {
            // This point's motion is inconsistent with local surface flow
            // Mark for removal
            keysToRemove.push_back(key);
        }
    }
    
    // Remove outlier points
    for (const GridKey& key : keysToRemove) {
        pointCloud.erase(key);
    }
}

} // namespace CameraSystem
} // namespace Editors
} // namespace Engine

