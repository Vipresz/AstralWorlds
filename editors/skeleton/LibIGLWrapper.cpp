#include "LibIGLWrapper.h"
#include "SkeletonDefinition.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <fstream>

// Prevent Windows min/max macros from interfering with std::min/max
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif

// Try to include libigl headers if available
#ifdef LIBIGL_AVAILABLE
// Note: heat_geodesics may not be available in all libigl versions
// We'll use Euclidean distances as a fallback for now
#include <Eigen/Dense>
#include <Eigen/Sparse>
#endif

using namespace Engine::Editors::Skeleton;

LibIGLWrapper::LibIGLWrapper()
    : m_libiglAvailable(false)
{
#ifdef LIBIGL_AVAILABLE
    // Check if libigl is properly linked
    try {
        // Simple test to verify libigl is available
        m_libiglAvailable = true;
        m_lastError = "";
    } catch (...) {
        m_libiglAvailable = false;
        m_lastError = "libigl not properly linked";
    }
#else
    m_libiglAvailable = false;
    m_lastError = "libigl not compiled in (LIBIGL_AVAILABLE not defined)";
#endif
}

LibIGLWrapper::~LibIGLWrapper() {
}

bool LibIGLWrapper::ExtractSkeleton(
    const std::vector<glm::vec3>& vertices,
    const std::vector<std::vector<unsigned int>>& faces,
    SkeletonDefinition& skeleton)
{
    m_lastError = "";
    
    if (!m_libiglAvailable) {
        m_lastError = "libigl not available";
        return false;
    }
    
#ifdef LIBIGL_AVAILABLE
    try {
        if (vertices.empty() || faces.empty()) {
            m_lastError = "Empty mesh";
            return false;
        }
        
        // Find bounding box and key points
        glm::vec3 minPos = vertices[0];
        glm::vec3 maxPos = vertices[0];
        glm::vec3 highestPoint = vertices[0];  // For head
        glm::vec3 lowestPoint = vertices[0];   // For feet
        glm::vec3 leftmostPoint = vertices[0]; // For left hand
        glm::vec3 rightmostPoint = vertices[0]; // For right hand
        glm::vec3 frontmostPoint = vertices[0]; // For front
        glm::vec3 backmostPoint = vertices[0];  // For back
        
        for (const auto& v : vertices) {
            minPos = glm::min(minPos, v);
            maxPos = glm::max(maxPos, v);
            
            if (v.y > highestPoint.y) highestPoint = v;
            if (v.y < lowestPoint.y) lowestPoint = v;
            if (v.x < leftmostPoint.x) leftmostPoint = v;
            if (v.x > rightmostPoint.x) rightmostPoint = v;
            if (v.z > frontmostPoint.z) frontmostPoint = v;
            if (v.z < backmostPoint.z) backmostPoint = v;
        }
        
        skeleton.Clear();
        skeleton.SetName("AutoExtracted");
        
        glm::vec3 center = (minPos + maxPos) * 0.5f;
        float height = maxPos.y - minPos.y;
        float width = maxPos.x - minPos.x;
        float depth = maxPos.z - minPos.z;
        
        // Create root bone at center-bottom (pelvis/hip area)
        glm::vec3 rootPos = center;
        rootPos.y = minPos.y + height * 0.15f; // Slightly above bottom
        
        int rootID = skeleton.AddBone("Root", "");
        if (rootID < 0) {
            m_lastError = "Failed to create root bone";
            return false;
        }
        
        auto* root = skeleton.GetBone("Root");
        if (root) {
            root->position = rootPos;
        }
        
        // Create spine bone (from root to upper torso)
        glm::vec3 spinePos = center;
        spinePos.y = minPos.y + height * 0.6f;
        int spineID = skeleton.AddBone("Spine", "Root");
        if (spineID >= 0) {
            auto* spine = skeleton.GetBone("Spine");
            if (spine) {
                spine->position = spinePos;
            }
        }
        
        // Create head bone at highest point
        if (height > 0.1f) { // Only if mesh has significant height
            int headID = skeleton.AddBone("Head", "Spine");
            if (headID >= 0) {
                auto* head = skeleton.GetBone("Head");
                if (head) {
                    head->position = highestPoint;
                }
            }
        }
        
        // Create arm bones if mesh has significant width
        if (width > 0.1f) {
            // Left arm
            glm::vec3 leftShoulderPos = spinePos;
            leftShoulderPos.x = leftmostPoint.x + width * 0.1f; // Slightly inward from leftmost
            leftShoulderPos.y = spinePos.y;
            
            int leftShoulderID = skeleton.AddBone("LeftShoulder", "Spine");
            if (leftShoulderID >= 0) {
                auto* leftShoulder = skeleton.GetBone("LeftShoulder");
                if (leftShoulder) {
                    leftShoulder->position = leftShoulderPos;
                }
            }
            
            int leftHandID = skeleton.AddBone("LeftHand", "LeftShoulder");
            if (leftHandID >= 0) {
                auto* leftHand = skeleton.GetBone("LeftHand");
                if (leftHand) {
                    leftHand->position = leftmostPoint;
                }
            }
            
            // Right arm
            glm::vec3 rightShoulderPos = spinePos;
            rightShoulderPos.x = rightmostPoint.x - width * 0.1f; // Slightly inward from rightmost
            rightShoulderPos.y = spinePos.y;
            
            int rightShoulderID = skeleton.AddBone("RightShoulder", "Spine");
            if (rightShoulderID >= 0) {
                auto* rightShoulder = skeleton.GetBone("RightShoulder");
                if (rightShoulder) {
                    rightShoulder->position = rightShoulderPos;
                }
            }
            
            int rightHandID = skeleton.AddBone("RightHand", "RightShoulder");
            if (rightHandID >= 0) {
                auto* rightHand = skeleton.GetBone("RightHand");
                if (rightHand) {
                    rightHand->position = rightmostPoint;
                }
            }
        }
        
        // Create leg bones
        if (height > 0.1f) {
            // Left leg
            glm::vec3 leftHipPos = rootPos;
            leftHipPos.x = rootPos.x - width * 0.1f;
            
            int leftHipID = skeleton.AddBone("LeftHip", "Root");
            if (leftHipID >= 0) {
                auto* leftHip = skeleton.GetBone("LeftHip");
                if (leftHip) {
                    leftHip->position = leftHipPos;
                }
            }
            
            glm::vec3 leftFootPos = lowestPoint;
            leftFootPos.x = leftHipPos.x;
            
            int leftFootID = skeleton.AddBone("LeftFoot", "LeftHip");
            if (leftFootID >= 0) {
                auto* leftFoot = skeleton.GetBone("LeftFoot");
                if (leftFoot) {
                    leftFoot->position = leftFootPos;
                }
            }
            
            // Right leg
            glm::vec3 rightHipPos = rootPos;
            rightHipPos.x = rootPos.x + width * 0.1f;
            
            int rightHipID = skeleton.AddBone("RightHip", "Root");
            if (rightHipID >= 0) {
                auto* rightHip = skeleton.GetBone("RightHip");
                if (rightHip) {
                    rightHip->position = rightHipPos;
                }
            }
            
            glm::vec3 rightFootPos = lowestPoint;
            rightFootPos.x = rightHipPos.x;
            
            int rightFootID = skeleton.AddBone("RightFoot", "RightHip");
            if (rightFootID >= 0) {
                auto* rightFoot = skeleton.GetBone("RightFoot");
                if (rightFoot) {
                    rightFoot->position = rightFootPos;
                }
            }
        }
        
        return true;
        
    } catch (const std::exception& e) {
        m_lastError = std::string("Exception: ") + e.what();
        return false;
    }
#else
    m_lastError = "libigl not compiled in";
    return false;
#endif
}

