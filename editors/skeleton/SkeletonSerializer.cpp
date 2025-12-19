#include "SkeletonSerializer.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iomanip>
#include <set>
#include <map>
#include <vector>
#include <cctype>

namespace Engine {
namespace Editors {
namespace Skeleton {

bool SkeletonSerializer::SaveToFile(const SkeletonDefinition& skeleton, const std::string& filePath) {
    std::ofstream file(filePath);
    if (!file.is_open()) {
        return false;
    }
    
    file << "# Skeleton Definition File (Gold Standard Format)\n";
    file << "# Format: Name|Parent|ID|LocalPosition|LocalRotation|LocalScale|InverseBindPose\n";
    file << "# All transforms are stored in LOCAL space (relative to parent)\n";
    file << "# This is the industry standard format (Unity, Unreal, Godot, glTF, Mixamo, FBX)\n";
    file << "\n";
    
    file << "name: " << skeleton.GetName() << "\n";
    file << "description: local\n";  // Always save as local coordinates
    file << "\n";
    
    file << "bones:\n";
    for (const auto& pair : skeleton.GetBones()) {
        file << FormatBone(pair.second) << "\n";
    }
    
    file.close();
    return true;
}

bool SkeletonSerializer::LoadFromFile(SkeletonDefinition& skeleton, const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        return false;
    }
    
    skeleton.Clear();
    
    std::string line;
    bool inBonesSection = false;
    bool isWorldCoordinates = false;
    
    // First pass: collect all bone definitions
    std::vector<BoneDefinition> pendingBones;
    
