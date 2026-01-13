#include "FeatureExtractor.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <cstring>
#include <set>
#include <map>
#include <tuple>
#include <cstdlib>

namespace fs = std::filesystem;

// Simple NPZ parsing (minimal implementation for ShapeNet format)
namespace {

// Read a little-endian value from a buffer
template<typename T>
T readLE(const char* data) {
    T result;
    std::memcpy(&result, data, sizeof(T));
    return result;
}

// Parse NumPy dtype string to get element size and type
struct DType {
    char type;  // 'f' = float, 'i' = int
    int size;   // 4 = float32/int32
    bool littleEndian;
};

DType parseDType(const std::string& descr) {
    DType dt = {'f', 4, true};
    if (descr.size() >= 3) {
        dt.littleEndian = (descr[0] == '<');
        dt.type = descr[1];
        dt.size = std::stoi(descr.substr(2));
    }
    return dt;
}

// Parse NPY header to get shape and dtype
bool parseNpyHeader(const char* header, size_t headerLen, 
                    std::vector<size_t>& shape, DType& dtype) {
    std::string headerStr(header, headerLen);
    
    // Find 'descr': '<f4' or similar
    size_t descrPos = headerStr.find("'descr':");
    if (descrPos != std::string::npos) {
        size_t start = headerStr.find("'", descrPos + 8);
        size_t end = headerStr.find("'", start + 1);
        if (start != std::string::npos && end != std::string::npos) {
            dtype = parseDType(headerStr.substr(start + 1, end - start - 1));
        }
    }
    
    // Find 'shape': (N, M) or (N,)
    size_t shapePos = headerStr.find("'shape':");
    if (shapePos != std::string::npos) {
        size_t start = headerStr.find("(", shapePos);
        size_t end = headerStr.find(")", start);
        if (start != std::string::npos && end != std::string::npos) {
            std::string shapeStr = headerStr.substr(start + 1, end - start - 1);
            std::stringstream ss(shapeStr);
            std::string token;
            while (std::getline(ss, token, ',')) {
                if (!token.empty()) {
                    // Trim whitespace
                    size_t first = token.find_first_not_of(" \t");
                    size_t last = token.find_last_not_of(" \t,");
                    if (first != std::string::npos && last != std::string::npos) {
                        int dim = std::stoi(token.substr(first, last - first + 1));
                        if (dim > 0) shape.push_back(dim);
                    }
                }
            }
        }
    }
    
    return !shape.empty();
}

// Read a single array from NPZ data at given offset
bool readNpyArray(const std::vector<char>& data, size_t offset, size_t compressedSize,
                  std::vector<float>& outData, std::vector<size_t>& outShape) {
    if (offset + compressedSize > data.size()) return false;
    
    // NPY format: magic string, version, header len, header, data
    const char* ptr = data.data() + offset;
    
    // Check magic: \x93NUMPY
    if (ptr[0] != '\x93' || ptr[1] != 'N' || ptr[2] != 'U' || 
        ptr[3] != 'M' || ptr[4] != 'P' || ptr[5] != 'Y') {
        return false;
    }
    
    uint8_t major = ptr[6];
    uint8_t minor = ptr[7];
    (void)minor;
    
    size_t headerOffset = 8;
    uint32_t headerLen;
    if (major >= 2) {
        headerLen = readLE<uint32_t>(ptr + 8);
        headerOffset = 12;
    } else {
        headerLen = readLE<uint16_t>(ptr + 8);
        headerOffset = 10;
    }
    
    DType dtype;
    if (!parseNpyHeader(ptr + headerOffset, headerLen, outShape, dtype)) {
        return false;
    }
    
    // Calculate total elements
    size_t totalElements = 1;
    for (size_t dim : outShape) {
        totalElements *= dim;
    }
    
    const char* dataPtr = ptr + headerOffset + headerLen;
    outData.resize(totalElements);
    
    if (dtype.type == 'f' && dtype.size == 4) {
        // float32
        std::memcpy(outData.data(), dataPtr, totalElements * sizeof(float));
    } else if (dtype.type == 'f' && dtype.size == 8) {
        // float64 -> float32
        const double* src = reinterpret_cast<const double*>(dataPtr);
        for (size_t i = 0; i < totalElements; i++) {
            outData[i] = static_cast<float>(src[i]);
        }
    } else if (dtype.type == 'i' && dtype.size == 4) {
        // int32 -> float32
        const int32_t* src = reinterpret_cast<const int32_t*>(dataPtr);
        for (size_t i = 0; i < totalElements; i++) {
            outData[i] = static_cast<float>(src[i]);
        }
    } else {
        std::cerr << "[NPZ] Unsupported dtype: " << dtype.type << dtype.size << "\n";
        return false;
    }
    
    return true;
}

} // anonymous namespace

