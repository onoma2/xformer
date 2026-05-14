# Output Authority Map

Derived from `assumption-matrix.md` Direction 4. **This is a source-derived hypothesis map, not a verified contract.** The update ordering, Teletype/rotation interaction, and routing vs track engine relationship need source verification before this can be treated as authoritative.

---

## Source-Derived Hypothesis: Update Order

```
Actual sequence (src/apps/sequencer/engine/Engine.cpp:188):
    1. trackEngines->update(dt)        for each track
    2. _midiOutputEngine.update()
    3. updateTrackOutputs()            // reads _cvRouteOutputs[]
    4. updateOverrides()               // calls updateCvRouteOutputs() → writes _cvRouteOutputs[]
    5. applyBusSafety()
    6. _cvOutput.update() / _gateOutput.update()  // hardware
```

**Hypothesis — likely one-cycle latency for CV route lanes:**
- `updateTrackOutputs()` reads `_cvRouteOutputs[]` (step 3)
- `updateCvRouteOutputs()` writes `_cvRouteOutputs[]` (inside step 4)
- Therefore CC route lane values visible to physical outputs may lag by one update cycle.
- **Needs source verification** before treating as a bug.

**Hypothesis — RoutingEngine is not a physical output writer:**
- `RoutingEngine::writeEngineTarget()` writes routed values to **model properties** (e.g., slide time, transpose, octave)
- Track engines read those properties on their next tick, not in `updateTrackOutputs()`
- The observable effect is **parameter modulation**, not direct CV/gate output
- **Needs verification:** confirm no `_cvOutput` or `_gateOutput` writes exist in RoutingEngine

**Hypothesis — rotation is remapping, not writing:**
- `updateTrackOutputs()` uses rotation offset to **select which track/slot** to read
- No rotation value is written to output; it is a lookup-time transform
- Exception: interpolated rotation mixes two values into one output (still a read transform)

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                              OUTPUT SOURCES                                 │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  ┌─────────────────┐    ┌─────────────────┐    ┌─────────────────────────┐  │
│  │ Track Engines   │    │ RoutingEngine   │    │ CV Route Lanes          │  │
│  │ (per track)     │    │ (per route)     │    │ (4 lanes)               │  │
│  │                 │    │                 │    │                         │  │
│  │ NoteTrackEngine │    │ writes to model │    │ scan + route inputs     │  │
│  │ CurveTrackEngine│    │ properties via  │    │ → _cvRouteOutputs[]     │  │
│  │ MidiCvTrackEng  │    │ writeEngineTarget│   │                         │  │
│  │ TuesdayTrackEng │    │                 │    │ Inputs: CV In / Bus CV  │  │
│  │ DiscreteMapTrack│    │ Target types:   │    │ Outputs: CV Out / Bus CV│  │
│  │ IndexedTrackEng │    │ - Track params  │    │                         │  │
│  │                 │    │ - Global params │    │                         │  │
│  └────────┬────────┘    └─────────────────┘    └─────────────────────────┘  │
│           │                                                                   │
│           │  CV values    Gate values                                         │
│           │  (float)      (bool)                                              │
│           ▼                                                                   │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │                    Engine::updateTrackOutputs()                        │   │
│  │                                                                         │   │
│  │  Step 1: Gate Rotation Pool                                             │   │
│  │    - Identify tracks with gate rotation enabled                         │   │
│  │    - Build gatePool[] of rotating outputs                               │   │
│  │                                                                         │   │
│  │  Step 2: CV Rotation Pool                                               │   │
│  │    - Identify tracks with CV rotation enabled                           │   │
│  │    - Build cvPool[] of rotating outputs                                 │   │
│  │                                                                         │   │
│  │  Step 3: Precompute CV slots                                            │   │
│  │    - Map each output channel to track index + slot index                │   │
│  │    - CV route lanes get special track index = CONFIG_TRACK_COUNT        │   │
│  │                                                                         │   │
│  │  Step 4: For each output channel i (0..7):                              │   │
│  │                                                                         │   │
│  │    GATE:                                                                │   │
│  │      if rotating: apply rotation to gatePool[]                         │   │
│  │      read from _trackEngines[gateOutputTrack]->gateOutput(slot)        │   │
│  │      → _gateOutput.setGate(i, value)                                   │   │
│  │                                                                         │   │
│  │    CV:                                                                  │   │
│  │      if rotating and interpolated:                                     │   │
│  │        use _routingEngine.cvRotateValue() + pool interpolation          │   │
│  │        mix values from _trackEngines[lowTrack]->cvOutput(lowSlot)      │   │
│  │              and _trackEngines[highTrack]->cvOutput(highSlot)            │   │
│  │        → _cvOutput.setChannel(i, mixed)                                │   │
│  │      elif rotating (discrete):                                          │   │
│  │        apply rotation offset to cvPool[]                                │   │
│  │        read from _trackEngines[cvOutputTrack]->cvOutput(slot)          │   │
│  │        → _cvOutput.setChannel(i, value)                                │   │
│  │      elif cv route lane:                                                │   │
│  │        read from _cvRouteOutputs[lane]                                  │   │
│  │        → _cvOutput.setChannel(i, laneValue)                            │   │
│  │      else (normal):                                                    │   │
│  │        read from _trackEngines[cvOutputTrack]->cvOutput(slot)          │   │
│  │        → _cvOutput.setChannel(i, value)                                │   │
│  │                                                                         │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
│           │                                                                   │
│           ▼                                                                   │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │                    Engine::updateOverrides()                           │   │
│  │                                                                         │   │
│  │  - _gateOutputOverride: if true, set all gates to _gateOutputOverrideValue│  │
│  │  - _cvOutputOverride: if true, set all CVs to _cvOutputOverrideValues[]   │   │
│  │  - else: updateCvRouteOutputs() → writes _cvRouteOutputs[] + bus CV     │   │
│  │                                                                         │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
│           │                                                                   │
│           ▼                                                                   │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │                    Engine::applyBusSafety()                            │   │
│  │                                                                         │   │
│  │  - If _busCvSafeMode: decay unwritten bus CV channels by 0.9995        │   │
│  │                                                                         │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
│           │                                                                   │
│           ▼                                                                   │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │                    Hardware Outputs                                      │   │
│  │                                                                         │   │
│  │  _cvOutput.update()  → DAC                                             │   │
│  │  _gateOutput.update() → Gate drivers                                   │   │
│  │                                                                         │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## Hypothesis: Output Priority (Source Verification Needed)

