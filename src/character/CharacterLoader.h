#pragma once

#include "character/CharacterInstance.h"
#include "character/CharacterTypes.h"
#include "character/EquipmentRules.h"
#include "character/ItemCatalog.h"

namespace Engine {
namespace ECS {
class Registry;
}
} // namespace Engine

namespace Game {
namespace Character {

class ICharacterDatabase;

// Builds character meshes from a server loadout and attachment rules.
class CharacterLoader {
public:
    CharacterLoader(ItemCatalog& catalog, EquipmentRules& rules, ICharacterDatabase& database);

    bool FetchLoginLoadout(const CharacterLoginContext& context, CharacterLoadout& out);
    bool ApplyLoadout(Engine::ECS::Registry& registry,
                      CharacterInstance& instance,
                      Engine::EntityID rootEntity,
                      const CharacterLoadout& loadout);
    bool LoginAndBuild(Engine::ECS::Registry& registry,
                       CharacterInstance& instance,
                       Engine::EntityID rootEntity,
                       const CharacterLoginContext& context);

    void PlaceOnTerrain(Engine::ECS::Registry& registry,
                        Engine::EntityID rootEntity,
                        const CharacterInstance& instance,
                        float worldX,
                        float worldZ,
                        float terrainHeight) const;

private:
    bool LoadBody(Engine::ECS::Registry& registry,
                  CharacterInstance& instance,
                  Engine::EntityID rootEntity,
                  const std::string& bodyItemId);
    bool AttachEquipment(Engine::ECS::Registry& registry,
                         CharacterInstance& instance,
                         const CharacterLoadout& loadout);
    bool LoadModelForItem(const ItemDefinition& item, std::unique_ptr<Engine::Platform::Render::Model>& outModel);

    ItemCatalog& m_catalog;
    EquipmentRules& m_rules;
    ICharacterDatabase& m_database;
};

} // namespace Character
} // namespace Game