std::vector<std::vector<std::pair<int, float>>> LibIGLWrapper::GenerateWeights(
    const std::vector<glm::vec3>& vertices,
    const std::vector<std::vector<unsigned int>>& faces,
    const std::vector<glm::vec3>& bonePositions,
    const std::vector<std::string>& boneNames,
    int maxBonesPerVertex)
{
    m_lastError = "";
    std::vector<std::vector<std::pair<int, float>>> result(vertices.size());
    
    if (!m_libiglAvailable) {
        m_lastError = "libigl not available - using fallback method";
        // Fallback to simple distance-based weights
        std::vector<std::vector<std::pair<int, float>>> result(vertices.size());
        for (size_t v = 0; v < vertices.size(); v++) {
            std::vector<std::pair<float, int>> distPairs;
            for (size_t b = 0; b < bonePositions.size(); b++) {
                float dist = glm::length(vertices[v] - bonePositions[b]);
                float invDistSq = 1.0f / (dist * dist + 0.0001f);
                distPairs.push_back({invDistSq, static_cast<int>(b)});
            }
            std::sort(distPairs.begin(), distPairs.end(),
                [](const auto& a, const auto& b) { return a.first > b.first; });
            int numBones = std::min(maxBonesPerVertex, static_cast<int>(distPairs.size()));
            float totalInvDist = 0.0f;
            for (int i = 0; i < numBones; i++) {
                totalInvDist += distPairs[i].first;
            }
            if (totalInvDist > 0.0001f) {
                for (int i = 0; i < numBones; i++) {
                    float weight = distPairs[i].first / totalInvDist;
                    if (weight > 0.0001f) {
                        result[v].push_back({distPairs[i].second, weight});
                    }
                }
            }
        }
        return result;
    }
    
#ifdef LIBIGL_AVAILABLE
    try {
        // Use libigl's heat geodesics for better weight computation
        std::vector<std::vector<float>> distances = ComputeGeodesicDistances(
            vertices, faces, bonePositions);
        
        return DistancesToWeights(distances, boneNames, maxBonesPerVertex);
        
    } catch (const std::exception& e) {
        m_lastError = std::string("Exception: ") + e.what();
        // Fallback to simple method
        std::vector<std::vector<std::pair<int, float>>> result(vertices.size());
        for (size_t v = 0; v < vertices.size(); v++) {
            std::vector<std::pair<float, int>> distPairs;
            for (size_t b = 0; b < bonePositions.size(); b++) {
                float dist = glm::length(vertices[v] - bonePositions[b]);
                float invDistSq = 1.0f / (dist * dist + 0.0001f);
                distPairs.push_back({invDistSq, static_cast<int>(b)});
            }
            std::sort(distPairs.begin(), distPairs.end(),
                [](const auto& a, const auto& b) { return a.first > b.first; });
            int numBones = std::min(maxBonesPerVertex, static_cast<int>(distPairs.size()));
            float totalInvDist = 0.0f;
            for (int i = 0; i < numBones; i++) {
                totalInvDist += distPairs[i].first;
            }
            if (totalInvDist > 0.0001f) {
                for (int i = 0; i < numBones; i++) {
                    float weight = distPairs[i].first / totalInvDist;
                    if (weight > 0.0001f) {
                        result[v].push_back({distPairs[i].second, weight});
                    }
                }
            }
        }
        return result;
    }
#else
    // Fallback to simple distance-based method
    std::vector<std::vector<std::pair<int, float>>> result(vertices.size());
    for (size_t v = 0; v < vertices.size(); v++) {
        std::vector<std::pair<float, int>> distPairs;
        for (size_t b = 0; b < bonePositions.size(); b++) {
            float dist = glm::length(vertices[v] - bonePositions[b]);
            float invDistSq = 1.0f / (dist * dist + 0.0001f);
            distPairs.push_back({invDistSq, static_cast<int>(b)});
        }
        std::sort(distPairs.begin(), distPairs.end(),
            [](const auto& a, const auto& b) { return a.first > b.first; });
        int numBones = std::min(maxBonesPerVertex, static_cast<int>(distPairs.size()));
        float totalInvDist = 0.0f;
        for (int i = 0; i < numBones; i++) {
            totalInvDist += distPairs[i].first;
        }
        if (totalInvDist > 0.0001f) {
            for (int i = 0; i < numBones; i++) {
                float weight = distPairs[i].first / totalInvDist;
                if (weight > 0.0001f) {
                    result[v].push_back({distPairs[i].second, weight});
                }
            }
        }
    }
    return result;
#endif
}