This table is a **hypothesis**, not a verified contract. The actual output composition is determined by `updateTrackOutputs()` reading from multiple sources. The "priority" here is logical read order, not writer-conflict arbitration.

| Order | Source | Condition | Effect |
|---|---|---|---|
| 1 | Track Engine | Default | `_trackEngines[track]->cvOutput(slot)` / `gateOutput(slot)` |
| 2 | CV Route Lane | `cvOutputTracks[i] == CONFIG_TRACK_COUNT` | `_cvRouteOutputs[lane]` replaces track engine value |
| 3 | Gate/CV Rotation | `isGateOutputRotated()` / `isCvOutputRotated()` | **Remaps** which track engine slot is read (not a writer) |
| 4 | RoutingEngine | `cvRotateInterpolated()` + `cvRotateValue()` | Provides rotation **offset** for CV pool interpolation (not a physical output writer) |
| 5 | Mute | `mute()` in `PlayState::TrackState` | Muted tracks produce no output (handled at track engine level) |
| 6 | Bus CV | `setBusCv()` via scripts / CV routes | Written to `_busCv[]`; **not directly connected to physical outputs** |
| 7 | Override | `_gateOutputOverride` / `_cvOutputOverride` | Replaces ALL gates/CVs with override values |

**Rule:** Overrides are absolute. Bus CV is additive — it is a separate signal path that feeds into CV route lanes and Teletype scripts, not a physical output writer.

---

## Dual-Path Outputs (What Is Intentionally Separate)

### 1. Teletype Direct Output (Bypass)

```
TeletypeTrackEngine::cvOutput(index):
    return _performerCvOutput[index]   // Written directly by Teletype script ops

TeletypeTrackEngine::gateOutput(index):
    return _performerGateOutput[index]  // Written directly by Teletype script ops
```

**Status:** Intentionally dual-path. Teletype scripts write directly to `_performerCvOutput[8]` and `_performerGateOutput[8]` via `handleCv()` and `handleTr()`. These bypass the normal track engine → rotation → CV route pipeline.

**Why:** Latency. Teletype scripts expect immediate output response. Routing through the full pipeline would add tick-level delay.

**Needs verification:** Teletype may still participate in rotation because it is a `TrackEngine` from `updateTrackOutputs()` perspective. If `cvOutputTracks[i]` resolves to a Teletype track, rotation applies. Verify whether the project allows Teletype tracks in `cvOutputTracks[]`.

### 2. RoutingEngine Model Writes

```
RoutingEngine::writeEngineTarget(Target target, float normalized):
    // Writes to model properties, not output buffers
    // Example: setSlideTime(), setTranspose(), setOctave()
```

**Status:** Intentionally separate. RoutingEngine does not write to `_cvOutput` or `_gateOutput` directly. It writes to **model properties** that track engines read on the next tick.

**Risk:** This is "parameter modulation" not "output modulation." The track engine reads the modulated parameter on its next `tick()`, so there is a 1-tick delay between routing update and audible effect.

