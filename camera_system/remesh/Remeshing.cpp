#include "Remeshing.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <cstddef>
#include <random>
#include <set>
#include <queue>
#include <map>
#include <unordered_map>
#include <functional>
#include <glm/glm.hpp>
#include <queue>

namespace Engine {
namespace Editors {
namespace CameraSystem {

Remeshing::Remeshing() {
}

Remeshing::~Remeshing() {
}

RemeshResult Remeshing::GenerateMesh(
    const std::vector<PointCloudPoint>& pointCloud,
    const std::map<GridKey, PointCloudPoint>& pointCloudGrid,
    bool useGridding,
    float gridSize,
    Algorithm algorithm,
    const AlgorithmParameters& params
) {
    RemeshResult result;
    result.triangleCount = 0;
    result.vertexCount = 0;

    // If gridding is enabled, convert grid to point cloud vector for all algorithms
    std::vector<PointCloudPoint> workingPointCloud;
    if (useGridding && !pointCloudGrid.empty()) {
        // Convert gridded points to regular point cloud vector
        workingPointCloud.reserve(pointCloudGrid.size());
        for (const auto& pair : pointCloudGrid) {
            workingPointCloud.push_back(pair.second);
        }
    } else if (!pointCloud.empty()) {
        // Use regular point cloud
        workingPointCloud = pointCloud;
    } else {
        // No data available
        return result;
    }

    // Apply selected algorithm to the point cloud (gridded or regular)
    AlgorithmParameters adjustedParams = params;
    if (adjustedParams.maxDistance <= 0.0f) {
        adjustedParams.maxDistance = gridSize * 3.0f;
    }
    
    switch (algorithm) {
        case Algorithm::GriddedQuads:
            // GriddedQuads can use the grid directly for better performance
            if (useGridding && !pointCloudGrid.empty()) {
                result = GenerateGriddedQuads(pointCloudGrid, gridSize);
            } else {
                // Fallback to nearest neighbor if no grid available
                result = GenerateNearestNeighbor(workingPointCloud, adjustedParams.maxDistance);
            }
            break;
        case Algorithm::NearestNeighbor:
            result = GenerateNearestNeighbor(workingPointCloud, adjustedParams.maxDistance);
            break;
        case Algorithm::Delaunay2D:
            result = GenerateDelaunay2D(workingPointCloud, adjustedParams);
            break;
        case Algorithm::PoissonLike:
            result = GeneratePoissonLike(workingPointCloud, adjustedParams);
            break;
        case Algorithm::FCTM:
            result = GenerateFCTM(workingPointCloud, adjustedParams);
            break;
        case Algorithm::GeodesicTree:
            result = GenerateGeodesicTree(workingPointCloud, adjustedParams);
            break;
        case Algorithm::GTree:
            result = GenerateGTree(workingPointCloud, adjustedParams);
            break;
        default:
            result = GenerateNearestNeighbor(workingPointCloud, adjustedParams.maxDistance);
            break;
    }

    if (!result.vertices.empty() && !result.indices.empty()) {
        CalculateNormals(result.vertices, result.indices);
        result.triangleCount = result.indices.size() / 3;
        result.vertexCount = result.vertices.size();
    }

    return result;
}

RemeshResult Remeshing::GenerateGriddedQuads(
    const std::map<GridKey, PointCloudPoint>& pointCloudGrid,
    float gridSize
) {
    RemeshResult result;
    std::map<GridKey, size_t> gridKeyToIndex;
    std::set<std::tuple<unsigned int, unsigned int, unsigned int>> triangleSet;
    size_t vertexIndex = 0;

    // First pass: create vertices from grid points
    for (const auto& pair : pointCloudGrid) {
        const PointCloudPoint& point = pair.second;
        MeshVertex vertex;
        vertex.position = point.position;
        vertex.color = point.color;
        vertex.normal = glm::vec3(0.0f, 0.0f, 1.0f);
        result.vertices.push_back(vertex);
        gridKeyToIndex[pair.first] = vertexIndex++;
    }

    // Helper to add triangle if valid and not duplicate
    auto addTriangle = [&](unsigned int i0, unsigned int i1, unsigned int i2) {
        if (IsValidTriangle(
            result.vertices[i0].position,
            result.vertices[i1].position,
            result.vertices[i2].position
        )) {
            AddTriangleIfValid(result.indices, triangleSet, i0, i1, i2, result.vertices);
        }
    };

    // Improved gridded mesh generation: create triangles from adjacent cells
    // This version fills gaps better by creating triangles even when not all 4 corners exist
    for (const auto& pair : pointCloudGrid) {
        const GridKey& key = pair.first;
        size_t currentIdx = gridKeyToIndex[key];

        // Try to create quads in XY plane (constant Z)
        GridKey xPlus = {key.x + 1, key.y, key.z};
        GridKey yPlus = {key.x, key.y + 1, key.z};
        GridKey xyPlus = {key.x + 1, key.y + 1, key.z};

        // If all 4 corners exist, create 2 triangles forming a quad
        if (gridKeyToIndex.find(xPlus) != gridKeyToIndex.end() &&
            gridKeyToIndex.find(yPlus) != gridKeyToIndex.end() &&
            gridKeyToIndex.find(xyPlus) != gridKeyToIndex.end()) {
            size_t idxX = gridKeyToIndex[xPlus];
            size_t idxY = gridKeyToIndex[yPlus];
            size_t idxXY = gridKeyToIndex[xyPlus];
            addTriangle(static_cast<unsigned int>(currentIdx), static_cast<unsigned int>(idxX), static_cast<unsigned int>(idxXY));
            addTriangle(static_cast<unsigned int>(currentIdx), static_cast<unsigned int>(idxXY), static_cast<unsigned int>(idxY));
        }
        // If only 3 corners exist, create a single triangle
        else if (gridKeyToIndex.find(xPlus) != gridKeyToIndex.end() &&
                 gridKeyToIndex.find(yPlus) != gridKeyToIndex.end()) {
            size_t idxX = gridKeyToIndex[xPlus];
            size_t idxY = gridKeyToIndex[yPlus];
            addTriangle(static_cast<unsigned int>(currentIdx), static_cast<unsigned int>(idxX), static_cast<unsigned int>(idxY));
        }
        else if (gridKeyToIndex.find(xPlus) != gridKeyToIndex.end() &&
                 gridKeyToIndex.find(xyPlus) != gridKeyToIndex.end()) {
            size_t idxX = gridKeyToIndex[xPlus];
            size_t idxXY = gridKeyToIndex[xyPlus];
            addTriangle(static_cast<unsigned int>(currentIdx), static_cast<unsigned int>(idxX), static_cast<unsigned int>(idxXY));
        }
        else if (gridKeyToIndex.find(yPlus) != gridKeyToIndex.end() &&
                 gridKeyToIndex.find(xyPlus) != gridKeyToIndex.end()) {
            size_t idxY = gridKeyToIndex[yPlus];
            size_t idxXY = gridKeyToIndex[xyPlus];
            addTriangle(static_cast<unsigned int>(currentIdx), static_cast<unsigned int>(idxY), static_cast<unsigned int>(idxXY));
        }

        // Try to create quads in XZ plane (constant Y)
        GridKey zPlus = {key.x, key.y, key.z + 1};
        GridKey xzPlus = {key.x + 1, key.y, key.z + 1};

        if (gridKeyToIndex.find(xPlus) != gridKeyToIndex.end() &&
            gridKeyToIndex.find(zPlus) != gridKeyToIndex.end() &&
            gridKeyToIndex.find(xzPlus) != gridKeyToIndex.end()) {
            size_t idxX = gridKeyToIndex[xPlus];
            size_t idxZ = gridKeyToIndex[zPlus];
            size_t idxXZ = gridKeyToIndex[xzPlus];
            addTriangle(static_cast<unsigned int>(currentIdx), static_cast<unsigned int>(idxX), static_cast<unsigned int>(idxXZ));
            addTriangle(static_cast<unsigned int>(currentIdx), static_cast<unsigned int>(idxXZ), static_cast<unsigned int>(idxZ));
        }
        else if (gridKeyToIndex.find(xPlus) != gridKeyToIndex.end() &&
                 gridKeyToIndex.find(zPlus) != gridKeyToIndex.end()) {
            size_t idxX = gridKeyToIndex[xPlus];
            size_t idxZ = gridKeyToIndex[zPlus];
            addTriangle(static_cast<unsigned int>(currentIdx), static_cast<unsigned int>(idxX), static_cast<unsigned int>(idxZ));
        }

        // Try to create quads in YZ plane (constant X)
        GridKey yzPlus = {key.x, key.y + 1, key.z + 1};

        if (gridKeyToIndex.find(yPlus) != gridKeyToIndex.end() &&
            gridKeyToIndex.find(zPlus) != gridKeyToIndex.end() &&
            gridKeyToIndex.find(yzPlus) != gridKeyToIndex.end()) {
            size_t idxY = gridKeyToIndex[yPlus];
            size_t idxZ = gridKeyToIndex[zPlus];
            size_t idxYZ = gridKeyToIndex[yzPlus];
            addTriangle(static_cast<unsigned int>(currentIdx), static_cast<unsigned int>(idxY), static_cast<unsigned int>(idxYZ));
            addTriangle(static_cast<unsigned int>(currentIdx), static_cast<unsigned int>(idxYZ), static_cast<unsigned int>(idxZ));
        }
        else if (gridKeyToIndex.find(yPlus) != gridKeyToIndex.end() &&
                 gridKeyToIndex.find(zPlus) != gridKeyToIndex.end()) {
            size_t idxY = gridKeyToIndex[yPlus];
            size_t idxZ = gridKeyToIndex[zPlus];
            addTriangle(static_cast<unsigned int>(currentIdx), static_cast<unsigned int>(idxY), static_cast<unsigned int>(idxZ));
        }
    }

    return result;
}

RemeshResult Remeshing::GenerateNearestNeighbor(
    const std::vector<PointCloudPoint>& pointCloud,
    float maxDistance
) {
    RemeshResult result;
    std::set<std::tuple<unsigned int, unsigned int, unsigned int>> triangleSet;

    // Create vertices from point cloud
    for (const auto& point : pointCloud) {
        MeshVertex vertex;
        vertex.position = point.position;
        vertex.color = point.color;
        vertex.normal = glm::vec3(0.0f, 0.0f, 1.0f);
        result.vertices.push_back(vertex);
    }

    // Improved nearest neighbor triangulation
    // For each point, find neighbors and create triangles more intelligently
    for (size_t i = 0; i < result.vertices.size(); i++) {
        std::vector<std::pair<float, size_t>> neighbors;
        FindNeighbors(i, pointCloud, maxDistance, neighbors);

        if (neighbors.size() < 2) continue;

        // Sort neighbors by distance
        std::sort(neighbors.begin(), neighbors.end());

        // Limit to closest neighbors to avoid creating too many triangles
        const size_t maxNeighbors = std::min(neighbors.size(), size_t(20));
        
        // Create triangles with the closest neighbors
        // Use a more systematic approach: for each neighbor pair, check if triangle is valid
        for (size_t n = 0; n < maxNeighbors; n++) {
            size_t j = neighbors[n].second;
            
            // Try to form triangles with other close neighbors
            for (size_t k = n + 1; k < maxNeighbors; k++) {
                size_t k_idx = neighbors[k].second;
                
                // Check all edge distances
                float dist_ij = neighbors[n].first;
                float dist_ik = neighbors[k].first;
                float dist_jk = glm::length(result.vertices[j].position - result.vertices[k_idx].position);
                
                // Only create triangle if all edges are reasonable
                if (dist_ij <= maxDistance && dist_ik <= maxDistance && dist_jk <= maxDistance) {
                    if (IsValidTriangle(
                        result.vertices[i].position,
                        result.vertices[j].position,
                        result.vertices[k_idx].position
                    )) {
                        AddTriangleIfValid(result.indices, triangleSet, 
                            static_cast<unsigned int>(i), 
                            static_cast<unsigned int>(j), 
                            static_cast<unsigned int>(k_idx),
                            result.vertices);
                    }
                }
            }
        }
    }

    return result;
}

RemeshResult Remeshing::GenerateDelaunay2D(
    const std::vector<PointCloudPoint>& pointCloud,
    const AlgorithmParameters& params
) {
    RemeshResult result;
    if (pointCloud.empty()) {
        return result;
    }

    // Use weights flag from parameters (set by UI)
    const bool useWeights = params.useWeights;

    // Project points to 2D plane
    std::vector<Point2D> points2D = ProjectToPlane(pointCloud, params.projectionPlane);
    
    if (points2D.size() < 3) {
        // Not enough points, fall back to nearest neighbor
        return GenerateNearestNeighbor(pointCloud, params.maxDistance);
    }

    // Create vertices from point cloud
    for (const auto& point : pointCloud) {
        MeshVertex vertex;
        vertex.position = point.position;
        vertex.color = point.color;
        vertex.normal = glm::vec3(0.0f, 0.0f, 1.0f);
        result.vertices.push_back(vertex);
    }

    // Simple incremental Delaunay triangulation
    // Start with a super-triangle that contains all points
    float minX = points2D[0].x, maxX = points2D[0].x;
    float minY = points2D[0].y, maxY = points2D[0].y;
    for (const auto& p : points2D) {
        minX = std::min(minX, p.x);
        maxX = std::max(maxX, p.x);
        minY = std::min(minY, p.y);
        maxY = std::max(maxY, p.y);
    }
    
    float margin = std::max(maxX - minX, maxY - minY) * 0.5f;
    // Note: super-triangle points are not used in current implementation
    // They would be needed for a full incremental Delaunay triangulation

    std::set<std::tuple<unsigned int, unsigned int, unsigned int>> triangleSet;
    
    // For each point, find triangles whose circumcircle contains it
    // Then flip edges to maintain Delaunay property
    for (size_t i = 0; i < points2D.size(); i++) {
        std::vector<std::tuple<unsigned int, unsigned int, unsigned int>> badTriangles;
        
        // Find all triangles whose circumcircle contains this point
        // For simplicity, check all existing triangles
        for (const auto& triKey : triangleSet) {
            unsigned int a = std::get<0>(triKey);
            unsigned int b = std::get<1>(triKey);
            unsigned int c = std::get<2>(triKey);
            
            if (a < points2D.size() && b < points2D.size() && c < points2D.size()) {
                if (IsPointInCircumcircle(points2D[a], points2D[b], points2D[c], points2D[i])) {
                    badTriangles.push_back(triKey);
                }
            }
        }
        
        // Remove bad triangles and create new ones
        for (const auto& badTri : badTriangles) {
            triangleSet.erase(badTri);
        }
        
        // Create new triangles connecting point i to edges of bad triangles
        // Simplified: just create triangles with nearest neighbors
        std::vector<std::pair<float, size_t>> neighbors;
        FindNeighbors(i, pointCloud, params.maxDistance, neighbors);
        
        if (neighbors.size() >= 2) {
            std::sort(neighbors.begin(), neighbors.end());
            const size_t maxNeighbors = std::min(neighbors.size(), size_t(params.maxNeighbors));
            
            // If using weights, sort by combined weight and distance
            if (useWeights) {
                std::vector<std::pair<float, size_t>> weightedNeighbors;
                for (const auto& neighbor : neighbors) {
                    float weight = pointCloud[neighbor.second].weight;
                    // Combine distance and weight: prefer closer points with higher weights
                    float score = neighbor.first / (weight + 0.001f);  // Lower score is better
                    weightedNeighbors.push_back({score, neighbor.second});
                }
                std::sort(weightedNeighbors.begin(), weightedNeighbors.end());
                neighbors = weightedNeighbors;
            }
            
            for (size_t n = 0; n < maxNeighbors; n++) {
                size_t j = neighbors[n].second;
                for (size_t k = n + 1; k < maxNeighbors; k++) {
                    size_t k_idx = neighbors[k].second;
                    
                    // Skip triangles with low weights if using weights
                    if (useWeights) {
                        float minWeight = std::min({pointCloud[i].weight, pointCloud[j].weight, pointCloud[k_idx].weight});
                        if (minWeight < 0.3f) continue;  // Skip triangles with very low weights
                    }
                    
                    float dist_ij = glm::length(result.vertices[i].position - result.vertices[j].position);
                    float dist_ik = glm::length(result.vertices[i].position - result.vertices[k_idx].position);
                    float dist_jk = glm::length(result.vertices[j].position - result.vertices[k_idx].position);
                    
                    if (dist_ij <= params.maxDistance && 
                        dist_ik <= params.maxDistance && 
                        dist_jk <= params.maxDistance) {
                        if (IsValidTriangle(
                            result.vertices[i].position,
                            result.vertices[j].position,
                            result.vertices[k_idx].position
                        )) {
                            AddTriangleIfValid(result.indices, triangleSet,
                                static_cast<unsigned int>(i),
                                static_cast<unsigned int>(j),
                                static_cast<unsigned int>(k_idx),
                                result.vertices);
                        }
                    }
                }
            }
        }
    }

    return result;
}

RemeshResult Remeshing::GeneratePoissonLike(
    const std::vector<PointCloudPoint>& pointCloud,
    const AlgorithmParameters& params
) {
    RemeshResult result;
    if (pointCloud.empty()) {
        return result;
    }

    // Use weights flag from parameters (set by UI)
    const bool useWeights = params.useWeights;

    // Create vertices with estimated normals (weighted if using weights)
    std::vector<glm::vec3> estimatedNormals(pointCloud.size());
    for (size_t i = 0; i < pointCloud.size(); i++) {
        if (useWeights && pointCloud[i].weight < 0.3f) {
            // Skip normal estimation for very low weight points
            estimatedNormals[i] = glm::vec3(0.0f, 0.0f, 1.0f);
        } else {
            estimatedNormals[i] = EstimateNormal(i, pointCloud, params.poissonRadius);
        }
        
        MeshVertex vertex;
        vertex.position = pointCloud[i].position;
        vertex.color = pointCloud[i].color;
        vertex.normal = params.poissonSmoothNormals ? estimatedNormals[i] : glm::vec3(0.0f, 0.0f, 1.0f);
        result.vertices.push_back(vertex);
    }

    // Poisson-like surface reconstruction using marching cubes-like approach
    // Create triangles by connecting points based on their normals and positions
    std::set<std::tuple<unsigned int, unsigned int, unsigned int>> triangleSet;
    
    // For each point, find neighbors and create triangles
    for (size_t i = 0; i < pointCloud.size(); i++) {
        // Skip points with very low weights if using weights
        if (useWeights && pointCloud[i].weight < 0.3f) continue;
        
        std::vector<std::pair<float, size_t>> neighbors;
        FindNeighbors(i, pointCloud, params.poissonRadius * 2.0f, neighbors);
        
        if (neighbors.size() < 2) continue;
        
        // If using weights, sort by combined weight and distance
        if (useWeights) {
            std::vector<std::pair<float, size_t>> weightedNeighbors;
            for (const auto& neighbor : neighbors) {
                float weight = pointCloud[neighbor.second].weight;
                // Combine distance and weight: prefer closer points with higher weights
                float score = neighbor.first / (weight + 0.001f);  // Lower score is better
                weightedNeighbors.push_back({score, neighbor.second});
            }
            std::sort(weightedNeighbors.begin(), weightedNeighbors.end());
            neighbors = weightedNeighbors;
        } else {
            std::sort(neighbors.begin(), neighbors.end());
        }
        
        const size_t maxNeighbors = std::min(neighbors.size(), size_t(params.maxNeighbors));
        
        // Create triangles with neighbors, prioritizing those with similar normals and higher weights
        for (size_t n = 0; n < maxNeighbors; n++) {
            size_t j = neighbors[n].second;
            
            // Skip neighbors with very low weights if using weights
            if (useWeights && pointCloud[j].weight < 0.3f) continue;
            
            glm::vec3 normal_i = estimatedNormals[i];
            glm::vec3 normal_j = estimatedNormals[j];
            float normalDot = glm::dot(normal_i, normal_j);
            
            // Prefer neighbors with similar normals (facing same direction)
            if (normalDot < 0.5f) continue;
            
            for (size_t k = n + 1; k < maxNeighbors; k++) {
                size_t k_idx = neighbors[k].second;
                
                // Skip neighbors with very low weights if using weights
                if (useWeights && pointCloud[k_idx].weight < 0.3f) continue;
                
                glm::vec3 normal_k = estimatedNormals[k_idx];
                
                float normalDot_jk = glm::dot(normal_j, normal_k);
                float normalDot_ik = glm::dot(normal_i, normal_k);
                
                // All three points should have similar normals
                if (normalDot_jk < 0.5f || normalDot_ik < 0.5f) continue;
                
                // Check triangle validity
                float dist_ij = glm::length(result.vertices[i].position - result.vertices[j].position);
                float dist_ik = glm::length(result.vertices[i].position - result.vertices[k_idx].position);
                float dist_jk = glm::length(result.vertices[j].position - result.vertices[k_idx].position);
                
                if (dist_ij <= params.poissonRadius * 2.0f &&
                    dist_ik <= params.poissonRadius * 2.0f &&
                    dist_jk <= params.poissonRadius * 2.0f) {
                    
                    // Check triangle orientation matches normals
                    glm::vec3 edge1 = result.vertices[j].position - result.vertices[i].position;
                    glm::vec3 edge2 = result.vertices[k_idx].position - result.vertices[i].position;
                    glm::vec3 triNormal = glm::normalize(glm::cross(edge1, edge2));
                    
                    // Triangle normal should align with point normals
                    if (glm::dot(triNormal, normal_i) > 0.0f) {
                        if (IsValidTriangle(
                            result.vertices[i].position,
                            result.vertices[j].position,
                            result.vertices[k_idx].position
                        )) {
                            AddTriangleIfValid(result.indices, triangleSet,
                                static_cast<unsigned int>(i),
                                static_cast<unsigned int>(j),
                                static_cast<unsigned int>(k_idx),
                                result.vertices);
                        }
                    }
                }
            }
        }
    }

    return result;
}

void Remeshing::CalculateNormals(
    std::vector<MeshVertex>& vertices,
    const std::vector<unsigned int>& indices
) {
    // Initialize normals to zero
    for (auto& vertex : vertices) {
        vertex.normal = glm::vec3(0.0f);
    }

    // Accumulate normals from all triangles
    for (size_t i = 0; i < indices.size(); i += 3) {
        if (i + 2 >= indices.size()) break;

        unsigned int i0 = indices[i];
        unsigned int i1 = indices[i + 1];
        unsigned int i2 = indices[i + 2];

        if (i0 >= vertices.size() || i1 >= vertices.size() || i2 >= vertices.size()) {
            continue;
        }

        glm::vec3 v0 = vertices[i0].position;
        glm::vec3 v1 = vertices[i1].position;
        glm::vec3 v2 = vertices[i2].position;

        glm::vec3 edge1 = v1 - v0;
        glm::vec3 edge2 = v2 - v0;
        glm::vec3 normal = glm::normalize(glm::cross(edge1, edge2));

        vertices[i0].normal += normal;
        vertices[i1].normal += normal;
        vertices[i2].normal += normal;
    }

    // Normalize all normals
    for (auto& vertex : vertices) {
        float length = glm::length(vertex.normal);
        if (length > 0.0001f) {
            vertex.normal = vertex.normal / length;
        } else {
            vertex.normal = glm::vec3(0.0f, 0.0f, 1.0f);
        }
    }
}

bool Remeshing::IsValidTriangle(
    const glm::vec3& v0,
    const glm::vec3& v1,
    const glm::vec3& v2,
    float minArea
) const {
    glm::vec3 edge1 = v1 - v0;
    glm::vec3 edge2 = v2 - v0;
    float area = glm::length(glm::cross(edge1, edge2)) * 0.5f;
    
    // Check for degenerate triangle
    if (area < minArea) {
        return false;
    }
    
    // Check for NaN or Inf
    if (std::isnan(area) || std::isinf(area)) {
        return false;
    }
    
    return true;
}

void Remeshing::AddTriangleIfValid(
    std::vector<unsigned int>& indices,
    std::set<std::tuple<unsigned int, unsigned int, unsigned int>>& triangleSet,
    unsigned int i0,
    unsigned int i1,
    unsigned int i2,
    const std::vector<MeshVertex>& vertices
) const {
    // Sort indices to create canonical representation
    unsigned int sorted[3] = {i0, i1, i2};
    if (sorted[0] > sorted[1]) std::swap(sorted[0], sorted[1]);
    if (sorted[1] > sorted[2]) std::swap(sorted[1], sorted[2]);
    if (sorted[0] > sorted[1]) std::swap(sorted[0], sorted[1]);

    auto key = std::make_tuple(sorted[0], sorted[1], sorted[2]);
    if (triangleSet.find(key) == triangleSet.end()) {
        triangleSet.insert(key);
        indices.push_back(i0);
        indices.push_back(i1);
        indices.push_back(i2);
    }
}

void Remeshing::FindNeighbors(
    size_t pointIndex,
    const std::vector<PointCloudPoint>& pointCloud,
    float maxDistance,
    std::vector<std::pair<float, size_t>>& neighbors
) const {
    if (pointIndex >= pointCloud.size()) return;

    const glm::vec3& pos = pointCloud[pointIndex].position;
    float maxDistSq = maxDistance * maxDistance;

    for (size_t j = 0; j < pointCloud.size(); j++) {
        if (j == pointIndex) continue;

        glm::vec3 diff = pos - pointCloud[j].position;
        float distSq = glm::dot(diff, diff);
        if (distSq <= maxDistSq) {
            float dist = std::sqrt(distSq);
            neighbors.push_back({dist, j});
        }
    }
}

std::vector<Point2D> Remeshing::ProjectToPlane(
    const std::vector<PointCloudPoint>& pointCloud,
    int planeAxis
) const {
    std::vector<Point2D> result;
    result.reserve(pointCloud.size());
    
    for (size_t i = 0; i < pointCloud.size(); i++) {
        Point2D p2d;
        p2d.originalIndex = i;
        p2d.originalPos = pointCloud[i].position;
        
        // Project to plane: 0=XY, 1=XZ, 2=YZ, 3=best fit (use XY for now)
        if (planeAxis == 0 || planeAxis == 3) {
            // XY plane (drop Z)
            p2d.x = pointCloud[i].position.x;
            p2d.y = pointCloud[i].position.y;
        } else if (planeAxis == 1) {
            // XZ plane (drop Y)
            p2d.x = pointCloud[i].position.x;
            p2d.y = pointCloud[i].position.z;
        } else if (planeAxis == 2) {
            // YZ plane (drop X)
            p2d.x = pointCloud[i].position.y;
            p2d.y = pointCloud[i].position.z;
        } else {
            // Default to XY
            p2d.x = pointCloud[i].position.x;
            p2d.y = pointCloud[i].position.y;
        }
        
        result.push_back(p2d);
    }
    
    return result;
}

bool Remeshing::IsPointInCircumcircle(
    const Point2D& a, const Point2D& b, const Point2D& c, const Point2D& p
) const {
    // Check if point p is inside the circumcircle of triangle abc
    // Using determinant method
    float ax = a.x - p.x, ay = a.y - p.y;
    float bx = b.x - p.x, by = b.y - p.y;
    float cx = c.x - p.x, cy = c.y - p.y;
    
    float det = (ax * ax + ay * ay) * (bx * cy - cx * by) -
                (bx * bx + by * by) * (ax * cy - cx * ay) +
                (cx * cx + cy * cy) * (ax * by - bx * ay);
    
    return det > 0.0f;
}

glm::vec3 Remeshing::EstimateNormal(
    size_t pointIndex,
    const std::vector<PointCloudPoint>& pointCloud,
    float radius
) const {
    if (pointIndex >= pointCloud.size()) {
        return glm::vec3(0.0f, 0.0f, 1.0f);
    }
    
    const glm::vec3& center = pointCloud[pointIndex].position;
    std::vector<std::pair<float, size_t>> neighbors;
    FindNeighbors(pointIndex, pointCloud, radius, neighbors);
    
    if (neighbors.size() < 3) {
        return glm::vec3(0.0f, 0.0f, 1.0f);
    }
    
    // Compute normal using PCA on neighbor points
    glm::vec3 centroid(0.0f);
    for (const auto& n : neighbors) {
        centroid += pointCloud[n.second].position;
    }
    centroid /= static_cast<float>(neighbors.size());
    
    // Compute covariance matrix (simplified - just use cross products)
    glm::vec3 normal(0.0f);
    for (size_t i = 0; i < neighbors.size(); i++) {
        size_t j = (i + 1) % neighbors.size();
        glm::vec3 v1 = pointCloud[neighbors[i].second].position - center;
        glm::vec3 v2 = pointCloud[neighbors[j].second].position - center;
        normal += glm::cross(v1, v2);
    }
    
    float length = glm::length(normal);
    if (length > 0.0001f) {
        normal = normal / length;
    } else {
        normal = glm::vec3(0.0f, 0.0f, 1.0f);
    }
    
    return normal;
}

void Remeshing::BuildOctree(
    const std::vector<PointCloudPoint>& pointCloud,
    float cellSize
) const {
    // Placeholder for future octree implementation
    // This would speed up neighbor searches
}

// Mean-shift clustering for 2D points (density-aware)
static void PerformMeanShift2D(const std::vector<glm::vec2>& points, float bandwidth, std::vector<glm::vec2>& clusterCenters) {
    if (points.empty() || bandwidth <= 0.0f) {
        return;
    }
    
    const float bandwidthSq = bandwidth * bandwidth;
    const int maxIterations = 100;
    const float convergenceThreshold = 0.01f;
    
    std::vector<glm::vec2> modes;
    std::vector<bool> processed(points.size(), false);
    
    // For each point, perform mean-shift to find mode
    for (size_t i = 0; i < points.size(); ++i) {
        if (processed[i]) continue;
        
        glm::vec2 mean = points[i];
        
        // Mean-shift iterations
        for (int iter = 0; iter < maxIterations; ++iter) {
            glm::vec2 newMean(0.0f);
            float weightSum = 0.0f;
            
            // Compute weighted mean of neighbors within bandwidth
            for (size_t j = 0; j < points.size(); ++j) {
                glm::vec2 diff = points[j] - mean;
                float distSq = glm::dot(diff, diff);
                
                if (distSq <= bandwidthSq) {
                    // Epanechnikov kernel weight
                    float weight = 1.0f - (distSq / bandwidthSq);
                    newMean += points[j] * weight;
                    weightSum += weight;
                }
            }
            
            if (weightSum > 0.0001f) {
                newMean /= weightSum;
            } else {
                break;
            }
            
            // Check convergence
            if (glm::length(newMean - mean) < convergenceThreshold) {
                mean = newMean;
                break;
            }
            
            mean = newMean;
        }
        
        // Check if this mode is close to an existing one (merge modes)
        bool merged = false;
        const float mergeThreshold = bandwidth * 0.5f;
        for (auto& mode : modes) {
            if (glm::length(mode - mean) < mergeThreshold) {
                // Merge: weighted average
                mode = (mode + mean) * 0.5f;
                merged = true;
                break;
            }
        }
        
        if (!merged) {
            modes.push_back(mean);
        }
        
        // Mark nearby points as processed
        for (size_t j = 0; j < points.size(); ++j) {
            if (glm::length(points[j] - mean) < bandwidth) {
                processed[j] = true;
            }
        }
    }
    
    clusterCenters = modes;
}

// K-means clustering for 3D points
static int PerformKMeans3D(const std::vector<glm::vec3>& points, int k, std::vector<glm::vec3>& centers) {
    if (points.empty() || k <= 0) return 0;
    if (static_cast<int>(points.size()) < k) k = static_cast<int>(points.size());
    
    centers.resize(k);
    std::vector<int> assignments(points.size(), -1);
    
    // Initialize centers with random points
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, static_cast<int>(points.size()) - 1);
    std::set<size_t> used;
    for (int i = 0; i < k && used.size() < points.size(); ++i) {
        size_t idx;
        do {
            idx = static_cast<size_t>(dis(gen));
        } while (used.count(idx) > 0 && used.size() < points.size());
        used.insert(idx);
        centers[i] = points[idx];
    }
    
