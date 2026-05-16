# Chaos Modulator Shapes — Implementation Spec

## Goal
Add `ChaosLorenz` and `ChaosLatoocarfian` as two new modulator shapes, leveraging the `Lorenz.h` and `Latoocarfian.h` generators already wired into `CurveTrackEngine`. Both shapes get 2-page UI (no pagination for 1-page shapes). No new model fields.

---

## 1. Model: `src/apps/sequencer/model/Modulator.h`

### Shape enum extension
```cpp
enum class Shape : uint8_t {
    Sine,
    Triangle,
    SawUp,
    SawDown,
    Square,
    Random,
    ADSR,
    ChaosLorenz,       // new
    ChaosLatoocarfian, // new
    Last
};
```

### Rate — dual semantics
- **LFO shapes** (Sine/Triangle/Saw/Square/Random/ADSR): `rate` = PPQN ticks per cycle, displayed as musical divisions ("1/4", "1/8T")
- **Chaos shapes**: `rate` = Hz × 10, stored as uint16, displayed as Hz
- **Serialization**: unchanged — `_rate` on disk is just a uint16, interpretation is runtime only

### `printRate()` branching (Hz for chaos)
```cpp
// Display: 0.01-0.99 → "0.05Hz"
//          1.0-9.9   → "2.5Hz"
//          10+       → "50Hz"
```

### `editRate()` branching (Hz for chaos)
```cpp
// Normal: step = 0.5 Hz
// Shift: step = 0.1 Hz
// Range: 0.1 Hz to 100.0 Hz
```

### Parameter mapping — Chaos shapes reuse model fields

| Display | Field | Lorenz semantics | Latoocarfian semantics |
|---------|-------|-----------------|------------------------|
| RATE | `_rate` | Hz × 10 | Hz × 10 |
| P1 | `_attack` / 20 | `rho = 10 + (attack/20) * 0.4` → 10..50 | `a = b = 0.5 + (attack/20) * 0.025` → 0.5..3.0 |
| P2 | `_decay` / 20 | `beta = 0.5 + (decay/20) * 0.035` → 0.5..4.0 | `c = d = 0.5 + (decay/20) * 0.025` → 0.5..3.0 |
| SLEW (Pg 2) | `_smooth` | **NOT APPLIED** — Lorenz is already continuous. Ignored. | Applied as post-chaos glide (same as Random Clocked) |
| OFFSET (Pg 2) | `_offset` | DC bias -64..+63 | DC bias -64..+63 |
| DEPTH | `_depth` | Output amplitude 0-127 | Output amplitude 0-127 |

**Key finding from CurveTrack research:** Latoocarfian `a` and `b` both derive from `p1`, while `c` and `d` both derive from `p2`. So P1 and P2 each map to two parameters symmetrically. This is how CurveTrack already does it; copy that behavior.

---

## 2. Engine: `src/apps/sequencer/engine/ModulatorEngine.h`

### New state members (160 B)
```cpp
struct ChaosState {
    float x, y, z;  // Lorenz uses x/y/z; Latoocarfian uses x/y only
};
std::array<ChaosState, CONFIG_MODULATOR_COUNT> _chaosState;    // 96 B
std::array<float, CONFIG_MODULATOR_COUNT> _chaosValue = {};      // 32 B
std::array<float, CONFIG_MODULATOR_COUNT> _chaosPhase = {};      // 32 B
```

**Total: 160 B** `.bss` (direct Engine member, outside containers)

### `tick()` chaos branch (inserted after ADSR, before Clocked Random)

```text
gate handling:
  Sync/Retrigger: reset chaos state to seed on rising edge
  Hold: reset on rising edge, freeze evolution while gate low
  Free: always advance

rate = _rate / 10.0f (Hz)
dt = tickDeltaMs / 1000.0f

if (Lorenz):
  subTicks = max(1, int(hz * 1000 * dt))
  subDt = dt / subTicks
  rho = 10.0f + (attack/20) * 0.4f
  beta = 0.5f + (decay/20) * 0.035f
  for i=0..subTicks-1:
    value = lorenz.next(subDt * hz, rho, beta)
  // apply depth, offset
  // NO SLEW — raw output

if (Latoocarfian):
  _chaosPhase += hz * dt
  if (_chaosPhase >= 1.0f):
    _chaosPhase -= 1.0f
    a = b = 0.5f + (attack/20) * 0.025f
    c = d = 0.5f + (decay/20) * 0.025f
    _chaosValue = latoocarfian.next(a, b, c, d)
  // apply depth, offset, smooth (slew)
```