namespace MLExtractor {

// ============================================================================
// PointCloud Implementation  
// ============================================================================

void PointCloud::ComputeBounds() {
    minBounds = glm::vec3(std::numeric_limits<float>::max());
    maxBounds = glm::vec3(std::numeric_limits<float>::lowest());
    
    for (const auto& p : positions) {
        minBounds = glm::min(minBounds, p);
        maxBounds = glm::max(maxBounds, p);
    }
}

// ============================================================================
// Mesh Implementation
// ============================================================================

void Mesh::ComputeTriangles() {
    triangles.clear();
    triangles.reserve(indices.size() / 3);
    
    for (size_t i = 0; i + 2 < indices.size(); i += 3) {
        Triangle tri;
        tri.v0 = vertices[indices[i]];
        tri.v1 = vertices[indices[i + 1]];
        tri.v2 = vertices[indices[i + 2]];
        
        glm::vec3 e1 = tri.v1 - tri.v0;
        glm::vec3 e2 = tri.v2 - tri.v0;
        tri.normal = glm::normalize(glm::cross(e1, e2));
        
        triangles.push_back(tri);
    }
}

void Mesh::ComputeBounds() {
    minBounds = glm::vec3(std::numeric_limits<float>::max());
    maxBounds = glm::vec3(std::numeric_limits<float>::lowest());
    
    for (const auto& v : vertices) {
        minBounds = glm::min(minBounds, v);
        maxBounds = glm::max(maxBounds, v);
    }
}

// Find closest point on a triangle to a query point
static glm::vec3 ClosestPointOnTriangle(const glm::vec3& p, const Triangle& tri) {
    glm::vec3 a = tri.v0;
    glm::vec3 b = tri.v1;
    glm::vec3 c = tri.v2;
    
    // Check if P in vertex region outside A
    glm::vec3 ab = b - a;
    glm::vec3 ac = c - a;
    glm::vec3 ap = p - a;
    float d1 = glm::dot(ab, ap);
    float d2 = glm::dot(ac, ap);
    if (d1 <= 0.0f && d2 <= 0.0f) return a;
    
    // Check if P in vertex region outside B
    glm::vec3 bp = p - b;
    float d3 = glm::dot(ab, bp);
    float d4 = glm::dot(ac, bp);
    if (d3 >= 0.0f && d4 <= d3) return b;
    
    // Check if P in edge region of AB
    float vc = d1 * d4 - d3 * d2;
    if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f) {
        float v = d1 / (d1 - d3);
        return a + v * ab;
    }
    
    // Check if P in vertex region outside C
    glm::vec3 cp = p - c;
    float d5 = glm::dot(ab, cp);
    float d6 = glm::dot(ac, cp);
    if (d6 >= 0.0f && d5 <= d6) return c;
    
    // Check if P in edge region of AC
    float vb = d5 * d2 - d1 * d6;
    if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f) {
        float w = d2 / (d2 - d6);
        return a + w * ac;
    }
    
    // Check if P in edge region of BC
    float va = d3 * d6 - d5 * d4;
    if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f) {
        float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
        return b + w * (c - b);
    }
    
    // P inside face region
    float denom = 1.0f / (va + vb + vc);
    float v = vb * denom;
    float w = vc * denom;
    return a + ab * v + ac * w;
}

glm::vec3 Mesh::ClosestPoint(const glm::vec3& query) const {
    glm::vec3 closest = vertices.empty() ? query : vertices[0];
    float minDist2 = std::numeric_limits<float>::max();
    
    for (const auto& tri : triangles) {
        glm::vec3 cp = ClosestPointOnTriangle(query, tri);
        float dist2 = glm::dot(query - cp, query - cp);
        if (dist2 < minDist2) {
            minDist2 = dist2;
            closest = cp;
        }
    }
    
    return closest;
}

std::vector<glm::vec3> Mesh::SampleSurface(int numSamples, std::mt19937& rng) const {
    std::vector<glm::vec3> samples;
    samples.reserve(numSamples);
    
    if (triangles.empty()) return samples;
    
    // Compute triangle areas for weighted sampling
    std::vector<float> areas(triangles.size());
    float totalArea = 0.0f;
    
    for (size_t i = 0; i < triangles.size(); i++) {
        const auto& tri = triangles[i];
        glm::vec3 e1 = tri.v1 - tri.v0;
        glm::vec3 e2 = tri.v2 - tri.v0;
        areas[i] = 0.5f * glm::length(glm::cross(e1, e2));
        totalArea += areas[i];
    }
    
    // Build CDF for weighted sampling
    std::vector<float> cdf(triangles.size());
    float cumulative = 0.0f;
    for (size_t i = 0; i < triangles.size(); i++) {
        cumulative += areas[i] / totalArea;
        cdf[i] = cumulative;
    }
    
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    
    for (int i = 0; i < numSamples; i++) {
        // Pick a triangle weighted by area
        float r = dist(rng);
        size_t triIdx = std::lower_bound(cdf.begin(), cdf.end(), r) - cdf.begin();
        triIdx = std::min(triIdx, triangles.size() - 1);
        
        const auto& tri = triangles[triIdx];
        
        // Random point on triangle using barycentric coordinates
        float u = dist(rng);
        float v = dist(rng);
        if (u + v > 1.0f) {
            u = 1.0f - u;
            v = 1.0f - v;
        }
        
        glm::vec3 point = tri.v0 + u * (tri.v1 - tri.v0) + v * (tri.v2 - tri.v0);
        samples.push_back(point);
    }
    
    return samples;
}

// ============================================================================
// DensityField Implementation
// ============================================================================