    // K-means iterations
    const int maxIterations = 50;
    for (int iter = 0; iter < maxIterations; ++iter) {
        bool changed = false;
        
        // Assign points to nearest center
        for (size_t i = 0; i < points.size(); ++i) {
            float minDist = std::numeric_limits<float>::max();
            int bestCluster = 0;
            for (int j = 0; j < k; ++j) {
                float dist = glm::length(points[i] - centers[j]);
                if (dist < minDist) {
                    minDist = dist;
                    bestCluster = j;
                }
            }
            if (assignments[i] != bestCluster) {
                assignments[i] = bestCluster;
                changed = true;
            }
        }
        
        if (!changed) break;
        
        // Update centers - compute mean, then snap to nearest actual point
        std::vector<glm::vec3> newCenters(k, glm::vec3(0.0f));
        std::vector<int> counts(k, 0);
        std::vector<std::vector<size_t>> clusterPoints(k);  // Store indices of points in each cluster
        
        for (size_t i = 0; i < points.size(); ++i) {
            int cluster = assignments[i];
            newCenters[cluster] += points[i];
            counts[cluster]++;
            clusterPoints[cluster].push_back(i);
        }
        
        for (int j = 0; j < k; ++j) {
            if (counts[j] > 0) {
                // Compute mean center
                glm::vec3 meanCenter = newCenters[j] / static_cast<float>(counts[j]);
                
                // Snap to nearest actual point from this cluster to guarantee center is within point cloud
                float minDist = std::numeric_limits<float>::max();
                size_t closestPointIdx = clusterPoints[j][0];
                for (size_t pointIdx : clusterPoints[j]) {
                    float dist = glm::length(points[pointIdx] - meanCenter);
                    if (dist < minDist) {
                        minDist = dist;
                        closestPointIdx = pointIdx;
                    }
                }
                centers[j] = points[closestPointIdx];
            }
        }
    }
    
