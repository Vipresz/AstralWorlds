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

float CharacterInstance::GetHeadHeightOffset() const {
    if (height > 0.01f) {
        return height * scale;
    }
    return 1.7f;
}

bool CharacterInstance::HasSkinnedBody() const {
    return m_skeleton != nullptr && m_skeleton->IsLoaded();
}

void CharacterInstance::WireAnimationComponent(Registry& registry) {
    if (rootEntity == Engine::INVALID_ENTITY || !m_skeleton || !m_skeleton->IsLoaded()) {
        return;
    }

    AnimationComponent* animComp = registry.GetComponent<AnimationComponent>(rootEntity);
    if (!animComp) {
        return;
    }

    animComp->skeleton = m_skeleton.get();
    animComp->idleAnimation = m_idleAnimation.get();
    animComp->walkingAnimation = m_walkingAnimation.get();
    animComp->runningAnimation = m_runningAnimation.get();
    animComp->jumpAnimation = m_jumpAnimation.get();
    animComp->currentAnimation = animComp->idleAnimation;
    animComp->currentAnimationTime = 0.0f;
    animComp->isLooping = true;
}

void CharacterInstance::Clear(Registry& registry) {
    for (Engine::EntityID equipmentEntity : m_equipmentEntities) {
        if (equipmentEntity != Engine::INVALID_ENTITY) {
            registry.DestroyEntity(equipmentEntity);
        }
    }
    m_equipmentEntities.clear();
    m_itemModels.clear();

    m_jumpAnimation.reset();
    m_runningAnimation.reset();
    m_walkingAnimation.reset();
    m_idleAnimation.reset();
    m_skeleton.reset();

    if (rootEntity != Engine::INVALID_ENTITY) {
        if (AnimationComponent* animComp = registry.GetComponent<AnimationComponent>(rootEntity)) {
            animComp->skeleton = nullptr;
            animComp->idleAnimation = nullptr;
            animComp->walkingAnimation = nullptr;
            animComp->runningAnimation = nullptr;
            animComp->jumpAnimation = nullptr;
            animComp->currentAnimation = nullptr;
            animComp->currentAnimationTime = 0.0f;
        }
        if (registry.HasComponent<MeshComponent>(rootEntity)) {
            registry.RemoveComponent<MeshComponent>(rootEntity);
        }
        if (registry.HasComponent<MaterialComponent>(rootEntity)) {
            registry.RemoveComponent<MaterialComponent>(rootEntity);
        }
    }

    if (m_bodyModel) {
        m_bodyModel->ClearModel();
        m_bodyModel.reset();
    }

    scale = 1.0f;
    height = 0.0f;
    bottomY = 0.0f;
}

} // namespace Character
} // namespace Game