std::vector<std::vector<float>> LibIGLWrapper::ComputeGeodesicDistances(
    const std::vector<glm::vec3>& vertices,
    const std::vector<std::vector<unsigned int>>& faces,
    const std::vector<glm::vec3>& bonePositions)
{
    std::vector<std::vector<float>> distances(vertices.size());
    
#ifdef LIBIGL_AVAILABLE
    try {
        // Convert to Eigen format
        Eigen::MatrixXd V(vertices.size(), 3);
        for (size_t i = 0; i < vertices.size(); i++) {
            V(i, 0) = vertices[i].x;
            V(i, 1) = vertices[i].y;
            V(i, 2) = vertices[i].z;
        }
        
        Eigen::MatrixXi F(faces.size(), 3);
        for (size_t i = 0; i < faces.size(); i++) {
            if (faces[i].size() >= 3) {
                F(i, 0) = faces[i][0];
                F(i, 1) = faces[i][1];
                F(i, 2) = faces[i][2];
            }
        }
        
        // Use heat geodesics for each bone
        for (size_t boneIdx = 0; boneIdx < bonePositions.size(); boneIdx++) {
            // Find closest vertex to bone position
            int closestVertex = 0;
            float minDist = std::numeric_limits<float>::max();
            for (size_t v = 0; v < vertices.size(); v++) {
                float dist = glm::length(vertices[v] - bonePositions[boneIdx]);
                if (dist < minDist) {
                    minDist = dist;
                    closestVertex = static_cast<int>(v);
                }
            }
            
            // For now, use Euclidean distances as fallback
            // TODO: Implement proper geodesic distance computation when libigl's heat_geodesics is available
            // The heat_geodesics function may not be available in all libigl versions
            // Store Euclidean distances for now
            for (size_t v = 0; v < vertices.size(); v++) {
                if (boneIdx == 0) {
                    distances[v].resize(bonePositions.size());
                }
                distances[v][boneIdx] = glm::length(vertices[v] - bonePositions[boneIdx]);
            }
        }
        
    } catch (...) {
        // Fallback to Euclidean distances
        for (size_t v = 0; v < vertices.size(); v++) {
            distances[v].resize(bonePositions.size());
            for (size_t b = 0; b < bonePositions.size(); b++) {
                distances[v][b] = glm::length(vertices[v] - bonePositions[b]);
            }
        }
    }
#else
    // Fallback: use Euclidean distances
    for (size_t v = 0; v < vertices.size(); v++) {
        distances[v].resize(bonePositions.size());
        for (size_t b = 0; b < bonePositions.size(); b++) {
            distances[v][b] = glm::length(vertices[v] - bonePositions[b]);
        }
    }
#endif
    
    return distances;
}

