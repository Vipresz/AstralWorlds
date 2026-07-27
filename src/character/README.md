# Character loading

Characters are built at **login** from a server loadout: body model plus equipped items. Items are resolved through the item catalog, positioned with attachment rules, and rendered as child ECS entities.

## Flow

```
Login (offline or online)
  -> ICharacterDatabase::FetchLoadout()
  -> CharacterLoader::ApplyLoadout()
       -> load body mesh on root entity
       -> spawn equipment entities + EquipmentAttachmentComponent
  -> CharacterEquipmentSystem syncs equipment transforms each frame
```

## Data files (copied next to `game.exe` as `character/`)

| File | Purpose |
|------|---------|
| `items.json` | Item id → model path, slot |
| `equipment_rules.json` | Per-slot local offset / rotation / scale |
| `loadouts/default.json` | Fallback loadout when no user file exists |
| `loadouts/<userId>.json` | Per-account loadout (simulates server DB in dev) |

### Example loadout

```json
{
  "body_item_id": "body_male_default",
  "equipment": [
    { "item_id": "helm_starter", "slot": "head" },
    { "item_id": "sword_starter", "slot": "mainhand" }
  ]
}
```

Add matching entries to `items.json` with model paths under `graphics/`.

## Code map

| File | Role |
|------|------|
| `CharacterTypes.h` | `EquipSlot`, `CharacterLoadout`, login context |
| `ItemCatalog` | Resolves item ids to models |
| `EquipmentRules` | Attachment offsets per slot |
| `ServerLoadoutDatabase` | Loads `loadouts/<userId>.json` at login |
| `CharacterLoader` | Fetches loadout and builds ECS entities |
| `CharacterInstance` | Owns models + equipment entity ids |
| `CharacterEquipmentSystem` | Parents equipment to character transform |

## Extending

1. Add item models and register them in `items.json`.
2. Tune slot offsets in `equipment_rules.json`.
3. Replace `ServerLoadoutDatabase` with an HTTP/API client that returns the same JSON shape.
4. Sync remote player loadouts over the network (`GenericData` channel) when ready.

## Slots

`body`, `head`, `chest`, `legs`, `feet`, `hands`, `mainhand`, `offhand`, `back`

Body is always on the root entity. Other slots spawn separate mesh entities.

## Nameplates

Each character can have a `NameplateComponent` with a display name rendered above their head in world space.

- Font: `FontRole::Hud` from `menus/main_menu/fonts` (Inter, via `UISystem`)
- Local player: name from `--name` CLI flag
- Remote players: name from server `PeerJoined` event (`--name` on each client)
- Hidden while the main menu is visible

Attach manually:

```cpp
Game::Character::AttachNameplate(registry, entityId, "Alice", headHeight + 0.15f);
```