    while (std::getline(file, line)) {
        line = Trim(line);
        
        if (line.empty() || line[0] == '#') {
            continue;
        }
        
        if (line.find("name:") == 0) {
            std::string name = line.substr(5);
            name = Trim(name);
            skeleton.SetName(name);
            continue;
        }
        
        if (line.find("description:") == 0) {
            std::string desc = line.substr(12);
            desc = Trim(desc);
            skeleton.SetDescription(desc);
            
            // Check if description indicates world coordinates
            std::string descLower = desc;
            std::transform(descLower.begin(), descLower.end(), descLower.begin(), 
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (descLower.find("world") != std::string::npos) {
                isWorldCoordinates = true;
            }
            continue;
        }
        
        if (line == "bones:") {
            inBonesSection = true;
            continue;
        }
        
        if (inBonesSection) {
            BoneDefinition bone;
            if (ParseBone(line, bone)) {
                // Store bone data - we'll add them all in a second pass
                pendingBones.push_back(bone);
            }
        }
    }
    
    file.close();
    
    // Second pass: Add all bones as root bones first (so they all exist)
    for (const auto& bone : pendingBones) {
        // Add bone without parent check - we'll set parents in next pass
        int boneID = skeleton.AddBone(bone.name, "");
        if (boneID >= 0) {
            BoneDefinition* addedBone = skeleton.GetBone(bone.name);
            if (addedBone) {
                // Copy all properties except parent (we'll set that next)
                addedBone->id = bone.id;
                addedBone->position = bone.position;
                addedBone->rotation = bone.rotation;
                addedBone->scale = bone.scale;
                addedBone->offsetMatrix = bone.offsetMatrix;
            }
        }
    }
    
    // Third pass: Set parent relationships
    for (const auto& bone : pendingBones) {
        if (!bone.parentName.empty()) {
            // Set parent relationship (parent should exist now)
            skeleton.SetBoneParent(bone.name, bone.parentName);
        }
    }
    
    // Convert world positions to local if needed
    if (isWorldCoordinates) {
        ConvertWorldToLocal(skeleton);
    }
    
    if (!skeleton.Validate()) {
        auto errors = skeleton.GetValidationErrors();
    }
    
    return true;
}

std::string SkeletonSerializer::ToString(const SkeletonDefinition& skeleton) {
    std::ostringstream oss;
    
    oss << "Skeleton: " << skeleton.GetName() << "\n";
    oss << "Description: " << skeleton.GetDescription() << "\n";
    oss << "Bone Count: " << skeleton.GetBoneCount() << "\n\n";
    
    oss << "Bones:\n";
    for (const auto& pair : skeleton.GetBones()) {
        oss << FormatBone(pair.second) << "\n";
    }
    
    return oss.str();
}

bool SkeletonSerializer::FromString(SkeletonDefinition& skeleton, const std::string& data) {
    std::istringstream iss(data);
    std::string line;
    
    skeleton.Clear();
    bool inBonesSection = false;
    
    while (std::getline(iss, line)) {
        line = Trim(line);
        
        if (line.empty() || line[0] == '#') {
            continue;
        }
        
        if (line.find("name:") == 0) {
            skeleton.SetName(Trim(line.substr(5)));
            continue;
        }
        
        if (line.find("description:") == 0) {
            skeleton.SetDescription(Trim(line.substr(12)));
            continue;
        }
        
        if (line == "bones:") {
            inBonesSection = true;
            continue;
        }
        
        if (inBonesSection) {
            BoneDefinition bone;
            if (ParseBone(line, bone)) {
                skeleton.AddBone(bone.name, bone.parentName);
                BoneDefinition* addedBone = skeleton.GetBone(bone.name);
                if (addedBone) {
                    *addedBone = bone;
                }
            }
        }
    }
    
    return true;
}

bool SkeletonSerializer::ParseBone(const std::string& line, BoneDefinition& bone) {
    std::vector<std::string> parts = Split(line, '|');
    
    if (parts.size() < 7) {
        return false;
    }
    
    bone.name = Trim(parts[0]);
    bone.parentName = Trim(parts[1]);
    
    try {
        bone.id = std::stoi(parts[2]);
    } catch (...) {
        return false;
    }
    
    if (!ParseVector3(parts[3], bone.position)) {
        return false;
    }
    
    if (!ParseQuaternion(parts[4], bone.rotation)) {
        return false;
    }
    
    if (!ParseVector3(parts[5], bone.scale)) {
        return false;
    }
    
    if (!ParseMatrix(parts[6], bone.offsetMatrix)) {
        return false;
    }
    
    return true;
}

std::string SkeletonSerializer::FormatBone(const BoneDefinition& bone) {
    std::ostringstream oss;
    oss << bone.name << "|"
        << bone.parentName << "|"
        << bone.id << "|"
        << FormatVector3(bone.position) << "|"
        << FormatQuaternion(bone.rotation) << "|"
        << FormatVector3(bone.scale) << "|"
        << FormatMatrix(bone.offsetMatrix);
    return oss.str();
}

std::string SkeletonSerializer::FormatMatrix(const glm::mat4& matrix) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(6);
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            if (i > 0 || j > 0) oss << ",";
            oss << matrix[j][i];
        }
    }
    return oss.str();
}

bool SkeletonSerializer::ParseMatrix(const std::string& str, glm::mat4& matrix) {
    std::vector<std::string> parts = Split(str, ',');
    if (parts.size() != 16) {
        return false;
    }
    
    try {
        float values[16];
        for (size_t i = 0; i < 16; i++) {
            values[i] = std::stof(Trim(parts[i]));
        }
        
        matrix = glm::mat4(
            values[0], values[4], values[8], values[12],
            values[1], values[5], values[9], values[13],
            values[2], values[6], values[10], values[14],
            values[3], values[7], values[11], values[15]
        );
        return true;
    } catch (...) {
        return false;
    }
}

std::string SkeletonSerializer::FormatVector3(const glm::vec3& vec) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(6);
    oss << "(" << vec.x << "," << vec.y << "," << vec.z << ")";
    return oss.str();
}