    return k;
}

// K-means clustering for 3D points with assignments (returns assignments and centers)
static int PerformKMeans3DWithAssignments(
    const std::vector<glm::vec3>& points, 
    int k, 
    std::vector<glm::vec3>& centers,
    std::vector<int>& assignments
) {
    if (points.empty() || k <= 0) return 0;
    if (static_cast<int>(points.size()) < k) k = static_cast<int>(points.size());
    
    assignments.resize(points.size(), -1);
    centers.resize(k);
    
    // Initialize centers with random points
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, static_cast<int>(points.size()) - 1);
    std::set<size_t> used;
    for (int i = 0; i < k && used.size() < points.size(); ++i) {
        size_t idx;
        do {
            idx = static_cast<size_t>(dis(gen));
        } while (used.count(idx) > 0 && used.size() < points.size());
        used.insert(idx);
        centers[i] = points[idx];
    }
    
    // K-means iterations
    const int maxIterations = 50;
    for (int iter = 0; iter < maxIterations; ++iter) {
        bool changed = false;
        
        // Assign points to nearest center
        for (size_t i = 0; i < points.size(); ++i) {
            float minDist = std::numeric_limits<float>::max();
            int bestCluster = 0;
            for (int j = 0; j < k; ++j) {
                float dist = glm::length(points[i] - centers[j]);
                if (dist < minDist) {
                    minDist = dist;
                    bestCluster = j;
                }
            }
            if (assignments[i] != bestCluster) {
                assignments[i] = bestCluster;
                changed = true;
            }
        }
        
        if (!changed) break;
        
        // Update centers - compute mean, then snap to nearest actual point
        std::vector<glm::vec3> newCenters(k, glm::vec3(0.0f));
        std::vector<int> counts(k, 0);
        std::vector<std::vector<size_t>> clusterPoints(k);  // Store indices of points in each cluster
        
        for (size_t i = 0; i < points.size(); ++i) {
            int cluster = assignments[i];
            newCenters[cluster] += points[i];
            counts[cluster]++;
            clusterPoints[cluster].push_back(i);
        }
        
        for (int j = 0; j < k; ++j) {
            if (counts[j] > 0) {
                // Compute mean center
                glm::vec3 meanCenter = newCenters[j] / static_cast<float>(counts[j]);
                
                // Snap to nearest actual point from this cluster to guarantee center is within point cloud
                float minDist = std::numeric_limits<float>::max();
                size_t closestPointIdx = clusterPoints[j][0];
                for (size_t pointIdx : clusterPoints[j]) {
                    float dist = glm::length(points[pointIdx] - meanCenter);
                    if (dist < minDist) {
                        minDist = dist;
                        closestPointIdx = pointIdx;
                    }
                }
                centers[j] = points[closestPointIdx];
            }
        }
    }
    
