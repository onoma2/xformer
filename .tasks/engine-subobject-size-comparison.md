# Engine Sub-Object Size Comparison

## CONFIG Values

| Parameter | XFORMER | Vinx | Modulove |
|-----------|---------|------|----------|
| CONFIG_PPQN | 192 | 192 | 192 |
| CONFIG_CV_INPUT_CHANNELS | 4 | 4 | 4 |
| CONFIG_CV_OUTPUT_CHANNELS | 8 | 8 | 8 |
| CONFIG_TRACK_COUNT | 8 | 8 | 16 |
| CONFIG_ROUTE_COUNT | 16 | 16 | 4 |
| CONFIG_MIDI_OUTPUT_COUNT | 16 | 16 | 16 |
| CONFIG_MODULATOR_COUNT | — | — | 8 |

## Per-Object sizeof() Breakdown

### 1. EngineState — All variants: **2 B**

| Member | Type | Size |
|--------|------|------|
| `_running` | bool | 1 B |
| `_recording` | bool | 1 B |
| **Total** | | **2 B** |

### 2. Clock — XFORMER: **164 B**, Vinx/Modulove: **160 B**

| Member | Type | Size | XFORMER | Vinx/Mod |
|--------|------|------|---------|----------|
| `_listener` | Listener* | 4 B | ✓ | ✓ |
| `_timer` | ClockTimer& | 4 B | ✓ | ✓ |
| `_ppqn` | int | 4 B | ✓ | ✓ |
| `_mode` | Mode (enum→int) | 4 B | ✓ | ✓ |
| `_masterBpm` | float | 4 B | ✓ | ✓ |
| `_slaves[4]` | struct Slave {int, bool} → padded 8 B × 4 | 32 B | ✓ | ✓ |
| `_output` | struct Output {6×int} | 24 B | ✓ | ✓ |
| `_outputState` | OutputState {3×bool, padded} | 4 B | ✓ | ✓ |
| `_requestedEvents` | uint32_t | 4 B | ✓ | ✓ |
| `_state` | State (enum→int) | 4 B | ✓ | ✓ |
| `_tick` | volatile uint32_t | 4 B | ✓ | ✓ |
| `_tickProcessed` | volatile uint32_t | 4 B | ✓ | ✓ |
| **`_lastTickUs`** | **volatile uint32_t** | **4 B** | **✓** | **—** |
| `_activeSlave` | volatile int32_t | 4 B | ✓ | ✓ |
| `_elapsedUs` | uint32_t | 4 B | ✓ | ✓ |
| `_lastSlaveTickUs` | uint32_t | 4 B | ✓ | ✓ |
| `_slaveTickPeriodUs` | uint32_t | 4 B | ✓ | ✓ |
| `_slaveSubTicksPending` | uint32_t | 4 B | ✓ | ✓ |
| `_slaveSubTickPeriodUs` | uint32_t | 4 B | ✓ | ✓ |
| `_nextSlaveSubTickUs` | uint32_t | 4 B | ✓ | ✓ |
| `_slaveBpmFiltered` | float | 4 B | ✓ | ✓ |
| `_slaveBpmAvg` | MovingAverage<float,4> | 28 B | ✓ | ✓ |
| `_slaveBpm` | float | 4 B | ✓ | ✓ |
| **Total** | | | **164 B** | **160 B** |

**Difference:** XFORMER has `volatile uint32_t _lastTickUs` (4 B) not present in Vinx/Modulove. Also XFORMER has `tickPeriodUs()` and `tickPosition()` methods (code, not data).

### 3. TapTempo — All variants: **52 B**

| Member | Type | Size |
|--------|------|------|
| `_lastTime` | uint32_t | 4 B |
| `_lastInterval` | uint32_t | 4 B |
| `_intervalAverage` | MovingAverage<uint32_t,8> | 44 B |
| **Total** | | **52 B** |

MovingAverage<uint32_t,8> breakdown: `_history[8]` = 32 B, `_sum` = 4 B, `_index` = 4 B, `_count` = 4 B. Total = 44 B.

### 4. NudgeTempo — All variants: **8 B**

| Member | Type | Size |
|--------|------|------|
| `_direction` | int8_t | 1 B |
| (padding) | | 3 B |
| `_strength` | float | 4 B |
| **Total** | | **8 B** |

### 5. MidiOutputEngine — All variants: **156 B**

