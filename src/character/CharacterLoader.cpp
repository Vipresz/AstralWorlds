#include "character/CharacterLoader.h"

#include "Logger.h"
#include "core/ecs/Entity.h"
#include "core/ecs/Registry.h"
#include "character/CharacterDatabase.h"
#include "character/EquipmentAttachmentComponent.h"
#include "render/Animation.h"
#include "render/Model.h"
#include "render/Skeleton.h"
#include "render/components/TransformComponent.h"
#include "render/components/MeshComponent.h"
#include "render/components/MaterialComponent.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/config.h>

#include <cmath>
#include <fstream>
#include <vector>

using Engine::ECS::Registry;
using Engine::Platform::Render::Animation;
using Engine::Platform::Render::Skeleton;
using Engine::Platform::Render::Components::MaterialComponent;
using Engine::Platform::Render::Components::MeshComponent;
using Engine::Platform::Render::Components::TransformComponent;
using EngineLogging::Logger;

namespace Game {
namespace Character {

namespace {

bool FileExists(const std::string& path) {
    std::ifstream file(path);
    return file.good();
}

bool LoadAnimationClip(const std::string& path, std::unique_ptr<Animation>& outAnimation) {
    if (!FileExists(path)) {
        return false;
    }
    outAnimation = std::make_unique<Animation>();
    if (!outAnimation->LoadAnimation(path)) {
        outAnimation.reset();
        return false;
    }
    return true;
}

} // namespace

bool CharacterLoader::LoadSkinnedBodyAnimations(const std::string& bodyModelPath,
                                                CharacterInstance& instance) {
    if (!instance.m_bodyModel || instance.m_bodyModel->GetBoneCount() == 0) {
        return false;
    }

    Assimp::Importer importer;
    // Keep flags aligned with Model::LoadModel so mesh bone indices match skeleton IDs.
    importer.SetPropertyBool(AI_CONFIG_IMPORT_FBX_PRESERVE_PIVOTS, false);
    const aiScene* scene = importer.ReadFile(
        bodyModelPath,
        aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_GenSmoothNormals |
            aiProcess_JoinIdenticalVertices | aiProcess_LimitBoneWeights);
    if (!scene) {
        LOG_WARNING_FMT("CharacterLoader", "Assimp failed to load skeleton from %s", bodyModelPath.c_str());
        return false;
    }

    instance.m_skeleton = std::make_unique<Skeleton>();
    if (!instance.m_skeleton->LoadSkeleton(scene)) {
        LOG_WARNING_FMT("CharacterLoader", "Failed to extract skeleton from %s", bodyModelPath.c_str());
        instance.m_skeleton.reset();
        return false;
    }

    {
        std::vector<std::string> meshBoneNames;
        meshBoneNames.reserve(instance.m_bodyModel->GetBoneCount());
        for (size_t i = 0; i < instance.m_bodyModel->GetBoneCount(); ++i) {
            meshBoneNames.push_back(instance.m_bodyModel->GetBoneNameByIndex(static_cast<unsigned int>(i)));
        }
        if (!instance.m_skeleton->AlignBoneIdsToMeshOrder(meshBoneNames)) {
            LOG_WARNING("CharacterLoader",
                        "Skeleton bone IDs could not be fully aligned to mesh VBO indices — skinning may be wrong");
        }
    }

    constexpr const char* kIdleAnim =
        "graphics/Models/character/animation/regular/Standing.fbx";
    constexpr const char* kWalkAnim =
        "graphics/Models/character/animation/regular/Walking.fbx";
    constexpr const char* kRunAnim =
        "graphics/Models/character/animation/regular/Run.fbx";
    constexpr const char* kJumpAnim =
        "graphics/Models/character/animation/regular/Jumping.fbx";

    LoadAnimationClip(kIdleAnim, instance.m_idleAnimation);
    LoadAnimationClip(kWalkAnim, instance.m_walkingAnimation);
    LoadAnimationClip(kRunAnim, instance.m_runningAnimation);
    LoadAnimationClip(kJumpAnim, instance.m_jumpAnimation);

    auto countChannelMatches = [&](const Animation* animation) -> size_t {
        if (!animation || !instance.m_skeleton) {
            return 0;
        }
        size_t matches = 0;
        for (const std::string& channelName : animation->GetBoneNames()) {
            if (instance.m_skeleton->GetBone(channelName) != nullptr) {
                ++matches;
            }
        }
        return matches;
    };

    const size_t walkMatches = countChannelMatches(instance.m_walkingAnimation.get());
    const size_t runMatches = countChannelMatches(instance.m_runningAnimation.get());
    const size_t walkChannels =
        instance.m_walkingAnimation ? instance.m_walkingAnimation->GetBoneNames().size() : 0;
    const size_t runChannels =
        instance.m_runningAnimation ? instance.m_runningAnimation->GetBoneNames().size() : 0;

    LOG_INFO_FMT("CharacterLoader",
                 "Skinned body animations loaded (bones=%zu, idle=%s walk=%s run=%s jump=%s, "
                 "walkMatches=%zu/%zu runMatches=%zu/%zu)",
                 instance.m_skeleton->GetBoneCount(),
                 instance.m_idleAnimation ? "yes" : "no",
                 instance.m_walkingAnimation ? "yes" : "no",
                 instance.m_runningAnimation ? "yes" : "no",
                 instance.m_jumpAnimation ? "yes" : "no",
                 walkMatches,
                 walkChannels,
                 runMatches,
                 runChannels);

    if (walkMatches < walkChannels || runMatches < runChannels) {
        LOG_WARNING_FMT("CharacterLoader",
                        "Animation/skeleton bone name mismatch — limbs may stay in bind pose "
                        "(walk %zu/%zu, run %zu/%zu)",
                        walkMatches,
                        walkChannels,
                        runMatches,
                        runChannels);
    }
    return true;
}

CharacterLoader::CharacterLoader(ItemCatalog& catalog, EquipmentRules& rules, ICharacterDatabase& database)
    : m_catalog(catalog), m_rules(rules), m_database(database) {}

bool CharacterLoader::FetchLoginLoadout(const CharacterLoginContext& context, CharacterLoadout& out) {
    return m_database.FetchLoadout(context, out);
}

bool CharacterLoader::LoadModelForItem(const ItemDefinition& item,
                                       std::unique_ptr<Engine::Platform::Render::Model>& outModel) {
    if (item.modelPath.empty() || !FileExists(item.modelPath)) {
        LOG_WARNING_FMT("CharacterLoader", "Model missing for item '%s' at %s",
                        item.id.c_str(), item.modelPath.c_str());
        return false;
    }

    outModel = std::make_unique<Engine::Platform::Render::Model>();
    try {
        outModel->LoadModel(item.modelPath);
    } catch (const std::exception& e) {
        LOG_ERROR_FMT("CharacterLoader", "Failed to load %s: %s", item.modelPath.c_str(), e.what());
        outModel.reset();
        return false;
    }
    return true;
}

bool CharacterLoader::LoadBody(Registry& registry,
                               CharacterInstance& instance,
                               Engine::EntityID rootEntity,
                               const std::string& bodyItemId) {
    const ItemDefinition* bodyItem = m_catalog.Find(bodyItemId);
    if (!bodyItem) {
        bodyItem = m_catalog.DefaultBody();
    }
    if (!bodyItem) {
        LOG_ERROR("CharacterLoader", "No body item definition available");
        return false;
    }

    if (!LoadModelForItem(*bodyItem, instance.m_bodyModel)) {
        return false;
    }

    instance.rootEntity = rootEntity;
    instance.height = instance.m_bodyModel->GetHeight();
    instance.bottomY = instance.m_bodyModel->GetBottomY();
    instance.scale = 1.0f;
    if (instance.height > 0.01f &&
        (instance.height > 5.0f || instance.height < 0.5f)) {
        instance.scale = CharacterInstance::kTargetHeightMeters / instance.height;
    }

    auto& mesh = registry.AddComponent<MeshComponent>(rootEntity);
    mesh.model = instance.m_bodyModel.get();

    auto& material = registry.AddComponent<MaterialComponent>(rootEntity);
    material.specularIntensity = 0.5f;
    material.shininess = 32.0f;

    if (TransformComponent* transform = registry.GetComponent<TransformComponent>(rootEntity)) {
        transform->SetScale(instance.scale);
    }

    LoadSkinnedBodyAnimations(bodyItem->modelPath, instance);

    LOG_INFO_FMT("CharacterLoader",
                 "Body '%s' loaded (height=%.2f, scale=%.4f)",
                 bodyItem->id.c_str(),
                 instance.height,
                 instance.scale);
    return true;
}

bool CharacterLoader::AttachEquipment(Registry& registry,
                                      CharacterInstance& instance,
                                      const CharacterLoadout& loadout) {
    for (const EquippedItem& equipped : loadout.equipment) {
        if (equipped.slot == EquipSlot::Body || equipped.itemId.empty()) {
            continue;
        }

        const ItemDefinition* item = m_catalog.Find(equipped.itemId);
        if (!item) {
            LOG_WARNING_FMT("CharacterLoader", "Unknown item id '%s'", equipped.itemId.c_str());
            continue;
        }

        const EquipSlot slot = equipped.slot != EquipSlot::Unknown ? equipped.slot : item->slot;
        if (slot == EquipSlot::Body || slot == EquipSlot::Unknown) {
            continue;
        }

        std::unique_ptr<Engine::Platform::Render::Model> itemModel;
        if (!LoadModelForItem(*item, itemModel)) {
            continue;
        }

        const Engine::ECS::Entity equipmentEntity = registry.CreateEntity();
        auto& mesh = registry.AddComponent<MeshComponent>(equipmentEntity.id);
        mesh.model = itemModel.get();

        auto& material = registry.AddComponent<MaterialComponent>(equipmentEntity.id);
        material.specularIntensity = 0.5f;
        material.shininess = 32.0f;

        const AttachmentRule& rule = m_rules.Get(slot);
        auto& attachment = registry.AddComponent<EquipmentAttachmentComponent>(equipmentEntity.id);
        attachment.parentEntity = instance.rootEntity;
        attachment.localOffset = rule.localOffset;
        attachment.localRotationDegrees = rule.localRotationDegrees;
        attachment.localScale = rule.localScale;
        attachment.inheritParentYaw = rule.inheritParentYaw;
        attachment.inheritParentScale = rule.inheritParentScale;

        registry.AddComponent<TransformComponent>(equipmentEntity.id);
        instance.m_itemModels.push_back(std::move(itemModel));
        instance.m_equipmentEntities.push_back(equipmentEntity.id);

        LOG_INFO_FMT("CharacterLoader",
                     "Equipped '%s' on slot %s",
                     item->id.c_str(),
                     EquipSlotToString(slot));
    }

    return true;
}

bool CharacterLoader::ApplyLoadout(Registry& registry,
                                 CharacterInstance& instance,
                                 Engine::EntityID rootEntity,
                                 const CharacterLoadout& loadout) {
    instance.Clear(registry);
    if (!LoadBody(registry, instance, rootEntity, loadout.bodyItemId)) {
        return false;
    }
    return AttachEquipment(registry, instance, loadout);
}

bool CharacterLoader::LoginAndBuild(Registry& registry,
                                    CharacterInstance& instance,
                                    Engine::EntityID rootEntity,
                                    const CharacterLoginContext& context) {
    CharacterLoadout loadout;
    if (!FetchLoginLoadout(context, loadout)) {
        return false;
    }
    return ApplyLoadout(registry, instance, rootEntity, loadout);
}

void CharacterLoader::PlaceOnTerrain(Registry& registry,
                                     Engine::EntityID rootEntity,
                                     const CharacterInstance& instance,
                                     float worldX,
                                     float worldZ,
                                     float terrainHeight) const {
    TransformComponent* transform = registry.GetComponent<TransformComponent>(rootEntity);
    if (!transform) {
        return;
    }

    float adjustedY = terrainHeight;
    if (instance.m_bodyModel && terrainHeight == 0.0f) {
        const float footOffset = -instance.bottomY * instance.scale;
        if (std::isfinite(footOffset) && footOffset > -10.0f && footOffset < 10.0f) {
            adjustedY = footOffset;
        }
    }

    transform->SetPosition(worldX, adjustedY, worldZ);
    transform->SetScale(instance.scale);
}

} // namespace Character
} // namespace Game