    return k;
}


static std::vector<std::vector<int>> DBSCAN2D(
    const std::vector<glm::vec2>& points,
    float eps,
    int minPts
) {
    const int UNVISITED = -2;
    const int NOISE     = -1;

    const int n = static_cast<int>(points.size());
    std::vector<int> labels(n, UNVISITED);
    std::vector<std::vector<int>> clusters;

    // Find neighbors of point i
    auto regionQuery = [&](int i) {
        std::vector<int> neighbors;
        for (int j = 0; j < n; ++j) {
            if (glm::length(points[i] - points[j]) <= eps)
                neighbors.push_back(j);
        }
        return neighbors;
    };

    for (int i = 0; i < n; ++i) {
        if (labels[i] != UNVISITED)
            continue;

        // Find neighbors
        auto neighbors = regionQuery(i);

        // Not enough density → noise (for now)
        if ((int)neighbors.size() < minPts) {
            labels[i] = NOISE;
            continue;
        }

        // Start new cluster
        int clusterId = static_cast<int>(clusters.size());
        clusters.emplace_back();

        labels[i] = clusterId;
        clusters.back().push_back(i);

        // Expand cluster
        for (size_t k = 0; k < neighbors.size(); ++k) {
            int j = neighbors[k];

            // Noise becomes border point
            if (labels[j] == NOISE)
                labels[j] = clusterId;

            if (labels[j] != UNVISITED)
                continue;

            labels[j] = clusterId;
            clusters.back().push_back(j);

            // Check if j is a core point
            auto jNeighbors = regionQuery(j);
            if ((int)jNeighbors.size() >= minPts) {
                neighbors.insert(neighbors.end(),
                                 jNeighbors.begin(),
                                 jNeighbors.end());
            }
        }
    }

    return clusters;
}


// Compute PCA axes for point cloud
void Remeshing::ComputePCAAxes(const std::vector<PointCloudPoint>& pointCloud, glm::vec3& axis1, glm::vec3& axis2, glm::vec3& axis3) const {
    if (pointCloud.empty()) {
        axis1 = glm::vec3(1.0f, 0.0f, 0.0f);
        axis2 = glm::vec3(0.0f, 1.0f, 0.0f);
        axis3 = glm::vec3(0.0f, 0.0f, 1.0f);
        return;
    }
    
    // Compute centroid
    glm::vec3 centroid(0.0f);
    for (const auto& point : pointCloud) {
        centroid += point.position;
    }
    centroid /= static_cast<float>(pointCloud.size());
    
    // Compute covariance matrix (3x3)
    float cov[9] = {0.0f};  // Row-major: [0,1,2,3,4,5,6,7,8] = [xx,xy,xz,yx,yy,yz,zx,zy,zz]
    for (const auto& point : pointCloud) {
        glm::vec3 diff = point.position - centroid;
        cov[0] += diff.x * diff.x;  // xx
        cov[1] += diff.x * diff.y;  // xy
        cov[2] += diff.x * diff.z;  // xz
        cov[3] += diff.y * diff.x;  // yx (same as xy)
        cov[4] += diff.y * diff.y;  // yy
        cov[5] += diff.y * diff.z;  // yz
        cov[6] += diff.z * diff.x;  // zx (same as xz)
        cov[7] += diff.z * diff.y;  // zy (same as yz)
        cov[8] += diff.z * diff.z;  // zz
    }
    
    float invCount = 1.0f / static_cast<float>(pointCloud.size());
    for (int i = 0; i < 9; ++i) {
        cov[i] *= invCount;
    }
    
    // Simplified PCA: find principal axis by computing eigenvalues/vectors
    // For simplicity, use power iteration to find largest eigenvector
    glm::vec3 v = glm::vec3(1.0f, 0.0f, 0.0f);
    for (int iter = 0; iter < 20; ++iter) {
        glm::vec3 v_new(
            cov[0] * v.x + cov[1] * v.y + cov[2] * v.z,
            cov[3] * v.x + cov[4] * v.y + cov[5] * v.z,
            cov[6] * v.x + cov[7] * v.y + cov[8] * v.z
        );
        float len = glm::length(v_new);
        if (len > 0.0001f) {
            v = v_new / len;
        } else {
            break;
        }
    }
    axis1 = v;
    
    // Second axis: orthogonal to first, in direction of second largest variance
    // Project out first axis and find next principal direction
    glm::vec3 v2 = glm::vec3(0.0f, 1.0f, 0.0f);
    v2 = v2 - glm::dot(v2, axis1) * axis1;  // Make orthogonal
    if (glm::length(v2) > 0.0001f) {
        v2 = glm::normalize(v2);
    } else {
        v2 = glm::vec3(0.0f, 0.0f, 1.0f);
        v2 = v2 - glm::dot(v2, axis1) * axis1;
        if (glm::length(v2) > 0.0001f) {
            v2 = glm::normalize(v2);
        }
    }
    
    for (int iter = 0; iter < 20; ++iter) {
        glm::vec3 v_new(
            cov[0] * v2.x + cov[1] * v2.y + cov[2] * v2.z,
            cov[3] * v2.x + cov[4] * v2.y + cov[5] * v2.z,
            cov[6] * v2.x + cov[7] * v2.y + cov[8] * v2.z
        );
        v_new = v_new - glm::dot(v_new, axis1) * axis1;  // Keep orthogonal
        float len = glm::length(v_new);
        if (len > 0.0001f) {
            v2 = v_new / len;
        } else {
            break;
        }
    }
    axis2 = v2;
    
    // Third axis: cross product of first two
    axis3 = glm::cross(axis1, axis2);
    if (glm::length(axis3) > 0.0001f) {
        axis3 = glm::normalize(axis3);
    } else {
        axis3 = glm::vec3(0.0f, 0.0f, 1.0f);
    }
}

// Estimate normal for a point (wrapper that uses existing EstimateNormal with reasonable radius)
glm::vec3 Remeshing::EstimateNormalForPoint(size_t pointIndex, const std::vector<PointCloudPoint>& pointCloud, float radius) const {
    return EstimateNormal(pointIndex, pointCloud, radius);
}
RemeshResult Remeshing::GenerateFCTM(
    const std::vector<PointCloudPoint>& pointCloud,
    const AlgorithmParameters& params
) {
    RemeshResult result;
    if (pointCloud.empty() || params.fctmSliceThickness <= 0.0f) {
        return result;
    }
    
    // Collect points
    std::vector<glm::vec3> allPoints;
    allPoints.reserve(pointCloud.size());
    for (const auto& p : pointCloud) {
        allPoints.push_back(p.position);
    }
    
    // Auto-determine initial number of clusters based on point count
    int initialK = static_cast<int>(std::sqrt(static_cast<float>(allPoints.size())) / 2.0f);
    initialK = std::max(1, std::min(initialK, static_cast<int>(allPoints.size())));
    
    std::vector<glm::vec3> initialClusterCenters;
    std::vector<int> pointAssignments(allPoints.size(), -1);
    
    // Perform initial k-means to identify major clusters (with assignments)
    PerformKMeans3DWithAssignments(allPoints, initialK, initialClusterCenters, pointAssignments);
    
    const size_t numInitialClusters = initialClusterCenters.size();
    if (numInitialClusters == 0) {
        return result;
    }

    // Group points by their assigned initial cluster (be defensive: assignment may be -1/out of range)
    std::vector<std::vector<glm::vec3>> clusterPoints(numInitialClusters);
    for (size_t i = 0; i < allPoints.size(); ++i) {
        int a = pointAssignments[i];
        if (a < 0 || static_cast<size_t>(a) >= numInitialClusters) {
            // Fallback: assign to nearest center (rare, but prevents crash)
            float best = std::numeric_limits<float>::max();
            int bestIdx = 0;
            for (size_t c = 0; c < numInitialClusters; ++c) {
                float d = glm::length(allPoints[i] - initialClusterCenters[c]);
                if (d < best) { best = d; bestIdx = static_cast<int>(c); }
            }
            a = bestIdx;
        }
        clusterPoints[static_cast<size_t>(a)].push_back(allPoints[i]);
    }

    const float sliceThickness = params.fctmSliceThickness;
    std::vector<glm::vec3> allClusterCenters;
    allClusterCenters.reserve(allPoints.size() / 10); // heuristic
    
    // Process each initial cluster separately
    for (size_t clusterIdx = 0; clusterIdx < clusterPoints.size(); ++clusterIdx) {
        const auto& clusterPointSet = clusterPoints[clusterIdx];
        if (clusterPointSet.empty()) continue;
        
        // Compute bounding sphere radius for this cluster (centered at cluster center)
        glm::vec3 clusterCenter = initialClusterCenters[clusterIdx];
        float maxRadius = 0.0f;
        for (const auto& pt : clusterPointSet) {
            maxRadius = std::max(maxRadius, glm::length(pt - clusterCenter));
        }
        if (maxRadius <= 1e-6f) continue;
        
        // Number of radial shells
        int N = static_cast<int>(std::ceil(maxRadius / sliceThickness));
        N = std::max(1, std::min(N, 100));
        
        for (int step = 0; step < N; ++step) {
            const float shrinkFactor = 1.0f - (static_cast<float>(step) / static_cast<float>(N));
            const float outerR = maxRadius * shrinkFactor;
            const float innerR = std::max(0.0f, outerR - sliceThickness);
            
            // Collect points in this shell
            std::vector<glm::vec3> pointsInSlice;
            pointsInSlice.reserve(clusterPointSet.size() / N + 8);
            for (const auto& pt : clusterPointSet) {
                float d = glm::length(pt - clusterCenter);
                if (d > innerR && d <= outerR) {
                    pointsInSlice.push_back(pt);
                }
            }
            if (pointsInSlice.size() < 3) continue;

            // Choose k for the shell (keep it small + stable)
            int k = static_cast<int>(std::sqrt(static_cast<float>(pointsInSlice.size())));
            float thicknessRatio = sliceThickness / maxRadius;
            k = static_cast<int>(k * thicknessRatio * 5.0f);
            k = std::max(1, std::min(k, static_cast<int>(pointsInSlice.size())));
            
            std::vector<glm::vec3> sliceCenters;
            PerformKMeans3D(pointsInSlice, k, sliceCenters);

            allClusterCenters.insert(allClusterCenters.end(), sliceCenters.begin(), sliceCenters.end());
        }
    }

    // Emit vertices (red)
    result.vertices.reserve(allClusterCenters.size());
    for (const auto& pos : allClusterCenters) {
        MeshVertex vtx;
        vtx.position = pos;
        vtx.color    = glm::vec3(1.0f, 0.0f, 0.0f);
        vtx.normal   = glm::vec3(0.0f, 0.0f, 1.0f);
        result.vertices.push_back(vtx);
    }

    result.vertexCount = result.vertices.size();
    result.triangleCount = 0;
    result.initialClusterCount = numInitialClusters;
    return result;
}

// ============================================================================
// GenerateGeodesicTree: Robust centerline via Slice → Centroid → Smooth → Connect
// ============================================================================

static glm::vec3 PowerIterLargestEigenvec3x3(const float cov[3][3]) {
    glm::vec3 v(1.0f, 0.0f, 0.0f);
    for (int iter = 0; iter < 20; ++iter) {
        glm::vec3 nv(
            cov[0][0] * v.x + cov[0][1] * v.y + cov[0][2] * v.z,
            cov[1][0] * v.x + cov[1][1] * v.y + cov[1][2] * v.z,
            cov[2][0] * v.x + cov[2][1] * v.y + cov[2][2] * v.z
        );
        float len = glm::length(nv);
        if (len < 1e-6f) break;
        v = nv / len;
    }
    return v;
}

RemeshResult Remeshing::GenerateGeodesicTree(
    const std::vector<PointCloudPoint>& pointCloud,
    const AlgorithmParameters& params
) {
    RemeshResult result;
    if (pointCloud.size() < 3) return result;

    // Collect points
    std::vector<glm::vec3> pts;
    pts.reserve(pointCloud.size());
    for (const auto& p : pointCloud) pts.push_back(p.position);

    // (Optional but strongly recommended) Split into connected-ish components via coarse voxel hashing.
    // This prevents "two objects" from ruining PCA. If you don't want it, set maxComponents=1.
    const int maxComponents = 8;

    // --- Build coarse voxel grid for cheap adjacency ---
    glm::vec3 minPt = pts[0], maxPt = pts[0];
    for (auto& p : pts) { minPt = glm::min(minPt, p); maxPt = glm::max(maxPt, p); }
    float diag = glm::length(maxPt - minPt);
    if (diag < 1e-6f) return result;

    // coarse cell size ~ 1% diag (tune). Larger = fewer components.
    float cell = std::max(diag * 0.01f, 1e-4f);

    struct Key { int x,y,z; bool operator==(const Key& o) const { return x==o.x && y==o.y && z==o.z; } };
    struct KeyHash { size_t operator()(const Key& k) const {
        return (size_t(k.x)*73856093u) ^ (size_t(k.y)*19349663u) ^ (size_t(k.z)*83492791u);
    } };

    std::unordered_map<Key, std::vector<size_t>, KeyHash> grid;
    grid.reserve(pts.size());
    std::vector<Key> keys(pts.size());

    for (size_t i = 0; i < pts.size(); ++i) {
        Key k;
        k.x = (int)std::floor((pts[i].x - minPt.x) / cell);
        k.y = (int)std::floor((pts[i].y - minPt.y) / cell);
        k.z = (int)std::floor((pts[i].z - minPt.z) / cell);
        keys[i] = k;
        grid[k].push_back(i);
    }

    // BFS components over voxels (27-neighborhood)
    std::vector<int> compId(pts.size(), -1);
    int compCount = 0;
    std::vector<std::vector<size_t>> comps;

    comps.reserve(maxComponents);

    for (size_t seed = 0; seed < pts.size() && compCount < maxComponents; ++seed) {
        if (compId[seed] != -1) continue;

        std::vector<size_t> comp;
        std::queue<size_t> q;
        q.push(seed);
        compId[seed] = compCount;

        while (!q.empty()) {
            size_t i = q.front(); q.pop();
            comp.push_back(i);

            const Key ck = keys[i];
            for (int dx=-1; dx<=1; ++dx) for (int dy=-1; dy<=1; ++dy) for (int dz=-1; dz<=1; ++dz) {
                Key nk{ ck.x+dx, ck.y+dy, ck.z+dz };
                auto it = grid.find(nk);
                if (it == grid.end()) continue;
                for (size_t j : it->second) {
                    if (compId[j] != -1) continue;
                    compId[j] = compCount;
                    q.push(j);
                }
            }
        }

        // ignore tiny noise blobs
        if (comp.size() >= 30) {
            comps.push_back(std::move(comp));
            compCount++;
        }
    }

    if (comps.empty()) {
        // fallback: treat all as one component
        comps.push_back({});
        comps.back().reserve(pts.size());
        for (size_t i = 0; i < pts.size(); ++i) comps.back().push_back(i);
    }

    // Helper: estimate 1-NN median with subsampling (O(S*N) worst-case, but small S)
    auto estimateMedianNN = [&](const std::vector<size_t>& indices) -> float {
        const size_t n = indices.size();
        if (n < 2) return diag / 50.0f;

        const size_t S = std::min<size_t>(80, n);
        std::vector<float> mins;
        mins.reserve(S);

        for (size_t si = 0; si < S; ++si) {
            size_t i = indices[si * n / S];
            float best = std::numeric_limits<float>::max();
            const glm::vec3 pi = pts[i];
            for (size_t sj = 0; sj < n; ++sj) {
                size_t j = indices[sj];
                if (i == j) continue;
                float d = glm::length(pi - pts[j]);
                if (d < best) best = d;
            }
            if (std::isfinite(best)) mins.push_back(best);
        }

        if (mins.empty()) return diag / 50.0f;
        std::sort(mins.begin(), mins.end());
        return mins[mins.size() / 2];
    };

    // Build result as polyline(s): we’ll just append all component centerlines (disconnected)
    for (const auto& comp : comps) {
        if (comp.size() < 3) continue;

        // centroid
        glm::vec3 c(0.0f);
        for (size_t idx : comp) c += pts[idx];
        c /= (float)comp.size();

        // covariance
        float cov[3][3] = {{0}};
        for (size_t idx : comp) {
            glm::vec3 v = pts[idx] - c;
            cov[0][0] += v.x*v.x; cov[0][1] += v.x*v.y; cov[0][2] += v.x*v.z;
            cov[1][1] += v.y*v.y; cov[1][2] += v.y*v.z;
            cov[2][2] += v.z*v.z;
        }
        cov[1][0]=cov[0][1]; cov[2][0]=cov[0][2]; cov[2][1]=cov[1][2];

        float tr = cov[0][0] + cov[1][1] + cov[2][2];
        if (tr > 1e-6f) {
            for (int r=0;r<3;++r) for (int k=0;k<3;++k) cov[r][k] /= tr;
        }

        glm::vec3 axis = PowerIterLargestEigenvec3x3(cov);
        if (glm::length(axis) < 1e-6f) continue;

        // project to t
        std::vector<float> t(comp.size());
        float tMin = std::numeric_limits<float>::max();
        float tMax = std::numeric_limits<float>::lowest();
        for (size_t ii = 0; ii < comp.size(); ++ii) {
            float tt = glm::dot(pts[comp[ii]] - c, axis);
            t[ii] = tt;
            tMin = std::min(tMin, tt);
            tMax = std::max(tMax, tt);
        }

        float nnMed = estimateMedianNN(comp);
        float dt = std::max(nnMed * 2.0f, (tMax - tMin) / 80.0f);
        if (dt < 1e-6f) continue;

        int binsN = (int)((tMax - tMin) / dt) + 1;
        binsN = std::max(1, std::min(binsN, 800));

        // Store points per bin (needed for Step 4 clustering)
        std::vector<std::vector<glm::vec3>> binPoints(binsN);
        std::vector<int> cnt(binsN, 0);

        for (size_t ii = 0; ii < comp.size(); ++ii) {
            int b = (int)((t[ii] - tMin) / dt);
            b = std::max(0, std::min(b, binsN - 1));
            binPoints[b].push_back(pts[comp[ii]]);
            cnt[b] += 1;
        }

        // Step 4: Project slice points to 2D plane and cluster
        // Compute u and v vectors for 2D plane (orthogonal to axis)
        glm::vec3 u, v;
        if (std::abs(axis.x) < 0.9f) {
            u = glm::normalize(glm::cross(axis, glm::vec3(1.0f, 0.0f, 0.0f)));
        } else {
            u = glm::normalize(glm::cross(axis, glm::vec3(0.0f, 1.0f, 0.0f)));
        }
        v = glm::normalize(glm::cross(axis, u));
        
        // DBSCAN helper function for 2D points
        auto DBSCAN2D = [](const std::vector<glm::vec2>& points, float eps, int minPts) -> std::vector<std::vector<int>> {
            const size_t n = points.size();
            std::vector<int> labels(n, -1); // -1 = noise, 0+ = cluster ID
            int clusterId = 0;
            std::vector<std::vector<int>> clusters;
            
            for (size_t i = 0; i < n; ++i) {
                if (labels[i] != -1) continue; // Already processed
                
                // Find neighbors
                std::vector<int> neighbors;
                for (size_t j = 0; j < n; ++j) {
                    float dist = glm::length(points[i] - points[j]);
                    if (dist <= eps) {
                        neighbors.push_back(static_cast<int>(j));
                    }
                }
                
                if (neighbors.size() < static_cast<size_t>(minPts)) {
                    labels[i] = -1; // Noise
                    continue;
                }
                
                // Start new cluster
                labels[i] = clusterId;
                clusters.push_back({static_cast<int>(i)});
                
                // Expand cluster
                size_t j = 0;
                while (j < neighbors.size()) {
                    int q = neighbors[j];
                    
                    if (labels[q] == -1) {
                        labels[q] = clusterId;
                        clusters[clusterId].push_back(q);
                        
                        // Find neighbors of q
                        std::vector<int> qNeighbors;
                        for (size_t k = 0; k < n; ++k) {
                            float dist = glm::length(points[q] - points[k]);
                            if (dist <= eps) {
                                qNeighbors.push_back(static_cast<int>(k));
                            }
                        }
                        if (qNeighbors.size() >= static_cast<size_t>(minPts)) {
                            for (int nIdx : qNeighbors) {
                                if (std::find(neighbors.begin(), neighbors.end(), nIdx) == neighbors.end()) {
                                    neighbors.push_back(nIdx);
                                }
                            }
                        }
                    }
                    j++;
                }
                
                clusterId++;
            }
            
            return clusters;
        };
        
        // Process each bin: project to 2D and cluster
        std::vector<glm::vec3> cl;
        cl.reserve(binsN);
        
        for (int b = 0; b < binsN; ++b) {
            if (cnt[b] < 8) continue; // ignore sparse bins
            
            const auto& slicePoints = binPoints[b];
            if (slicePoints.empty()) continue;
            
            // Compute slice center
            glm::vec3 sliceCenter(0.0f);
            for (const auto& p : slicePoints) {
                sliceCenter += p;
            }
            sliceCenter /= static_cast<float>(slicePoints.size());
            
            // Project slice points to 2D plane
            std::vector<glm::vec2> projected;
            std::vector<glm::vec3> originals;
            projected.reserve(slicePoints.size());
            originals.reserve(slicePoints.size());
            
            for (const auto& p : slicePoints) {
                glm::vec3 d = p - sliceCenter;
                glm::vec3 inPlane = d - axis * glm::dot(d, axis);
                projected.push_back(glm::vec2(glm::dot(inPlane, u), glm::dot(inPlane, v)));
                originals.push_back(p);
            }
            
            // Cluster in 2D using DBSCAN
            float eps = nnMed * 0.5f; // DBSCAN epsilon based on median NN distance
            int minPts = std::max(3, std::min(8, static_cast<int>(slicePoints.size()) / 10)); // Adaptive minPts
            
            std::vector<std::vector<int>> clusters = DBSCAN2D(projected, eps, minPts);
            
            // Create nodes from clusters
            for (const auto& cluster : clusters) {
                if (cluster.empty()) continue;
                glm::vec3 c(0.0f);
                for (int idx : cluster) {
                    c += originals[idx];
                }
                c /= static_cast<float>(cluster.size());
                cl.push_back(c);
            }
        }
        
        if (cl.size() < 2) continue;

        // Smooth (moving average, 2 passes)
        for (int pass = 0; pass < 2; ++pass) {
            std::vector<glm::vec3> tmp = cl;
            for (size_t i = 0; i < cl.size(); ++i) {
                glm::vec3 sum(0.0f);
                int w = 0;
                int i0 = (int)std::max<int>(0, (int)i - 1);
                int i1 = (int)std::min<int>((int)cl.size() - 1, (int)i + 1);
                for (int j = i0; j <= i1; ++j) { sum += tmp[j]; w++; }
                cl[i] = sum / (float)w;
            }
        }

        // Emit vertices + edges
        const unsigned int base = (unsigned int)result.vertices.size();
        for (auto& p : cl) {
            MeshVertex vtx;
            vtx.position = p;
            vtx.color    = glm::vec3(1.0f, 0.0f, 0.0f);
            vtx.normal   = glm::vec3(0.0f, 0.0f, 1.0f);
            result.vertices.push_back(vtx);
        }
        for (unsigned int i = 0; i + 1 < (unsigned int)cl.size(); ++i) {
            result.indices.push_back(base + i);
            result.indices.push_back(base + i + 1);
        }
    }

    result.vertexCount = result.vertices.size();
    result.triangleCount = 0;
    result.initialClusterCount = 0;
        return result;
    }
    
// ============================================================================
// GenerateGTree: Tube-following algorithm for branching structures
// ============================================================================

RemeshResult Remeshing::GenerateGTree(
    const std::vector<PointCloudPoint>& pointCloud,
    const AlgorithmParameters& params
) {
    RemeshResult result;
    if (pointCloud.size() < 3) return result;
    
    // Extract points
    std::vector<glm::vec3> allPoints;
    allPoints.reserve(pointCloud.size());
    for (const auto& p : pointCloud) {
        allPoints.push_back(p.position);
    }
    
    const size_t numPoints = allPoints.size();
    
    // Step 1: Use k-means to find clusters (objects in scene)
    // Auto-determine number of clusters based on point count (expecting ~4 objects)
    int numClusters = 4; // Default to 4 objects
    if (numPoints < 100) {
        numClusters = 1;
    } else if (numPoints < 500) {
        numClusters = 2;
    } else if (numPoints < 2000) {
        numClusters = 3;
    } else {
        numClusters = std::min(8, static_cast<int>(std::sqrt(static_cast<float>(numPoints)) / 10.0f));
    }
    
    std::vector<glm::vec3> clusterCenters;
    std::vector<int> pointAssignments(numPoints, -1);
    
    // Perform k-means clustering
    PerformKMeans3DWithAssignments(allPoints, numClusters, clusterCenters, pointAssignments);
    
    // Group points by their assigned cluster
    std::vector<std::vector<size_t>> clusters(numClusters);
    for (size_t i = 0; i < numPoints; ++i) {
        int clusterId = pointAssignments[i];
        if (clusterId >= 0 && clusterId < numClusters) {
            clusters[clusterId].push_back(i);
        }
    }
    
    // Filter out empty or tiny clusters
    std::vector<std::vector<size_t>> validClusters;
    for (const auto& cluster : clusters) {
        if (cluster.size() >= 20) { // Minimum cluster size
            validClusters.push_back(cluster);
        }
    }
    clusters = validClusters;
    
    if (clusters.empty()) {
        // Fallback: treat all points as one cluster
        clusters.push_back({});
        for (size_t i = 0; i < numPoints; ++i) {
            clusters[0].push_back(i);
        }
    }
    
    // Estimate median nearest neighbor distance for step size
    float medianNN = 0.0f;
    std::vector<float> nnDists;
    const size_t sampleSize = std::min<size_t>(100, numPoints);
    for (size_t i = 0; i < sampleSize; ++i) {
        size_t idx = i * numPoints / sampleSize;
        float minDist = std::numeric_limits<float>::max();
        for (size_t j = 0; j < numPoints; ++j) {
            if (idx == j) continue;
            float dist = glm::length(allPoints[idx] - allPoints[j]);
            if (dist < minDist) minDist = dist;
        }
        if (std::isfinite(minDist)) nnDists.push_back(minDist);
    }
    if (!nnDists.empty()) {
        std::sort(nnDists.begin(), nnDists.end());
        medianNN = nnDists[nnDists.size() / 2];
    } else {
        // Fallback: estimate from bounding box
        glm::vec3 minPt = allPoints[0], maxPt = allPoints[0];
        for (const auto& p : allPoints) {
            minPt = glm::min(minPt, p);
            maxPt = glm::max(maxPt, p);
        }
        medianNN = glm::length(maxPt - minPt) / std::pow(static_cast<float>(numPoints), 1.0f/3.0f);
    }
    
    // Step 2: For each cluster, use plane-based path following
    const float STEP_SIZE = medianNN * 1.5f; // Distance to move along path
    const int MIN_POINTS_IN_SPHERE = 15; // Minimum points needed to compute a plane
    
    for (const auto& cluster : clusters) {
        if (cluster.size() < 20) continue; // Skip tiny clusters
        
        // Get cluster points
        std::vector<glm::vec3> clusterPoints;
        clusterPoints.reserve(cluster.size());
        for (size_t idx : cluster) {
            clusterPoints.push_back(allPoints[idx]);
        }
        
        // Compute cluster origin (centroid)
        glm::vec3 origin(0.0f);
        for (const auto& p : clusterPoints) {
            origin += p;
        }
        origin /= static_cast<float>(clusterPoints.size());
        
        // Use new plane-based path following algorithm
        std::vector<glm::vec3> treeNodes = FollowPlanePath(origin, clusterPoints, MIN_POINTS_IN_SPHERE, STEP_SIZE);
        
        // Add tree nodes to result and build edges
        const unsigned int baseIdx = static_cast<unsigned int>(result.vertices.size());
        std::map<size_t, unsigned int> nodeToVertexIndex; // Map from treeNodes index to result vertex index
        
        for (size_t i = 0; i < treeNodes.size(); ++i) {
            MeshVertex vtx;
            vtx.position = treeNodes[i];
            vtx.color = glm::vec3(0.0f, 1.0f, 0.0f); // Green for GTree
            vtx.normal = glm::vec3(0.0f, 0.0f, 1.0f);
            nodeToVertexIndex[i] = static_cast<unsigned int>(result.vertices.size());
            result.vertices.push_back(vtx);
        }
        
        // Connect nodes: for each node after origin, connect to nearest previous node
        // This creates a tree structure that follows the path
        for (size_t i = 1; i < treeNodes.size(); ++i) {
            // Find nearest previous node to connect to
            float minDist = std::numeric_limits<float>::max();
            size_t bestParent = 0;
            
            for (size_t j = 0; j < i; ++j) {
                float dist = glm::length(treeNodes[i] - treeNodes[j]);
                if (dist < minDist && dist < STEP_SIZE * 2.5f) {
                    minDist = dist;
                    bestParent = j;
                }
            }
            
            // Connect to best parent
            result.indices.push_back(nodeToVertexIndex[bestParent]);
            result.indices.push_back(nodeToVertexIndex[i]);
        }
    }
    
    result.vertexCount = result.vertices.size();
    result.triangleCount = 0;
    result.initialClusterCount = static_cast<int>(clusters.size());
    return result;
}

// ============================================================================
// FollowPlanePath: Plane-based path following algorithm
// ============================================================================

std::vector<glm::vec3> Remeshing::FollowPlanePath(
    const glm::vec3& startPoint,
    const std::vector<glm::vec3>& clusterPoints,
    int minPointsInSphere,
    float stepSize
) {
    std::vector<glm::vec3> pathNodes;
    pathNodes.push_back(startPoint);
    
    // Helper: Expand sphere from a point until it captures N points
    auto expandSphere = [&](const glm::vec3& center, float& radius) -> std::vector<glm::vec3> {
        std::vector<glm::vec3> pointsInSphere;
        radius = stepSize * 0.5f; // Start with small radius
        
        const float maxRadius = stepSize * 10.0f; // Maximum expansion
        const float radiusStep = stepSize * 0.2f;
        
        while (pointsInSphere.size() < static_cast<size_t>(minPointsInSphere) && radius < maxRadius) {
            pointsInSphere.clear();
            for (const auto& p : clusterPoints) {
                float dist = glm::length(p - center);
                if (dist <= radius) {
                    pointsInSphere.push_back(p);
                }
            }
            if (pointsInSphere.size() < static_cast<size_t>(minPointsInSphere)) {
                radius += radiusStep;
            }
        }
        
        return pointsInSphere;
    };
    
    // Helper: Compute plane from points using PCA
    auto computePlane = [](const std::vector<glm::vec3>& points, const glm::vec3& center) -> std::pair<glm::vec3, bool> {
        if (points.size() < 3) {
            return {glm::vec3(0.0f), false};
        }
        
        // Compute covariance matrix
        float cov[3][3] = {{0.0f}};
        for (const auto& p : points) {
            glm::vec3 d = p - center;
            cov[0][0] += d.x * d.x;
            cov[0][1] += d.x * d.y;
            cov[0][2] += d.x * d.z;
            cov[1][1] += d.y * d.y;
            cov[1][2] += d.y * d.z;
            cov[2][2] += d.z * d.z;
        }
        
        float n = static_cast<float>(points.size());
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; ++j) {
                cov[i][j] /= n;
            }
        }
        cov[1][0] = cov[0][1];
        cov[2][0] = cov[0][2];
        cov[2][1] = cov[1][2];
        
