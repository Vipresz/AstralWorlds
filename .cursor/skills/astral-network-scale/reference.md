# Network Scale Reference

## Metric definitions

### Engine join-load profile (`NetServerJoinLoadTest`)

| Metric | Meaning |
|--------|---------|
| **JoinP95** | p95 of connect+login time per client during burst |
| **MoveP95** | p95 movement command → ack RTT; each client uses `NetClient::Poll()` only |
| **TickP95** | Server sim tick duration p95 |
| **Layer** | 1 when replication layering active |
| **ShardR** | Shard redirect count (multi-process tests only) |

### Game e2e profile (`GameNetworkE2ETest`)

| Metric | Meaning |
|--------|---------|
| **ConnectP95** | `NetworkSystem::ConnectAndLogin` duration |
| **CharP95** | `CharacterLoader::LoginAndBuild` (64-player path with mesh load) |
| **MoveP95** | Movement ack RTT; must use `PollNetwork()` in wait loop |
| **BurstMs** | Wall time for full join burst |

## Known gaps (engine vs game)

| Gap | Root cause | Fix |
|-----|------------|-----|
| Game MoveP95 >> Engine at 1048 | `Update()` in ack spin loop | `PollNetwork()` only |
| CharP95 ~46s at 64 | Serialized FBX load per client | `CharacterBodyAssetCache` |
| JoinP95 ~1.6s at 1048 | Login thundering herd | `SleepJoinStagger` + server admission |

## Scale policy fields (`join_load_stress_scale.json`)

```json
"layerMinShardPlayers": 256,
"layerSplitSeedPlayers": 128,
"cellLayerDensityThreshold": 24,
"layerMidRadius": 48,
"observersCapCrowded": 16
```

Layering shrinks interest radius and observer caps when density + player count thresholds are met.

## Zone instance split (`zone_default.json`)

- `shard.maxPlayers`: 256
- `shard.overflowShardId`: overflow layer id
- At 256 local + overflow ≥ 128: redirect new logins
- At 256 local + overflow < 128: `split_pending`, migrate 128 players

## Rebuild and run

```powershell
cmake --build build --config Release --target GameTests Platform
cmake --build RenderingEngine/build-platform --config Release --target EngineTests

.\RenderingEngine\scripts\run_join_load_stress_profile.ps1 -AllowPartialSuccess
.\scripts\run_game_e2e_stress_profile.ps1 -AllowPartialSuccess
```

## CI

Fast CI runs unit tests only. Join-load and game e2e stress profiles are manual/nightly (`ENGINE_RUN_JOIN_LOAD_TEST`, `GAME_E2E_STRESS_PROFILE`).
