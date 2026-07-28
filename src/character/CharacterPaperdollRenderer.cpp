#include "character/CharacterPaperdollRenderer.h"

#include "character/EquipmentAttachmentComponent.h"
#include "core/ecs/Registry.h"
#include "render/Animation.h"
#include "render/Shader.h"
#include "render/components/MaterialComponent.h"
#include "render/components/MeshComponent.h"
#include "render/components/AnimationComponent.h"
#include "render/Skeleton.h"
#include "render/Model.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <cmath>
#include <map>
#include <vector>

using Engine::ECS::Registry;
using Engine::Platform::Render::Components::MaterialComponent;
using Engine::Platform::Render::Components::MeshComponent;
using Engine::Platform::Render::Components::AnimationComponent;
using Engine::Platform::Render::Model;
using Engine::Platform::Render::Shader;

namespace Game {
namespace Character {

namespace {

glm::vec3 RotateOffsetByYaw(const glm::vec3& offset, float yawDegrees) {
    const float yawRad = glm::radians(yawDegrees);
    const float cosYaw = std::cos(yawRad);
    const float sinYaw = std::sin(yawRad);
    return glm::vec3(
        offset.x * cosYaw + offset.z * sinYaw,
        offset.y,
        -offset.x * sinYaw + offset.z * cosYaw);
}

glm::mat4 MatrixFromTransform(const glm::vec3& position,
                              const glm::vec3& rotationDegrees,
                              const glm::vec3& scale) {
    glm::mat4 transform = glm::mat4(1.0f);
    transform = glm::translate(transform, position);
    transform = glm::rotate(transform, glm::radians(rotationDegrees.z), glm::vec3(0.0f, 0.0f, 1.0f));
    transform = glm::rotate(transform, glm::radians(rotationDegrees.y), glm::vec3(0.0f, 1.0f, 0.0f));
    transform = glm::rotate(transform, glm::radians(rotationDegrees.x), glm::vec3(1.0f, 0.0f, 0.0f));
    transform = glm::scale(transform, scale);
    return transform;
}

constexpr int kMaxShaderBones = 100;

void UploadBoneUniforms(GLuint shaderProgramID, const Engine::Platform::Render::Skeleton* skeleton) {
    const GLint uniformBoneCount = glGetUniformLocation(shaderProgramID, "boneCount");
    const GLint uniformBoneTransforms = glGetUniformLocation(shaderProgramID, "boneTransforms");

    int boneCount = 0;
    if (skeleton && skeleton->IsLoaded() && !skeleton->IsBindPoseActive()) {
        std::vector<glm::mat4> boneMatrices;
        skeleton->GetFinalBoneMatrices(boneMatrices);
        boneCount = static_cast<int>(std::min(boneMatrices.size(), static_cast<size_t>(kMaxShaderBones)));
        if (uniformBoneTransforms != -1 && boneCount > 0) {
            glUniformMatrix4fv(uniformBoneTransforms, boneCount, GL_FALSE, glm::value_ptr(boneMatrices[0]));
        }
    }

    if (uniformBoneCount != -1) {
        glUniform1i(uniformBoneCount, boneCount);
    }
}

struct SavedSkeletonPose {
    bool bindPoseActive = true;
    std::vector<std::pair<std::string, glm::mat4>> boneFinals;
};

SavedSkeletonPose CaptureSkeletonPose(Engine::Platform::Render::Skeleton& skeleton) {
    SavedSkeletonPose saved;
    saved.bindPoseActive = skeleton.IsBindPoseActive();
    saved.boneFinals.reserve(skeleton.GetBoneCount());
    for (const auto& pair : skeleton.GetBones()) {
        saved.boneFinals.emplace_back(pair.first, pair.second.finalTransformation);
    }
    return saved;
}

void RestoreSkeletonPose(Engine::Platform::Render::Skeleton& skeleton, const SavedSkeletonPose& saved) {
    if (saved.bindPoseActive) {
        skeleton.ResetToBindPose();
        return;
    }

    for (const auto& entry : saved.boneFinals) {
        if (Engine::Platform::Render::Bone* bone = skeleton.GetBone(entry.first)) {
            bone->finalTransformation = entry.second;
        }
    }
    skeleton.SetBindPoseActive(false);
}

void ApplyStandingIdlePose(Engine::Platform::Render::Skeleton& skeleton,
                           const Engine::Platform::Render::Animation& idleAnimation,
                           float& idleAnimationTimeSeconds,
                           float deltaSeconds) {
    const float ticksPerSecond = idleAnimation.GetTicksPerSecond();
    const float durationTicks = idleAnimation.GetDuration();
    if (ticksPerSecond <= 0.0f) {
        skeleton.ResetToBindPose();
        return;
    }

    idleAnimationTimeSeconds += std::max(0.0f, deltaSeconds);
    const float durationSeconds = durationTicks / ticksPerSecond;
    if (durationSeconds > 0.0f) {
        idleAnimationTimeSeconds = std::fmod(idleAnimationTimeSeconds, durationSeconds);
        if (idleAnimationTimeSeconds < 0.0f) {
            idleAnimationTimeSeconds += durationSeconds;
        }
    }

    std::map<std::string, glm::mat4> transforms;
    idleAnimation.GetBoneTransforms(idleAnimationTimeSeconds * ticksPerSecond, transforms);
    skeleton.UpdateBoneTransforms(transforms);
}

glm::mat4 BuildPreviewRootMatrix(const CharacterInstance& instance, float yawDegrees) {
    const float uniformScale = instance.scale;
    const float footY = -instance.bottomY * uniformScale;
    const float worldMinY = footY + instance.bottomY * uniformScale;
    const float worldMaxY = footY + (instance.bottomY + instance.height) * uniformScale;
    const float centerY = (worldMinY + worldMaxY) * 0.5f;

    // Rotate around the vertical body-center axis, then keep feet planted.
    glm::mat4 transform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, centerY, 0.0f));
    transform = glm::rotate(transform, glm::radians(yawDegrees), glm::vec3(0.0f, 1.0f, 0.0f));
    transform = glm::translate(transform, glm::vec3(0.0f, footY - centerY, 0.0f));
    transform = glm::scale(transform, glm::vec3(uniformScale));
    return transform;
}

glm::mat4 BuildPreviewEquipmentMatrix(const CharacterInstance& instance,
                                      const EquipmentAttachmentComponent& attachment,
                                      float yawDegrees) {
    const float uniformScale = instance.scale;
    const float footY = -instance.bottomY * uniformScale;

    const glm::vec3 parentPosition(0.0f, footY, 0.0f);
    const glm::vec3 parentRotation(0.0f, yawDegrees, 0.0f);
    const glm::vec3 parentScale(uniformScale);

    glm::vec3 worldOffset = attachment.localOffset;
    if (attachment.inheritParentYaw) {
        worldOffset = RotateOffsetByYaw(worldOffset, parentRotation.y);
    }
    if (attachment.inheritParentScale) {
        worldOffset *= parentScale.x;
    }

    const glm::vec3 childPosition = parentPosition + worldOffset;
    const glm::vec3 childRotation = parentRotation + attachment.localRotationDegrees;
    const glm::vec3 childScale = attachment.inheritParentScale
                                     ? parentScale * attachment.localScale
                                     : attachment.localScale;
    return MatrixFromTransform(childPosition, childRotation, childScale);
}

struct PaperdollCamera {
    glm::vec3 eye;
    glm::vec3 center;
    float fovDegrees = 32.0f;
};

PaperdollCamera ComputePaperdollCamera(const CharacterInstance& instance, float aspect) {
    const float uniformScale = instance.scale;
    const float footY = -instance.bottomY * uniformScale;
    const float worldMinY = footY + instance.bottomY * uniformScale;
    const float worldMaxY = footY + (instance.bottomY + instance.height) * uniformScale;
    const float centerY = (worldMinY + worldMaxY) * 0.5f;
    const float frameHeight = std::max(0.5f, worldMaxY - worldMinY);
    const float frameWidth = frameHeight * 0.42f;

    constexpr float kFovDegrees = 32.0f;
    const float fovRad = glm::radians(kFovDegrees);
    const float margin = 1.22f;
    const float halfFovY = fovRad * 0.5f;
    const float halfFovX = std::atan(std::tan(halfFovY) * std::max(0.1f, aspect));

    float distance = (frameHeight * 0.5f * margin) / std::tan(halfFovY);
    const float distanceForWidth = (frameWidth * 0.5f * margin) / std::tan(halfFovX);
    distance = std::max(distance, distanceForWidth);

    PaperdollCamera camera;
    camera.center = glm::vec3(0.0f, centerY, 0.0f);
    camera.eye = glm::vec3(0.0f, centerY, distance);
    camera.fovDegrees = kFovDegrees;
    return camera;
}

struct SavedGlState {
    GLint framebuffer = 0;
    GLint viewport[4]{};
    GLboolean depthTest = GL_FALSE;
    GLboolean blend = GL_FALSE;
    GLint blendSrcRgb = 0;
    GLint blendDstRgb = 0;
    GLint blendSrcAlpha = 0;
    GLint blendDstAlpha = 0;
    GLint program = 0;
    GLint activeTexture = 0;
    GLint texture0 = 0;
    GLint texture1 = 0;
};

SavedGlState SaveGlState() {
    SavedGlState state;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &state.framebuffer);
    glGetIntegerv(GL_VIEWPORT, state.viewport);
    state.depthTest = glIsEnabled(GL_DEPTH_TEST);
    state.blend = glIsEnabled(GL_BLEND);
    glGetIntegerv(GL_BLEND_SRC_RGB, &state.blendSrcRgb);
    glGetIntegerv(GL_BLEND_DST_RGB, &state.blendDstRgb);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &state.blendSrcAlpha);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &state.blendDstAlpha);
    glGetIntegerv(GL_CURRENT_PROGRAM, &state.program);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &state.activeTexture);
    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &state.texture0);
    glActiveTexture(GL_TEXTURE1);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &state.texture1);
    glActiveTexture(state.activeTexture);
    return state;
}

