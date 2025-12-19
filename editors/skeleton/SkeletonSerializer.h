#pragma once

#include "SkeletonDefinition.h"
#include <string>

namespace Engine {
namespace Editors {
namespace Skeleton {

// Serialization format for skeleton definitions
class SkeletonSerializer {
public:
    // Save skeleton to file (JSON-like format)
    static bool SaveToFile(const SkeletonDefinition& skeleton, const std::string& filePath);
    
    // Load skeleton from file
    static bool LoadFromFile(SkeletonDefinition& skeleton, const std::string& filePath);
    
    // Convert to string (for debugging/inspection)
    static std::string ToString(const SkeletonDefinition& skeleton);
    
    // Convert from string
    static bool FromString(SkeletonDefinition& skeleton, const std::string& data);

    // Coordinate conversion
    static void ConvertWorldToLocal(SkeletonDefinition& skeleton);

private:
    // Helper methods for parsing
    static bool ParseBone(const std::string& line, BoneDefinition& bone);
    static std::string FormatBone(const BoneDefinition& bone);
    static std::string FormatMatrix(const glm::mat4& matrix);
    static bool ParseMatrix(const std::string& str, glm::mat4& matrix);
    static std::string FormatVector3(const glm::vec3& vec);
    static bool ParseVector3(const std::string& str, glm::vec3& vec);
    static std::string FormatQuaternion(const glm::quat& quat);
    static bool ParseQuaternion(const std::string& str, glm::quat& quat);
    
    // String utilities
    static std::string Trim(const std::string& str);
    static std::vector<std::string> Split(const std::string& str, char delimiter);
};

} // namespace Skeleton
} // namespace Editors
} // namespace Engine