        // Compute smallest eigenvector (normal to plane) using power iteration
        glm::vec3 v(1.0f, 0.0f, 0.0f);
        for (int iter = 0; iter < 20; ++iter) {
            glm::vec3 nv(
                cov[0][0] * v.x + cov[0][1] * v.y + cov[0][2] * v.z,
                cov[1][0] * v.x + cov[1][1] * v.y + cov[1][2] * v.z,
                cov[2][0] * v.x + cov[2][1] * v.y + cov[2][2] * v.z
            );
            float len = glm::length(nv);
            if (len < 1e-6f) break;
            v = nv / len;
        }
        
        // Check if points are too scattered (eigenvalue ratio test)
        // If the smallest eigenvalue is too large relative to others, points are scattered
        float trace = cov[0][0] + cov[1][1] + cov[2][2];
        float minEigenvalue = glm::dot(v, glm::vec3(
            cov[0][0] * v.x + cov[0][1] * v.y + cov[0][2] * v.z,
            cov[1][0] * v.x + cov[1][1] * v.y + cov[1][2] * v.z,
            cov[2][0] * v.x + cov[2][1] * v.y + cov[2][2] * v.z
        ));
        
        // If smallest eigenvalue is more than 30% of trace, points are too scattered
        bool isValid = (minEigenvalue / trace) < 0.3f;
        
