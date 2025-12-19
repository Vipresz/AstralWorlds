#include "Remeshing.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <cstddef>
#include <glm/glm.hpp>

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
            
            for (size_t n = 0; n < maxNeighbors; n++) {
                size_t j = neighbors[n].second;
                for (size_t k = n + 1; k < maxNeighbors; k++) {
                    size_t k_idx = neighbors[k].second;
                    
                    float dist_ij = neighbors[n].first;
                    float dist_ik = neighbors[k].first;
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

    // Create vertices with estimated normals
    std::vector<glm::vec3> estimatedNormals(pointCloud.size());
    for (size_t i = 0; i < pointCloud.size(); i++) {
        estimatedNormals[i] = EstimateNormal(i, pointCloud, params.poissonRadius);
        
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
        std::vector<std::pair<float, size_t>> neighbors;
        FindNeighbors(i, pointCloud, params.poissonRadius * 2.0f, neighbors);
        
        if (neighbors.size() < 2) continue;
        
        std::sort(neighbors.begin(), neighbors.end());
        const size_t maxNeighbors = std::min(neighbors.size(), size_t(params.maxNeighbors));
        
        // Create triangles with neighbors, prioritizing those with similar normals
        for (size_t n = 0; n < maxNeighbors; n++) {
            size_t j = neighbors[n].second;
            glm::vec3 normal_i = estimatedNormals[i];
            glm::vec3 normal_j = estimatedNormals[j];
            float normalDot = glm::dot(normal_i, normal_j);
            
            // Prefer neighbors with similar normals (facing same direction)
            if (normalDot < 0.5f) continue;
            
            for (size_t k = n + 1; k < maxNeighbors; k++) {
                size_t k_idx = neighbors[k].second;
                glm::vec3 normal_k = estimatedNormals[k_idx];
                
                float normalDot_jk = glm::dot(normal_j, normal_k);
                float normalDot_ik = glm::dot(normal_i, normal_k);
                
                // All three points should have similar normals
                if (normalDot_jk < 0.5f || normalDot_ik < 0.5f) continue;
                
                // Check triangle validity
                float dist_ij = neighbors[n].first;
                float dist_ik = neighbors[k].first;
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

}
}
}