bool SkeletonSerializer::ParseVector3(const std::string& str, glm::vec3& vec) {
    std::string cleaned = Trim(str);
    if (cleaned.front() == '(') cleaned = cleaned.substr(1);
    if (cleaned.back() == ')') cleaned = cleaned.substr(0, cleaned.length() - 1);
    
    std::vector<std::string> parts = Split(cleaned, ',');
    if (parts.size() != 3) {
        return false;
    }
    
    try {
        vec.x = std::stof(Trim(parts[0]));
        vec.y = std::stof(Trim(parts[1]));
        vec.z = std::stof(Trim(parts[2]));
        return true;
    } catch (...) {
        return false;
    }
}

std::string SkeletonSerializer::FormatQuaternion(const glm::quat& quat) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(6);
    oss << "(" << quat.x << "," << quat.y << "," << quat.z << "," << quat.w << ")";
    return oss.str();
}

bool SkeletonSerializer::ParseQuaternion(const std::string& str, glm::quat& quat) {
    std::string cleaned = Trim(str);
    if (cleaned.front() == '(') cleaned = cleaned.substr(1);
    if (cleaned.back() == ')') cleaned = cleaned.substr(0, cleaned.length() - 1);
    
    std::vector<std::string> parts = Split(cleaned, ',');
    if (parts.size() != 4) {
        return false;
    }
    
    try {
        quat.x = std::stof(Trim(parts[0]));
        quat.y = std::stof(Trim(parts[1]));
        quat.z = std::stof(Trim(parts[2]));
        quat.w = std::stof(Trim(parts[3]));
        return true;
    } catch (...) {
        return false;
    }
}

std::string SkeletonSerializer::Trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\n\r");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\n\r");
    return str.substr(first, (last - first + 1));
}

std::vector<std::string> SkeletonSerializer::Split(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(str);
    while (std::getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

void SkeletonSerializer::ConvertWorldToLocal(SkeletonDefinition& skeleton) {
    // Get all bones and process them in hierarchy order (parents before children)
    std::vector<std::string> rootBones = skeleton.GetRootBones();
    std::map<std::string, glm::vec3> worldPositions; // Store original world positions
    
    // First pass: collect all world positions
    auto allBoneNames = skeleton.GetAllBoneNames();
    for (const std::string& boneName : allBoneNames) {
        const BoneDefinition* bone = skeleton.GetBone(boneName);
        if (bone) {
            worldPositions[boneName] = bone->position; // Store original world position
        }
    }
    
    // Second pass: convert to local positions
    // Process bones in hierarchy order (breadth-first)
    std::vector<std::string> toProcess = rootBones;
    std::set<std::string> processed;
    
    while (!toProcess.empty()) {
        std::string currentBoneName = toProcess.front();
        toProcess.erase(toProcess.begin());
        
        if (processed.find(currentBoneName) != processed.end()) {
            continue; // Already processed
        }
        
        BoneDefinition* bone = skeleton.GetBone(currentBoneName);
        if (!bone) {
            continue;
        }
        
        glm::vec3 worldPos = worldPositions[currentBoneName];
        glm::vec3 localPos;
        
        if (bone->parentName.empty()) {
            // Root bone: world position = local position
            localPos = worldPos;
        } else {
            // Child bone: local position = world position - parent's world position
            auto parentIt = worldPositions.find(bone->parentName);
            if (parentIt != worldPositions.end()) {
                localPos = worldPos - parentIt->second;
            } else {
                // Parent not found - keep world position (shouldn't happen in valid skeleton)
                localPos = worldPos;
            }
        }
        
        // Update bone's local position
        skeleton.SetBonePosition(currentBoneName, localPos);
        processed.insert(currentBoneName);
        
        // Add children to processing queue
        auto children = skeleton.GetBoneChildren(currentBoneName);
        for (const std::string& childName : children) {
            if (processed.find(childName) == processed.end()) {
                toProcess.push_back(childName);
            }
        }
    }
}

} // namespace Skeleton
} // namespace Editors
} // namespace Engine