void RestoreGlState(const SavedGlState& state) {
    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(state.framebuffer));
    glViewport(state.viewport[0], state.viewport[1], state.viewport[2], state.viewport[3]);
    if (state.depthTest) {
        glEnable(GL_DEPTH_TEST);
    } else {
        glDisable(GL_DEPTH_TEST);
    }
    if (state.blend) {
        glEnable(GL_BLEND);
    } else {
        glDisable(GL_BLEND);
    }
    glBlendFuncSeparate(
        static_cast<GLenum>(state.blendSrcRgb),
        static_cast<GLenum>(state.blendDstRgb),
        static_cast<GLenum>(state.blendSrcAlpha),
        static_cast<GLenum>(state.blendDstAlpha));
    glUseProgram(static_cast<GLuint>(state.program));
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(state.texture0));
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(state.texture1));
    glActiveTexture(state.activeTexture);
}

} // namespace

CharacterPaperdollRenderer::~CharacterPaperdollRenderer() {
    Shutdown();
}

void CharacterPaperdollRenderer::Shutdown() {
    DestroyFramebuffer();
    DestroyDefaultTextures();
    m_yawDegrees = 0.0f;
    m_idleAnimationTime = 0.0f;
    m_hasIdleAnimClock = false;
}

void CharacterPaperdollRenderer::EnsureDefaultTextures() {
    if (m_defaultShadowMap == 0) {
        glGenTextures(1, &m_defaultShadowMap);
        glBindTexture(GL_TEXTURE_2D, m_defaultShadowMap);
        const unsigned char whitePixel[4] = {255, 255, 255, 255};
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, whitePixel);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    if (m_defaultWhiteTexture == 0) {
        glGenTextures(1, &m_defaultWhiteTexture);
        glBindTexture(GL_TEXTURE_2D, m_defaultWhiteTexture);
        const unsigned char whitePixel[4] = {255, 255, 255, 255};
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, whitePixel);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    }

    glBindTexture(GL_TEXTURE_2D, 0);
}

