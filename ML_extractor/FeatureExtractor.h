#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <random>

namespace MLExtractor {

// A triangle in the mesh (for closest point queries)
struct Triangle {
    glm::vec3 v0, v1, v2;
    glm::vec3 normal;
};

// Loaded mesh data
struct Mesh {
    std::vector<glm::vec3> vertices;
    std::vector<unsigned int> indices;
    std::vector<Triangle> triangles;
    glm::vec3 minBounds, maxBounds;
    
    void ComputeTriangles();
    void ComputeBounds();
    
    // Find closest point on mesh surface to query point
    glm::vec3 ClosestPoint(const glm::vec3& query) const;
    
    // Sample random points on the mesh surface
    std::vector<glm::vec3> SampleSurface(int numSamples, std::mt19937& rng) const;
};

// Box feature definition (random box around query point)
struct BoxFeature {
    glm::ivec3 offset;  // Offset from query point (in voxels)
    glm::ivec3 halfSize; // Half-extents of box (in voxels)
};

// 3D density field with integral volume for fast box queries
struct DensityField {
    std::vector<float> density;
    std::vector<float> integralVolume;
    glm::vec3 minBounds;
    float voxelSize;
    int dimX, dimY, dimZ;
    
    // Initialize from point cloud
    void Build(const std::vector<glm::vec3>& points, float voxelSize, float splatSigma);
    
    // Build integral volume for O(1) box queries
    void BuildIntegralVolume();
    
    // Fast O(1) box-sum query
    float BoxSum(int x0, int y0, int z0, int x1, int y1, int z1) const;
    
    // Convert between world and voxel coordinates
    glm::ivec3 WorldToVoxel(const glm::vec3& world) const;
    glm::vec3 VoxelToWorld(int x, int y, int z) const;
    
    // Index helper
    int Index(int x, int y, int z) const {
        return x + y * dimX + z * dimX * dimY;
    }
    
    bool InBounds(int x, int y, int z) const {
        return x >= 0 && x < dimX && y >= 0 && y < dimY && z >= 0 && z < dimZ;
    }
};

// Training sample: input features + ground truth output
struct TrainingSample {
    std::vector<float> features;  // Φ(x) - box-sum features
    glm::vec3 delta;              // Δ_S(x) = x - π_S(x) - ground truth
    glm::vec3 queryPoint;         // Original query point (for debugging)
};

// Feature extractor configuration
struct ExtractorConfig {
    int numBoxFeatures = 2000;      // Number of random box features
    int maxBoxOffset = 10;          // Max offset from query point (voxels)
    int maxBoxHalfSize = 8;         // Max half-size of boxes (voxels)
    float voxelSize = 0.02f;        // Voxel size in world units
    float splatSigma = 0.03f;       // Gaussian splat sigma
    float sampleRadius = 0.1f;      // How far from surface to sample query points
    int samplesPerMesh = 50000;     // Number of training samples per mesh
    unsigned int randomSeed = 42;   // For reproducibility
};

// Point cloud with optional normals (for ShapeNet formats)
struct PointCloud {
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<int> labels;  // Part labels
    glm::vec3 minBounds, maxBounds;
    
    void ComputeBounds();
    bool HasNormals() const { return !normals.empty() && normals.size() == positions.size(); }
    bool HasLabels() const { return !labels.empty() && labels.size() == positions.size(); }
};

// Main feature extractor class
class FeatureExtractor {
public:
    FeatureExtractor(const ExtractorConfig& config);
    
    // Load an OBJ file and return mesh
    bool LoadOBJ(const std::string& filepath, Mesh& outMesh);
    
    // Load ShapeNet point cloud formats
    bool LoadShapeNetTXT(const std::string& filepath, PointCloud& outCloud);
    bool LoadShapeNetNPZ(const std::string& filepath, PointCloud& outCloud);
    
    // Auto-detect and load any supported format
    bool LoadAnyFormat(const std::string& filepath, PointCloud& outCloud, Mesh* outMesh = nullptr);
    
    // Generate training samples from a mesh
    std::vector<TrainingSample> ExtractFeatures(const Mesh& mesh);
    
    // Generate training samples from a point cloud (uses point-to-point queries)
    std::vector<TrainingSample> ExtractFeaturesFromPointCloud(const PointCloud& cloud);
    
    // Save training samples to file (binary format)
    bool SaveTrainingData(const std::string& filepath, 
                          const std::vector<TrainingSample>& samples);
    
    // Save training samples to CSV (for debugging/visualization)
    bool SaveTrainingDataCSV(const std::string& filepath,
                             const std::vector<TrainingSample>& samples);
    
    // Get the box features (for saving/loading the feature definition)
    const std::vector<BoxFeature>& GetBoxFeatures() const { return m_boxFeatures; }
    
    // Save box feature definitions
    bool SaveBoxFeatures(const std::string& filepath);
    
private:
    ExtractorConfig m_config;
    std::vector<BoxFeature> m_boxFeatures;
    std::mt19937 m_rng;
    
    // Generate random box features
    void GenerateBoxFeatures();
    
    // Extract feature vector for a single query point
    std::vector<float> ExtractFeaturesAtPoint(const DensityField& field, 
                                               const glm::ivec3& voxelCoord);
};

// Utility: process all supported files in a directory (OBJ, TXT, NPZ)
bool ProcessDirectory(const std::string& inputDir, 
                      const std::string& outputDir,
                      const ExtractorConfig& config);

// Process ShapeNet-style directory structure (category/instance)
bool ProcessShapeNetDirectory(const std::string& inputDir,
                              const std::string& outputDir,
                              const ExtractorConfig& config,
                              int maxFilesPerCategory = -1);

} // namespace MLExtractor
