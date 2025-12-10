#include "SkeletonEditor.h"
#include <glm/gtc/quaternion.hpp>

namespace Engine {
namespace Editors {
namespace Skeleton {

SkeletonEditor::SkeletonEditor()
    : m_hasUnsavedChanges(false)
    , m_currentFilePath("")
{
    NewSkeleton();
}

SkeletonEditor::~SkeletonEditor() {
}

bool SkeletonEditor::NewSkeleton(const std::string& name) {
    m_skeleton.Clear();
    m_skeleton.SetName(name);
    m_skeleton.SetDescription("");
    m_currentFilePath = "";
    m_hasUnsavedChanges = false;
    return true;
}

bool SkeletonEditor::LoadSkeleton(const std::string& filePath) {
    if (SkeletonSerializer::LoadFromFile(m_skeleton, filePath)) {
        m_currentFilePath = filePath;
        m_hasUnsavedChanges = false;
        return true;
    }
    return false;
}

bool SkeletonEditor::SaveSkeleton(const std::string& filePath) {
    if (SkeletonSerializer::SaveToFile(m_skeleton, filePath)) {
        m_currentFilePath = filePath;
        m_hasUnsavedChanges = false;
        return true;
    }
    return false;
}

bool SkeletonEditor::AddBone(const std::string& name, const std::string& parentName) {
    if (m_skeleton.AddBone(name, parentName) >= 0) {
        MarkChanged();
        return true;
    }
    return false;
}

bool SkeletonEditor::RemoveBone(const std::string& name) {
    if (m_skeleton.RemoveBone(name)) {
        MarkChanged();
        return true;
    }
    return false;
}

bool SkeletonEditor::RenameBone(const std::string& oldName, const std::string& newName) {
    BoneDefinition* bone = m_skeleton.GetBone(oldName);
    if (!bone) {
        return false;
    }
    
    if (m_skeleton.GetBone(newName)) {
        return false;
    }
    
    BoneDefinition boneData = *bone;
    std::string parentName = boneData.parentName;
    std::vector<std::string> children = m_skeleton.GetBoneChildren(oldName);
    
    if (!m_skeleton.RemoveBone(oldName)) {
        return false;
    }
    
    int id = m_skeleton.AddBone(newName, parentName);
    if (id < 0) {
        return false;
    }
    
    BoneDefinition* newBone = m_skeleton.GetBone(newName);
    if (newBone) {
        *newBone = boneData;
        newBone->name = newName;
    }
    
    for (const std::string& childName : children) {
        m_skeleton.SetBoneParent(childName, newName);
    }
    
    MarkChanged();
    return true;
}

bool SkeletonEditor::SetBoneParent(const std::string& boneName, const std::string& parentName) {
    if (m_skeleton.SetBoneParent(boneName, parentName)) {
        MarkChanged();
        return true;
    }
    return false;
}

bool SkeletonEditor::SetBonePosition(const std::string& name, float x, float y, float z) {
    m_skeleton.SetBonePosition(name, glm::vec3(x, y, z));
    MarkChanged();
    return true;
}

bool SkeletonEditor::SetBoneRotationEuler(const std::string& name, float pitch, float yaw, float roll) {
    m_skeleton.SetBoneRotationEuler(name, pitch, yaw, roll);
    MarkChanged();
    return true;
}

bool SkeletonEditor::SetBoneScale(const std::string& name, float x, float y, float z) {
    m_skeleton.SetBoneScale(name, glm::vec3(x, y, z));
    MarkChanged();
    return true;
}

std::vector<std::string> SkeletonEditor::GetAllBoneNames() const {
    return m_skeleton.GetAllBoneNames();
}

std::vector<std::string> SkeletonEditor::GetRootBones() const {
    return m_skeleton.GetRootBones();
}

std::vector<std::string> SkeletonEditor::GetBoneChildren(const std::string& boneName) const {
    return m_skeleton.GetBoneChildren(boneName);
}

std::string SkeletonEditor::GetBoneParent(const std::string& boneName) const {
    return m_skeleton.GetBoneParent(boneName);
}

bool SkeletonEditor::Validate() const {
    return m_skeleton.Validate();
}

std::vector<std::string> SkeletonEditor::GetValidationErrors() const {
    return m_skeleton.GetValidationErrors();
}

void SkeletonEditor::CreateHumanoidSkeleton() {
    NewSkeleton("Humanoid Skeleton");
    
    AddBone("Hips", "");
    SetBonePosition("Hips", 0.0f, 0.9f, 0.0f);
    
    AddBone("Spine", "Hips");
    SetBonePosition("Spine", 0.0f, 0.15f, 0.0f);
    
    AddBone("Spine1", "Spine");
    SetBonePosition("Spine1", 0.0f, 0.15f, 0.0f);
    
    AddBone("Spine2", "Spine1");
    SetBonePosition("Spine2", 0.0f, 0.15f, 0.0f);
    
    AddBone("Neck", "Spine2");
    SetBonePosition("Neck", 0.0f, 0.15f, 0.0f);
    
    AddBone("Head", "Neck");
    SetBonePosition("Head", 0.0f, 0.15f, 0.0f);
    
    AddBone("LeftShoulder", "Spine2");
    SetBonePosition("LeftShoulder", -0.15f, 0.1f, 0.0f);
    
    AddBone("LeftArm", "LeftShoulder");
    SetBonePosition("LeftArm", -0.2f, 0.0f, 0.0f);
    
    AddBone("LeftForeArm", "LeftArm");
    SetBonePosition("LeftForeArm", -0.25f, 0.0f, 0.0f);
    
    AddBone("LeftHand", "LeftForeArm");
    SetBonePosition("LeftHand", -0.2f, 0.0f, 0.0f);
    
    AddBone("RightShoulder", "Spine2");
    SetBonePosition("RightShoulder", 0.15f, 0.1f, 0.0f);
    
    AddBone("RightArm", "RightShoulder");
    SetBonePosition("RightArm", 0.2f, 0.0f, 0.0f);
    
    AddBone("RightForeArm", "RightArm");
    SetBonePosition("RightForeArm", 0.25f, 0.0f, 0.0f);
    
    AddBone("RightHand", "RightForeArm");
    SetBonePosition("RightHand", 0.2f, 0.0f, 0.0f);
    
    AddBone("LeftUpLeg", "Hips");
    SetBonePosition("LeftUpLeg", -0.1f, -0.2f, 0.0f);
    
    AddBone("LeftLeg", "LeftUpLeg");
    SetBonePosition("LeftLeg", 0.0f, -0.35f, 0.0f);
    
    AddBone("LeftFoot", "LeftLeg");
    SetBonePosition("LeftFoot", 0.0f, -0.35f, 0.05f);
    
    AddBone("RightUpLeg", "Hips");
    SetBonePosition("RightUpLeg", 0.1f, -0.2f, 0.0f);
    
    AddBone("RightLeg", "RightUpLeg");
    SetBonePosition("RightLeg", 0.0f, -0.35f, 0.0f);
    
    AddBone("RightFoot", "RightLeg");
    SetBonePosition("RightFoot", 0.0f, -0.35f, 0.05f);
    
    MarkChanged();
}

void SkeletonEditor::CreateQuadrupedSkeleton() {
    NewSkeleton("Quadruped Skeleton");
    
    AddBone("Root", "");
    SetBonePosition("Root", 0.0f, 0.5f, 0.0f);
    
    AddBone("Spine", "Root");
    SetBonePosition("Spine", 0.0f, 0.0f, 0.2f);
    
    AddBone("Spine1", "Spine");
    SetBonePosition("Spine1", 0.0f, 0.0f, 0.2f);
    
    AddBone("Spine2", "Spine1");
    SetBonePosition("Spine2", 0.0f, 0.0f, 0.2f);
    
    AddBone("Neck", "Spine2");
    SetBonePosition("Neck", 0.0f, 0.1f, 0.15f);
    
    AddBone("Head", "Neck");
    SetBonePosition("Head", 0.0f, 0.0f, 0.2f);
    
    AddBone("LeftFrontShoulder", "Spine");
    SetBonePosition("LeftFrontShoulder", -0.15f, -0.1f, 0.1f);
    
    AddBone("LeftFrontLeg", "LeftFrontShoulder");
    SetBonePosition("LeftFrontLeg", 0.0f, -0.2f, 0.0f);
    
    AddBone("LeftFrontPaw", "LeftFrontLeg");
    SetBonePosition("LeftFrontPaw", 0.0f, -0.2f, 0.0f);
    
    AddBone("RightFrontShoulder", "Spine");
    SetBonePosition("RightFrontShoulder", 0.15f, -0.1f, 0.1f);
    
    AddBone("RightFrontLeg", "RightFrontShoulder");
    SetBonePosition("RightFrontLeg", 0.0f, -0.2f, 0.0f);
    
    AddBone("RightFrontPaw", "RightFrontLeg");
    SetBonePosition("RightFrontPaw", 0.0f, -0.2f, 0.0f);
    
    AddBone("LeftHip", "Root");
    SetBonePosition("LeftHip", -0.15f, -0.1f, -0.1f);
    
    AddBone("LeftBackLeg", "LeftHip");
    SetBonePosition("LeftBackLeg", 0.0f, -0.2f, 0.0f);
    
    AddBone("LeftBackPaw", "LeftBackLeg");
    SetBonePosition("LeftBackPaw", 0.0f, -0.2f, 0.0f);
    
    AddBone("RightHip", "Root");
    SetBonePosition("RightHip", 0.15f, -0.1f, -0.1f);
    
    AddBone("RightBackLeg", "RightHip");
    SetBonePosition("RightBackLeg", 0.0f, -0.2f, 0.0f);
    
    AddBone("RightBackPaw", "RightBackLeg");
    SetBonePosition("RightBackPaw", 0.0f, -0.2f, 0.0f);
    
    MarkChanged();
}

} // namespace Skeleton
} // namespace Editors
} // namespace Engine