void DensityField::Build(const std::vector<glm::vec3>& points, float voxSize, float splatSigma) {
    if (points.empty()) return;
    
    voxelSize = voxSize;
    
    // Compute bounds
    glm::vec3 minP(std::numeric_limits<float>::max());
    glm::vec3 maxP(std::numeric_limits<float>::lowest());
    for (const auto& p : points) {
        minP = glm::min(minP, p);
        maxP = glm::max(maxP, p);
    }
    
    // Add margin
    float margin = splatSigma * 4.0f;
    minBounds = minP - glm::vec3(margin);
    glm::vec3 maxBounds = maxP + glm::vec3(margin);
    
    // Compute grid dimensions
    dimX = static_cast<int>(std::ceil((maxBounds.x - minBounds.x) / voxelSize)) + 1;
    dimY = static_cast<int>(std::ceil((maxBounds.y - minBounds.y) / voxelSize)) + 1;
    dimZ = static_cast<int>(std::ceil((maxBounds.z - minBounds.z) / voxelSize)) + 1;
    
    // Limit grid size
    const int maxDim = 150;
    if (dimX > maxDim || dimY > maxDim || dimZ > maxDim) {
        float maxExtent = std::max({maxBounds.x - minBounds.x, 
                                    maxBounds.y - minBounds.y, 
                                    maxBounds.z - minBounds.z});
        voxelSize = maxExtent / (maxDim - 10);
        dimX = static_cast<int>(std::ceil((maxBounds.x - minBounds.x) / voxelSize)) + 1;
        dimY = static_cast<int>(std::ceil((maxBounds.y - minBounds.y) / voxelSize)) + 1;
        dimZ = static_cast<int>(std::ceil((maxBounds.z - minBounds.z) / voxelSize)) + 1;
    }
    
    size_t totalVoxels = static_cast<size_t>(dimX) * dimY * dimZ;
    density.resize(totalVoxels, 0.0f);
    
    // Gaussian splatting
    float sigmaSq2 = 2.0f * splatSigma * splatSigma;
    int splatRadius = static_cast<int>(std::ceil(3.0f * splatSigma / voxelSize));
    
    for (const auto& p : points) {
        glm::ivec3 center = WorldToVoxel(p);
        
        for (int dz = -splatRadius; dz <= splatRadius; dz++) {
            for (int dy = -splatRadius; dy <= splatRadius; dy++) {
                for (int dx = -splatRadius; dx <= splatRadius; dx++) {
                    int nx = center.x + dx;
                    int ny = center.y + dy;
                    int nz = center.z + dz;
                    
                    if (!InBounds(nx, ny, nz)) continue;
                    
                    glm::vec3 voxelPos = VoxelToWorld(nx, ny, nz);
                    float dist2 = glm::dot(p - voxelPos, p - voxelPos);
                    float weight = std::exp(-dist2 / sigmaSq2);
                    
                    density[Index(nx, ny, nz)] += weight;
                }
            }
        }
    }
    
    // Build integral volume
    BuildIntegralVolume();
}

void DensityField::BuildIntegralVolume() {
    size_t totalVoxels = static_cast<size_t>(dimX) * dimY * dimZ;
    integralVolume.resize(totalVoxels, 0.0f);
    
    for (int z = 0; z < dimZ; z++) {
        for (int y = 0; y < dimY; y++) {
            for (int x = 0; x < dimX; x++) {
                float val = density[Index(x, y, z)];
                
                if (x > 0) val += integralVolume[Index(x-1, y, z)];
                if (y > 0) val += integralVolume[Index(x, y-1, z)];
                if (z > 0) val += integralVolume[Index(x, y, z-1)];
                
                if (x > 0 && y > 0) val -= integralVolume[Index(x-1, y-1, z)];
                if (x > 0 && z > 0) val -= integralVolume[Index(x-1, y, z-1)];
                if (y > 0 && z > 0) val -= integralVolume[Index(x, y-1, z-1)];
                
                if (x > 0 && y > 0 && z > 0) val += integralVolume[Index(x-1, y-1, z-1)];
                
                integralVolume[Index(x, y, z)] = val;
            }
        }
    }
}

float DensityField::BoxSum(int x0, int y0, int z0, int x1, int y1, int z1) const {
    // Clamp to valid range
    x0 = std::max(0, std::min(x0, dimX - 1));
    y0 = std::max(0, std::min(y0, dimY - 1));
    z0 = std::max(0, std::min(z0, dimZ - 1));
    x1 = std::max(0, std::min(x1, dimX - 1));
    y1 = std::max(0, std::min(y1, dimY - 1));
    z1 = std::max(0, std::min(z1, dimZ - 1));
    
    float sum = integralVolume[Index(x1, y1, z1)];
    
    if (x0 > 0) sum -= integralVolume[Index(x0-1, y1, z1)];
    if (y0 > 0) sum -= integralVolume[Index(x1, y0-1, z1)];
    if (z0 > 0) sum -= integralVolume[Index(x1, y1, z0-1)];
    
    if (x0 > 0 && y0 > 0) sum += integralVolume[Index(x0-1, y0-1, z1)];
    if (x0 > 0 && z0 > 0) sum += integralVolume[Index(x0-1, y1, z0-1)];
    if (y0 > 0 && z0 > 0) sum += integralVolume[Index(x1, y0-1, z0-1)];
    
    if (x0 > 0 && y0 > 0 && z0 > 0) sum -= integralVolume[Index(x0-1, y0-1, z0-1)];
    
    return sum;
}

glm::ivec3 DensityField::WorldToVoxel(const glm::vec3& world) const {
    glm::vec3 local = (world - minBounds) / voxelSize;
    return glm::ivec3(
        static_cast<int>(std::round(local.x)),
        static_cast<int>(std::round(local.y)),
        static_cast<int>(std::round(local.z))
    );
}

glm::vec3 DensityField::VoxelToWorld(int x, int y, int z) const {
    return minBounds + glm::vec3(x, y, z) * voxelSize;
}

// ============================================================================
// FeatureExtractor Implementation
// ============================================================================

FeatureExtractor::FeatureExtractor(const ExtractorConfig& config)
    : m_config(config)
    , m_rng(config.randomSeed)
{
    GenerateBoxFeatures();
}