| Member | Type | Size |
|--------|------|------|
| `_engine` | Engine& | 4 B |
| `_midiOutput` | const MidiOutput& | 4 B |
| `_outputStates[16]` | OutputState × 16 | 144 B |
| `_lastSendCCTicks` | uint32_t | 4 B |
| **Total** | | **156 B** |

OutputState = 9 B (event:1, target._port:1, target._channel:1, requests:1, note:1, slide:1, velocity:1, control:1, activeNote:1). All uint8_t/enum:uint8_t, no padding (align=1). 16 × 9 = 144 B.

### 6. MidiLearn — All variants: **28 B**

| Member | Type | Size |
|--------|------|------|
| `_callback` | std::function<…> | 12 B |
| `_port` | MidiPort (enum:uint8_t) | 1 B |
| `_channel` | int8_t | 1 B |
| `_controlNumber` | int8_t | 1 B |
| `_lastControlNumber` | int8_t | 1 B |
| `_note` | int8_t | 1 B |
| (padding) | | 3 B |
| `_lastResult` | Result | 4 B |
| `_eventCounters[4]` | uint8_t × 4 | 4 B |
| **Total** | | **28 B** |

### 7. RoutingEngine — **THE DOMINANT DIFFERENCE**

#### XFORMER: **7,484 B**
#### Vinx: **108 B**
#### Modulove: **36 B**

#### XFORMER RouteState — **460 B** (per route)

```
struct RouteState {
    Routing::Target target;          // uint8_t, 1 B  [offset 0]
    uint8_t tracks;                  // 1 B           [offset 1]
    std::array<Shaper, 8> shaper;    // 8 × uint8_t   [offset 2-9]
    // padding to align TrackState   // 2 B           [offset 10-11]
    std::array<TrackState, 8> shaperState;  // 8 × 56 B [offset 12+]
};
```

**XFORMER TrackState — 56 B each** (8 per route → 448 B per route):

| Member | Type | Offset | Size | Notes |
|--------|------|--------|------|-------|
| `location` | float | 0 | 4 B | |
| `envelope` | float | 4 | 4 B | |
| `freqAcc` | float | 8 | 4 B | |
| `freqSign` | bool | 12 | 1 B | |
| (padding) | | 13 | 3 B | align next float |
| `activityPrev` | float | 16 | 4 B | |
| `activityLevel` | float | 20 | 4 B | |
| `activitySign` | bool | 24 | 1 B | |
| (padding) | | 25 | 1 B | align uint16_t |
| `ffHold` | uint16_t | 26 | 2 B | |
| `actHold` | uint16_t | 28 | 2 B | |
| (padding) | | 30 | 2 B | align next float |
| `progCount` | float | 32 | 4 B | |
| `progThreshold` | float | 36 | 4 B | |
| `progSign` | bool | 40 | 1 B | |
| (padding) | | 41 | 3 B | align next float |
| `progOut` | float | 44 | 4 B | |
| `progOutSlewed` | float | 48 | 4 B | |
| `progHold` | uint16_t | 52 | 2 B | |
| (padding) | | 54 | 2 B | struct tail |
| **Total** | | | **56 B** | |

#### Vinx RouteState — **2 B** (per route)

```
struct RouteState {
    Routing::Target target;    // uint8_t, 1 B
    uint8_t tracks;            // 1 B
};
```

#### Complete RoutingEngine Breakdown

| Member | XFORMER Size | Vinx Size | Modulove Size |
|--------|-------------|-----------|---------------|
| `_engine` (ref) | 4 B | 4 B | 4 B |
| `_routing` (ref) | 4 B | 4 B | 4 B |
| `_sourceValues[ROUTE_COUNT]` | 16×4=64 B | 16×4=64 B | 4×4=16 B |
| `_routeStates[ROUTE_COUNT]` | 16×460=7,360 B | 16×2=32 B | 4×2=8 B |
| `_cvRotateValues[TRACK_COUNT]` | 8×4=32 B | — | — |
| `_cvRotateInterp[TRACK_COUNT]` | 8×1=8 B | — | — |
| `_lastPlayToggleActive` | 1 B | 1 B | 1 B |
| `_lastRecordToggleActive` | 1 B | 1 B | 1 B |
| `_lastResetActive[TRACK_COUNT]` | 8×1=8 B | — | — |
| (tail padding) | 2 B | 2 B | 2 B |
| **Total** | **7,484 B** | **108 B** | **36 B** |

