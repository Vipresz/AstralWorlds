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
| `items.json` | Equipment item id → model path, slot |
| `game_items.json` | Inventory item id, numeric id, icon, stack size |
| `equipment_rules.json` | Per-slot local offset / rotation / scale |
| `loadouts/default.json` | New-character template (body, equipment, starter inventory) |
| `loadouts/<userId>.json` | Per-account loadout overrides (dev) |
| `astral_worlds.db` | Local SQLite inventory DB (`accounts`, `account_inventory`) |

### Example loadout

```json
{
  "body_item_id": "body_male_default",
  "equipment": [
    { "item_id": "helm_starter", "slot": "head" },
    { "item_id": "sword_starter", "slot": "mainhand" }
  ],
  "inventory": [
    { "item_id": "maple", "quantity": 10 }
  ]
}
```

`inventory` entries use string ids from `game_items.json` (e.g. `maple`). Offline play seeds the local bag from the loadout. Online play uses `default.json` (plus per-user overrides) as a **template for new characters only**. Returning characters load inventory rows from `character/astral_worlds.db` on the server (`account_id`, `item_id`, `quantity`). Bag slot layout stays client-side.

Per-user loadouts (`loadouts/<userId>.json`) override `default.json` for body and equipment. If a user file has no `inventory` section, starter items from `default.json` are merged in.

Add matching equipment entries to `items.json` with model paths under `graphics/`.

Inventory materials use string ids in `game_items.json` (e.g. `gold_bar`). Those ids are the canonical network key for `PlayerItemOwnership` replication; `numeric_id` is the compact key for bag UI and crafting.

## Code map

| File | Role |
|------|------|
| `CharacterTypes.h` | `EquipSlot`, `CharacterLoadout`, login context |
| `ItemCatalog` | Resolves equipment ids to models |
| `GameItemCatalog` | Resolves inventory ids to icons and numeric ids; string ids match engine ownership replication |
| `EquipmentRules` | Attachment offsets per slot |
| `ServerLoadoutDatabase` | Loads `loadouts/<userId>.json` at login |
| `CharacterInventoryDatabase` | SQLite persistence for `account_inventory` (server) |
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