void FeatureExtractor::GenerateBoxFeatures() {
    m_boxFeatures.clear();
    m_boxFeatures.reserve(m_config.numBoxFeatures);
    
    std::uniform_int_distribution<int> offsetDist(-m_config.maxBoxOffset, m_config.maxBoxOffset);
    std::uniform_int_distribution<int> sizeDist(1, m_config.maxBoxHalfSize);
    
    for (int i = 0; i < m_config.numBoxFeatures; i++) {
        BoxFeature box;
        box.offset = glm::ivec3(offsetDist(m_rng), offsetDist(m_rng), offsetDist(m_rng));
        box.halfSize = glm::ivec3(sizeDist(m_rng), sizeDist(m_rng), sizeDist(m_rng));
        m_boxFeatures.push_back(box);
    }
    
    std::cout << "[FeatureExtractor] Generated " << m_boxFeatures.size() << " random box features\n";
}

bool FeatureExtractor::LoadOBJ(const std::string& filepath, Mesh& outMesh) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "[FeatureExtractor] Failed to open: " << filepath << "\n";
        return false;
    }
    
    outMesh.vertices.clear();
    outMesh.indices.clear();
    
    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string prefix;
        iss >> prefix;
        
        if (prefix == "v") {
            // Vertex
            float x, y, z;
            iss >> x >> y >> z;
            outMesh.vertices.push_back(glm::vec3(x, y, z));
        } else if (prefix == "f") {
            // Face (handle v, v/vt, v/vt/vn, v//vn formats)
            std::vector<unsigned int> faceIndices;
            std::string token;
            while (iss >> token) {
                // Parse first number before any /
                size_t slashPos = token.find('/');
                std::string indexStr = (slashPos != std::string::npos) 
                    ? token.substr(0, slashPos) 
                    : token;
                
                if (!indexStr.empty()) {
                    int idx = std::stoi(indexStr);
                    // OBJ indices are 1-based
                    if (idx > 0) {
                        faceIndices.push_back(static_cast<unsigned int>(idx - 1));
                    } else {
                        // Negative indices (relative)
                        faceIndices.push_back(static_cast<unsigned int>(outMesh.vertices.size() + idx));
                    }
                }
            }
            
            // Triangulate if more than 3 vertices (fan triangulation)
            for (size_t i = 1; i + 1 < faceIndices.size(); i++) {
                outMesh.indices.push_back(faceIndices[0]);
                outMesh.indices.push_back(faceIndices[i]);
                outMesh.indices.push_back(faceIndices[i + 1]);
            }
        }
    }
    
    file.close();
    
    if (outMesh.vertices.empty() || outMesh.indices.empty()) {
        std::cerr << "[FeatureExtractor] No geometry found in: " << filepath << "\n";
        return false;
    }
    
    outMesh.ComputeTriangles();
    outMesh.ComputeBounds();
    
    std::cout << "[FeatureExtractor] Loaded " << filepath << ": " 
              << outMesh.vertices.size() << " vertices, "
              << outMesh.triangles.size() << " triangles\n";
    
    return true;
}

bool FeatureExtractor::LoadShapeNetTXT(const std::string& filepath, PointCloud& outCloud) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "[FeatureExtractor] Failed to open: " << filepath << "\n";
        return false;
    }
    
    outCloud.positions.clear();
    outCloud.normals.clear();
    outCloud.labels.clear();
    
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        
        std::istringstream iss(line);
        float x, y, z, nx, ny, nz, label;
        
        // Format: x y z nx ny nz label
        if (iss >> x >> y >> z >> nx >> ny >> nz >> label) {
            outCloud.positions.push_back(glm::vec3(x, y, z));
            outCloud.normals.push_back(glm::normalize(glm::vec3(nx, ny, nz)));
            outCloud.labels.push_back(static_cast<int>(label));
        } else {
            // Try simpler format: x y z
            std::istringstream iss2(line);
            if (iss2 >> x >> y >> z) {
                outCloud.positions.push_back(glm::vec3(x, y, z));
            }
        }
    }
    
    file.close();
    
    if (outCloud.positions.empty()) {
        std::cerr << "[FeatureExtractor] No points found in: " << filepath << "\n";
        return false;
    }
    
    outCloud.ComputeBounds();
    
    std::cout << "[FeatureExtractor] Loaded ShapeNet TXT " << filepath << ": " 
              << outCloud.positions.size() << " points";
    if (outCloud.HasNormals()) std::cout << " (with normals)";
    if (outCloud.HasLabels()) std::cout << " (with labels)";
    std::cout << "\n";
    
    return true;
}

