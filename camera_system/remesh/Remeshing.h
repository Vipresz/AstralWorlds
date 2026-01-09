#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <map>
#include <cstdint>
#include <set>
#include <tuple>

namespace Engine {
namespace Editors {
namespace CameraSystem {

// Grid key for spatial hashing/gridding
struct GridKey {
    int32_t x, y, z;
    
    bool operator<(const GridKey& other) const {
        if (x != other.x) return x < other.x;
        if (y != other.y) return y < other.y;
        return z < other.z;
    }
    
    bool operator==(const GridKey& other) const {
        return x == other.x && y == other.y && z == other.z;
    }
    
    GridKey() : x(0), y(0), z(0) {}
    GridKey(int32_t x, int32_t y, int32_t z) : x(x), y(y), z(z) {}
};

// Point in a point cloud
struct PointCloudPoint {
    glm::vec3 position;
    glm::vec3 color;
    float weight;  // Weight for mesh algorithms (0.0-1.0, default 1.0)
    int32_t segmentId;  // Segmentation ID for object identification (-1 = unassigned)
    int32_t cameraId;   // Camera that captured this point (-1 = unknown)
    
    PointCloudPoint() : position(0.0f), color(1.0f), weight(1.0f), segmentId(-1), cameraId(-1) {}
    PointCloudPoint(const glm::vec3& pos, const glm::vec3& col) : position(pos), color(col), weight(1.0f), segmentId(-1), cameraId(-1) {}
    PointCloudPoint(const glm::vec3& pos, const glm::vec3& col, float w) : position(pos), color(col), weight(w), segmentId(-1), cameraId(-1) {}
};

// 2D point for Delaunay triangulation
struct Point2D {
    float x, y;
    size_t originalIndex;
    glm::vec3 originalPos;
    
    Point2D() : x(0.0f), y(0.0f), originalIndex(0) {}
};

// Mesh vertex
struct MeshVertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec3 color;
    
    MeshVertex() : position(0.0f), normal(0.0f, 0.0f, 1.0f), color(1.0f) {}
};

// Remeshing algorithm enum
enum class Algorithm {
    GriddedQuads,
    NearestNeighbor,
    Delaunay2D,
    PoissonLike,
    FCTM,
    GeodesicTree,
    GTree,
    DensitySDF       // Density-based SDF reconstruction with marching cubes
};

// Algorithm parameters
struct AlgorithmParameters {
    float maxDistance;
    int maxNeighbors;
    int projectionPlane;  // 0=XY, 1=XZ, 2=YZ, 3=best fit
    float delaunayThreshold;
    float poissonRadius;
    int poissonDepth;
    float poissonDensity;
    bool poissonSmoothNormals;
    float fctmSliceThickness;  // Thickness of slice near each face (steps are auto-calculated from this)
    int geodesicKNeighbors;     // Number of neighbors for kNN graph construction
    int geodesicNumSeeds;       // Number of seed points (0 = auto-determine using k-means)
    float geodesicInteriorBias; // Bias weight for staying inside point cloud (0.0-1.0, higher = more interior)
    bool useWeights;            // Whether to use point weights in mesh generation
    
    // DensitySDF parameters (density-based SDF reconstruction)
    float densityVoxelSize;     // Size of voxels for density field (world units)
    float densitySmoothRadius;  // Gaussian smoothing radius for density field
    float densityIsoValue;      // Iso-value for surface extraction (0.5 = half max density)
    int densityBoxFeatures;     // Number of random box features for local pattern analysis
    float densitySignRadius;    // Radius for normal estimation (sign determination)
    
    AlgorithmParameters()
        : maxDistance(1.0f)
        , maxNeighbors(20)
        , projectionPlane(0)
        , delaunayThreshold(0.1f)
        , poissonRadius(0.1f)
        , poissonDepth(8)
        , poissonDensity(1.0f)
        , poissonSmoothNormals(true)
        , fctmSliceThickness(0.1f)
        , geodesicKNeighbors(10)
        , geodesicNumSeeds(0)
        , geodesicInteriorBias(0.5f)
        , useWeights(false)
        , densityVoxelSize(2.0f)
        , densitySmoothRadius(0.0f)  // 0 = match training ratio (splatSigma/voxelSize = 1.5)
        , densityIsoValue(0.5f)
        , densityBoxFeatures(32)
        , densitySignRadius(4.0f)
    {}
};

// Remeshing result
struct RemeshResult {
    std::vector<MeshVertex> vertices;
    std::vector<unsigned int> indices;
    size_t triangleCount;
    size_t vertexCount;
    size_t initialClusterCount;  // For FCTM: number of initial clusters computed
    