void CharacterPaperdollRenderer::DestroyDefaultTextures() {
    if (m_defaultShadowMap != 0) {
        glDeleteTextures(1, &m_defaultShadowMap);
        m_defaultShadowMap = 0;
    }
    if (m_defaultWhiteTexture != 0) {
        glDeleteTextures(1, &m_defaultWhiteTexture);
        m_defaultWhiteTexture = 0;
    }
}

void CharacterPaperdollRenderer::DestroyFramebuffer() {
    if (m_depthRenderbuffer != 0) {
        glDeleteRenderbuffers(1, &m_depthRenderbuffer);
        m_depthRenderbuffer = 0;
    }
    if (m_colorTexture != 0) {
        glDeleteTextures(1, &m_colorTexture);
        m_colorTexture = 0;
    }
    if (m_fbo != 0) {
        glDeleteFramebuffers(1, &m_fbo);
        m_fbo = 0;
    }
    m_width = 0;
    m_height = 0;
}

void CharacterPaperdollRenderer::EnsureFramebuffer(int width, int height) {
    width = std::max(64, width);
    height = std::max(64, height);
    if (m_fbo != 0 && m_width == width && m_height == height) {
        return;
    }

    DestroyFramebuffer();

    glGenTextures(1, &m_colorTexture);
    glBindTexture(GL_TEXTURE_2D, m_colorTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenRenderbuffers(1, &m_depthRenderbuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, m_depthRenderbuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);

    glGenFramebuffers(1, &m_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_colorTexture, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_depthRenderbuffer);

    const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);

    if (status != GL_FRAMEBUFFER_COMPLETE) {
        DestroyFramebuffer();
        return;
    }

    m_width = width;
    m_height = height;
}