bool FeatureExtractor::LoadShapeNetNPZ(const std::string& filepath, PointCloud& outCloud) {
    // NPZ is a ZIP archive containing .npy files
    // We'll use a simple approach: shell out to Python for extraction
    // This is more reliable than implementing full ZIP+NPY parsing
    
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "[FeatureExtractor] Failed to open: " << filepath << "\n";
        return false;
    }
    
    // Read entire file
    file.seekg(0, std::ios::end);
    size_t fileSize = file.tellg();
    file.seekg(0, std::ios::beg);
    
    std::vector<char> fileData(fileSize);
    file.read(fileData.data(), fileSize);
    file.close();
    
    // Check for ZIP magic (NPZ is just a ZIP file)
    if (fileSize < 4 || fileData[0] != 'P' || fileData[1] != 'K') {
        // Try as raw NPY
        std::vector<float> data;
        std::vector<size_t> shape;
        if (readNpyArray(fileData, 0, fileSize, data, shape)) {
            if (shape.size() == 2 && shape[1] >= 3) {
                size_t numPoints = shape[0];
                outCloud.positions.resize(numPoints);
                for (size_t i = 0; i < numPoints; i++) {
                    outCloud.positions[i] = glm::vec3(
                        data[i * shape[1] + 0],
                        data[i * shape[1] + 1],
                        data[i * shape[1] + 2]
                    );
                }
                outCloud.ComputeBounds();
                std::cout << "[FeatureExtractor] Loaded NPY " << filepath << ": "
                          << numPoints << " points\n";
                return true;
            }
        }
        std::cerr << "[FeatureExtractor] Not a valid NPZ/NPY file: " << filepath << "\n";
        return false;
    }
    
    // For NPZ, we'll use Python to extract and save as text
    // Create a temporary script
    std::string tempDir = fs::temp_directory_path().string();
    std::string tempTxt = tempDir + "/npz_extract_" + std::to_string(rand()) + ".txt";
    
    std::stringstream cmd;
    cmd << "python3 -c \""
        << "import numpy as np; "
        << "d = np.load('" << filepath << "'); "
        << "pc = d['pc'] if 'pc' in d else (d['points'] if 'points' in d else None); "
        << "sn = d['sn'] if 'sn' in d else (d['normals'] if 'normals' in d else None); "
        << "labels = d['part_label'] if 'part_label' in d else None; "
        << "f = open('" << tempTxt << "', 'w'); "
        << "if pc is not None: "
        << "  for i in range(len(pc)): "
        << "    line = ' '.join(map(str, pc[i])); "
        << "    if sn is not None: line += ' ' + ' '.join(map(str, sn[i])); "
        << "    if labels is not None: line += ' ' + str(labels[i]); "
        << "    f.write(line + '\\n'); "
        << "f.close()"
        << "\" 2>/dev/null";
    
    int ret = std::system(cmd.str().c_str());
    if (ret != 0) {
        std::cerr << "[FeatureExtractor] Python extraction failed for: " << filepath << "\n";
        return false;
    }
    
    // Load the extracted text file
    bool success = LoadShapeNetTXT(tempTxt, outCloud);
    
    // Clean up
    fs::remove(tempTxt);
    
    if (success) {
        std::cout << "[FeatureExtractor] Loaded ShapeNet NPZ " << filepath << ": " 
                  << outCloud.positions.size() << " points\n";
    }
    
    return success;
}

bool FeatureExtractor::LoadAnyFormat(const std::string& filepath, PointCloud& outCloud, Mesh* outMesh) {
    std::string ext = fs::path(filepath).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
    if (ext == ".obj") {
        Mesh mesh;
        if (!LoadOBJ(filepath, mesh)) {
            return false;
        }
        
        // Convert mesh to point cloud (sample surface)
        outCloud.positions = mesh.SampleSurface(
            std::min(static_cast<int>(mesh.vertices.size()) * 10, m_config.samplesPerMesh * 2), 
            m_rng
        );
        
        // Estimate normals from triangles
        outCloud.normals.resize(outCloud.positions.size());
        for (size_t i = 0; i < outCloud.positions.size(); i++) {
            // Find closest triangle for normal
            const glm::vec3& p = outCloud.positions[i];
            float minDist2 = std::numeric_limits<float>::max();
            glm::vec3 bestNormal(0, 1, 0);
            
            for (const auto& tri : mesh.triangles) {
                glm::vec3 center = (tri.v0 + tri.v1 + tri.v2) / 3.0f;
                float dist2 = glm::dot(p - center, p - center);
                if (dist2 < minDist2) {
                    minDist2 = dist2;
                    bestNormal = tri.normal;
                }
            }
            outCloud.normals[i] = bestNormal;
        }
        
        outCloud.ComputeBounds();
        
        if (outMesh) {
            *outMesh = std::move(mesh);
        }
        
        return true;
    }
    else if (ext == ".txt") {
        return LoadShapeNetTXT(filepath, outCloud);
    }
    else if (ext == ".npz" || ext == ".npy") {
        return LoadShapeNetNPZ(filepath, outCloud);
    }
    
    std::cerr << "[FeatureExtractor] Unsupported format: " << ext << "\n";
    return false;
}

