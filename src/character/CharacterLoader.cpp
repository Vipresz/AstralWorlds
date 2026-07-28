#include "character/CharacterLoader.h"

#include "Logger.h"
#include "core/ecs/Entity.h"
#include "core/ecs/Registry.h"
#include "character/CharacterDatabase.h"
#include "character/EquipmentAttachmentComponent.h"
#include "render/Animation.h"
#include "render/MeshAssetResolve.h"
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
#include <thread>
#include <chrono>
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

bool LoadAnimationClip(const std::string& path,
                       std::unique_ptr<Animation>& outAnimation,
                       bool stripRootMotion = false,
                       bool stripVerticalRootMotion = false) {
    if (!FileExists(path)) {
        return false;
    }
    outAnimation = std::make_unique<Animation>();
    if (!outAnimation->LoadAnimation(path, stripRootMotion, stripVerticalRootMotion)) {
        outAnimation.reset();
        return false;
    }
    return true;
}

} // namespace

namespace {

constexpr unsigned int kBodyAssimpFlags =
    aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_GenSmoothNormals |
    aiProcess_JoinIdenticalVertices | aiProcess_LimitBoneWeights;

const aiScene* ReadBodySceneWithRetry(Assimp::Importer& importer, const std::string& pathToRead) {
    importer.SetPropertyBool(AI_CONFIG_IMPORT_FBX_PRESERVE_PIVOTS, false);
    // OneDrive / AV can briefly lock the FBX right after a prior open; retry a few times.
    for (int attempt = 0; attempt < 4; ++attempt) {
        const aiScene* scene = importer.ReadFile(pathToRead, kBodyAssimpFlags);
        if (scene && !(scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) && scene->mRootNode) {
            return scene;
        }
        LOG_WARNING_FMT("CharacterLoader",
                        "Assimp body read failed (attempt %d) for %s: %s",
                        attempt + 1,
                        pathToRead.c_str(),
                        importer.GetErrorString());
        std::this_thread::sleep_for(std::chrono::milliseconds(40 * (attempt + 1)));
    }
    return nullptr;
}

} // namespace

