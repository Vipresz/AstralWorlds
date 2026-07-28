#include "character/CharacterBodyAssetCache.h"

#include "Logger.h"
#include "character/CharacterInstance.h"
#include "render/Animation.h"
#include "render/MeshAssetResolve.h"
#include "render/Model.h"
#include "render/Skeleton.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/config.h>

#include <chrono>
#include <fstream>
#include <thread>

using Engine::Platform::Render::Animation;
using Engine::Platform::Render::Model;
using Engine::Platform::Render::Skeleton;
using EngineLogging::Logger;

namespace Game {
namespace Character {

namespace {

bool FileExists(const std::string& path) {
    std::ifstream file(path);
    return file.good();
}

constexpr unsigned int kBodyAssimpFlags =
    aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_GenSmoothNormals |
    aiProcess_JoinIdenticalVertices | aiProcess_LimitBoneWeights;

const aiScene* ReadBodySceneWithRetry(Assimp::Importer& importer, const std::string& pathToRead) {
    importer.SetPropertyBool(AI_CONFIG_IMPORT_FBX_PRESERVE_PIVOTS, false);
    for (int attempt = 0; attempt < 4; ++attempt) {
        const aiScene* scene = importer.ReadFile(pathToRead, kBodyAssimpFlags);
        if (scene && !(scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) && scene->mRootNode) {
            return scene;
        }
        LOG_WARNING_FMT("CharacterBodyAssetCache",
                        "Assimp body read failed (attempt %d) for %s: %s",
                        attempt + 1,
                        pathToRead.c_str(),
                        importer.GetErrorString());
        std::this_thread::sleep_for(std::chrono::milliseconds(40 * (attempt + 1)));
    }
    return nullptr;
}

bool LoadAnimationClip(const std::string& path,
                       std::shared_ptr<Animation>& outAnimation,
                       bool stripRootMotion = false,
                       bool stripVerticalRootMotion = false) {
    if (!FileExists(path)) {
        return false;
    }
    auto animation = std::make_shared<Animation>();
    if (!animation->LoadAnimation(path, stripRootMotion, stripVerticalRootMotion)) {
        return false;
    }
    outAnimation = std::move(animation);
    return true;
}

bool LoadLocomotionClips(const std::shared_ptr<Skeleton>& skeleton,
                         const std::shared_ptr<CharacterBodyAssets>& assets) {
    if (!assets) {
        return false;
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

    LoadAnimationClip(kIdleAnim, assets->idleAnimation);
    LoadAnimationClip(kWalkAnim, assets->walkingAnimation, true);
    LoadAnimationClip(kRunAnim, assets->runningAnimation, true);
    LoadAnimationClip(kJumpInPlaceAnim, assets->jumpAnimation, true, true);
    LoadAnimationClip(kJumpMovingAnim, assets->jumpMovingAnimation, true, true);
    LoadAnimationClip(kFallingAnim, assets->fallingAnimation, true, true);
    LoadAnimationClip(kLandingAnim, assets->landingAnimation, true, true);
    LoadAnimationClip(kLeftTurnAnim, assets->leftTurnAnimation, true);
    LoadAnimationClip(kRightTurnAnim, assets->rightTurnAnimation, true);

    if (!skeleton) {
        return false;
    }

    auto countChannelMatches = [&](const std::shared_ptr<Animation>& animation) -> size_t {
        if (!animation) {
            return 0;
        }
        size_t matches = 0;
        for (const std::string& channelName : animation->GetBoneNames()) {
            if (skeleton->GetBone(channelName) != nullptr) {
                ++matches;
            }
        }
        return matches;
    };

    const size_t walkMatches = countChannelMatches(assets->walkingAnimation);
    const size_t runMatches = countChannelMatches(assets->runningAnimation);
    const size_t walkChannels =
        assets->walkingAnimation ? assets->walkingAnimation->GetBoneNames().size() : 0;
    const size_t runChannels =
        assets->runningAnimation ? assets->runningAnimation->GetBoneNames().size() : 0;

    LOG_INFO_FMT("CharacterBodyAssetCache",
                 "Locomotion clips loaded (bones=%zu walk=%zu/%zu run=%zu/%zu)",
                 skeleton->GetBoneCount(),
                 walkMatches,
                 walkChannels,
                 runMatches,
                 runChannels);
    return true;
}

std::shared_ptr<const CharacterBodyAssets> BuildBodyAssets(const std::string& modelPath) {
    const std::string resolved = Engine::Platform::Render::ResolveMeshLoadPath(modelPath);
    const std::string pathToRead = resolved.empty() ? modelPath : resolved;
    if (!FileExists(pathToRead)) {
        LOG_WARNING_FMT("CharacterBodyAssetCache", "Model missing at %s", pathToRead.c_str());
        return {};
    }

    Assimp::Importer importer;
    const aiScene* scene = ReadBodySceneWithRetry(importer, pathToRead);
    if (!scene) {
        LOG_ERROR_FMT("CharacterBodyAssetCache", "Failed to import body FBX: %s", pathToRead.c_str());
        return {};
    }

    auto assets = std::make_shared<CharacterBodyAssets>();
    assets->bodyModel = std::make_shared<Model>();
  if (!assets->bodyModel->LoadModelFromScene(scene, pathToRead)) {
    return {};
  }

  assets->skeleton = std::make_shared<Skeleton>();
  if (!assets->skeleton->LoadSkeleton(scene)) {
    return {};
  }

  {
    std::vector<std::string> meshBoneNames;
    meshBoneNames.reserve(assets->bodyModel->GetBoneCount());
    for (size_t i = 0; i < assets->bodyModel->GetBoneCount(); ++i) {
      meshBoneNames.push_back(
          assets->bodyModel->GetBoneNameByIndex(static_cast<unsigned int>(i)));
    }
    if (!assets->skeleton->AlignBoneIdsToMeshOrder(meshBoneNames)) {
      LOG_WARNING("CharacterBodyAssetCache",
                  "Skeleton bone IDs could not be fully aligned to mesh VBO indices");
    }
  }

  LoadLocomotionClips(assets->skeleton, assets);

  assets->height = assets->bodyModel->GetHeight();
  assets->bottomY = assets->bodyModel->GetBottomY();
  assets->scale = 1.0f;
  if (assets->height > 0.01f && (assets->height > 5.0f || assets->height < 0.5f)) {
    assets->scale = CharacterInstance::kTargetHeightMeters / assets->height;
  }

  LOG_INFO_FMT("CharacterBodyAssetCache",
               "Cached body from %s (height=%.2f scale=%.4f)",
               pathToRead.c_str(),
               assets->height,
               assets->scale);
  return assets;
}

} // namespace

CharacterBodyAssetCache& CharacterBodyAssetCache::Instance() {
    static CharacterBodyAssetCache cache;
    return cache;
}

std::shared_ptr<const CharacterBodyAssets> CharacterBodyAssetCache::GetOrLoad(
    const std::string& cacheKey,
    const std::string& modelPath) {
    if (cacheKey.empty() || modelPath.empty()) {
        return {};
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        const auto existing = m_entries.find(cacheKey);
        if (existing != m_entries.end()) {
            if (auto cached = existing->second.lock()) {
                return cached;
            }
        }
    }

    std::shared_ptr<const CharacterBodyAssets> loaded = BuildBodyAssets(modelPath);
    if (!loaded) {
        return {};
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    m_entries[cacheKey] = loaded;
    return loaded;
}

void CharacterBodyAssetCache::Clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_entries.clear();
}

size_t CharacterBodyAssetCache::EntryCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_entries.size();
}

} // namespace Character
} // namespace Game