std::vector<TrainingSample> FeatureExtractor::ExtractFeaturesFromPointCloud(const PointCloud& cloud) {
    std::vector<TrainingSample> samples;
    
    if (cloud.positions.empty()) {
        std::cerr << "[FeatureExtractor] Empty point cloud\n";
        return samples;
    }
    
    std::cout << "[FeatureExtractor] Building density field from " << cloud.positions.size() << " points\n";
    
    // Build density field from point cloud
    DensityField field;
    field.Build(cloud.positions, m_config.voxelSize, m_config.splatSigma);
    
    std::cout << "[FeatureExtractor] Density field: " << field.dimX << "x" << field.dimY << "x" << field.dimZ << "\n";
    
    // Build a simple spatial hash for nearest-neighbor queries
    float cellSize = m_config.sampleRadius * 2.0f;
    std::map<std::tuple<int,int,int>, std::vector<size_t>> spatialHash;
    
    for (size_t i = 0; i < cloud.positions.size(); i++) {
        const glm::vec3& p = cloud.positions[i];
        int cx = static_cast<int>(std::floor(p.x / cellSize));
        int cy = static_cast<int>(std::floor(p.y / cellSize));
        int cz = static_cast<int>(std::floor(p.z / cellSize));
        spatialHash[{cx, cy, cz}].push_back(i);
    }
    
    auto findClosestPoint = [&](const glm::vec3& query) -> glm::vec3 {
        int cx = static_cast<int>(std::floor(query.x / cellSize));
        int cy = static_cast<int>(std::floor(query.y / cellSize));
        int cz = static_cast<int>(std::floor(query.z / cellSize));
        
        float minDist2 = std::numeric_limits<float>::max();
        size_t closestIdx = 0;
        
        // Check nearby cells
        for (int dz = -1; dz <= 1; dz++) {
            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    auto it = spatialHash.find({cx + dx, cy + dy, cz + dz});
                    if (it != spatialHash.end()) {
                        for (size_t idx : it->second) {
                            float dist2 = glm::dot(query - cloud.positions[idx], query - cloud.positions[idx]);
                            if (dist2 < minDist2) {
                                minDist2 = dist2;
                                closestIdx = idx;
                            }
                        }
                    }
                }
            }
        }
        
        return cloud.positions[closestIdx];
    };
    
    // Generate query points
    std::uniform_real_distribution<float> offsetDist(-m_config.sampleRadius, m_config.sampleRadius);
    std::uniform_real_distribution<float> surfaceRatio(0.0f, 1.0f);
    std::uniform_int_distribution<size_t> pointDist(0, cloud.positions.size() - 1);
    
    samples.reserve(m_config.samplesPerMesh);
    
    for (int i = 0; i < m_config.samplesPerMesh; i++) {
        // Pick a random point from the cloud
        size_t baseIdx = pointDist(m_rng);
        glm::vec3 basePoint = cloud.positions[baseIdx];
        
        // Random offset from surface
        glm::vec3 offset(offsetDist(m_rng), offsetDist(m_rng), offsetDist(m_rng));
        
        // Some samples exactly on surface
        if (surfaceRatio(m_rng) < 0.3f) {
            offset = glm::vec3(0.0f);
        }
        
        glm::vec3 queryPoint = basePoint + offset;
        
        // Compute ground truth: closest point and delta
        glm::vec3 closestPoint = findClosestPoint(queryPoint);
        glm::vec3 delta = queryPoint - closestPoint;
        
        // Skip if too far
        if (glm::length(delta) > m_config.sampleRadius * 2.0f) {
            continue;
        }
        
        // Extract features
        glm::ivec3 voxelCoord = field.WorldToVoxel(queryPoint);
        if (!field.InBounds(voxelCoord.x, voxelCoord.y, voxelCoord.z)) {
            continue;
        }
        
        TrainingSample sample;
        sample.features = ExtractFeaturesAtPoint(field, voxelCoord);
        sample.delta = delta;
        sample.queryPoint = queryPoint;
        
        samples.push_back(sample);
        
        if (samples.size() % 10000 == 0) {
            std::cout << "[FeatureExtractor] Extracted " << samples.size() << " samples...\n";
        }
    }
    
    std::cout << "[FeatureExtractor] Extracted " << samples.size() << " training samples from point cloud\n";
    return samples;
}

std::vector<float> FeatureExtractor::ExtractFeaturesAtPoint(
    const DensityField& field, 
    const glm::ivec3& voxelCoord
) {
    std::vector<float> features(m_boxFeatures.size());
    
    for (size_t i = 0; i < m_boxFeatures.size(); i++) {
        const auto& box = m_boxFeatures[i];
        
        int x0 = voxelCoord.x + box.offset.x - box.halfSize.x;
        int y0 = voxelCoord.y + box.offset.y - box.halfSize.y;
        int z0 = voxelCoord.z + box.offset.z - box.halfSize.z;
        int x1 = voxelCoord.x + box.offset.x + box.halfSize.x;
        int y1 = voxelCoord.y + box.offset.y + box.halfSize.y;
        int z1 = voxelCoord.z + box.offset.z + box.halfSize.z;
        
        features[i] = field.BoxSum(x0, y0, z0, x1, y1, z1);
    }
    
    return features;
}

std::vector<TrainingSample> FeatureExtractor::ExtractFeatures(const Mesh& mesh) {
    std::vector<TrainingSample> samples;
    
    // Sample points on the mesh surface
    std::vector<glm::vec3> surfacePoints = mesh.SampleSurface(m_config.samplesPerMesh * 2, m_rng);
    
    if (surfacePoints.empty()) {
        std::cerr << "[FeatureExtractor] No surface points sampled\n";
        return samples;
    }
    
    std::cout << "[FeatureExtractor] Building density field from " << surfacePoints.size() << " surface points\n";
    
    // Build density field from surface points (simulating a point cloud)
    DensityField field;
    field.Build(surfacePoints, m_config.voxelSize, m_config.splatSigma);
    
    std::cout << "[FeatureExtractor] Density field: " << field.dimX << "x" << field.dimY << "x" << field.dimZ << "\n";
    
    // Generate query points: some on surface, some offset from surface
    std::uniform_real_distribution<float> offsetDist(-m_config.sampleRadius, m_config.sampleRadius);
    std::uniform_real_distribution<float> surfaceRatio(0.0f, 1.0f);
    
    samples.reserve(m_config.samplesPerMesh);
    
    for (int i = 0; i < m_config.samplesPerMesh && i < static_cast<int>(surfacePoints.size()); i++) {
        glm::vec3 surfacePoint = surfacePoints[i];
        
        // Random offset from surface
        glm::vec3 offset(offsetDist(m_rng), offsetDist(m_rng), offsetDist(m_rng));
        
        // Some samples exactly on surface, others offset
        if (surfaceRatio(m_rng) < 0.3f) {
            offset = glm::vec3(0.0f);  // On surface
        }
        
        glm::vec3 queryPoint = surfacePoint + offset;
        
        // Compute ground truth: closest point on mesh and delta
        glm::vec3 closestOnMesh = mesh.ClosestPoint(queryPoint);
        glm::vec3 delta = queryPoint - closestOnMesh;
        
        // Skip if too far from surface (not useful for training)
        if (glm::length(delta) > m_config.sampleRadius * 2.0f) {
            continue;
        }
        
        // Extract features
        glm::ivec3 voxelCoord = field.WorldToVoxel(queryPoint);
        if (!field.InBounds(voxelCoord.x, voxelCoord.y, voxelCoord.z)) {
            continue;
        }
        
        TrainingSample sample;
        sample.features = ExtractFeaturesAtPoint(field, voxelCoord);
        sample.delta = delta;
        sample.queryPoint = queryPoint;
        
        samples.push_back(sample);
        
        if (samples.size() % 10000 == 0) {
            std::cout << "[FeatureExtractor] Extracted " << samples.size() << " samples...\n";
        }
    }
    
    std::cout << "[FeatureExtractor] Extracted " << samples.size() << " training samples\n";
    return samples;
}