bool CharacterLoader::LoadSkinnedBodyAnimations(const std::string& bodyModelPath,
                                                CharacterInstance& instance) {
    if (!instance.m_bodyModel || instance.m_bodyModel->GetBoneCount() == 0) {
        return false;
    }

    // Prefer skeleton already extracted from the same Assimp scene as the mesh.
    if (!instance.m_skeleton || !instance.m_skeleton->IsLoaded()) {
        const std::string resolved = Engine::Platform::Render::ResolveMeshLoadPath(bodyModelPath);
        const std::string pathToRead = resolved.empty() ? bodyModelPath : resolved;
        LOG_INFO_FMT("CharacterLoader", "Loading skeleton from %s", pathToRead.c_str());

        Assimp::Importer importer;
        const aiScene* scene = ReadBodySceneWithRetry(importer, pathToRead);
        if (!scene) {
            LOG_WARNING_FMT("CharacterLoader", "Assimp failed to load skeleton from %s", pathToRead.c_str());
            return false;
        }

        instance.m_skeleton = std::make_unique<Skeleton>();
        if (!instance.m_skeleton->LoadSkeleton(scene)) {
            LOG_WARNING_FMT("CharacterLoader", "Failed to extract skeleton from %s", pathToRead.c_str());
            instance.m_skeleton.reset();
            return false;
        }
    }

    {
        std::vector<std::string> meshBoneNames;
        meshBoneNames.reserve(instance.m_bodyModel->GetBoneCount());
        for (size_t i = 0; i < instance.m_bodyModel->GetBoneCount(); ++i) {
            meshBoneNames.push_back(
                instance.m_bodyModel->GetBoneNameByIndex(static_cast<unsigned int>(i)));
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
    constexpr const char* kJumpInPlaceAnim =
        "graphics/Models/character/animation/regular/Jump_inplace.fbx";
    constexpr const char* kJumpMovingAnim =
        "graphics/Models/character/animation/regular/Moving_jump.fbx";
    constexpr const char* kFallingAnim =
        "graphics/Models/character/animation/regular/Falling.fbx";
    constexpr const char* kLandingAnim =
        "graphics/Models/character/animation/regular/Landing.fbx";
    constexpr const char* kLeftTurnAnim =
        "graphics/Models/character/animation/regular/Left_turn.fbx";
    constexpr const char* kRightTurnAnim =
        "graphics/Models/character/animation/regular/Right_turn.fbx";

    LoadAnimationClip(kIdleAnim, instance.m_idleAnimation);
    LoadAnimationClip(kWalkAnim, instance.m_walkingAnimation, true);
    LoadAnimationClip(kRunAnim, instance.m_runningAnimation, true);
    // Jump/fall/land: strip all hips root translation — physics owns position; clips are pose-only.
    LoadAnimationClip(kJumpInPlaceAnim, instance.m_jumpAnimation, true, true);
    LoadAnimationClip(kJumpMovingAnim, instance.m_jumpMovingAnimation, true, true);
    LoadAnimationClip(kFallingAnim, instance.m_fallingAnimation, true, true);
    LoadAnimationClip(kLandingAnim, instance.m_landingAnimation, true, true);
    // Standing turn-in-place: yaw is applied on the transform; strip planar root slide.
    LoadAnimationClip(kLeftTurnAnim, instance.m_leftTurnAnimation, true);
    LoadAnimationClip(kRightTurnAnim, instance.m_rightTurnAnimation, true);

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
    const size_t jumpMatches = countChannelMatches(instance.m_jumpAnimation.get());
    const size_t jumpMovingMatches = countChannelMatches(instance.m_jumpMovingAnimation.get());
    const size_t walkChannels =
        instance.m_walkingAnimation ? instance.m_walkingAnimation->GetBoneNames().size() : 0;
    const size_t runChannels =
        instance.m_runningAnimation ? instance.m_runningAnimation->GetBoneNames().size() : 0;
    const size_t jumpChannels =
        instance.m_jumpAnimation ? instance.m_jumpAnimation->GetBoneNames().size() : 0;
    const size_t jumpMovingChannels =
        instance.m_jumpMovingAnimation ? instance.m_jumpMovingAnimation->GetBoneNames().size() : 0;

    LOG_INFO_FMT("CharacterLoader",
                 "Skinned body animations loaded (bones=%zu, idle=%s walk=%s run=%s "
                 "jumpInPlace=%s jumpMoving=%s falling=%s landing=%s leftTurn=%s rightTurn=%s, "
                 "walkMatches=%zu/%zu runMatches=%zu/%zu jumpMatches=%zu/%zu jumpMovingMatches=%zu/%zu)",
                 instance.m_skeleton->GetBoneCount(),
                 instance.m_idleAnimation ? "yes" : "no",
                 instance.m_walkingAnimation ? "yes" : "no",
                 instance.m_runningAnimation ? "yes" : "no",
                 instance.m_jumpAnimation ? "yes" : "no",
                 instance.m_jumpMovingAnimation ? "yes" : "no",
                 instance.m_fallingAnimation ? "yes" : "no",
                 instance.m_landingAnimation ? "yes" : "no",
                 instance.m_leftTurnAnimation ? "yes" : "no",
                 instance.m_rightTurnAnimation ? "yes" : "no",
                 walkMatches,
                 walkChannels,
                 runMatches,
                 runChannels,
                 jumpMatches,
                 jumpChannels,
                 jumpMovingMatches,
                 jumpMovingChannels);

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

    if (bodyItem->modelPath.empty()) {
        LOG_WARNING_FMT("CharacterLoader", "Model path empty for item '%s'", bodyItem->id.c_str());
        return false;
    }

    // One Assimp import builds both the GPU mesh and the skeleton. Re-opening the same
    // FBX immediately after Model::LoadModel was flaky under OneDrive file locks and
    // could kill the process mid enter-world load.
    const std::string resolved = Engine::Platform::Render::ResolveMeshLoadPath(bodyItem->modelPath);
    const std::string pathToRead = resolved.empty() ? bodyItem->modelPath : resolved;
    if (!FileExists(pathToRead)) {
        LOG_WARNING_FMT("CharacterLoader", "Model missing for item '%s' at %s",
                        bodyItem->id.c_str(), pathToRead.c_str());
        return false;
    }
    if (!resolved.empty() && resolved != bodyItem->modelPath) {
        LOG_INFO_FMT("Model", "Resolved mesh path \"%s\" -> \"%s\"",
                     bodyItem->modelPath.c_str(), resolved.c_str());
    }

    Assimp::Importer importer;
    const aiScene* scene = ReadBodySceneWithRetry(importer, pathToRead);
    if (!scene) {
        LOG_ERROR_FMT("CharacterLoader", "Failed to import body FBX: %s", pathToRead.c_str());
        return false;
    }

    instance.m_bodyModel = std::make_unique<Engine::Platform::Render::Model>();
    if (!instance.m_bodyModel->LoadModelFromScene(scene, pathToRead)) {
        instance.m_bodyModel.reset();
        return false;
    }

    instance.m_skeleton = std::make_unique<Skeleton>();
    if (!instance.m_skeleton->LoadSkeleton(scene)) {
        LOG_WARNING_FMT("CharacterLoader", "Failed to extract skeleton from %s", pathToRead.c_str());
        instance.m_skeleton.reset();
        instance.m_bodyModel.reset();
        return false;
    }

    // Animation clips are separate FBXs; skeleton is already populated above.
    if (!LoadSkinnedBodyAnimations(bodyItem->modelPath, instance)) {
        LOG_WARNING("CharacterLoader", "Body animations failed to load");
    }

    instance.rootEntity = rootEntity;
    instance.height = instance.m_bodyModel->GetHeight();
    instance.bottomY = instance.m_bodyModel->GetBottomY();
    instance.scale = 1.0f;
    if (instance.height > 0.01f &&
        (instance.height > 5.0f || instance.height < 0.5f)) {
        instance.scale = CharacterInstance::kTargetHeightMeters / instance.height;
    }

    // Attach render components only after mesh + skeleton + clips are ready.
    MeshComponent* mesh = registry.GetComponent<MeshComponent>(rootEntity);
    if (!mesh) {
        mesh = &registry.AddComponent<MeshComponent>(rootEntity);
    }
    mesh->model = instance.m_bodyModel.get();

    MaterialComponent* material = registry.GetComponent<MaterialComponent>(rootEntity);
    if (!material) {
        material = &registry.AddComponent<MaterialComponent>(rootEntity);
    }
    material->specularIntensity = 0.5f;
    material->shininess = 32.0f;

    if (TransformComponent* transform = registry.GetComponent<TransformComponent>(rootEntity)) {
        transform->SetScale(instance.scale);
    }

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
    // Bind root before Clear so placeholder Mesh/Material from NetworkSystem
    // (AttachRemotePlayerVisual) are removed before LoadBody re-adds them.
    instance.rootEntity = rootEntity;
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
