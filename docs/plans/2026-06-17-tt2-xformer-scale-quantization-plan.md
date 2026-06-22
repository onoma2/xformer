---
id: feat-tt2-xformer-scale-quantization
schema: plan
title: "feat: TT2 QT.S/QT.CS use Performer Scale/UserScale"
type: feat
date: 2026-06-17
status: active
depth: standard
---

# TT2 Xformer Scale Quantization Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make TT2 `QT.S` and `QT.CS` quantize through the Performer `Scale` / `UserScale` system, so scripts can target built-in and user scales by `Scale::get(index)`.

**Architecture:** Keep Teletype pitch raw units at the op boundary, but convert internally to Performer float volts for scale lookup. Add a small pure quantization helper that depends on `model/Scale.h`; wire only `QT.S` and `QT.CS` through it. Keep numeric `QT` and Teletype bank ops `QT.B` / `QT.BX` legacy-compatible because they are not scale-index ops.

**Tech Stack:** C++17, UnitTest/ctest under `build/`, existing native TT2 evaluator in `src/apps/sequencer/engine/TeletypeNativeOps.cpp`.

---

## Rationale

- `QT` is numeric quantize-to-multiple, not pitch-scale quantization. It stays unchanged.
- `QT.S raw transpose scale` already has a scale argument. Port that argument to the Performer scale index space: `0..Scale::Count-1`, including User1-User4.
- `QT.CS raw transpose scale degree voices` also has a scale argument. Port it to Performer scale indices, then build the chord mask from scale degrees.
- `QT.B` and `QT.BX` use mutable Teletype `N.B` / `N.BX` 12-bit bank state (`runtime.variables.n_scale_bits/root`). Performer `UserScale` is global indexed scale data, not eight per-scene bitmask banks. Leave these ops legacy in this plan and make that explicit in tests/comments.
- Do **not** use `TT2TrackEngine::rawToVolts()`. That converts physical output raw `0..16383` to `-5V..+5V`. Pitch ops use Teletype 1V/oct pitch raw where `N 12 == 1638`, so conversion is `pitchVolts = raw / 1638.f`.

## Files

- Create: `src/apps/sequencer/engine/TT2ScaleQuantize.h`
- Modify: `src/apps/sequencer/engine/TeletypeNativeOps.cpp`
- Modify: `src/tests/unit/sequencer/TestTeletypeV2Pitch.cpp`
- Optional test-only include: `src/apps/sequencer/model/UserScale.h`

No model storage, serialization, project version, UI, or RAM-significant state changes.

---

## Task 1: Pure Scale Quantization Helper

**Files:**
- Create: `src/apps/sequencer/engine/TT2ScaleQuantize.h`
- Test: `src/tests/unit/sequencer/TestTeletypeV2Pitch.cpp`

- [ ] **Step 1: Write failing helper tests**

Append these cases to `UNIT_TEST("TeletypeV2Pitch")` in `src/tests/unit/sequencer/TestTeletypeV2Pitch.cpp`. Add includes at the top:

```cpp
#include "Config.h"
#include "engine/TT2ScaleQuantize.h"
#include "model/Scale.h"
#include "model/UserScale.h"
```

Then add:

```cpp
CASE("xformer_scale_quantize_major_scale") {
    const Scale &major = Scale::get(1); // Performer Major
    expectEqual(int(tt2QuantizeRawToScale(300, 0, major)), 273, "300 raw -> nearest major degree D");
}

CASE("xformer_scale_quantize_user_scale") {
    int user1 = Scale::Count - CONFIG_USER_SCALE_COUNT;
    auto &scale = UserScale::userScales[0];
    scale.clear();
    scale.setMode(UserScale::Mode::Chromatic);
    scale.setSize(3);
    scale.setItem(0, 0);
    scale.setItem(1, 3);
    scale.setItem(2, 7);

    expectEqual(int(tt2QuantizeRawToScale(300, 0, Scale::get(user1))), 410,
                "User1 {0,3,7}: 300 raw -> minor third");
}

CASE("xformer_scale_quantize_chord_subset") {
    const Scale &major = Scale::get(1);
    expectEqual(int(tt2QuantizeRawToScaleChord(300, 0, major, 1, 3)), 546,
                "Major triad degrees 1/3/5: 300 raw -> E");
}
```

- [ ] **Step 2: Run tests to verify failure**

Run:

```bash
ctest --test-dir build -R TeletypeV2Pitch --output-on-failure
```

Expected: compile failure because `engine/TT2ScaleQuantize.h` and the helper functions do not exist.

- [ ] **Step 3: Implement helper**

Create `src/apps/sequencer/engine/TT2ScaleQuantize.h`:

```cpp
#pragma once

#include "model/Scale.h"

#include "core/math/Math.h"

#include <cmath>
#include <cstdint>

static constexpr float TT2_PITCH_RAW_PER_VOLT = 1638.f;

inline float tt2PitchRawToVolts(int16_t raw) {
    return raw / TT2_PITCH_RAW_PER_VOLT;
}

inline int16_t tt2PitchVoltsToRaw(float volts) {
    int32_t raw = int32_t(std::lround(volts * TT2_PITCH_RAW_PER_VOLT));
    return int16_t(clamp<int32_t>(raw, -32768, 32767));
}

inline int tt2WrappedScaleIndex(int index) {
    if (Scale::Count <= 0) {
        return 0;
    }
    index %= Scale::Count;
    if (index < 0) {
        index += Scale::Count;
    }
    return index;
}

inline float tt2NearestScaleVolts(float volts, const Scale &scale) {
    int center = scale.noteFromVolts(volts);
    int span = scale.notesPerOctave();
    if (span < 1) {
        span = 1;
    }

    float bestVolts = scale.noteToVolts(center);
    float bestDist = std::fabs(volts - bestVolts);

    for (int d = -span - 1; d <= span + 1; ++d) {
        int note = center + d;
        float candidate = scale.noteToVolts(note);
        float dist = std::fabs(volts - candidate);
        if (dist < bestDist) {
            bestDist = dist;
            bestVolts = candidate;
        }
    }
    return bestVolts;
}

inline int16_t tt2QuantizeRawToScale(int16_t raw, int16_t transposeRaw, const Scale &scale) {
    float transpose = tt2PitchRawToVolts(transposeRaw);
    float local = tt2PitchRawToVolts(raw) - transpose;
    return tt2PitchVoltsToRaw(tt2NearestScaleVolts(local, scale) + transpose);
}

inline int tt2PositiveModulo(int value, int mod) {
    if (mod <= 0) {
        return 0;
    }
    value %= mod;
    if (value < 0) {
        value += mod;
    }
    return value;
}

inline float tt2NearestScaleChordVolts(float volts, const Scale &scale, int degree, int voices) {
    int notes = scale.notesPerOctave();
    if (notes < 1) {
        notes = 1;
    }
    voices = clamp(voices, 1, 7);

    int start = tt2PositiveModulo(degree - 1, notes);
    int center = scale.noteFromVolts(volts);
    int centerOct = roundDownDivide(center, notes);

    bool haveBest = false;
    float bestVolts = 0.f;
    float bestDist = 0.f;

    for (int oct = centerOct - 2; oct <= centerOct + 2; ++oct) {
        int deg = start;
        for (int v = 0; v < voices; ++v) {
            int note = oct * notes + deg;
            float candidate = scale.noteToVolts(note);
            float dist = std::fabs(volts - candidate);
            if (!haveBest || dist < bestDist) {
                haveBest = true;
                bestDist = dist;
                bestVolts = candidate;
            }
            deg = (deg + 2) % notes;
        }
    }

    return haveBest ? bestVolts : 0.f;
}

inline int16_t tt2QuantizeRawToScaleChord(int16_t raw, int16_t transposeRaw,
                                          const Scale &scale, int degree, int voices) {
    float transpose = tt2PitchRawToVolts(transposeRaw);
    float local = tt2PitchRawToVolts(raw) - transpose;
    return tt2PitchVoltsToRaw(tt2NearestScaleChordVolts(local, scale, degree, voices) + transpose);
}
```

- [ ] **Step 4: Run tests to verify pass**

Run:

```bash
ctest --test-dir build -R TeletypeV2Pitch --output-on-failure
```

Expected: `TestTeletypeV2Pitch` passes.

- [ ] **Step 5: Commit**

```bash
git add src/apps/sequencer/engine/TT2ScaleQuantize.h src/tests/unit/sequencer/TestTeletypeV2Pitch.cpp
git commit -m "feat(tt2): add Performer scale quantize helper"
```

---

## Task 2: Port `QT.S` to Performer `Scale::get`

**Files:**
- Modify: `src/apps/sequencer/engine/TeletypeNativeOps.cpp`
- Test: `src/tests/unit/sequencer/TestTeletypeV2Pitch.cpp`

- [ ] **Step 1: Write failing evaluator tests**

Append to `UNIT_TEST("TeletypeV2Pitch")`:

```cpp
CASE("qt_s_uses_performer_scale_index") {
    TT2Runtime runtime = {}; init(runtime);
    TT2OutputState output = {}; init(output);
    expectEqual(int(getv("QT.S 300 0 1", runtime, output)), 273,
                "Scale index 1 = Performer Major");
}

CASE("qt_s_reaches_user_scale") {
    int user1 = Scale::Count - CONFIG_USER_SCALE_COUNT;
    auto &scale = UserScale::userScales[0];
    scale.clear();
    scale.setMode(UserScale::Mode::Chromatic);
    scale.setSize(3);
    scale.setItem(0, 0);
    scale.setItem(1, 3);
    scale.setItem(2, 7);

    TT2Runtime runtime = {}; init(runtime);
    TT2OutputState output = {}; init(output);
    FixedStringBuilder<32> line("QT.S 300 0 %d", user1);
    expectEqual(int(getv(line, runtime, output)), 410,
                "QT.S can address User1 through Scale::get");
}
```

If `FixedStringBuilder` is not already available in this test file, add:

```cpp
#include "core/utils/FixedStringBuilder.h"
```

- [ ] **Step 2: Run tests to verify failure**

Run:

```bash
ctest --test-dir build -R TeletypeV2Pitch --output-on-failure
```

Expected: `QT.S 300 0 1` still uses Teletype `table_n_s[1]`, so at least one assertion fails.

- [ ] **Step 3: Wire `QT.S`**

In `src/apps/sequencer/engine/TeletypeNativeOps.cpp`, add includes near the other engine/model includes:

```cpp
#include "TT2ScaleQuantize.h"
#include "model/Scale.h"
```

Replace `opQtS` with:

```cpp
// QT.S v_in transpose scale
static void opQtS(TT2Runtime &, TT2OutputState &, const TeletypeProgram *,
                  int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t vIn = 0, transpose = 0, scale = 0;
    if (!popStack(stack, stackSize, vIn, error)) return;
    if (!popStack(stack, stackSize, transpose, error)) return;
    if (!popStack(stack, stackSize, scale, error)) return;
    const Scale &performerScale = Scale::get(tt2WrappedScaleIndex(scale));
    pushStack(stack, stackSize, tt2QuantizeRawToScale(vIn, transpose, performerScale), error);
}
```

- [ ] **Step 4: Run tests to verify pass**

Run:

```bash
ctest --test-dir build -R TeletypeV2Pitch --output-on-failure
```

Expected: `TestTeletypeV2Pitch` passes.

- [ ] **Step 5: Commit**

```bash
git add src/apps/sequencer/engine/TeletypeNativeOps.cpp src/tests/unit/sequencer/TestTeletypeV2Pitch.cpp
git commit -m "feat(tt2): route QT.S through Performer scales"
```

---

## Task 3: Port `QT.CS` Chord Quantization

**Files:**
- Modify: `src/apps/sequencer/engine/TeletypeNativeOps.cpp`
- Test: `src/tests/unit/sequencer/TestTeletypeV2Pitch.cpp`

- [ ] **Step 1: Write failing evaluator tests**

Append to `UNIT_TEST("TeletypeV2Pitch")`:

```cpp
CASE("qt_cs_uses_performer_scale_chord_subset") {
    TT2Runtime runtime = {}; init(runtime);
    TT2OutputState output = {}; init(output);
    expectEqual(int(getv("QT.CS 300 0 1 1 3", runtime, output)), 546,
                "Major triad from Performer Major: nearest to 300 raw is E");
}

CASE("qt_cs_uses_user_scale_chord_subset") {
    int user1 = Scale::Count - CONFIG_USER_SCALE_COUNT;
    auto &scale = UserScale::userScales[0];
    scale.clear();
    scale.setMode(UserScale::Mode::Chromatic);
    scale.setSize(4);
    scale.setItem(0, 0);
    scale.setItem(1, 2);
    scale.setItem(2, 5);
    scale.setItem(3, 9);

    TT2Runtime runtime = {}; init(runtime);
    TT2OutputState output = {}; init(output);
    FixedStringBuilder<40> line("QT.CS 700 0 %d 1 2", user1);
    expectEqual(int(getv(line, runtime, output)), 683,
                "User1 chord degrees 1/3: nearest to 700 raw is A-ish degree");
}
```

- [ ] **Step 2: Run tests to verify failure**

Run:

```bash
ctest --test-dir build -R TeletypeV2Pitch --output-on-failure
```

Expected: `QT.CS` still uses Teletype `table_n_s` / `table_n_c` chord masks, so at least one assertion fails.

- [ ] **Step 3: Wire `QT.CS`**

Replace `opQtCS` in `src/apps/sequencer/engine/TeletypeNativeOps.cpp` with:

```cpp
// QT.CS v_in transpose scale degree voices
static void opQtCS(TT2Runtime &, TT2OutputState &, const TeletypeProgram *,
                   int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t vIn = 0, transpose = 0, scale = 0, degree = 0, voices = 0;
    if (!popStack(stack, stackSize, vIn, error)) return;
    if (!popStack(stack, stackSize, transpose, error)) return;
    if (!popStack(stack, stackSize, scale, error)) return;
    if (!popStack(stack, stackSize, degree, error)) return;
    if (!popStack(stack, stackSize, voices, error)) return;
    const Scale &performerScale = Scale::get(tt2WrappedScaleIndex(scale));
    pushStack(stack, stackSize,
              tt2QuantizeRawToScaleChord(vIn, transpose, performerScale, degree, voices),
              error);
}
```

- [ ] **Step 4: Run tests to verify pass**

Run:

```bash
ctest --test-dir build -R TeletypeV2Pitch --output-on-failure
```

Expected: `TestTeletypeV2Pitch` passes.

- [ ] **Step 5: Commit**

```bash
git add src/apps/sequencer/engine/TeletypeNativeOps.cpp src/tests/unit/sequencer/TestTeletypeV2Pitch.cpp
git commit -m "feat(tt2): route QT.CS through Performer scales"
```

---

## Task 4: Fence Legacy `QT.B` / `QT.BX` and Regression-Test It

**Files:**
- Modify: `src/apps/sequencer/engine/TeletypeNativeOps.cpp`
- Test: `src/tests/unit/sequencer/TestTeletypeV2Language.cpp`

- [ ] **Step 1: Strengthen bank compatibility tests**

Extend the existing `scale_bank_nb_roundtrip` case in `src/tests/unit/sequencer/TestTeletypeV2Language.cpp`:

```cpp
int16_t q0 = getv("QT.B 300", runtime, output);
expectTrue(q0 >= 0 && q0 <= 16383, "QT.B remains legacy bank quantize");

evalText("N.BX 2 1387 0", runtime, output);
expectEqual(runtime.variables.n_scale_bits[2], int16_t(1387), "N.BX set slot 2");
int16_t q2 = getv("QT.BX 300 2", runtime, output);
expectTrue(q2 >= 0 && q2 <= 16383, "QT.BX remains legacy bank quantize");
```

- [ ] **Step 2: Run tests to verify current pass**

Run:

```bash
ctest --test-dir build -R TeletypeV2Language --output-on-failure
```

Expected: pass before code comments; this task is a regression fence.

- [ ] **Step 3: Add explicit comments in native ops**

In `src/apps/sequencer/engine/TeletypeNativeOps.cpp`, update comments above `opQtB` and `opQtBX`:

```cpp
// QT.B v_in (uses Teletype N.B bank slot 0).
// Deliberately legacy: N.B/N.BX are mutable 12-bit Teletype scale banks,
// not Performer Scale/UserScale indices.
```

```cpp
// QT.BX v_in scale_nb (uses Teletype N.BX bank slots).
// Deliberately legacy for the same reason as QT.B.
```

- [ ] **Step 4: Run tests**

Run:

```bash
ctest --test-dir build -R "TeletypeV2Pitch|TeletypeV2Language" --output-on-failure
```

Expected: both tests pass.

- [ ] **Step 5: Commit**

```bash
git add src/apps/sequencer/engine/TeletypeNativeOps.cpp src/tests/unit/sequencer/TestTeletypeV2Language.cpp
git commit -m "test(tt2): fence legacy QT bank quantizers"
```

---

## Verification

- `ctest --test-dir build -R "TeletypeV2Pitch|TeletypeV2Language" --output-on-failure`
- `ctest --test-dir build -R TeletypeV2 --output-on-failure`
- STM32 release link per repo rule: `cd build/stm32/release && make sequencer`

Expected behavior after all tasks:

- `QT` still quantizes arbitrary numbers to a multiple.
- `QT.S raw transpose scale` uses Performer `Scale::get(scale)`, including User1-User4.
- `QT.CS raw transpose scale degree voices` uses Performer `Scale::get(scale)` and quantizes to a chord subset built from that scale's degrees.
- `QT.B` / `QT.BX` continue to use Teletype `N.B` / `N.BX` 12-bit banks.
- No project serialization, UI, or RAM-significant state changes.

## Deferred Follow-Up

If `QT.B` / `QT.BX` must also use Performer scales, design a separate TT2 scale-bank model:

- decide whether eight TT2 banks store `Scale::get()` indices, copied `UserScale` snapshots, or legacy bitmasks;
- define save/load semantics for human-readable TT2 scenes;
- define how `N.B` / `N.BX` set/get behavior changes without breaking existing TT scripts.

Do not fold that into this plan; it is a storage and compatibility decision, not a helper swap.