bool FeatureExtractor::SaveTrainingData(const std::string& filepath, 
                                         const std::vector<TrainingSample>& samples) {
    std::ofstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "[FeatureExtractor] Failed to create: " << filepath << "\n";
        return false;
    }
    
    // Header
    uint32_t numSamples = static_cast<uint32_t>(samples.size());
    uint32_t numFeatures = samples.empty() ? 0 : static_cast<uint32_t>(samples[0].features.size());
    
    file.write(reinterpret_cast<const char*>(&numSamples), sizeof(numSamples));
    file.write(reinterpret_cast<const char*>(&numFeatures), sizeof(numFeatures));
    
    // Write samples
    for (const auto& sample : samples) {
        // Features
        file.write(reinterpret_cast<const char*>(sample.features.data()), 
                   numFeatures * sizeof(float));
        // Delta (ground truth)
        file.write(reinterpret_cast<const char*>(&sample.delta), sizeof(glm::vec3));
    }
    
    file.close();
    std::cout << "[FeatureExtractor] Saved " << numSamples << " samples to " << filepath << "\n";
    return true;
}

bool FeatureExtractor::SaveTrainingDataCSV(const std::string& filepath,
                                            const std::vector<TrainingSample>& samples) {
    std::ofstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "[FeatureExtractor] Failed to create: " << filepath << "\n";
        return false;
    }
    
    // Header
    file << "delta_x,delta_y,delta_z,distance";
    size_t numFeatures = samples.empty() ? 0 : samples[0].features.size();
    for (size_t i = 0; i < numFeatures; i++) {
        file << ",f" << i;
    }
    file << "\n";
    
    // Data (limit to first 10000 for CSV)
    size_t maxRows = std::min(samples.size(), size_t(10000));
    for (size_t i = 0; i < maxRows; i++) {
        const auto& sample = samples[i];
        float distance = glm::length(sample.delta);
        
        file << sample.delta.x << "," << sample.delta.y << "," << sample.delta.z << "," << distance;
        for (float f : sample.features) {
            file << "," << f;
        }
        file << "\n";
    }
    
    file.close();
    std::cout << "[FeatureExtractor] Saved CSV (first " << maxRows << " samples) to " << filepath << "\n";
    return true;
}

bool FeatureExtractor::SaveBoxFeatures(const std::string& filepath) {
    std::ofstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "[FeatureExtractor] Failed to create: " << filepath << "\n";
        return false;
    }
    
    file << "# Box features: offset_x,offset_y,offset_z,halfsize_x,halfsize_y,halfsize_z\n";
    file << "# Total: " << m_boxFeatures.size() << " features\n";
    
    for (const auto& box : m_boxFeatures) {
        file << box.offset.x << "," << box.offset.y << "," << box.offset.z << ","
             << box.halfSize.x << "," << box.halfSize.y << "," << box.halfSize.z << "\n";
    }
    
    file.close();
    std::cout << "[FeatureExtractor] Saved box features to " << filepath << "\n";
    return true;
}

// ============================================================================
// Directory Processing
// ============================================================================

