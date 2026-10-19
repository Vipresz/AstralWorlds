---
name: astral-network-scale
description: >-
  AstralWorlds concurrent-player networking, replication layering, zone instance
  splits, stress profiling, inventory/trade/chat channels, and latency optimization.
  Use when working on NetServer, NetworkSystem, AuthoritativeGameSimulator, join load,
  movement RTT, character load at scale, layer split, inventory authority, or running
  join/game e2e stress profiles.
---

# AstralWorlds Network Scale

## Architecture (two layers)

| Layer | What | When active |
|-------|------|-------------|
| **Replication layering** | Interest-tier reduction inside one server process | `connectedPlayers >= layerMinShardPlayers` (default 256) AND cell density threshold |
| **Zone instance layers** | Separate shard processes, 256 cap, half-split migration | `maxPlayers` on shard + `overflowShardId` |

Config files (`RenderingEngine/config/`):

| File | Use |
|------|-----|
| `join_load_stress_scale.json` | Stress profile arena (1200 cap, LayerFirst) |
| `join_load_scale.json` | Unit tests + legacy join-load harness |
| `zone_default.json` | Production-style zone (256 cap, overflow shard id 2) |

Code map:
- `RenderingEngine/platform/network/NetServer.cpp` — admission, layering, split, interest, rate limits
- `RenderingEngine/platform/network/AuthoritativeGameSimulator.cpp` — movement, inventory, trade, chat
- `RenderingEngine/core/network/LayerInstancePolicy.*` — 256 cap / 128 seed split
- `RenderingEngine/core/network/ScalePolicyConfig.*` — `IsLayeringActive()`
- `RenderingEngine/core/network/ReplicationTier.*` — Full/Mid/Far/Hidden transform reduction
- `RenderingEngine/platform/network/InterestManager.*` — spatial cell grid, observer caps
- `RenderingEngine/platform/network/systems/NetworkSystem.*` — client replication path
- `RenderingEngine/platform/network/systems/ReplicationSystem.*` — ECS snapshot build/apply

## Generic data channels

Defined in `RenderingEngine/core/network/MovementCommand.h`. All client→server payloads on authoritative servers go through `NetServer::SetGenericDataInterceptor` → `AuthoritativeGameSimulator::HandleGenericData`.

| Channel | Authority | Server handler | Client/game wiring |
|---------|-----------|----------------|-------------------|
| `movement` | Server sim + immediate ack | `HandleMovementCommand` | UDP/TCP movement commands |
| `loadout` | Server applies equipment | `ApplyLoadout` | Equipment changes |
| `item_ownership` | **Layout only** — multiset must match | `ApplyClientBagLayout` | Bag drag rearrange (AstralWorlds `main.cpp`) |
| `inventory_action` | **Use/Drop** — server removes items | `ApplyInventoryAction` | Bag Use/Drop actions |
| `trade` | Full trade session | `ApplyTradeCommand` | Trade UI |
| `chat` | Sanitized broadcast | `ApplyChatMessage` → `BroadcastGenericData` | Chat input |

### Inventory authority (critical split)

**Do not** send Use/Drop on `item_ownership`. That channel only accepts slot permutations where item totals are unchanged (`OwnershipMultisetMatches`).

| Action | Channel | Server behavior |
|--------|---------|-----------------|
| Drag rearrange | `item_ownership` | Accept if multiset matches; reject forgery and resync |
| Use (consume 1) | `inventory_action` | `RemoveFromSlot(slot, 1, false)` after validator |
| Drop (remove one stack) | `inventory_action` | `RemoveFromSlot(slot, 0, true)` — entire stack at that slot only |

Game hooks:
- `SetItemStackCapResolver` — per-item stack caps (`GameItemCatalog`)
- `SetInventoryActionValidator` — e.g. only Resource/Consumable for Use (`CanUseItem`)

Bag UX in AstralWorlds (`src/menus/src/Menu.cpp`, `InventoryInteraction.h`):
- Left-click usable item → select (gold border); second usable → combined use stub
- Right-click → action panel; **Use** selects only (does not consume online)
- Consumption only on successful combined use or server `inventory_action` Use

## Trade and chat

**Trade** (`TradeSession`, `TradeReplication`):
- `RequestTrade` / `RespondToTrade` / `SetTradeOffer` / lock/unlock
- Atomic exchange on dual-lock; respects stack caps
- Replicated via `kTradeReplicationComponent`

**Chat** (`ChatMessage`):
- Max 125 chars; control chars stripped
- 250 ms per-user rate limit on server
- Broadcast as `GenericData` on `chat` channel with `ChatMessage` payload

## Client network loop (long-term pattern)

**Do not** call full `NetworkSystem::Update()` while waiting for movement acks or login. Prefer `WaitForMovementAck()` or `PollNetwork()` only.

```cpp
// Time-sensitive movement ack wait
networkSystem->WaitForMovementAck(command.tick, std::chrono::milliseconds(500));

// Or manual poll loop during login
while (!done) {
    networkSystem->PollNetwork();
}

// Fixed gameplay tick (once per frame)
networkSystem->ProcessNetworkFrame(deltaTime, registry, tickNumber);
// equivalent to Update() which calls PollNetwork + ProcessNetworkFrame
```

Inbound **entity/component snapshots** are queued during `PollNetwork()` and flushed at the start of `ProcessNetworkFrame()`. This prevents replication/ECS work from inflating movement RTT under load.