### Sync seed offset
On gate rising when shape is Lorenz:
```cpp
// Instead of reset() to (0.1, 0, 0):
float perturbation = _phase / 36000.0f;  // _phase 0-359 → 0.000-0.00999
_x = 0.1f + perturbation;
_y = perturbation * 0.5f;
_z = 0.0f;
```
Phase (0-359°) becomes a deterministic seed perturbation. Different phase values = different starting basin points. Same phase = reproducible reset.

### `reset()` extension
Reset chaos state for all modulators on engine reset.

---

## 3. UI: `src/apps/sequencer/ui/pages/ModulatorPage.cpp`

### Page count
```cpp
if (isADSR || isChaos) {
    _totalPages = 2;
} else {
    _totalPages = 1;
}
```

### Footer — Page 1 (chaos)
```
[SHAPE] [RATE] [P1] [P2] [DEPTH]
```

### Footer — Page 2 (chaos)
```
[SLEW] [OFFSET]
```
(SLEW dimmed/ignored for Lorenz — shown but has no effect)

### Parameter display — Page 1
| Function | Lorenz display | Latoocarfian display |
|----------|---------------|---------------------|
| SHAPE | "ChaosL" / "ChaosLa" | same |
| RATE | Hz display (2.5Hz) | same |
| P1 | "RHO 28" or "P1 45" | "A 1.5" or "P1 45" |
| P2 | "BETA 2.7" | "C/D 1.2" |
| DEPTH | "%d" (same) | same |

### Parameter display — Page 2
| Function | Display |
|----------|---------|
| SLEW | "%dms" (smooth) — dimmed for Lorenz |
| OFFSET | "%+d" (same) |

### Encoder handling — chaos
```cpp
// Page 1
SHAPE: editShape
RATE:  editRate (Hz mode)
P1:    editAttack
P2:    editDecay
DEPTH: editDepth

// Page 2
SLEW:  editSmooth
OFFSET: editOffset
```

### Waveform visualization
- **Lorenz**: Mini-simulation in `updateWaveformCache()`. Warm-up 100 steps to skip transient. Sample 112 points. Draw as connected line.
- **Latoocarfian**: Each cache point = one `next()` call. Draw as step plot (discrete points or horizontal segments).
- `currentValue` level bar: same as all shapes (slim vertical bar showing 0-127).

### Waveform cache label
Show "Chaos" or shape name in header area (same pattern as "ADSR").

---

## 4. Files to touch

| File | Change |
|------|--------|
| `model/Modulator.h` | +2 enum values, `isChaosShape()`, `printRate()` branching, `editRate()` branching |
| `engine/ModulatorEngine.h` | `ChaosState`, `_chaosValue[]`, `_chaosPhase[]`, chaos tick branch, sub-tick loop, sync seed offset |
| `ui/pages/ModulatorPage.cpp` | `_totalPages=2` for chaos, footer labels, encoder dispatch, parameter display, `updateWaveformCache()` chaos branches |
| `ui/pages/ModulatorPage.h` | None (unless adding helper booleans) |

**No changes to:** `WaveformGenerator.h`, `Engine.cpp`, `MidiOutputEngine`, serialization, `Project.h`.

---

## 5. RAM Budget

| Component | Size |
|-----------|------|
| `_chaosState[8]` (12 B struct) | 96 B |
| `_chaosValue[8]` (4 B float) | 32 B |
| `_chaosPhase[8]` (4 B float) | 32 B |
| **Total** | **160 B** `.bss` |

No model change, no container inflation. Under 500 B remaining budget.

---

## 6. Open UX questions (decide at implementation time)

1. **P1/P2 display label**: Use param names ("RHO"/"BETA", "A"/"C/D") or generic "P1"/"P2"? Generic is simpler, names are more informative.
2. **SLEW on Lorenz page 2**: Show dimmed with "N/A" value, or hide entirely? Showing it with "N/A" maintains UI symmetry across shapes.
3. **Rate default for chaos**: Currently `_rate=96` (quarter note for LFO) → chaos starts at 9.6 Hz. Good or change default? 9.6 Hz is a reasonable middle ground.
4. **Chaos shape names**: "ChaosL" vs "ChaosLrtz" vs "Lorenz" — OLED width is limited. "ChaosL" and "ChaosLa" are 7 chars each.

---

## 7. Decisions from CurveTrack research

| Decision | Source |
|----------|--------|
| Latoocarfian c/d derive from P2 (not fixed) | `CurveTrackEngine.cpp: float c = 0.5f + p2 * 2.5f; float d = 0.5f + p2 * 2.5f;` |
| Rate display format `%.2fHz / %.1fHz / %.0fHz` | `CurveSequence.h: void printChaosRate(StringBuilder &str)` |
| Hz storage as float 0.01-50.0, mapped from 0-127 | `CurveSequence.h: chaosHz()` function with segmented mapping |