### 8. CvGateToMidiConverter — All variants: **8 B**

| Member | Type | Size |
|--------|------|------|
| `_lastGateOff` | uint32_t | 4 B |
| `_gate` | uint8_t | 1 B |
| `_note` | int8_t | 1 B |
| (padding) | | 2 B |
| **Total** | | **8 B** |

### 9. CvInput — All variants: **20 B**

| Member | Type | Size |
|--------|------|------|
| `_adc` | Adc& | 4 B |
| `_channels[4]` | float × 4 | 16 B |
| **Total** | | **20 B** |

### 10. CvOutput — All variants: **40 B**

| Member | Type | Size |
|--------|------|------|
| `_dac` | Dac& | 4 B |
| `_calibration` | const Calibration& | 4 B |
| `_channels[8]` | float × 8 | 32 B |
| **Total** | | **40 B** |

### 11. ModulatorEngine — Only in Modulove: **256 B**

| Member | Type | Size |
|--------|------|------|
| `_phaseAccumulator[8]` | uint32_t × 8 | 32 B |
| `_currentValue[8]` | int × 8 | 32 B |
| `_lastRandomValue[8]` | int × 8 | 32 B |
| `_lastPhase[8]` | uint16_t × 8 | 16 B |
| `_lastGate[8]` | bool × 8 | 8 B |
| `_targetValue[8]` | int × 8 | 32 B |
| `_rng[8]` | Random × 8 (uint32_t _state each) | 32 B |
| `_adsrState[8]` | ADSRState:uint8_t × 8 | 8 B |
| `_adsrLevel[8]` | int × 8 | 32 B |
| `_adsrTimer[8]` | uint32_t × 8 | 32 B |
| **Total** | | **256 B** |

## Summary Table

| Sub-object | XFORMER | Vinx | Modulove | Δ (X-V) | Δ (X-M) |
|-----------|---------|------|----------|---------|---------|
| EngineState | 2 | 2 | 2 | 0 | 0 |
| Clock | 164 | 160 | 160 | +4 | +4 |
| TapTempo | 52 | 52 | 52 | 0 | 0 |
| NudgeTempo | 8 | 8 | 8 | 0 | 0 |
| MidiOutputEngine | 156 | 156 | 156 | 0 | 0 |
| MidiLearn | 28 | 28 | 28 | 0 | 0 |
| **RoutingEngine** | **7,484** | **108** | **36** | **+7,376** | **+7,448** |
| CvGateToMidiConverter | 8 | 8 | 8 | 0 | 0 |
| CvInput | 20 | 20 | 20 | 0 | 0 |
| CvOutput | 40 | 40 | 40 | 0 | 0 |
| ModulatorEngine | — | — | 256 | 0 | -256 |
| **Non-container sub-object total** | **7,962** | **582** | **766** | **+7,380** | **+7,196** |

## Answer: What accounts for the ~8.2 KB gap?

**The RoutingEngine is responsible for virtually the entire difference (7,376 B out of ~8,200 B = ~90%).**

The cause is the massive expansion of `RouteState` in XFORMER:

- **XFORMER**: Each RouteState has a full `TrackState` (56 B) × 8 tracks = 448 B of shaping/per-track state per route. With CONFIG_ROUTE_COUNT=16, that's **7,360 B** in `_routeStates` alone. Plus additional per-track arrays `_cvRotateValues`, `_cvRotateInterp`, and `_lastResetActive` (48 B total).

- **Vinx/Modulove**: Each RouteState is just `{Target target, uint8_t tracks}` = 2 B. No per-track shaping state whatsoever. With 16 routes = **32 B** total. No cvRotate/lastReset arrays.

**Breakdown of the 7,376 B gap:**
| Source | Bytes |
|--------|-------|
| Per-track shaping state in RouteState (16 routes × 448 B) | 7,168 B |
| RouteState fields alignment/padding (16 routes × 10 B) | 160 B |
| `_cvRotateValues[8]` + `_cvRotateInterp[8]` | 40 B |
| `_lastResetActive[8]` | 8 B |
| **Total RoutingEngine difference** | **7,376 B** |

The remaining ~800 B of the ~8.2 KB gap would come from other Engine members not in these sub-objects (e.g., the larger track engine structures, handler std::functions, etc.).