Client prediction (`NetworkSystem`):
- `InputBuffer` (256 commands) + `SetClientPredictionEnabled`
- `ReconcileWithRollback` on server correction
- `CharacterMovementSimulator` on server tick

## Join admission stagger

Spread burst logins with `Engine::Net::SleepJoinStagger(playerIndex, playerCount)` from `RenderingEngine/core/network/JoinAdmission.h`.

- No-op for `playerCount <= 32`
- Spreads over `clamp(playerCount/4, 250ms, 2000ms)` via hashed index

Use in stress tests and any multi-client launcher before `ConnectAndLogin`.

## Character loading at scale

`CharacterBodyAssetCache` (`src/character/CharacterBodyAssetCache.*`) loads each body FBX + skeleton + locomotion clips **once** per item id. `CharacterLoader::LoadBody` attaches shared assets; instances only differ by equipment.

Boot preload (game `main.cpp`):
```cpp
CharacterBodyAssetCache::Instance().GetOrLoad(defaultBody->id, defaultBody->modelPath);
```

**Never** use a global character-load mutex in stress tests — rely on cache + `std::call_once` for catalog/rules.

## Stress profiles

| Script | Binary | Metrics |
|--------|--------|---------|
| `RenderingEngine/scripts/run_join_load_stress_profile.ps1` | `EngineTests` | JoinP95, MoveP95, layering |
| `RenderingEngine/scripts/run_join_load_test.ps1` | `EngineTests` | Legacy single-run join load |
| `scripts/run_game_e2e_stress_profile.ps1` | `GameTests` | ConnectP95, CharP95, MoveP95 |

Player counts: 64 / 256 / 1048. Use `-AllowPartialSuccess` for local sweeps.

Env gates:
- Engine: `ENGINE_RUN_JOIN_LOAD_TEST=1`, `JOIN_LOAD_STRESS_PROFILE=1`
- Game: `GAME_E2E_STRESS_PROFILE=1`

Latest logs:
- `RenderingEngine/scripts/logs/join_load_stress_profile_latest.txt`
- `scripts/logs/game_e2e_stress_profile_latest.txt`

## Baseline targets (local, Jul 2026)

Machine-dependent; treat as regression hints not hard SLOs.

| Scale | Engine MoveP95 | Game MoveP95 | Notes |
|------:|---------------:|-------------:|-------|
| 64 | ~31 ms | ~32 ms | Game may load character mesh |
| 256 | ~16–33 ms | <50 ms | Layering on |
| 1048 | ~41–77 ms | <60 ms goal | Variance with CPU load; was ~189 ms pre PollNetwork fix |

Join/connect p95 at 256+ shares server queueing (~500–1600 ms) — mitigate with stagger + future login admission queue.

## Optimization checklist

When game movement RTT >> engine movement RTT:

1. Confirm ack waits use `PollNetwork()` only
2. Profile `ProcessNetworkFrame` — `ApplyEntitySnapshot`, `InterpolateRemoteTransforms`
3. Verify layering active at 256+ (`layering=1` in engine profile)
4. Check `login_dropped` / `movement_dropped` in profile output

When character load is slow:

1. Confirm `CharacterBodyAssetCache` hit (second load should be microseconds)
2. Preload default body at boot
3. Equipment models still load per-instance — consider `AssetManager` for props

When inventory desyncs:

1. Confirm drag uses `item_ownership`, Use/Drop uses `inventory_action`
2. Check server logs for "Rejected bag mutation" or "Rejected inventory action"
3. Verify `SetInventoryActionValidator` matches game catalog rules

## Tests

### Scale / client loop
- `RenderingEngine/tests/network/NetworkSystemTest.cpp` — deferred snapshots
- `RenderingEngine/tests/core/LayerInstancePolicyTest.cpp` — zone split policy
- `RenderingEngine/tests/network/NetServerJoinLoadTest.cpp` — join-load profile
- `RenderingEngine/tests/network/NetServerScalePolicyMultiUserTest.cpp` — multi-user policy

### Inventory / trade / chat
- `RenderingEngine/tests/network/AuthoritativeGameSimulatorOwnershipTest.cpp` — grant, forgery, permutation
- `RenderingEngine/tests/network/AuthoritativeGameSimulatorChannelsTest.cpp` — Use/Drop, layout+use, chat broadcast
- `RenderingEngine/tests/network/AuthoritativeGameSimulatorTradeTest.cpp` — two-player trade
- `RenderingEngine/tests/core/PlayerItemOwnershipComponentTest.cpp` — stacking, rollback
- `RenderingEngine/tests/core/TradeSessionTest.cpp` — atomic exchange
- `RenderingEngine/tests/core/ChatMessageTest.cpp` — sanitization unit tests

### Interest / tier / policy
- `RenderingEngine/tests/network/InterestManagerTest.cpp`
- `RenderingEngine/tests/core/ReplicationTierTest.cpp`
- `RenderingEngine/tests/core/ScalePolicyTest.cpp`
- `RenderingEngine/tests/core/ZoneScaleConfigTest.cpp`
- `RenderingEngine/tests/network/RateLimiterTest.cpp`

### Game stack
- `tests/CharacterBodyAssetCacheTest.cpp` — shared body cache
- `tests/GameNetworkE2ETest.cpp` — full game stack stress

## Additional reference

See [reference.md](reference.md) for metric definitions, config field glossary, server metrics, and troubleshooting.
