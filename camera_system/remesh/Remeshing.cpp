#include "Remeshing.h"
#include "../DensitySDFModel.h"  // Trained Random Forest model for SDF prediction
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
        case Algorithm::DensitySDF:
            result = GenerateDensitySDF(workingPointCloud, adjustedParams);
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
    
    // Collect points and colors
    std::vector<glm::vec3> allPoints;
    std::vector<glm::vec3> allColors;
    allPoints.reserve(pointCloud.size());
    allColors.reserve(pointCloud.size());
    for (const auto& p : pointCloud) {
        allPoints.push_back(p.position);
        allColors.push_back(p.color);
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
    std::vector<std::vector<glm::vec3>> clusterColors(numInitialClusters);
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
        clusterColors[static_cast<size_t>(a)].push_back(allColors[i]);
    }

    const float sliceThickness = params.fctmSliceThickness;
    std::vector<glm::vec3> allClusterCenters;
    std::vector<glm::vec3> allClusterCenterColors;
    allClusterCenters.reserve(allPoints.size() / 10); // heuristic
    allClusterCenterColors.reserve(allPoints.size() / 10);
    
    // Process each initial cluster separately
    for (size_t clusterIdx = 0; clusterIdx < clusterPoints.size(); ++clusterIdx) {
        const auto& clusterPointSet = clusterPoints[clusterIdx];
        const auto& clusterColorSet = clusterColors[clusterIdx];
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
            
            // Collect points and colors in this shell
            std::vector<glm::vec3> pointsInSlice;
            std::vector<glm::vec3> colorsInSlice;
            pointsInSlice.reserve(clusterPointSet.size() / N + 8);
            colorsInSlice.reserve(clusterPointSet.size() / N + 8);
            for (size_t i = 0; i < clusterPointSet.size(); ++i) {
                float d = glm::length(clusterPointSet[i] - clusterCenter);
                if (d > innerR && d <= outerR) {
                    pointsInSlice.push_back(clusterPointSet[i]);
                    colorsInSlice.push_back(clusterColorSet[i]);
                }
            }
            if (pointsInSlice.size() < 3) continue;

            // Choose k for the shell (keep it small + stable)
            int k = static_cast<int>(std::sqrt(static_cast<float>(pointsInSlice.size())));
            float thicknessRatio = sliceThickness / maxRadius;
            k = static_cast<int>(k * thicknessRatio * 5.0f);
            k = std::max(1, std::min(k, static_cast<int>(pointsInSlice.size())));
            
            std::vector<glm::vec3> sliceCenters;
            std::vector<int> sliceAssignments(pointsInSlice.size(), -1);
            PerformKMeans3DWithAssignments(pointsInSlice, k, sliceCenters, sliceAssignments);
            
            // Compute average color for each cluster center
            std::vector<glm::vec3> sliceCenterColors(sliceCenters.size(), glm::vec3(0.0f));
            std::vector<int> sliceCenterCounts(sliceCenters.size(), 0);
            for (size_t i = 0; i < pointsInSlice.size(); ++i) {
                int a = sliceAssignments[i];
                if (a >= 0 && a < static_cast<int>(sliceCenters.size())) {
                    sliceCenterColors[a] += colorsInSlice[i];
                    sliceCenterCounts[a]++;
                }
            }
            for (size_t i = 0; i < sliceCenters.size(); ++i) {
                if (sliceCenterCounts[i] > 0) {
                    sliceCenterColors[i] /= static_cast<float>(sliceCenterCounts[i]);
                } else {
                    sliceCenterColors[i] = glm::vec3(1.0f, 0.0f, 0.0f); // Fallback to red
                }
            }

            allClusterCenters.insert(allClusterCenters.end(), sliceCenters.begin(), sliceCenters.end());
            allClusterCenterColors.insert(allClusterCenterColors.end(), sliceCenterColors.begin(), sliceCenterColors.end());
        }
    }

    // Emit vertices with computed colors
    result.vertices.reserve(allClusterCenters.size());
    for (size_t i = 0; i < allClusterCenters.size(); ++i) {
        MeshVertex vtx;
        vtx.position = allClusterCenters[i];
        vtx.color    = allClusterCenterColors[i];
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

    // Collect points and colors
    std::vector<glm::vec3> pts;
    std::vector<glm::vec3> ptColors;
    pts.reserve(pointCloud.size());
    ptColors.reserve(pointCloud.size());
    for (const auto& p : pointCloud) {
        pts.push_back(p.position);
        ptColors.push_back(p.color);
    }

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
        // For each centerline point, average colors of nearby original points
        const unsigned int base = (unsigned int)result.vertices.size();
        float colorRadius = glm::length(maxPt - minPt) * 0.05f; // 5% of bounding box
        for (auto& p : cl) {
            MeshVertex vtx;
            vtx.position = p;
            
            // Average colors of nearby points
            glm::vec3 avgColor(0.0f);
            int colorCount = 0;
            for (size_t idx : comp) {
                if (glm::length(pts[idx] - p) < colorRadius) {
                    avgColor += ptColors[idx];
                    colorCount++;
                }
            }
            if (colorCount > 0) {
                vtx.color = avgColor / static_cast<float>(colorCount);
            } else {
                vtx.color = glm::vec3(1.0f, 0.0f, 0.0f); // Fallback to red
            }
            
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
    
    // Extract points and colors
    std::vector<glm::vec3> allPoints;
    std::vector<glm::vec3> allColors;
    allPoints.reserve(pointCloud.size());
    allColors.reserve(pointCloud.size());
    for (const auto& p : pointCloud) {
        allPoints.push_back(p.position);
        allColors.push_back(p.color);
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
        
        // Get cluster points and colors
        std::vector<glm::vec3> clusterPoints;
        std::vector<glm::vec3> clusterPointColors;
        clusterPoints.reserve(cluster.size());
        clusterPointColors.reserve(cluster.size());
        for (size_t idx : cluster) {
            clusterPoints.push_back(allPoints[idx]);
            clusterPointColors.push_back(allColors[idx]);
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
        
        // Calculate color averaging radius based on step size
        float colorRadius = STEP_SIZE * 2.0f;
        
        for (size_t i = 0; i < treeNodes.size(); ++i) {
            MeshVertex vtx;
            vtx.position = treeNodes[i];
            
            // Average colors of nearby cluster points
            glm::vec3 avgColor(0.0f);
            int colorCount = 0;
            for (size_t j = 0; j < clusterPoints.size(); ++j) {
                if (glm::length(clusterPoints[j] - treeNodes[i]) < colorRadius) {
                    avgColor += clusterPointColors[j];
                    colorCount++;
                }
            }
            if (colorCount > 0) {
                vtx.color = avgColor / static_cast<float>(colorCount);
            } else {
                vtx.color = glm::vec3(0.0f, 1.0f, 0.0f); // Fallback to green
            }
            
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

// ============================================================================
// DensitySDF: Density-based SDF reconstruction with Marching Cubes
// ============================================================================
// Based on the "learned regularizing surface predictor" concept, but using
// analytical density gradient estimation instead of a trained regression forest.
//
// Pipeline:
// 1. Rasterize point cloud to 3D density field
// 2. Compute 3D integral volume for fast box queries
// 3. For each voxel, estimate projection-difference vector Δ from density gradient
// 4. Convert to signed distance using normal estimation
// 5. Extract iso-surface with Marching Cubes
// ============================================================================

// Helper: 3D density field with integral volume for fast box queries
struct DensityField {
    std::vector<float> density;      // 3D density values (flat array)
    std::vector<float> integralVol;  // 3D integral volume for box sums
    std::vector<glm::vec3> colorSum; // Color accumulator per voxel
    int dimX, dimY, dimZ;
    glm::vec3 minBounds;
    float voxelSize;
    
    DensityField() : dimX(0), dimY(0), dimZ(0), voxelSize(1.0f) {}
    
    int index(int x, int y, int z) const {
        return x + y * dimX + z * dimX * dimY;
    }
    
    float sample(int x, int y, int z) const {
        if (x < 0 || x >= dimX || y < 0 || y >= dimY || z < 0 || z >= dimZ) return 0.0f;
        return density[index(x, y, z)];
    }
    
    // Box sum using integral volume (fast O(1) query)
    float boxSum(int x0, int y0, int z0, int x1, int y1, int z1) const {
        x0 = std::max(0, std::min(x0, dimX));
        y0 = std::max(0, std::min(y0, dimY));
        z0 = std::max(0, std::min(z0, dimZ));
        x1 = std::max(0, std::min(x1, dimX));
        y1 = std::max(0, std::min(y1, dimY));
        z1 = std::max(0, std::min(z1, dimZ));
        
        if (x1 <= x0 || y1 <= y0 || z1 <= z0) return 0.0f;
        
        // 3D inclusion-exclusion formula
        auto I = [this](int x, int y, int z) -> float {
            if (x < 0 || y < 0 || z < 0) return 0.0f;
            x = std::min(x, dimX - 1);
            y = std::min(y, dimY - 1);
            z = std::min(z, dimZ - 1);
            return integralVol[index(x, y, z)];
        };
        
        return I(x1-1, y1-1, z1-1) 
             - I(x0-1, y1-1, z1-1) - I(x1-1, y0-1, z1-1) - I(x1-1, y1-1, z0-1)
             + I(x0-1, y0-1, z1-1) + I(x0-1, y1-1, z0-1) + I(x1-1, y0-1, z0-1)
             - I(x0-1, y0-1, z0-1);
    }
    
    // Build integral volume for fast box queries
    void buildIntegralVolume() {
        integralVol.resize(density.size());
        
        for (int z = 0; z < dimZ; z++) {
            for (int y = 0; y < dimY; y++) {
                for (int x = 0; x < dimX; x++) {
                    float val = density[index(x, y, z)];
                    
                    // Add contributions from previous cells
                    if (x > 0) val += integralVol[index(x-1, y, z)];
                    if (y > 0) val += integralVol[index(x, y-1, z)];
                    if (z > 0) val += integralVol[index(x, y, z-1)];
                    if (x > 0 && y > 0) val -= integralVol[index(x-1, y-1, z)];
                    if (x > 0 && z > 0) val -= integralVol[index(x-1, y, z-1)];
                    if (y > 0 && z > 0) val -= integralVol[index(x, y-1, z-1)];
                    if (x > 0 && y > 0 && z > 0) val += integralVol[index(x-1, y-1, z-1)];
                    
                    integralVol[index(x, y, z)] = val;
                }
            }
        }
    }
    
    // World position to voxel coordinates
    void worldToVoxel(const glm::vec3& pos, int& vx, int& vy, int& vz) const {
        glm::vec3 local = (pos - minBounds) / voxelSize;
        vx = static_cast<int>(std::floor(local.x));
        vy = static_cast<int>(std::floor(local.y));
        vz = static_cast<int>(std::floor(local.z));
    }
    
    // Voxel coordinates to world position (center of voxel)
    glm::vec3 voxelToWorld(int vx, int vy, int vz) const {
        return minBounds + glm::vec3(vx + 0.5f, vy + 0.5f, vz + 0.5f) * voxelSize;
    }
};

// Complete Marching Cubes lookup tables
static const int MC_EDGE_TABLE[256] = {
    0x0  , 0x109, 0x203, 0x30a, 0x406, 0x50f, 0x605, 0x70c,
    0x80c, 0x905, 0xa0f, 0xb06, 0xc0a, 0xd03, 0xe09, 0xf00,
    0x190, 0x99 , 0x393, 0x29a, 0x596, 0x49f, 0x795, 0x69c,
    0x99c, 0x895, 0xb9f, 0xa96, 0xd9a, 0xc93, 0xf99, 0xe90,
    0x230, 0x339, 0x33 , 0x13a, 0x636, 0x73f, 0x435, 0x53c,
    0xa3c, 0xb35, 0x83f, 0x936, 0xe3a, 0xf33, 0xc39, 0xd30,
    0x3a0, 0x2a9, 0x1a3, 0xaa , 0x7a6, 0x6af, 0x5a5, 0x4ac,
    0xbac, 0xaa5, 0x9af, 0x8a6, 0xfaa, 0xea3, 0xda9, 0xca0,
    0x460, 0x569, 0x663, 0x76a, 0x66 , 0x16f, 0x265, 0x36c,
    0xc6c, 0xd65, 0xe6f, 0xf66, 0x86a, 0x963, 0xa69, 0xb60,
    0x5f0, 0x4f9, 0x7f3, 0x6fa, 0x1f6, 0xff , 0x3f5, 0x2fc,
    0xdfc, 0xcf5, 0xfff, 0xef6, 0x9fa, 0x8f3, 0xbf9, 0xaf0,
    0x650, 0x759, 0x453, 0x55a, 0x256, 0x35f, 0x55 , 0x15c,
    0xe5c, 0xf55, 0xc5f, 0xd56, 0xa5a, 0xb53, 0x859, 0x950,
    0x7c0, 0x6c9, 0x5c3, 0x4ca, 0x3c6, 0x2cf, 0x1c5, 0xcc ,
    0xfcc, 0xec5, 0xdcf, 0xcc6, 0xbca, 0xac3, 0x9c9, 0x8c0,
    0x8c0, 0x9c9, 0xac3, 0xbca, 0xcc6, 0xdcf, 0xec5, 0xfcc,
    0xcc , 0x1c5, 0x2cf, 0x3c6, 0x4ca, 0x5c3, 0x6c9, 0x7c0,
    0x950, 0x859, 0xb53, 0xa5a, 0xd56, 0xc5f, 0xf55, 0xe5c,
    0x15c, 0x55 , 0x35f, 0x256, 0x55a, 0x453, 0x759, 0x650,
    0xaf0, 0xbf9, 0x8f3, 0x9fa, 0xef6, 0xfff, 0xcf5, 0xdfc,
    0x2fc, 0x3f5, 0xff , 0x1f6, 0x6fa, 0x7f3, 0x4f9, 0x5f0,
    0xb60, 0xa69, 0x963, 0x86a, 0xf66, 0xe6f, 0xd65, 0xc6c,
    0x36c, 0x265, 0x16f, 0x66 , 0x76a, 0x663, 0x569, 0x460,
    0xca0, 0xda9, 0xea3, 0xfaa, 0x8a6, 0x9af, 0xaa5, 0xbac,
    0x4ac, 0x5a5, 0x6af, 0x7a6, 0xaa , 0x1a3, 0x2a9, 0x3a0,
    0xd30, 0xc39, 0xf33, 0xe3a, 0x936, 0x83f, 0xb35, 0xa3c,
    0x53c, 0x435, 0x73f, 0x636, 0x13a, 0x33 , 0x339, 0x230,
    0xe90, 0xf99, 0xc93, 0xd9a, 0xa96, 0xb9f, 0x895, 0x99c,
    0x69c, 0x795, 0x49f, 0x596, 0x29a, 0x393, 0x99 , 0x190,
    0xf00, 0xe09, 0xd03, 0xc0a, 0xb06, 0xa0f, 0x905, 0x80c,
    0x70c, 0x605, 0x50f, 0x406, 0x30a, 0x203, 0x109, 0x0
};

// Complete Marching Cubes triangle table - all 256 cases
static const int MC_TRI_TABLE[256][16] = {
    {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {0,8,3,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {0,1,9,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {1,8,3,9,8,1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {1,2,10,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {0,8,3,1,2,10,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {9,2,10,0,2,9,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {2,8,3,2,10,8,10,9,8,-1,-1,-1,-1,-1,-1,-1},
    {3,11,2,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {0,11,2,8,11,0,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {1,9,0,2,3,11,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {1,11,2,1,9,11,9,8,11,-1,-1,-1,-1,-1,-1,-1},
    {3,10,1,11,10,3,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {0,10,1,0,8,10,8,11,10,-1,-1,-1,-1,-1,-1,-1},
    {3,9,0,3,11,9,11,10,9,-1,-1,-1,-1,-1,-1,-1},
    {9,8,10,10,8,11,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {4,7,8,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {4,3,0,7,3,4,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {0,1,9,8,4,7,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {4,1,9,4,7,1,7,3,1,-1,-1,-1,-1,-1,-1,-1},
    {1,2,10,8,4,7,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {3,4,7,3,0,4,1,2,10,-1,-1,-1,-1,-1,-1,-1},
    {9,2,10,9,0,2,8,4,7,-1,-1,-1,-1,-1,-1,-1},
    {2,10,9,2,9,7,2,7,3,7,9,4,-1,-1,-1,-1},
    {8,4,7,3,11,2,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {11,4,7,11,2,4,2,0,4,-1,-1,-1,-1,-1,-1,-1},
    {9,0,1,8,4,7,2,3,11,-1,-1,-1,-1,-1,-1,-1},
    {4,7,11,9,4,11,9,11,2,9,2,1,-1,-1,-1,-1},
    {3,10,1,3,11,10,7,8,4,-1,-1,-1,-1,-1,-1,-1},
    {1,11,10,1,4,11,1,0,4,7,11,4,-1,-1,-1,-1},
    {4,7,8,9,0,11,9,11,10,11,0,3,-1,-1,-1,-1},
    {4,7,11,4,11,9,9,11,10,-1,-1,-1,-1,-1,-1,-1},
    {9,5,4,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {9,5,4,0,8,3,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {0,5,4,1,5,0,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {8,5,4,8,3,5,3,1,5,-1,-1,-1,-1,-1,-1,-1},
    {1,2,10,9,5,4,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {3,0,8,1,2,10,4,9,5,-1,-1,-1,-1,-1,-1,-1},
    {5,2,10,5,4,2,4,0,2,-1,-1,-1,-1,-1,-1,-1},
    {2,10,5,3,2,5,3,5,4,3,4,8,-1,-1,-1,-1},
    {9,5,4,2,3,11,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {0,11,2,0,8,11,4,9,5,-1,-1,-1,-1,-1,-1,-1},
    {0,5,4,0,1,5,2,3,11,-1,-1,-1,-1,-1,-1,-1},
    {2,1,5,2,5,8,2,8,11,4,8,5,-1,-1,-1,-1},
    {10,3,11,10,1,3,9,5,4,-1,-1,-1,-1,-1,-1,-1},
    {4,9,5,0,8,1,8,10,1,8,11,10,-1,-1,-1,-1},
    {5,4,0,5,0,11,5,11,10,11,0,3,-1,-1,-1,-1},
    {5,4,8,5,8,10,10,8,11,-1,-1,-1,-1,-1,-1,-1},
    {9,7,8,5,7,9,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {9,3,0,9,5,3,5,7,3,-1,-1,-1,-1,-1,-1,-1},
    {0,7,8,0,1,7,1,5,7,-1,-1,-1,-1,-1,-1,-1},
    {1,5,3,3,5,7,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {9,7,8,9,5,7,10,1,2,-1,-1,-1,-1,-1,-1,-1},
    {10,1,2,9,5,0,5,3,0,5,7,3,-1,-1,-1,-1},
    {8,0,2,8,2,5,8,5,7,10,5,2,-1,-1,-1,-1},
    {2,10,5,2,5,3,3,5,7,-1,-1,-1,-1,-1,-1,-1},
    {7,9,5,7,8,9,3,11,2,-1,-1,-1,-1,-1,-1,-1},
    {9,5,7,9,7,2,9,2,0,2,7,11,-1,-1,-1,-1},
    {2,3,11,0,1,8,1,7,8,1,5,7,-1,-1,-1,-1},
    {11,2,1,11,1,7,7,1,5,-1,-1,-1,-1,-1,-1,-1},
    {9,5,8,8,5,7,10,1,3,10,3,11,-1,-1,-1,-1},
    {5,7,0,5,0,9,7,11,0,1,0,10,11,10,0,-1},
    {11,10,0,11,0,3,10,5,0,8,0,7,5,7,0,-1},
    {11,10,5,7,11,5,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {10,6,5,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {0,8,3,5,10,6,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {9,0,1,5,10,6,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {1,8,3,1,9,8,5,10,6,-1,-1,-1,-1,-1,-1,-1},
    {1,6,5,2,6,1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {1,6,5,1,2,6,3,0,8,-1,-1,-1,-1,-1,-1,-1},
    {9,6,5,9,0,6,0,2,6,-1,-1,-1,-1,-1,-1,-1},
    {5,9,8,5,8,2,5,2,6,3,2,8,-1,-1,-1,-1},
    {2,3,11,10,6,5,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {11,0,8,11,2,0,10,6,5,-1,-1,-1,-1,-1,-1,-1},
    {0,1,9,2,3,11,5,10,6,-1,-1,-1,-1,-1,-1,-1},
    {5,10,6,1,9,2,9,11,2,9,8,11,-1,-1,-1,-1},
    {6,3,11,6,5,3,5,1,3,-1,-1,-1,-1,-1,-1,-1},
    {0,8,11,0,11,5,0,5,1,5,11,6,-1,-1,-1,-1},
    {3,11,6,0,3,6,0,6,5,0,5,9,-1,-1,-1,-1},
    {6,5,9,6,9,11,11,9,8,-1,-1,-1,-1,-1,-1,-1},
    {5,10,6,4,7,8,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {4,3,0,4,7,3,6,5,10,-1,-1,-1,-1,-1,-1,-1},
    {1,9,0,5,10,6,8,4,7,-1,-1,-1,-1,-1,-1,-1},
    {10,6,5,1,9,7,1,7,3,7,9,4,-1,-1,-1,-1},
    {6,1,2,6,5,1,4,7,8,-1,-1,-1,-1,-1,-1,-1},
    {1,2,5,5,2,6,3,0,4,3,4,7,-1,-1,-1,-1},
    {8,4,7,9,0,5,0,6,5,0,2,6,-1,-1,-1,-1},
    {7,3,9,7,9,4,3,2,9,5,9,6,2,6,9,-1},
    {3,11,2,7,8,4,10,6,5,-1,-1,-1,-1,-1,-1,-1},
    {5,10,6,4,7,2,4,2,0,2,7,11,-1,-1,-1,-1},
    {0,1,9,4,7,8,2,3,11,5,10,6,-1,-1,-1,-1},
    {9,2,1,9,11,2,9,4,11,7,11,4,5,10,6,-1},
    {8,4,7,3,11,5,3,5,1,5,11,6,-1,-1,-1,-1},
    {5,1,11,5,11,6,1,0,11,7,11,4,0,4,11,-1},
    {0,5,9,0,6,5,0,3,6,11,6,3,8,4,7,-1},
    {6,5,9,6,9,11,4,7,9,7,11,9,-1,-1,-1,-1},
    {10,4,9,6,4,10,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {4,10,6,4,9,10,0,8,3,-1,-1,-1,-1,-1,-1,-1},
    {10,0,1,10,6,0,6,4,0,-1,-1,-1,-1,-1,-1,-1},
    {8,3,1,8,1,6,8,6,4,6,1,10,-1,-1,-1,-1},
    {1,4,9,1,2,4,2,6,4,-1,-1,-1,-1,-1,-1,-1},
    {3,0,8,1,2,9,2,4,9,2,6,4,-1,-1,-1,-1},
    {0,2,4,4,2,6,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {8,3,2,8,2,4,4,2,6,-1,-1,-1,-1,-1,-1,-1},
    {10,4,9,10,6,4,11,2,3,-1,-1,-1,-1,-1,-1,-1},
    {0,8,2,2,8,11,4,9,10,4,10,6,-1,-1,-1,-1},
    {3,11,2,0,1,6,0,6,4,6,1,10,-1,-1,-1,-1},
    {6,4,1,6,1,10,4,8,1,2,1,11,8,11,1,-1},
    {9,6,4,9,3,6,9,1,3,11,6,3,-1,-1,-1,-1},
    {8,11,1,8,1,0,11,6,1,9,1,4,6,4,1,-1},
    {3,11,6,3,6,0,0,6,4,-1,-1,-1,-1,-1,-1,-1},
    {6,4,8,11,6,8,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {7,10,6,7,8,10,8,9,10,-1,-1,-1,-1,-1,-1,-1},
    {0,7,3,0,10,7,0,9,10,6,7,10,-1,-1,-1,-1},
    {10,6,7,1,10,7,1,7,8,1,8,0,-1,-1,-1,-1},
    {10,6,7,10,7,1,1,7,3,-1,-1,-1,-1,-1,-1,-1},
    {1,2,6,1,6,8,1,8,9,8,6,7,-1,-1,-1,-1},
    {2,6,9,2,9,1,6,7,9,0,9,3,7,3,9,-1},
    {7,8,0,7,0,6,6,0,2,-1,-1,-1,-1,-1,-1,-1},
    {7,3,2,6,7,2,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {2,3,11,10,6,8,10,8,9,8,6,7,-1,-1,-1,-1},
    {2,0,7,2,7,11,0,9,7,6,7,10,9,10,7,-1},
    {1,8,0,1,7,8,1,10,7,6,7,10,2,3,11,-1},
    {11,2,1,11,1,7,10,6,1,6,7,1,-1,-1,-1,-1},
    {8,9,6,8,6,7,9,1,6,11,6,3,1,3,6,-1},
    {0,9,1,11,6,7,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {7,8,0,7,0,6,3,11,0,11,6,0,-1,-1,-1,-1},
    {7,11,6,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {7,6,11,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {3,0,8,11,7,6,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {0,1,9,11,7,6,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {8,1,9,8,3,1,11,7,6,-1,-1,-1,-1,-1,-1,-1},
    {10,1,2,6,11,7,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {1,2,10,3,0,8,6,11,7,-1,-1,-1,-1,-1,-1,-1},
    {2,9,0,2,10,9,6,11,7,-1,-1,-1,-1,-1,-1,-1},
    {6,11,7,2,10,3,10,8,3,10,9,8,-1,-1,-1,-1},
    {7,2,3,6,2,7,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {7,0,8,7,6,0,6,2,0,-1,-1,-1,-1,-1,-1,-1},
    {2,7,6,2,3,7,0,1,9,-1,-1,-1,-1,-1,-1,-1},
    {1,6,2,1,8,6,1,9,8,8,7,6,-1,-1,-1,-1},
    {10,7,6,10,1,7,1,3,7,-1,-1,-1,-1,-1,-1,-1},
    {10,7,6,1,7,10,1,8,7,1,0,8,-1,-1,-1,-1},
    {0,3,7,0,7,10,0,10,9,6,10,7,-1,-1,-1,-1},
    {7,6,10,7,10,8,8,10,9,-1,-1,-1,-1,-1,-1,-1},
    {6,8,4,11,8,6,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {3,6,11,3,0,6,0,4,6,-1,-1,-1,-1,-1,-1,-1},
    {8,6,11,8,4,6,9,0,1,-1,-1,-1,-1,-1,-1,-1},
    {9,4,6,9,6,3,9,3,1,11,3,6,-1,-1,-1,-1},
    {6,8,4,6,11,8,2,10,1,-1,-1,-1,-1,-1,-1,-1},
    {1,2,10,3,0,11,0,6,11,0,4,6,-1,-1,-1,-1},
    {4,11,8,4,6,11,0,2,9,2,10,9,-1,-1,-1,-1},
    {10,9,3,10,3,2,9,4,3,11,3,6,4,6,3,-1},
    {8,2,3,8,4,2,4,6,2,-1,-1,-1,-1,-1,-1,-1},
    {0,4,2,4,6,2,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {1,9,0,2,3,4,2,4,6,4,3,8,-1,-1,-1,-1},
    {1,9,4,1,4,2,2,4,6,-1,-1,-1,-1,-1,-1,-1},
    {8,1,3,8,6,1,8,4,6,6,10,1,-1,-1,-1,-1},
    {10,1,0,10,0,6,6,0,4,-1,-1,-1,-1,-1,-1,-1},
    {4,6,3,4,3,8,6,10,3,0,3,9,10,9,3,-1},
    {10,9,4,6,10,4,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {4,9,5,7,6,11,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {0,8,3,4,9,5,11,7,6,-1,-1,-1,-1,-1,-1,-1},
    {5,0,1,5,4,0,7,6,11,-1,-1,-1,-1,-1,-1,-1},
    {11,7,6,8,3,4,3,5,4,3,1,5,-1,-1,-1,-1},
    {9,5,4,10,1,2,7,6,11,-1,-1,-1,-1,-1,-1,-1},
    {6,11,7,1,2,10,0,8,3,4,9,5,-1,-1,-1,-1},
    {7,6,11,5,4,10,4,2,10,4,0,2,-1,-1,-1,-1},
    {3,4,8,3,5,4,3,2,5,10,5,2,11,7,6,-1},
    {7,2,3,7,6,2,5,4,9,-1,-1,-1,-1,-1,-1,-1},
    {9,5,4,0,8,6,0,6,2,6,8,7,-1,-1,-1,-1},
    {3,6,2,3,7,6,1,5,0,5,4,0,-1,-1,-1,-1},
    {6,2,8,6,8,7,2,1,8,4,8,5,1,5,8,-1},
    {9,5,4,10,1,6,1,7,6,1,3,7,-1,-1,-1,-1},
    {1,6,10,1,7,6,1,0,7,8,7,0,9,5,4,-1},
    {4,0,10,4,10,5,0,3,10,6,10,7,3,7,10,-1},
    {7,6,10,7,10,8,5,4,10,4,8,10,-1,-1,-1,-1},
    {6,9,5,6,11,9,11,8,9,-1,-1,-1,-1,-1,-1,-1},
    {3,6,11,0,6,3,0,5,6,0,9,5,-1,-1,-1,-1},
    {0,11,8,0,5,11,0,1,5,5,6,11,-1,-1,-1,-1},
    {6,11,3,6,3,5,5,3,1,-1,-1,-1,-1,-1,-1,-1},
    {1,2,10,9,5,11,9,11,8,11,5,6,-1,-1,-1,-1},
    {0,11,3,0,6,11,0,9,6,5,6,9,1,2,10,-1},
    {11,8,5,11,5,6,8,0,5,10,5,2,0,2,5,-1},
    {6,11,3,6,3,5,2,10,3,10,5,3,-1,-1,-1,-1},
    {5,8,9,5,2,8,5,6,2,3,8,2,-1,-1,-1,-1},
    {9,5,6,9,6,0,0,6,2,-1,-1,-1,-1,-1,-1,-1},
    {1,5,8,1,8,0,5,6,8,3,8,2,6,2,8,-1},
    {1,5,6,2,1,6,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {1,3,6,1,6,10,3,8,6,5,6,9,8,9,6,-1},
    {10,1,0,10,0,6,9,5,0,5,6,0,-1,-1,-1,-1},
    {0,3,8,5,6,10,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {10,5,6,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {11,5,10,7,5,11,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {11,5,10,11,7,5,8,3,0,-1,-1,-1,-1,-1,-1,-1},
    {5,11,7,5,10,11,1,9,0,-1,-1,-1,-1,-1,-1,-1},
    {10,7,5,10,11,7,9,8,1,8,3,1,-1,-1,-1,-1},
    {11,1,2,11,7,1,7,5,1,-1,-1,-1,-1,-1,-1,-1},
    {0,8,3,1,2,7,1,7,5,7,2,11,-1,-1,-1,-1},
    {9,7,5,9,2,7,9,0,2,2,11,7,-1,-1,-1,-1},
    {7,5,2,7,2,11,5,9,2,3,2,8,9,8,2,-1},
    {2,5,10,2,3,5,3,7,5,-1,-1,-1,-1,-1,-1,-1},
    {8,2,0,8,5,2,8,7,5,10,2,5,-1,-1,-1,-1},
    {9,0,1,5,10,3,5,3,7,3,10,2,-1,-1,-1,-1},
    {9,8,2,9,2,1,8,7,2,10,2,5,7,5,2,-1},
    {1,3,5,3,7,5,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {0,8,7,0,7,1,1,7,5,-1,-1,-1,-1,-1,-1,-1},
    {9,0,3,9,3,5,5,3,7,-1,-1,-1,-1,-1,-1,-1},
    {9,8,7,5,9,7,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {5,8,4,5,10,8,10,11,8,-1,-1,-1,-1,-1,-1,-1},
    {5,0,4,5,11,0,5,10,11,11,3,0,-1,-1,-1,-1},
    {0,1,9,8,4,10,8,10,11,10,4,5,-1,-1,-1,-1},
    {10,11,4,10,4,5,11,3,4,9,4,1,3,1,4,-1},
    {2,5,1,2,8,5,2,11,8,4,5,8,-1,-1,-1,-1},
    {0,4,11,0,11,3,4,5,11,2,11,1,5,1,11,-1},
    {0,2,5,0,5,9,2,11,5,4,5,8,11,8,5,-1},
    {9,4,5,2,11,3,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {2,5,10,3,5,2,3,4,5,3,8,4,-1,-1,-1,-1},
    {5,10,2,5,2,4,4,2,0,-1,-1,-1,-1,-1,-1,-1},
    {3,10,2,3,5,10,3,8,5,4,5,8,0,1,9,-1},
    {5,10,2,5,2,4,1,9,2,9,4,2,-1,-1,-1,-1},
    {8,4,5,8,5,3,3,5,1,-1,-1,-1,-1,-1,-1,-1},
    {0,4,5,1,0,5,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {8,4,5,8,5,3,9,0,5,0,3,5,-1,-1,-1,-1},
    {9,4,5,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {4,11,7,4,9,11,9,10,11,-1,-1,-1,-1,-1,-1,-1},
    {0,8,3,4,9,7,9,11,7,9,10,11,-1,-1,-1,-1},
    {1,10,11,1,11,4,1,4,0,7,4,11,-1,-1,-1,-1},
    {3,1,4,3,4,8,1,10,4,7,4,11,10,11,4,-1},
    {4,11,7,9,11,4,9,2,11,9,1,2,-1,-1,-1,-1},
    {9,7,4,9,11,7,9,1,11,2,11,1,0,8,3,-1},
    {11,7,4,11,4,2,2,4,0,-1,-1,-1,-1,-1,-1,-1},
    {11,7,4,11,4,2,8,3,4,3,2,4,-1,-1,-1,-1},
    {2,9,10,2,7,9,2,3,7,7,4,9,-1,-1,-1,-1},
    {9,10,7,9,7,4,10,2,7,8,7,0,2,0,7,-1},
    {3,7,10,3,10,2,7,4,10,1,10,0,4,0,10,-1},
    {1,10,2,8,7,4,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {4,9,1,4,1,7,7,1,3,-1,-1,-1,-1,-1,-1,-1},
    {4,9,1,4,1,7,0,8,1,8,7,1,-1,-1,-1,-1},
    {4,0,3,7,4,3,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {4,8,7,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {9,10,8,10,11,8,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {3,0,9,3,9,11,11,9,10,-1,-1,-1,-1,-1,-1,-1},
    {0,1,10,0,10,8,8,10,11,-1,-1,-1,-1,-1,-1,-1},
    {3,1,10,11,3,10,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {1,2,11,1,11,9,9,11,8,-1,-1,-1,-1,-1,-1,-1},
    {3,0,9,3,9,11,1,2,9,2,11,9,-1,-1,-1,-1},
    {0,2,11,8,0,11,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {3,2,11,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {2,3,8,2,8,10,10,8,9,-1,-1,-1,-1,-1,-1,-1},
    {9,10,2,0,9,2,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {2,3,8,2,8,10,0,1,8,1,10,8,-1,-1,-1,-1},
    {1,10,2,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {1,3,8,9,1,8,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {0,9,1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {0,3,8,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}
};

RemeshResult Remeshing::GenerateDensitySDF(
    const std::vector<PointCloudPoint>& pointCloud,
    const AlgorithmParameters& params
) {
    RemeshResult result;
    
    if (pointCloud.size() < 4) {
        fprintf(stderr, "[DensitySDF] Point cloud too small: %zu points\n", pointCloud.size());
        return result;
    }
    
    fprintf(stderr, "[DensitySDF] Starting with %zu points, voxelSize=%.2f, smoothRadius=%.2f, isoValue=%.2f\n",
            pointCloud.size(), params.densityVoxelSize, params.densitySmoothRadius, params.densityIsoValue);
    
    const float voxelSize = params.densityVoxelSize;
    const float smoothRadius = params.densitySmoothRadius;
    const float isoValue = params.densityIsoValue;  // Controls density threshold (0 = tight, 1 = loose)
    
    // =========================================================================
    // STEP 1: Compute bounding box with margin
    // =========================================================================
    glm::vec3 minBounds(std::numeric_limits<float>::max());
    glm::vec3 maxBounds(std::numeric_limits<float>::lowest());
    
    for (const auto& p : pointCloud) {
        minBounds = glm::min(minBounds, p.position);
        maxBounds = glm::max(maxBounds, p.position);
    }
    
    // Expand bounds by margin for surface extraction
    float margin = voxelSize * 4.0f;
    minBounds -= glm::vec3(margin);
    maxBounds += glm::vec3(margin);
    
    // =========================================================================
    // STEP 2: Create voxel grid
    // =========================================================================
    DensityField field;
    field.minBounds = minBounds;
    field.voxelSize = voxelSize;
    field.dimX = static_cast<int>(std::ceil((maxBounds.x - minBounds.x) / voxelSize)) + 1;
    field.dimY = static_cast<int>(std::ceil((maxBounds.y - minBounds.y) / voxelSize)) + 1;
    field.dimZ = static_cast<int>(std::ceil((maxBounds.z - minBounds.z) / voxelSize)) + 1;
    
    // Limit grid size for performance
    const int maxDim = 100;
    if (field.dimX > maxDim || field.dimY > maxDim || field.dimZ > maxDim) {
        float maxExtent = std::max({maxBounds.x - minBounds.x, maxBounds.y - minBounds.y, maxBounds.z - minBounds.z});
        field.voxelSize = maxExtent / (maxDim - 4);
        field.dimX = static_cast<int>(std::ceil((maxBounds.x - minBounds.x) / field.voxelSize)) + 1;
        field.dimY = static_cast<int>(std::ceil((maxBounds.y - minBounds.y) / field.voxelSize)) + 1;
        field.dimZ = static_cast<int>(std::ceil((maxBounds.z - minBounds.z) / field.voxelSize)) + 1;
    }
    
    size_t totalVoxels = static_cast<size_t>(field.dimX) * field.dimY * field.dimZ;
    
    fprintf(stderr, "[DensitySDF] Grid: %dx%dx%d = %zu voxels, voxelSize=%.3f\n", 
            field.dimX, field.dimY, field.dimZ, totalVoxels, field.voxelSize);
    
    // =========================================================================
    // STEP 3: Rasterize point cloud into density field using Gaussian splatting
    // This follows the paper's approach: ρ(x) via trilinear splatting
    // =========================================================================
    std::vector<float> density(totalVoxels, 0.0f);
    field.colorSum.resize(totalVoxels, glm::vec3(0.0f));
    std::vector<float> colorWeights(totalVoxels, 0.0f);
    
    // Gaussian splat radius: larger = smoother density field
    // The key insight: points contribute to a neighborhood, creating continuous density
    // Training used splatSigma/voxelSize ratio of 1.5 (0.03/0.02), match that here
    // smoothRadius=0 → ratio=1.5, smoothRadius=1 → ratio=2.0, etc.
    const float splatSigma = field.voxelSize * (1.5f + smoothRadius * 0.5f);
    const float sigmaSq2 = 2.0f * splatSigma * splatSigma;
    const int splatRadius = static_cast<int>(std::ceil(3.0f * splatSigma / field.voxelSize));
    
    fprintf(stderr, "[DensitySDF] Splatting with sigma=%.3f, radius=%d voxels\n", splatSigma, splatRadius);
    
    for (const auto& p : pointCloud) {
        // Convert point to voxel coordinates
        glm::vec3 voxelCoord = (p.position - minBounds) / field.voxelSize;
        int cx = static_cast<int>(std::round(voxelCoord.x));
        int cy = static_cast<int>(std::round(voxelCoord.y));
        int cz = static_cast<int>(std::round(voxelCoord.z));
        
        // Splat Gaussian contribution to nearby voxels
        for (int dz = -splatRadius; dz <= splatRadius; dz++) {
            for (int dy = -splatRadius; dy <= splatRadius; dy++) {
                for (int dx = -splatRadius; dx <= splatRadius; dx++) {
                    int nx = cx + dx, ny = cy + dy, nz = cz + dz;
                    if (nx < 0 || nx >= field.dimX || 
                        ny < 0 || ny >= field.dimY || 
                        nz < 0 || nz >= field.dimZ) continue;
                    
                    glm::vec3 voxelPos = field.voxelToWorld(nx, ny, nz);
                    float dist2 = glm::dot(p.position - voxelPos, p.position - voxelPos);
                    float w = std::exp(-dist2 / sigmaSq2) * p.weight;
                    
                    int idx = field.index(nx, ny, nz);
                    density[idx] += w;
                    field.colorSum[idx] += p.color * w;
                    colorWeights[idx] += w;
                }
            }
        }
    }
    
    // Normalize colors
    for (size_t i = 0; i < totalVoxels; i++) {
        if (colorWeights[i] > 0.001f) {
            field.colorSum[i] /= colorWeights[i];
        } else {
            field.colorSum[i] = glm::vec3(0.7f);  // Default gray
        }
    }
    
    // Find max density for threshold calculation
    float maxDensity = 0.0f;
    for (size_t i = 0; i < totalVoxels; i++) {
        maxDensity = std::max(maxDensity, density[i]);
    }
    
    if (maxDensity < 0.0001f) {
        fprintf(stderr, "[DensitySDF] Error: No density accumulated (maxDensity=%.6f)\n", maxDensity);
        return result;
    }
    
    fprintf(stderr, "[DensitySDF] Max density: %.4f\n", maxDensity);
    
    // =========================================================================
    // STEP 4: Build integral volume (3D prefix sums) for fast box-sum queries
    // This enables O(1) queries of density sum in any axis-aligned box
    // =========================================================================
    std::vector<float> integralVol(totalVoxels, 0.0f);
    
    // Compute 3D prefix sums
    for (int z = 0; z < field.dimZ; z++) {
        for (int y = 0; y < field.dimY; y++) {
            for (int x = 0; x < field.dimX; x++) {
                int idx = field.index(x, y, z);
                float val = density[idx];
                
                // Add contributions from adjacent prefix sums
                if (x > 0) val += integralVol[field.index(x-1, y, z)];
                if (y > 0) val += integralVol[field.index(x, y-1, z)];
                if (z > 0) val += integralVol[field.index(x, y, z-1)];
                
                // Subtract double-counted edges
                if (x > 0 && y > 0) val -= integralVol[field.index(x-1, y-1, z)];
                if (x > 0 && z > 0) val -= integralVol[field.index(x-1, y, z-1)];
                if (y > 0 && z > 0) val -= integralVol[field.index(x, y-1, z-1)];
                
                // Add back triple-counted corner
                if (x > 0 && y > 0 && z > 0) val += integralVol[field.index(x-1, y-1, z-1)];
                
                integralVol[idx] = val;
            }
        }
    }
    
    // Box-sum query helper (inclusive bounds)
    auto boxSum = [&](int x0, int y0, int z0, int x1, int y1, int z1) -> float {
        // Clamp bounds
        x0 = std::max(0, std::min(x0, field.dimX - 1));
        y0 = std::max(0, std::min(y0, field.dimY - 1));
        z0 = std::max(0, std::min(z0, field.dimZ - 1));
        x1 = std::max(0, std::min(x1, field.dimX - 1));
        y1 = std::max(0, std::min(y1, field.dimY - 1));
        z1 = std::max(0, std::min(z1, field.dimZ - 1));
        
        float sum = integralVol[field.index(x1, y1, z1)];
        
        if (x0 > 0) sum -= integralVol[field.index(x0-1, y1, z1)];
        if (y0 > 0) sum -= integralVol[field.index(x1, y0-1, z1)];
        if (z0 > 0) sum -= integralVol[field.index(x1, y1, z0-1)];
        
        if (x0 > 0 && y0 > 0) sum += integralVol[field.index(x0-1, y0-1, z1)];
        if (x0 > 0 && z0 > 0) sum += integralVol[field.index(x0-1, y1, z0-1)];
        if (y0 > 0 && z0 > 0) sum += integralVol[field.index(x1, y0-1, z0-1)];
        
        if (x0 > 0 && y0 > 0 && z0 > 0) sum -= integralVol[field.index(x0-1, y0-1, z0-1)];
        
        return sum;
    };
    
    // =========================================================================
    // STEP 5: Compute density gradient for surface normal estimation
    // The gradient points from low to high density (toward the surface)
    // =========================================================================
    std::vector<glm::vec3> densityGradient(totalVoxels, glm::vec3(0.0f));
    
    for (int z = 1; z < field.dimZ - 1; z++) {
        for (int y = 1; y < field.dimY - 1; y++) {
            for (int x = 1; x < field.dimX - 1; x++) {
                int idx = field.index(x, y, z);
                
                // Central differences for gradient
                float gx = (density[field.index(x+1, y, z)] - density[field.index(x-1, y, z)]) / (2.0f * field.voxelSize);
                float gy = (density[field.index(x, y+1, z)] - density[field.index(x, y-1, z)]) / (2.0f * field.voxelSize);
                float gz = (density[field.index(x, y, z+1)] - density[field.index(x, y, z-1)]) / (2.0f * field.voxelSize);
                
                densityGradient[idx] = glm::vec3(gx, gy, gz);
            }
        }
    }
    
    // =========================================================================
    // STEP 6: Convert density to SDF following the paper's approach
    // 
    // Key insight: The surface is the iso-level where density = threshold
    // For the SDF:
    //   - High density (above threshold) = inside = negative SDF
    //   - Low density (below threshold) = outside = positive SDF
    //   - At threshold = surface = SDF ≈ 0
    // 
    // The magnitude is estimated by: |density - threshold| / |∇density|
    // This converts density difference to physical distance
    // =========================================================================
    std::vector<float> sdf(totalVoxels);
    
    // Density threshold: controls how tight the mesh fits
    // isoValue = 0 → threshold at max density (very tight, may have holes)
    // isoValue = 0.5 → threshold at half max (balanced)
    // isoValue = 1 → threshold near zero (very loose/puffy)
    float densityThreshold = maxDensity * (1.0f - isoValue * 0.9f);
    
    fprintf(stderr, "[DensitySDF] Density threshold: %.4f (%.1f%% of max)\n", 
            densityThreshold, 100.0f * densityThreshold / maxDensity);
    
    // Statistics
    int insideCount = 0;
    float minSdf = std::numeric_limits<float>::max();
    float maxSdf = std::numeric_limits<float>::lowest();
    
    for (int z = 0; z < field.dimZ; z++) {
        for (int y = 0; y < field.dimY; y++) {
            for (int x = 0; x < field.dimX; x++) {
                int idx = field.index(x, y, z);
                
                float d = density[idx];
                glm::vec3 grad = densityGradient[idx];
                float gradMag = glm::length(grad);
                
                // Density difference from threshold
                float densityDiff = densityThreshold - d;  // Positive outside, negative inside
                
                // Convert to physical distance using trained Random Forest model
                // The model predicts Δ_S(x) = x - π_S(x), the vector from query to closest surface
                float distance;
                
                // Use trained model to predict projection difference vector
                // Extract features using the same box definitions used during training
                float features[DensitySDF::NUM_FEATURES];
                for (uint32_t fi = 0; fi < DensitySDF::NUM_FEATURES; fi++) {
                    const auto& bf = DensitySDF::BOX_FEATURES[fi];
                    int x0 = x + bf.offsetX - bf.halfSizeX;
                    int y0 = y + bf.offsetY - bf.halfSizeY;
                    int z0 = z + bf.offsetZ - bf.halfSizeZ;
                    int x1 = x + bf.offsetX + bf.halfSizeX;
                    int y1 = y + bf.offsetY + bf.halfSizeY;
                    int z1 = z + bf.offsetZ + bf.halfSizeZ;
                    features[fi] = boxSum(x0, y0, z0, x1, y1, z1);
                }
                
                // Get prediction from Random Forest
                float deltaX, deltaY, deltaZ;
                DensitySDF::PredictDelta(features, deltaX, deltaY, deltaZ);
                
                // The predicted delta vector tells us direction and distance to surface
                float predictedDist = std::sqrt(deltaX*deltaX + deltaY*deltaY + deltaZ*deltaZ);
                
                // Determine sign: use gradient if available, otherwise use delta direction
                float sign = 1.0f;
                if (gradMag > 0.001f * maxDensity / field.voxelSize) {
                    // Good gradient available - use it for sign
                    // If gradient points same way as delta, we're outside (positive SDF)
                    // If gradient points opposite, we're inside (negative SDF)
                    glm::vec3 normalizedGrad = densityGradient[idx] / gradMag;
                    glm::vec3 deltaDir = glm::vec3(deltaX, deltaY, deltaZ);
                    float deltaMag = predictedDist;
                    if (deltaMag > 0.0001f) {
                        deltaDir /= deltaMag;
                        // Gradient points toward high density (inside)
                        // Delta points from query to surface
                        // If they agree, query is outside
                        sign = glm::dot(normalizedGrad, deltaDir) < 0.0f ? -1.0f : 1.0f;
                    } else {
                        // Very close to surface - use density to determine sign
                        sign = (d > densityThreshold) ? -1.0f : 1.0f;
                    }
                } else {
                    // Weak gradient - use density threshold for sign
                    sign = (d > densityThreshold) ? -1.0f : 1.0f;
                }
                
                // Blend model prediction with gradient-based estimate if gradient is strong
                if (gradMag > 0.01f * maxDensity / field.voxelSize) {
                    float gradientDist = std::abs(densityDiff) / gradMag;
                    // Weighted blend - trust gradient more when it's strong
                    float gradWeight = std::min(1.0f, gradMag / (0.1f * maxDensity / field.voxelSize));
                    distance = sign * (gradWeight * gradientDist + (1.0f - gradWeight) * predictedDist);
                } else {
                    distance = sign * predictedDist;
                }
                
                // Clamp SDF to reasonable range
                float maxDist = field.voxelSize * 20.0f;
                distance = std::clamp(distance, -maxDist, maxDist);
                
                sdf[idx] = distance;
                
                minSdf = std::min(minSdf, distance);
                maxSdf = std::max(maxSdf, distance);
                if (distance < 0.0f) insideCount++;
            }
        }
    }
    
    fprintf(stderr, "[DensitySDF] SDF range: [%.3f, %.3f], inside: %d (%.1f%%)\n", 
            minSdf, maxSdf, insideCount, 100.0f * insideCount / totalVoxels);
    
    // =========================================================================
    // STEP 7: Light Gaussian smoothing of SDF (optional, helps regularize)
    // =========================================================================
    if (smoothRadius > 0.5f) {
        std::vector<float> smoothedSdf(totalVoxels);
        int smoothK = std::min(2, static_cast<int>(std::ceil(smoothRadius * 0.5f)));
        float smoothSigmaSq2 = 2.0f * (smoothRadius * 0.5f) * (smoothRadius * 0.5f);
        
        for (int z = 0; z < field.dimZ; z++) {
            for (int y = 0; y < field.dimY; y++) {
                for (int x = 0; x < field.dimX; x++) {
                    float sum = 0.0f;
                    float wSum = 0.0f;
                    
                    for (int dz = -smoothK; dz <= smoothK; dz++) {
                        for (int dy = -smoothK; dy <= smoothK; dy++) {
                            for (int dx = -smoothK; dx <= smoothK; dx++) {
                                int nx = x + dx, ny = y + dy, nz = z + dz;
                                if (nx < 0 || nx >= field.dimX || 
                                    ny < 0 || ny >= field.dimY || 
                                    nz < 0 || nz >= field.dimZ) continue;
                                
                                float d2 = static_cast<float>(dx*dx + dy*dy + dz*dz);
                                float w = std::exp(-d2 / smoothSigmaSq2);
                                sum += sdf[field.index(nx, ny, nz)] * w;
                                wSum += w;
                            }
                        }
                    }
                    
                    smoothedSdf[field.index(x, y, z)] = sum / wSum;
                }
            }
        }
        sdf = std::move(smoothedSdf);
    }
    
    // =========================================================================
    // STEP 8: Marching Cubes - Extract iso-surface at SDF = 0
    // =========================================================================
    std::vector<MeshVertex> vertices;
    std::vector<unsigned int> indices;
    std::map<std::tuple<int,int,int,int>, unsigned int> edgeToVertex;
    
    static const int edgeVerts[12][2] = {
        {0,1}, {1,2}, {2,3}, {3,0},
        {4,5}, {5,6}, {6,7}, {7,4},
        {0,4}, {1,5}, {2,6}, {3,7}
    };
    
    static const int cubeVerts[8][3] = {
        {0,0,0}, {1,0,0}, {1,1,0}, {0,1,0},
        {0,0,1}, {1,0,1}, {1,1,1}, {0,1,1}
    };
    
    auto getOrCreateVertex = [&](int x, int y, int z, int edge) -> unsigned int {
        auto key = std::make_tuple(x, y, z, edge);
        auto it = edgeToVertex.find(key);
        if (it != edgeToVertex.end()) {
            return it->second;
        }
        
        int v0 = edgeVerts[edge][0];
        int v1 = edgeVerts[edge][1];
        
        int x0 = x + cubeVerts[v0][0], y0 = y + cubeVerts[v0][1], z0 = z + cubeVerts[v0][2];
        int x1 = x + cubeVerts[v1][0], y1 = y + cubeVerts[v1][1], z1 = z + cubeVerts[v1][2];
        
        float d0 = sdf[field.index(x0, y0, z0)];
        float d1 = sdf[field.index(x1, y1, z1)];
        
        float t = (std::abs(d1 - d0) > 0.0001f) ? (-d0 / (d1 - d0)) : 0.5f;
        t = std::clamp(t, 0.0f, 1.0f);
        
        glm::vec3 p0 = field.voxelToWorld(x0, y0, z0);
        glm::vec3 p1 = field.voxelToWorld(x1, y1, z1);
        
        MeshVertex v;
        v.position = p0 + t * (p1 - p0);
        
        glm::vec3 c0 = field.colorSum[field.index(x0, y0, z0)];
        glm::vec3 c1 = field.colorSum[field.index(x1, y1, z1)];
        v.color = c0 + t * (c1 - c0);
        v.normal = glm::vec3(0.0f);
        
        unsigned int idx = static_cast<unsigned int>(vertices.size());
        vertices.push_back(v);
        edgeToVertex[key] = idx;
        return idx;
    };
    
    // Process each cube
    int surfaceCubes = 0;
    for (int z = 0; z < field.dimZ - 1; z++) {
        for (int y = 0; y < field.dimY - 1; y++) {
            for (int x = 0; x < field.dimX - 1; x++) {
                float vals[8];
                for (int i = 0; i < 8; i++) {
                    int cx = x + cubeVerts[i][0];
                    int cy = y + cubeVerts[i][1];
                    int cz = z + cubeVerts[i][2];
                    vals[i] = sdf[field.index(cx, cy, cz)];
                }
                
                int cubeIndex = 0;
                for (int i = 0; i < 8; i++) {
                    if (vals[i] < 0.0f) cubeIndex |= (1 << i);
                }
                
                if (MC_EDGE_TABLE[cubeIndex] == 0) continue;
                surfaceCubes++;
                
                int edgeMask = MC_EDGE_TABLE[cubeIndex];
                unsigned int vertList[12];
                for (int e = 0; e < 12; e++) {
                    if (edgeMask & (1 << e)) {
                        vertList[e] = getOrCreateVertex(x, y, z, e);
                    }
                }
                
                for (int i = 0; MC_TRI_TABLE[cubeIndex][i] != -1; i += 3) {
                    indices.push_back(vertList[MC_TRI_TABLE[cubeIndex][i]]);
                    indices.push_back(vertList[MC_TRI_TABLE[cubeIndex][i+1]]);
                    indices.push_back(vertList[MC_TRI_TABLE[cubeIndex][i+2]]);
                }
            }
        }
    }
    
    fprintf(stderr, "[DensitySDF] Marching Cubes: %d surface cubes processed\n", surfaceCubes);
    
    // =========================================================================
    // STEP 9: Compute smooth normals
    // =========================================================================
    if (!vertices.empty() && !indices.empty()) {
        CalculateNormals(vertices, indices);
    }
    
    fprintf(stderr, "[DensitySDF] Result: %zu vertices, %zu indices (%zu triangles)\n",
            vertices.size(), indices.size(), indices.size() / 3);
    
    result.vertices = std::move(vertices);
    result.indices = std::move(indices);
    result.triangleCount = result.indices.size() / 3;
    result.vertexCount = result.vertices.size();
    
    return result;
}

} // namespace CameraSystem
} // namespace Editors
} // namespace Engine



