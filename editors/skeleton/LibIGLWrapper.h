#pragma once

#include <vector>
#include <string>
#include <glm/glm.hpp>

namespace Engine {
namespace Editors {
namespace Skeleton {

// Forward declarations
struct BoneDefinition;
class SkeletonDefinition;

/**
 * Wrapper class for libigl-based skeleton and weight extraction.
 * Provides simple interface for:
 * - Automatic skeleton extraction from mesh
 * - High-quality weight computation using libigl algorithms
 */
class LibIGLWrapper {
public:
    LibIGLWrapper();
    ~LibIGLWrapper();
    
    /**
     * Extract skeleton from mesh using libigl algorithms.
     * @param vertices Mesh vertices (x, y, z per vertex)
     * @param faces Mesh faces (indices)
     * @param skeleton Output skeleton definition to populate
     * @return true if successful
     */
    bool ExtractSkeleton(
        const std::vector<glm::vec3>& vertices,
        const std::vector<std::vector<unsigned int>>& faces,
        SkeletonDefinition& skeleton
    );
    
    /**
     * Generate vertex weights using libigl's ARAP (As-Rigid-As-Possible) or similar.
     * @param vertices Mesh vertices
     * @param faces Mesh faces
     * @param bonePositions Bone positions in world space
     * @param boneNames Bone names corresponding to positions
     * @param maxBonesPerVertex Maximum bones influencing each vertex (default: 4)
     * @return Vector of vertex weights: [vertexIndex][{boneID, weight}, ...]
     */
    std::vector<std::vector<std::pair<int, float>>> GenerateWeights(
        const std::vector<glm::vec3>& vertices,
        const std::vector<std::vector<unsigned int>>& faces,
        const std::vector<glm::vec3>& bonePositions,
        const std::vector<std::string>& boneNames,
        int maxBonesPerVertex = 4
    );
    
    /**
     * Check if libigl is available and properly initialized.
     */
    bool IsAvailable() const { return m_libiglAvailable; }
    
    /**
     * Get last error message if operation failed.
     */
    const std::string& GetLastError() const { return m_lastError; }

private:
    bool m_libiglAvailable;
    std::string m_lastError;
    
    // Helper: Compute geodesic distances from vertices to bone positions
    std::vector<std::vector<float>> ComputeGeodesicDistances(
        const std::vector<glm::vec3>& vertices,
        const std::vector<std::vector<unsigned int>>& faces,
        const std::vector<glm::vec3>& bonePositions
    );
    
    // Helper: Convert geodesic distances to weights using heat diffusion
    std::vector<std::vector<std::pair<int, float>>> DistancesToWeights(
        const std::vector<std::vector<float>>& distances,
        const std::vector<std::string>& boneNames,
        int maxBonesPerVertex
    );
};

} // namespace Skeleton
} // namespace Editors
} // namespace Engine