    RemeshResult() : triangleCount(0), vertexCount(0), initialClusterCount(0) {}
};

// Remeshing class
class Remeshing {
public:
    Remeshing();
    ~Remeshing();
    
    RemeshResult GenerateMesh(
        const std::vector<PointCloudPoint>& pointCloud,
        const std::map<GridKey, PointCloudPoint>& pointCloudGrid,
        bool useGridding,
        float gridSize,
        Algorithm algorithm,
        const AlgorithmParameters& params
    );
    
private:
    RemeshResult GenerateGriddedQuads(
        const std::map<GridKey, PointCloudPoint>& pointCloudGrid,
        float gridSize
    );
    
    RemeshResult GenerateNearestNeighbor(
        const std::vector<PointCloudPoint>& pointCloud,
        float maxDistance
    );
    
    RemeshResult GenerateDelaunay2D(
        const std::vector<PointCloudPoint>& pointCloud,
        const AlgorithmParameters& params
    );
    
    RemeshResult GeneratePoissonLike(
        const std::vector<PointCloudPoint>& pointCloud,
        const AlgorithmParameters& params
    );
    
    RemeshResult GenerateFCTM(
        const std::vector<PointCloudPoint>& pointCloud,
        const AlgorithmParameters& params
    );
    
    RemeshResult GenerateGeodesicTree(
        const std::vector<PointCloudPoint>& pointCloud,
        const AlgorithmParameters& params
    );
    
    RemeshResult GenerateGTree(
        const std::vector<PointCloudPoint>& pointCloud,
        const AlgorithmParameters& params
    );
    
    RemeshResult GenerateDensitySDF(
        const std::vector<PointCloudPoint>& pointCloud,
        const AlgorithmParameters& params
    );
    
    // Helper function: Tube-following algorithm
    std::vector<glm::vec3> FollowTube(
        const glm::vec3& origin,
        const std::vector<glm::vec3>& clusterPoints,
        float stepSize,
        float tubeRadius,
        int maxNodes
    );
    
    // Helper function: Plane-based path following
    std::vector<glm::vec3> FollowPlanePath(
        const glm::vec3& startPoint,
        const std::vector<glm::vec3>& clusterPoints,
        int minPointsInSphere,
        float stepSize
    );
    
    // Helper functions for FCTM
    void ComputePCAAxes(const std::vector<PointCloudPoint>& pointCloud, glm::vec3& axis1, glm::vec3& axis2, glm::vec3& axis3) const;
    glm::vec3 EstimateNormalForPoint(size_t pointIndex, const std::vector<PointCloudPoint>& pointCloud, float radius) const;
    
    std::vector<Point2D> ProjectToPlane(
        const std::vector<PointCloudPoint>& pointCloud,
        int planeAxis
    ) const;
    
    void FindNeighbors(
        size_t pointIndex,
        const std::vector<PointCloudPoint>& pointCloud,
        float maxDistance,
        std::vector<std::pair<float, size_t>>& neighbors
    ) const;
    
    bool IsPointInCircumcircle(
        const Point2D& a,
        const Point2D& b,
        const Point2D& c,
        const Point2D& p
    ) const;
    
    glm::vec3 EstimateNormal(
        size_t pointIndex,
        const std::vector<PointCloudPoint>& pointCloud,
        float radius
    ) const;
    
    void BuildOctree(
        const std::vector<PointCloudPoint>& pointCloud,
        float cellSize
    ) const;
    
    bool IsValidTriangle(
        const glm::vec3& a,
        const glm::vec3& b,
        const glm::vec3& c,
        float minArea = 0.0001f
    ) const;
    
    void AddTriangleIfValid(
        std::vector<unsigned int>& indices,
        std::set<std::tuple<unsigned int, unsigned int, unsigned int>>& triangleSet,
        unsigned int i,
        unsigned int j,
        unsigned int k,
        const std::vector<MeshVertex>& vertices
    ) const;
    
    void CalculateNormals(
        std::vector<MeshVertex>& vertices,
        const std::vector<unsigned int>& indices
    );
};

} // namespace CameraSystem
} // namespace Editors
} // namespace Engine