### 3. Bus CV

```
Engine::setBusCv(index, volts):
    _busCvWritten[index] = true;
    _busCv[index] = clamp(volts, -5.f, 5.f);  // or slew-limited in safe mode
```

**Status:** Separate additive path. Bus CV is:
- Written by Teletype scripts (`setBusCv` op)
- Written by CV route lanes (`OutputDest::Bus`)
- Read by CV route lanes (`InputSource::Bus`)
- Read by Teletype scripts (`busCv` op)
- **Not** directly connected to physical CV outputs

### 4. CV Route Lanes

```
Engine::updateCvRouteOutputs():
    // 4 lanes: each can scan/route/interpolate inputs
    // Outputs go to _cvRouteOutputs[lane]
    // Each lane can output to: CV Out, Bus, or None
```

**Status:** Separate processing stage. CV route lanes:
- Read from CV inputs or bus CV
- Apply scan + route interpolation
- Write to `_cvRouteOutputs[]` or `_busCv[]`
- Are consumed by `updateTrackOutputs()` as a track engine alternative (lane track index = `CONFIG_TRACK_COUNT`)

---

## Do Not Unify Without Proving Behavior

| Dual-Path | Reason to Prove Before Unifying |
|---|---|
| Teletype direct output | Prove that latency remains acceptable and rotation still applies correctly if unified through standard pipeline. |
| RoutingEngine model writes | Prove that direct output writes do not create feedback loops between routing and track engines. |
| Bus CV | Prove that the bus concept (inter-script/track modulation) is preserved if unified with track CV outputs. |
| CV route lanes | Prove that scan/route interpolation behavior is preserved if merged into track engine outputs. |

---

## Conflict Rules

### Rule 1: Multiple Writers to Same Physical Output

```
If _cvOutputOverride is true:
    All CV outputs = _cvOutputOverrideValues[i]  (override wins)
Else:
    If cvOutputTracks[i] == CONFIG_TRACK_COUNT:
        CV output = _cvRouteOutputs[lane]  (CV route wins over track engine)
    Else:
        CV output = _trackEngines[track]->cvOutput(slot)  (track engine wins)

If _gateOutputOverride is true:
    All gate outputs = _gateOutputOverrideValue  (override wins)
Else:
    Gate output = _trackEngines[track]->gateOutput(slot)  (track engine wins)
```

### Rule 2: Rotation Conflicts

```
Gate rotation:
    - Only tracks with isGateOutputRotated() participate
    - Rotation offset is applied per-track, not per-output
    - Non-rotating tracks keep their direct mapping

CV rotation:
    - Discrete rotation: offset wraps within pool
    - Interpolated rotation: _routingEngine provides fractional offset, pool values are mixed
    - If routing CV rotate is active, _routingEngine.cvRotateValue(track) provides the offset
```

### Rule 3: RoutingEngine vs Track Engine

```
RoutingEngine does NOT conflict with track engines for physical outputs.
RoutingEngine writes to MODEL properties (e.g., slide time, transpose).
Track engines READ those properties on next tick.
This is a producer/consumer relationship, not a conflict.
```

---

## Load-Bearing Assumptions

| Assumption | Risk If Changed |
|---|---|
| Teletype bypass is for latency | If forced through standard pipeline, script ops like `CV 1 V 5` would have 1-tick delay, breaking Teletype timing semantics. |
| RoutingEngine writes to model, not output | If changed to direct output writes, feedback loops between routing and track engine would destabilize modulation. |
| Override is absolute | If override became additive or mixed, live performance override semantics would break. |
| Bus CV is separate from track CV | If unified, scripts using `busCv` for inter-track modulation would output unintended voltages. |
| CV route lanes are a separate stage | If merged into track engine, the scan/route interpolation would be lost or duplicated. |

---

## Documentation Gaps

| Gap | What to Document |
|---|---|
| Teletype bypass | Document that Teletype CV/gate outputs bypass rotation and CV route lanes for latency reasons. |
| RoutingEngine delay | Document that routing modulation has 1-tick delay because it writes model properties, not output buffers. |
| Override semantics | Document that overrides replace all outputs, including routing and bus CV effects. |
| Bus CV decay | Document that bus CV channels decay in safe mode if not written within a tick. |
| CV route lane mapping | Document that lane outputs map to physical CV outputs via `cvOutputTracks[i] == CONFIG_TRACK_COUNT`. |

---

## Where Findings Go

- Output authority map → architecture research (Direction 4) — documentation only, no RAM recovery
- Teletype bypass latency rationale → architecture research + Teletype boundary doc (Direction 6)
- RoutingEngine model-write semantics → architecture research (Direction 3) — explains why routing is modulation, not passthrough
- Override/bus CV/route lane documentation → user-facing documentation or inline code comments
