---
name: astral-network-scale
description: >-
  AstralWorlds concurrent-player networking, replication layering, zone instance
  splits, stress profiling, and latency optimization. Use when working on NetServer,
  NetworkSystem, join load, movement RTT, character load at scale, layer split,
  or running join/game e2e stress profiles.
---

# AstralWorlds Network Scale

## Architecture (two layers)

| Layer | What | When active |
|-------|------|-------------|
| **Replication layering** | Interest-tier reduction inside one server process | `connectedPlayers >= layerMinShardPlayers` (default 256) AND cell density threshold |
| **Zone instance layers** | Separate shard processes, 256 cap, half-split migration | `maxPlayers` on shard + `overflowShardId` |

Config: `RenderingEngine/config/join_load_stress_scale.json`, `RenderingEngine/config/zone_default.json`.

Code map:
- `RenderingEngine/platform/network/NetServer.cpp` — admission, layering, split
- `RenderingEngine/platform/network/AuthoritativeGameSimulator.cpp` — movement ack, spawn
- `RenderingEngine/core/network/LayerInstancePolicy.*` — 256 cap / 128 seed split
- `RenderingEngine/core/network/ScalePolicyConfig.*` — `IsLayeringActive()`
- `RenderingEngine/platform/network/systems/NetworkSystem.*` — client replication path

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
| `scripts/run_game_e2e_stress_profile.ps1` | `GameTests` | ConnectP95, CharP95, MoveP95 |

Player counts: 64 / 256 / 1048. Use `-AllowPartialSuccess` for local sweeps.

Env gates:
- Engine: `ENGINE_RUN_JOIN_LOAD_TEST=1`, `JOIN_LOAD_STRESS_PROFILE=1`
- Game: `GAME_E2E_STRESS_PROFILE=1`

Latest logs:
- `RenderingEngine/scripts/logs/join_load_stress_profile_latest.txt`
- `scripts/logs/game_e2e_stress_profile_latest.txt`

## Baseline targets (local, Jul 2026)

| Scale | Engine MoveP95 | Game MoveP95 | Notes |
|------:|---------------:|-------------:|-------|
| 64 | ~31 ms | ~32 ms | Game may load character mesh |
| 256 | ~33 ms | <50 ms | Layering on |
| 1048 | ~41 ms | <60 ms goal post PollNetwork fix | Was ~189 ms with Update in ack loop |

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

## Tests

- `RenderingEngine/tests/network/NetworkSystemTest.cpp` — deferred snapshots
- `RenderingEngine/tests/core/LayerInstancePolicyTest.cpp` — zone split policy
- `tests/CharacterBodyAssetCacheTest.cpp` — shared body cache
- `tests/GameNetworkE2ETest.cpp` — full game stack stress

## Additional reference

See [reference.md](reference.md) for metric definitions and troubleshooting.
