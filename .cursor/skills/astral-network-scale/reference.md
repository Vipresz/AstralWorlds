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

## Server tick metrics (`ServerTickMetrics`)

Collected per tick and session in `AuthoritativeGameSimulator::Update`:

| Field | Meaning |
|-------|---------|
| `tickMs` / `tickP95Ms` / `tickMsMax` | Sim tick duration |
| `connectedPlayers` | Logged-in count |
| `movementCommandsProcessed` / `movementCommandsDropped` | Movement queue drain |
| `dirtySnapshotsSent` | Ownership/loadout/trade snapshots flushed |
| `visibilityEvents` | Interest enter/leave events |
| `cellDensityMax` | Peak users in one interest cell |
| `layeringActive` | Replication tier reduction enabled |
| `shardRedirectRecommended` | Admission wants overflow redirect |
| `peerTransformsSent` / `peerTransformsDeferred` | Transform fan-out vs deferred queue |
| `replicationQueueDepth` | Pending peer transform depth |

## Generic data channel payloads

| Channel | Serialize | Deserialize |
|---------|-----------|-------------|
| `movement` | `SerializeMovementCommand` | `DeserializeMovementCommand` |
| `loadout` | `ReplicatedLoadout::Serialize` | `ReplicatedLoadout::Deserialize` |
| `item_ownership` | `ReplicatedItemOwnership::Serialize` | `ReplicatedItemOwnership::Deserialize` |
| `inventory_action` | `SerializeInventoryAction` | `DeserializeInventoryAction` |
| `trade` | `SerializeTradeCommand` | `DeserializeTradeCommand` |
| `chat` (client→server) | `SerializeChatCommand` | `DeserializeChatCommand` |
| `chat` (server→clients) | `SerializeChatMessage` | `DeserializeChatMessage` |

## Known gaps (engine vs game)

| Gap | Root cause | Fix |
|-----|------------|-----|
| Game MoveP95 >> Engine at 1048 | `Update()` in ack spin loop | `PollNetwork()` only |
| CharP95 ~46s at 64 | Serialized FBX load per client | `CharacterBodyAssetCache` |
| JoinP95 ~1.6s at 1048 | Login thundering herd | `SleepJoinStagger` + server admission |
| Drop has no world entity | Inventory-only removal | Future world-drop replication |
| Combined use is stub | No recipe validation | Wire crafting recipes to server |
| Use in bag panel doesn't consume | By design (select UX) | Only `inventory_action` or combined use consumes |

## Scale policy fields (`join_load_stress_scale.json`)

Core layering fields (also see `zone_default.json` for production caps):

```json
"layerMinShardPlayers": 256,
"layerSplitSeedPlayers": 128,
"cellLayerDensityThreshold": 24,
"layerFullRadius": 20.0,
"layerMidRadius": 48.0,
"layerFarRadius": 96.0,
"layerFullRadiusCrowded": 12.0,
"observersCapNormal": 48,
"observersCapCrowded": 16
```

Additional stress-arena fields:

| Field | Role |
|-------|------|
| `shardSoftCap` / `shardHardCap` | Login admission thresholds |
| `shardSplitThreshold` | When split planning activates |
| `shardTickMsSoftLimit` / `shardTickMsHardLimit` | Tick pressure for redirect |
| `densityCellSize` | InterestManager cell size |
| `maxPeerTransformsPerTick` | Transform send budget per tick |
| `maxVisibilityEventsPerTick` | Visibility event budget |
| `spawnSpacingMin` / `spawnSpacingMax` | Spawn spread on login |
| `targetMaxCellDensity` | Crowding target for spawn logic |
| `snapshotIntervalNormal/Crowded/StressedSeconds` | Snapshot throttle tiers |

`ReplicationTier` maps distance to Full/Mid/Far/Hidden and `ReduceTransformForTier` strips rotation/velocity fields for distant peers.

## Zone instance split (`zone_default.json`)

- `shard.maxPlayers`: 256
- `shard.overflowShardId`: overflow layer id (2)
- At 256 local + overflow ≥ 128: redirect new logins
- At 256 local + overflow < 128: `split_pending`, migrate 128 players

Stress config (`join_load_stress_scale.json`) uses a single shard with `maxPlayers: 1200` — different model for load testing, not production zone layout.

## Security and transport

| Feature | Default | Notes |
|---------|---------|-------|
| `CredentialStore` | On | Username/password login |
| `SetRequiredAuthToken` | Optional | Token gate on login |
| `ENGINE_ENABLE_TLS` | OFF | mbedTLS for TCP; stub when disabled |
| UDP movement | On | `UdpChannel` + `UdpMessageAuth` |

## Rebuild and run

```powershell
cmake --build build --config Release --target GameTests Platform
cmake --build RenderingEngine/build-platform --config Release --target EngineTests

.\RenderingEngine\scripts\run_join_load_stress_profile.ps1 -AllowPartialSuccess
.\RenderingEngine\scripts\run_join_load_test.ps1
.\scripts\run_game_e2e_stress_profile.ps1 -AllowPartialSuccess
```

Run channel tests:

```powershell
.\RenderingEngine\build-platform\bin\Release\EngineTests.exe --gtest_filter=AuthoritativeGameSimulatorChannelsTest.*
```

## CI

Fast CI runs unit tests only (`BUILD_ENGINE_PLATFORM=OFF`). Platform tests (ownership, trade, channels, join-load) require `BUILD_ENGINE_PLATFORM=ON` job `test-platform-windows`.

Join-load and game e2e stress profiles are manual/nightly (`ENGINE_RUN_JOIN_LOAD_TEST`, `GAME_E2E_STRESS_PROFILE`).