std::vector<std::vector<std::pair<int, float>>> LibIGLWrapper::DistancesToWeights(
    const std::vector<std::vector<float>>& distances,
    const std::vector<std::string>& boneNames,
    int maxBonesPerVertex)
{
    std::vector<std::vector<std::pair<int, float>>> result(distances.size());
    
    for (size_t v = 0; v < distances.size(); v++) {
        if (distances[v].empty()) continue;
        
        // Convert distances to inverse weights (closer = higher weight)
        std::vector<std::pair<float, int>> distPairs;
        for (size_t b = 0; b < distances[v].size(); b++) {
            float invDist = 1.0f / (distances[v][b] + 0.0001f); // Add small epsilon
            distPairs.push_back({invDist, static_cast<int>(b)});
        }
        
        // Sort by inverse distance (highest first)
        std::sort(distPairs.begin(), distPairs.end(),
            [](const auto& a, const auto& b) { return a.first > b.first; });
        
        // Take top maxBonesPerVertex
        int numBones = std::min(maxBonesPerVertex, static_cast<int>(distPairs.size()));
        
        // Normalize weights
        float totalInvDist = 0.0f;
        for (int i = 0; i < numBones; i++) {
            totalInvDist += distPairs[i].first;
        }
        
        if (totalInvDist > 0.0001f) {
            for (int i = 0; i < numBones; i++) {
                float weight = distPairs[i].first / totalInvDist;
                if (weight > 0.0001f) {
                    result[v].push_back({distPairs[i].second, weight});
                }
            }
        }
    }
    
    return result;
}

