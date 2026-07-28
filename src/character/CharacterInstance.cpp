#include "character/CharacterInstance.h"

#include "core/Types.h"
#include "core/ecs/Registry.h"
#include "render/components/AnimationComponent.h"
#include "render/components/MeshComponent.h"
#include "render/components/MaterialComponent.h"
#include "render/Model.h"
#include "render/Skeleton.h"
#include "render/Animation.h"
#include "character/EquipmentAttachmentComponent.h"

using Engine::ECS::Registry;
using Engine::Platform::Render::Components::AnimationComponent;
using Engine::Platform::Render::Components::MaterialComponent;
using Engine::Platform::Render::Components::MeshComponent;

namespace Game {
namespace Character {

CharacterInstance::~CharacterInstance() = default;

Engine::Platform::Render::Model* CharacterInstance::GetBodyModel() const {
    return m_bodyAssets && m_bodyAssets->bodyModel ? m_bodyAssets->bodyModel.get() : nullptr;
}

float CharacterInstance::GetHeadHeightOffset() const {
    if (height > 0.01f) {
        return height * scale;
    }
    return 1.7f;
}

bool CharacterInstance::HasSkinnedBody() const {
    return m_bodyAssets && m_bodyAssets->skeleton && m_bodyAssets->skeleton->IsLoaded();
}

void CharacterInstance::SetBodyAssets(std::shared_ptr<const CharacterBodyAssets> assets) {
    m_bodyAssets = std::move(assets);
    if (!m_bodyAssets) {
        height = 0.0f;
        bottomY = 0.0f;
        scale = 1.0f;
        return;
    }

    height = m_bodyAssets->height;
    bottomY = m_bodyAssets->bottomY;
    scale = m_bodyAssets->scale;
}

void CharacterInstance::WireAnimationComponent(Registry& registry) {
    if (rootEntity == Engine::INVALID_ENTITY || !HasSkinnedBody()) {
        return;
    }

    AnimationComponent* animComp = registry.GetComponent<AnimationComponent>(rootEntity);
    if (!animComp) {
        animComp = &registry.AddComponent<AnimationComponent>(rootEntity);
    }

    animComp->skeleton = m_bodyAssets->skeleton.get();
    animComp->idleAnimation = m_bodyAssets->idleAnimation.get();
    animComp->walkingAnimation = m_bodyAssets->walkingAnimation.get();
    animComp->runningAnimation = m_bodyAssets->runningAnimation.get();
    animComp->jumpAnimation = m_bodyAssets->jumpAnimation.get();
    animComp->jumpMovingAnimation = m_bodyAssets->jumpMovingAnimation.get();
    animComp->fallingAnimation = m_bodyAssets->fallingAnimation.get();
    animComp->landingAnimation = m_bodyAssets->landingAnimation.get();
    animComp->leftTurnAnimation = m_bodyAssets->leftTurnAnimation.get();
    animComp->rightTurnAnimation = m_bodyAssets->rightTurnAnimation.get();
    if (!animComp->currentAnimation) {
        animComp->currentAnimation = animComp->idleAnimation;
        animComp->currentAnimationTime = 0.0f;
        animComp->isLooping = true;
    }
}

void CharacterInstance::Clear(Registry& registry) {
    for (Engine::EntityID equipmentEntity : m_equipmentEntities) {
        if (equipmentEntity != Engine::INVALID_ENTITY) {
            registry.DestroyEntity(equipmentEntity);
        }
    }
    m_equipmentEntities.clear();
    m_itemModels.clear();

    if (rootEntity != Engine::INVALID_ENTITY) {
        if (AnimationComponent* animComp = registry.GetComponent<AnimationComponent>(rootEntity)) {
            animComp->skeleton = nullptr;
            animComp->idleAnimation = nullptr;
            animComp->walkingAnimation = nullptr;
            animComp->runningAnimation = nullptr;
            animComp->jumpAnimation = nullptr;
            animComp->jumpMovingAnimation = nullptr;
            animComp->fallingAnimation = nullptr;
            animComp->landingAnimation = nullptr;
            animComp->leftTurnAnimation = nullptr;
            animComp->rightTurnAnimation = nullptr;
            animComp->currentAnimation = nullptr;
            animComp->currentAnimationTime = 0.0f;
            animComp->landingActive = false;
            animComp->wasAirborneLastFrame = false;
            animComp->fallingTimeThisAirtime = 0.0f;
        }
        if (registry.HasComponent<MeshComponent>(rootEntity)) {
            registry.RemoveComponent<MeshComponent>(rootEntity);
        }
        if (registry.HasComponent<MaterialComponent>(rootEntity)) {
            registry.RemoveComponent<MaterialComponent>(rootEntity);
        }
    }

    m_bodyAssets.reset();
    scale = 1.0f;
    height = 0.0f;
    bottomY = 0.0f;
}

} // namespace Character
} // namespace Game
