#include "character/CharacterInstance.h"

#include "core/Types.h"
#include "core/ecs/Registry.h"
#include "render/components/MeshComponent.h"
#include "render/components/MaterialComponent.h"
#include "character/EquipmentAttachmentComponent.h"

using Engine::ECS::Registry;
using Engine::Platform::Render::Components::MaterialComponent;
using Engine::Platform::Render::Components::MeshComponent;

namespace Game {
namespace Character {

float CharacterInstance::GetHeadHeightOffset() const {
    if (height > 0.01f) {
        return height * scale;
    }
    return 1.7f;
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
