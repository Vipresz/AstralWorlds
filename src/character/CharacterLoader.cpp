#include "character/CharacterLoader.h"

#include "Logger.h"
#include "character/CharacterBodyAssetCache.h"
#include "core/ecs/Entity.h"
#include "core/ecs/Registry.h"
#include "character/CharacterDatabase.h"
#include "character/EquipmentAttachmentComponent.h"
#include "render/AssetManager.h"
#include "render/Model.h"
#include "render/components/TransformComponent.h"
#include "render/components/MeshComponent.h"
#include "render/components/MaterialComponent.h"

#include <cmath>
#include <fstream>

using Engine::ECS::Registry;
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

} // namespace

CharacterLoader::CharacterLoader(ItemCatalog& catalog, EquipmentRules& rules, ICharacterDatabase& database)
    : m_catalog(catalog), m_rules(rules), m_database(database) {}

bool CharacterLoader::FetchLoginLoadout(const CharacterLoginContext& context, CharacterLoadout& out) {
    return m_database.FetchLoadout(context, out);
}

bool CharacterLoader::BuildDefaultLoadout(CharacterLoadout& out) const {
    const ItemDefinition* body = m_catalog.DefaultBody();
    if (!body) {
        return false;
    }
    out = CharacterLoadout{};
    out.bodyItemId = body->id;
    return true;
}

bool CharacterLoader::LoadModelForItem(const ItemDefinition& item,
                                       std::shared_ptr<Engine::Platform::Render::Model>& outModel) {
    if (item.modelPath.empty() || !FileExists(item.modelPath)) {
        LOG_WARNING_FMT("CharacterLoader", "Model missing for item '%s' at %s",
                        item.id.c_str(), item.modelPath.c_str());
        return false;
    }

    std::shared_ptr<Engine::Platform::Render::Model> cached =
        Engine::Platform::Render::AssetManager::Instance().LoadModel(item.modelPath);
    if (!cached) {
        LOG_ERROR_FMT("CharacterLoader", "Failed to load %s via AssetManager", item.modelPath.c_str());
        return false;
    }

    outModel = cached;
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

    std::shared_ptr<const CharacterBodyAssets> bodyAssets =
        CharacterBodyAssetCache::Instance().GetOrLoad(bodyItem->id, bodyItem->modelPath);
    if (!bodyAssets || !bodyAssets->bodyModel) {
        LOG_ERROR_FMT("CharacterLoader", "Failed to load cached body for item '%s'", bodyItem->id.c_str());
        return false;
    }

    instance.SetBodyAssets(bodyAssets);
    instance.rootEntity = rootEntity;

    if (!registry.HasComponent<MeshComponent>(rootEntity)) {
        registry.AddComponent<MeshComponent>(rootEntity);
    }
    MeshComponent* mesh = registry.GetComponent<MeshComponent>(rootEntity);
    if (!mesh) {
        LOG_ERROR("CharacterLoader", "Failed to attach body mesh component");
        return false;
    }
    mesh->model = bodyAssets->bodyModel.get();

    if (!registry.HasComponent<MaterialComponent>(rootEntity)) {
        registry.AddComponent<MaterialComponent>(rootEntity);
    }
    MaterialComponent* material = registry.GetComponent<MaterialComponent>(rootEntity);
    if (!material) {
        LOG_ERROR("CharacterLoader", "Failed to attach body material component");
        return false;
    }
    material->specularIntensity = 0.5f;
    material->shininess = 32.0f;

    if (TransformComponent* transform = registry.GetComponent<TransformComponent>(rootEntity)) {
        transform->SetScale(instance.scale);
    }

    LOG_INFO_FMT("CharacterLoader",
                 "Body '%s' attached from cache (height=%.2f, scale=%.4f)",
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

        std::shared_ptr<Engine::Platform::Render::Model> itemModel;
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
    if (instance.GetBodyModel() && terrainHeight == 0.0f) {
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