        return {glm::normalize(v), isValid};
    };
    
    // Follow path in both directions from start point
    for (int direction = -1; direction <= 1; direction += 2) {
        glm::vec3 currentPos = startPoint;
        const int MAX_STEPS = 200;
        
        for (int step = 0; step < MAX_STEPS; ++step) {
            // Expand sphere from current position
            float sphereRadius = 0.0f;
            std::vector<glm::vec3> pointsInSphere = expandSphere(currentPos, sphereRadius);
            
            if (pointsInSphere.size() < static_cast<size_t>(minPointsInSphere)) {
                // Not enough points, stop
                break;
            }
            
            // Compute plane from points in sphere
            glm::vec3 sphereCenter = currentPos;
            for (const auto& p : pointsInSphere) {
                sphereCenter += p;
            }
            sphereCenter /= static_cast<float>(pointsInSphere.size() + 1);
            
            auto [planeNormal, isValid] = computePlane(pointsInSphere, sphereCenter);
            
            if (!isValid || glm::length(planeNormal) < 0.001f) {
                // Points too scattered, cannot compute plane, stop
                break;
            }
            
            // Move perpendicular to plane in the current direction
            glm::vec3 moveDir = planeNormal * static_cast<float>(direction);
            glm::vec3 nextPos = currentPos + moveDir * stepSize;
            
            // Add to path
            pathNodes.push_back(nextPos);
            
            // Update current position
            currentPos = nextPos;
        }
    }
    
    return pathNodes;
}

// ============================================================================
// FollowTube: Tube-following algorithm (extracted for future use)
// ============================================================================

std::vector<glm::vec3> Remeshing::FollowTube(
    const glm::vec3& origin,
    const std::vector<glm::vec3>& clusterPoints,
    float stepSize,
    float tubeRadius,
    int maxNodes
) {
    // This function is kept for future use but not currently called
    // The tube-following algorithm code can be moved here if needed
    return std::vector<glm::vec3>();
}

} // namespace CameraSystem
} // namespace Editors
} // namespace Engine