bool ProcessDirectory(const std::string& inputDir, 
                      const std::string& outputDir,
                      const ExtractorConfig& config) {
    
    // Create output directory
    fs::create_directories(outputDir);
    
    FeatureExtractor extractor(config);
    
    // Save box feature definitions
    extractor.SaveBoxFeatures(outputDir + "/box_features.csv");
    
    std::vector<TrainingSample> allSamples;
    int fileCount = 0;
    
    // Supported extensions
    std::set<std::string> supportedExts = {".obj", ".txt", ".npz", ".npy"};
    
    // Process all supported files
    for (const auto& entry : fs::directory_iterator(inputDir)) {
        if (!entry.is_regular_file()) continue;
        
        std::string ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        
        if (supportedExts.find(ext) == supportedExts.end()) continue;
        
        // Skip NPZ files with special suffixes (SOM data)
        std::string stem = entry.path().stem().string();
        if (stem.find("_8x8") != std::string::npos || 
            stem.find("_som") != std::string::npos) {
            continue;
        }
        
        std::string filepath = entry.path().string();
        
        std::cout << "\n[ProcessDirectory] Processing: " << stem << ext << "\n";
        
        PointCloud cloud;
        Mesh mesh;
        std::vector<TrainingSample> samples;
        
        if (ext == ".obj") {
            if (!extractor.LoadOBJ(filepath, mesh)) {
                std::cerr << "[ProcessDirectory] Failed to load: " << filepath << "\n";
                continue;
            }
            samples = extractor.ExtractFeatures(mesh);
        } else {
            if (!extractor.LoadAnyFormat(filepath, cloud, nullptr)) {
                std::cerr << "[ProcessDirectory] Failed to load: " << filepath << "\n";
                continue;
            }
            samples = extractor.ExtractFeaturesFromPointCloud(cloud);
        }
        
        if (!samples.empty()) {
            // Save per-file data
            extractor.SaveTrainingData(outputDir + "/" + stem + ".bin", samples);
            extractor.SaveTrainingDataCSV(outputDir + "/" + stem + ".csv", samples);
            
            // Accumulate for combined output
            allSamples.insert(allSamples.end(), samples.begin(), samples.end());
            fileCount++;
        }
    }
    
    if (fileCount > 0) {
        std::cout << "\n[ProcessDirectory] Processed " << fileCount << " files\n";
        std::cout << "[ProcessDirectory] Total samples: " << allSamples.size() << "\n";
        
        // Save combined training data
        extractor.SaveTrainingData(outputDir + "/combined_training_data.bin", allSamples);
        
        // Save a sample CSV for verification
        if (allSamples.size() > 10000) {
            // Shuffle and take subset for CSV
            std::shuffle(allSamples.begin(), allSamples.end(), std::mt19937(42));
            allSamples.resize(10000);
        }
        extractor.SaveTrainingDataCSV(outputDir + "/combined_sample.csv", allSamples);
    }
    
    return fileCount > 0;
}

bool ProcessShapeNetDirectory(const std::string& inputDir,
                              const std::string& outputDir,
                              const ExtractorConfig& config,
                              int maxFilesPerCategory) {
    
    // Create output directory
    fs::create_directories(outputDir);
    
    FeatureExtractor extractor(config);
    
    // Save box feature definitions
    extractor.SaveBoxFeatures(outputDir + "/box_features.csv");
    
    std::vector<TrainingSample> allSamples;
    int totalFiles = 0;
    int categoryCount = 0;
    
    // ShapeNet structure: inputDir/category_id/instance_id.txt (or .npz)
    for (const auto& categoryEntry : fs::directory_iterator(inputDir)) {
        if (!categoryEntry.is_directory()) continue;
        
        std::string categoryPath = categoryEntry.path().string();
        std::string categoryName = categoryEntry.path().filename().string();
        
        // Skip non-category directories
        if (categoryName == "train_test_split" || categoryName == "util") continue;
        
        std::cout << "\n[ShapeNet] Processing category: " << categoryName << "\n";
        
        int filesInCategory = 0;
        std::vector<TrainingSample> categorySamples;
        
        for (const auto& instanceEntry : fs::directory_iterator(categoryPath)) {
            if (!instanceEntry.is_regular_file()) continue;
            
            std::string ext = instanceEntry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            
            // Only process .txt point cloud files (not _8x8.npz SOM files)
            if (ext != ".txt") continue;
            
            if (maxFilesPerCategory > 0 && filesInCategory >= maxFilesPerCategory) {
                break;
            }
            
            std::string filepath = instanceEntry.path().string();
            std::string instanceName = instanceEntry.path().stem().string();
            
            PointCloud cloud;
            if (!extractor.LoadShapeNetTXT(filepath, cloud)) {
                continue;
            }
            
            auto samples = extractor.ExtractFeaturesFromPointCloud(cloud);
            
            if (!samples.empty()) {
                categorySamples.insert(categorySamples.end(), samples.begin(), samples.end());
                filesInCategory++;
                totalFiles++;
            }
        }
        
        if (!categorySamples.empty()) {
            // Save per-category data
            std::string categoryOutputDir = outputDir + "/" + categoryName;
            fs::create_directories(categoryOutputDir);
            
            extractor.SaveTrainingData(categoryOutputDir + "/training_data.bin", categorySamples);
            
            // Subsample for CSV
            if (categorySamples.size() > 5000) {
                std::shuffle(categorySamples.begin(), categorySamples.end(), std::mt19937(42));
                categorySamples.resize(5000);
            }
            extractor.SaveTrainingDataCSV(categoryOutputDir + "/sample.csv", categorySamples);
            
            allSamples.insert(allSamples.end(), categorySamples.begin(), categorySamples.end());
            categoryCount++;
            
            std::cout << "[ShapeNet] Category " << categoryName << ": " 
                      << filesInCategory << " files, " << categorySamples.size() << " samples\n";
        }
    }
    
    if (totalFiles > 0) {
        std::cout << "\n[ShapeNet] Processed " << categoryCount << " categories, " 
                  << totalFiles << " files\n";
        std::cout << "[ShapeNet] Total samples: " << allSamples.size() << "\n";
        
        // Save combined training data
        extractor.SaveTrainingData(outputDir + "/combined_training_data.bin", allSamples);
        
        // Save a sample CSV for verification
        if (allSamples.size() > 10000) {
            std::shuffle(allSamples.begin(), allSamples.end(), std::mt19937(42));
            allSamples.resize(10000);
        }
        extractor.SaveTrainingDataCSV(outputDir + "/combined_sample.csv", allSamples);
    }
    
    return totalFiles > 0;
}

} // namespace MLExtractor