bool CharacterPaperdollRenderer::RenderScene(Registry& registry,
                                           Engine::EntityID characterEntity,
                                           const CharacterInstance& instance,
                                           Shader& shader,
                                           int viewportWidth,
                                           int viewportHeight) {
    std::vector<Engine::EntityID> renderEntities;
    renderEntities.push_back(characterEntity);
    const auto equipmentEntities = registry.View<EquipmentAttachmentComponent, MeshComponent>();
    for (Engine::EntityID entityId : equipmentEntities) {
        const EquipmentAttachmentComponent* attachment =
            registry.GetComponent<EquipmentAttachmentComponent>(entityId);
        if (attachment && attachment->parentEntity == characterEntity) {
            renderEntities.push_back(entityId);
        }
    }

    shader.UseShader();
    if (!shader.IsValid()) {
        return false;
    }

    const float aspect = static_cast<float>(viewportWidth) / static_cast<float>(viewportHeight);
    const PaperdollCamera camera = ComputePaperdollCamera(instance, aspect);
    const glm::mat4 view = glm::lookAt(camera.eye, camera.center, glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 projection =
        glm::perspective(glm::radians(camera.fovDegrees), aspect, 0.05f, 50.0f);
    projection = glm::scale(projection, glm::vec3(1.0f, -1.0f, 1.0f));

    const GLint uniformModel = shader.GetModelLocation();
    const GLint uniformView = shader.GetViewLocation();
    const GLint uniformProjection = shader.GetProjectionLocation();
    const GLint uniformEyePosition = shader.GetEyePositionLocation();
    if (uniformView == -1 || uniformProjection == -1) {
        return false;
    }

    glUniformMatrix4fv(uniformView, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(uniformProjection, 1, GL_FALSE, glm::value_ptr(projection));
    if (uniformEyePosition != -1) {
        glUniform3f(uniformEyePosition, camera.eye.x, camera.eye.y, camera.eye.z);
    }

    const GLuint shaderProgramID = shader.GetShaderID();
    const GLint uniformPointLightCount = glGetUniformLocation(shaderProgramID, "pointLightCount");
    const GLint uniformSpotLightCount = glGetUniformLocation(shaderProgramID, "spotLightCount");
    const GLint uniformDirectionalLightColour =
        glGetUniformLocation(shaderProgramID, "directionalLight.base.colour");
    const GLint uniformDirectionalLightAmbient =
        glGetUniformLocation(shaderProgramID, "directionalLight.base.ambientIntensity");
    const GLint uniformDirectionalLightDiffuse =
        glGetUniformLocation(shaderProgramID, "directionalLight.base.diffuseIntensity");
    const GLint uniformDirectionalLightDirection =
        glGetUniformLocation(shaderProgramID, "directionalLight.direction");

    if (uniformPointLightCount != -1) {
        glUniform1i(uniformPointLightCount, 0);
    }
    if (uniformSpotLightCount != -1) {
        glUniform1i(uniformSpotLightCount, 0);
    }
    if (uniformDirectionalLightColour != -1) {
        glUniform3f(uniformDirectionalLightColour, 1.0f, 1.0f, 1.0f);
    }
    if (uniformDirectionalLightAmbient != -1) {
        glUniform1f(uniformDirectionalLightAmbient, 0.4f);
    }
    if (uniformDirectionalLightDiffuse != -1) {
        glUniform1f(uniformDirectionalLightDiffuse, 0.8f);
    }
    if (uniformDirectionalLightDirection != -1) {
        glUniform3f(uniformDirectionalLightDirection, 0.2f, -0.8f, -0.4f);
    }

    shader.SetTexture(1);
    glm::mat4 lightTransform = glm::mat4(1.0f);
    shader.SetDirectionalLightTransform(&lightTransform);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_defaultShadowMap);
    const GLint uniformShadowMap = glGetUniformLocation(shaderProgramID, "directionalShadowMap");
    if (uniformShadowMap != -1) {
        glUniform1i(uniformShadowMap, 0);
    }

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_defaultWhiteTexture);

    const glm::mat4 previewRootMatrix = BuildPreviewRootMatrix(instance, m_yawDegrees);
    AnimationComponent* animComp = registry.GetComponent<AnimationComponent>(characterEntity);
    Engine::Platform::Render::Skeleton* skeleton =
        (animComp && animComp->skeleton && animComp->skeleton->IsLoaded()) ? animComp->skeleton
                                                                            : nullptr;

    SavedSkeletonPose savedPose;
    bool restoredPose = false;
    if (skeleton) {
        savedPose = CaptureSkeletonPose(*skeleton);

        const auto now = std::chrono::steady_clock::now();
        float deltaSeconds = 1.0f / 60.0f;
        if (m_hasIdleAnimClock) {
            deltaSeconds = std::chrono::duration<float>(now - m_lastIdleAnimTime).count();
            deltaSeconds = std::clamp(deltaSeconds, 0.0f, 0.05f);
        }
        m_lastIdleAnimTime = now;
        m_hasIdleAnimClock = true;

        if (animComp->idleAnimation && animComp->idleAnimation->IsLoaded()) {
            ApplyStandingIdlePose(
                *skeleton, *animComp->idleAnimation, m_idleAnimationTime, deltaSeconds);
        } else {
            skeleton->ResetToBindPose();
        }
        restoredPose = true;
    }

    UploadBoneUniforms(shaderProgramID, skeleton);

    bool drewAnyMesh = false;
    for (Engine::EntityID entityId : renderEntities) {
        const MeshComponent* meshComp = registry.GetComponent<MeshComponent>(entityId);
        if (!meshComp || !meshComp->IsLoaded() || !meshComp->model) {
            continue;
        }

        glm::mat4 modelMatrix = previewRootMatrix;
        if (entityId != characterEntity) {
            const EquipmentAttachmentComponent* attachment =
                registry.GetComponent<EquipmentAttachmentComponent>(entityId);
            if (attachment) {
                modelMatrix = BuildPreviewEquipmentMatrix(instance, *attachment, m_yawDegrees);
            }
        }

        if (uniformModel != -1) {
            glUniformMatrix4fv(uniformModel, 1, GL_FALSE, glm::value_ptr(modelMatrix));
        }

        GLfloat specularIntensity = 0.5f;
        GLfloat shininess = 32.0f;
        if (const MaterialComponent* materialComp = registry.GetComponent<MaterialComponent>(entityId)) {
            specularIntensity = materialComp->specularIntensity;
            shininess = materialComp->shininess;
        }

        const GLint uniformSpecularIntensity = shader.GetSpecularIntensityLocation();
        const GLint uniformShininess = shader.GetShininessLocation();
        if (uniformSpecularIntensity != -1) {
            glUniform1f(uniformSpecularIntensity, specularIntensity);
        }
        if (uniformShininess != -1) {
            glUniform1f(uniformShininess, shininess);
        }

        meshComp->model->RenderModelPerMeshTexture(shader.GetUseTextureLocation());
        drewAnyMesh = true;
    }

    if (restoredPose && skeleton) {
        RestoreSkeletonPose(*skeleton, savedPose);
    }

    return drewAnyMesh;
}

bool CharacterPaperdollRenderer::RenderToTexture(Registry& registry,
                                                   Engine::EntityID characterEntity,
                                                   const CharacterInstance& instance,
                                                   Shader& shader,
                                                   int pixelWidth,
                                                   int pixelHeight) {
    if (characterEntity == Engine::INVALID_ENTITY || !shader.IsValid()) {
        return false;
    }

    const MeshComponent* bodyMesh = registry.GetComponent<MeshComponent>(characterEntity);
    if (!bodyMesh || !bodyMesh->IsLoaded()) {
        return false;
    }

    const int renderWidth = std::clamp(pixelWidth, 64, 1024);
    const int renderHeight = std::clamp(pixelHeight, 64, 1024);
    EnsureFramebuffer(renderWidth, renderHeight);
    if (m_fbo == 0 || m_colorTexture == 0) {
        return false;
    }

    EnsureDefaultTextures();

    const SavedGlState savedState = SaveGlState();

    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glViewport(0, 0, renderWidth, renderHeight);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    const bool rendered =
        RenderScene(registry, characterEntity, instance, shader, renderWidth, renderHeight);

    RestoreGlState(savedState);
    return rendered;
}

} // namespace Character
} // namespace Game
