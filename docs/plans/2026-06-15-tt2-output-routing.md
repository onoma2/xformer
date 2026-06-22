---
id: feat-tt2-output-routing
schema: plan
title: "feat: TT2 outputs auto-reach jacks by index (additive CV + OR gate, zero setup)"
type: feat
date: 2026-06-15
status: active
depth: standard
---

# TT2 Output Routing — automatic, additive, zero-setup — Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Open any TT2 track, write `CV 1 10000` / `TR 1 1`, get the value at **physical jack 1** — no Layout, no pickers, no setup. Every TT2 track's `CV n` is **summed** onto physical CV jack `n`; every `TR n` is **OR'd** into physical gate jack `n`. Fixed index mapping, additive — so it's automatic and multiple TT2 tracks coexist.

**Architecture:** Pure engine layer. No model/Project fields, no serialization. In the output precompute the engine first resolves the legacy Layout source path for non-TT2 tracks, including rotation, then applies TT2 as a **post-rotation physical-jack layer**: per physical jack `n`, sum all TT2 tracks' `cvOutput(n)` (`Engine.cpp:754`) and OR all TT2 tracks' `gateOutput(n)` (`:733`). The design rests on **one ownership rule: a TT2 track emits to jacks *only* via this auto-index — it is excluded from the legacy Layout source path and rotation pools** (so it contributes exactly once; see Key Decisions). Two tiny pure helpers carry the testable logic.

**Tech Stack:** C++17, STM32F405. Unit tests under `build/`; sim under `build/sim/debug`.

---

## Context & references

- **CV jack today:** `applyModulatorOffset(jack, base)` → `_cvOutput.setChannel(jack, …)` (`Engine.cpp:754`), inside the `_cvOutputOverride` guard (`:749`). Base = the jack's assigned track/router.
- **Gate jack today:** `_gateOutput.setGate(jack, trackEngines[gateOutputTracks[jack]]->gateOutput(slot))` inside the `_gateOutputOverride` guard (`:731-733`); a rotation pool runs first (`:720`).
- **TT2 outputs:** `cvOutput(0..7)` / `gateOutput(0..7)` per TT2 track (`TT2_OUTPUT_CV_COUNT = TT2_OUTPUT_TR_COUNT = 8`). `CV n` writes internal `cv[n-1]`; `TR n` writes `tr[n-1]`.
- **Non-TT2 engines ignore the index** in `cvOutput(i)`/`gateOutput(i)` (`NoteTrackEngine.h:56`, `TuesdayTrackEngine.h:26`), so the TT2 contribution must be gathered **only from tracks in `TrackMode::TeletypeV2`**.
- **No model change, so no serialization, no RAM growth, no no-migration concern.**

## Key design decisions

- **Fixed index mapping, zero setup.** Physical CV jack `n` (0-based `j`) gets `+ Σ over TT2 tracks of cvOutput(j)`, clamped ±5V. Physical gate jack `j` gets `OR over TT2 tracks of gateOutput(j)`. No selector, no field, no UI.
- **Ownership rule — TT2 emits ONLY via auto-index; it is excluded from the legacy Layout path.** This is the one rule that makes it correct. The legacy CV base (`Engine.cpp:752`, reads `cvOutputTracks[j]`'s track) and gate (`:733`, `gateOutputTracks[j]`) must **skip tracks in `TeletypeV2` mode** — return base `0` / gate `false` for them. Otherwise the default `_cvOutputTracks[i]=i` (`Project.cpp:106`) would have jack `i` read TT2 `CV1` as its base **and** the auto-index would add it again → 2× / clamped, not the written value. With the skip: a TT2 track contributes to jacks *only* by index, exactly once.
- **Rotation is a legacy Layout-source transform; TT2 is post-rotation.** The CV/gate rotation pools are built by assigned track + route mask *before* output resolution (`Engine.cpp:682`/`:699`). A TT2-assigned jack left in a pool would let rotation shuffle a now-silent TT2 legacy source between jacks. So a jack whose assigned track is TT2 must be **excluded from the rotation pool** (consistent with TT2 not being a Layout source). Rotation continues to operate only over non-TT2 jacks. After that, the TT2 auto-index layer is added by physical jack index and does not rotate. This matches the existing modulator-offset order: `applyModulatorOffset(i, rotatedSourceValue)` is post-rotation and keyed by physical channel `i`.
- **What remains additive:** the TT2 auto-index term sums on top of any **non-TT2** base + modulator on jack `j` (e.g. a Note track owns jack 5 and a TT2 `CV 5` layers onto it). TT2 tracks just never go through Layout themselves.
- **Multiple TT2 tracks** both contribute to jack `n` (CV sums, gate ORs) — intentional, like modulators stacking; still exactly-once each because none of them route via Layout.
- **Untouched:** `_cvOutputModulators`/`ModulatorPage`, override guards, and the legacy path for **non-TT2** tracks stay exactly as-is. Rotation is unchanged *for non-TT2 jacks* (TT2-assigned jacks drop out of the pool, per the rule above).

---

## Scope Boundaries

**In scope:** two pure helpers; their use in the CV/gate output precompute; the TT2-skip in the legacy base/gate resolution **and rotation-pool build**; preserving the order `legacy rotation -> legacy base/gate -> physical-jack modulator offset -> TT2 auto-index add/OR -> write`.

**Out of scope:** any per-jack selector / model field (explicitly *not* needed — that was the discarded "manual setup" direction); the I/O config page (`2026-06-15-tt2-hw-parity.md` Phase 1B); the quad; touching modulators or override logic.

**Accepted limitation (no UI in this plan):** the Layout CV/gate source pickers (`CvOutputListModel.h:30`, `GateOutputListModel.h:30`) can still *display* a TT2 track selected as a source, but the engine now ignores it (TT2 routes by index). This inert display is accepted for now; **optional follow-up** (not this plan): exclude/grey TT2 tracks in the Layout source lists so the dead option isn't offered.

---

## Phase 0 — pure helpers (headless TDD)

### Task 1: `sumTt2Cv` / `anyTt2Gate` at a jack index

**Files:**
- Create: `src/apps/sequencer/engine/Tt2OutputMix.h`
- Test: `src/tests/unit/sequencer/TestTt2OutputMix.cpp` (new); register in `src/tests/unit/sequencer/CMakeLists.txt`

**Step 1: Write the failing test**

```cpp
#include "UnitTest.h"
#include "engine/Tt2OutputMix.h"

UNIT_TEST("Tt2OutputMix") {
CASE("cv_sum_adds_active_tracks") {
    // per-track CV at jack index j; only "active" (TT2) tracks count
    float cv[8] = {1.0f, 2.0f, 0,0,0,0,0,0};
    bool  tt2[8] = {true, true, false,false,false,false,false,false};
    expectTrue(Tt2OutputMix::sumCv(cv, tt2, 8) == 3.0f, "1+2 from two TT2 tracks");
}
CASE("cv_sum_skips_non_tt2") {
    float cv[8] = {1.0f, 9.0f, 0,0,0,0,0,0};
    bool  tt2[8] = {true, false, false,false,false,false,false,false};
    expectTrue(Tt2OutputMix::sumCv(cv, tt2, 8) == 1.0f, "track1 (non-TT2) ignored");
}
CASE("gate_or_any_active_tt2_high") {
    bool g[8] = {false, true, false,false,false,false,false,false};
    bool tt2[8] = {true, true, false,false,false,false,false,false};
    expectTrue(Tt2OutputMix::anyGate(g, tt2, 8) == true, "a TT2 gate is high");
    bool tt2b[8] = {true, false, false,false,false,false,false,false};
    expectTrue(Tt2OutputMix::anyGate(g, tt2b, 8) == false, "the only high gate is non-TT2");
}
}
```

**Step 2: Run → fail.** `cmake -S . -B build && make -C build TestTt2OutputMix && ctest --test-dir build -R Tt2OutputMix --output-on-failure` → FAIL (no header).

**Step 3: Implement `Tt2OutputMix.h`** (pure — caller pre-fetches one jack's per-track values):

```cpp
#pragma once
namespace Tt2OutputMix {
// cvAtJack[t] / gateAtJack[t] = track t's CV/gate output for this jack index;
// isTt2[t] = track t is in TeletypeV2 mode. Aggregate only the TT2 tracks.
inline float sumCv(const float *cvAtJack, const bool *isTt2, int nTracks) {
    float s = 0.f;
    for (int t = 0; t < nTracks; ++t) if (isTt2[t]) s += cvAtJack[t];
    return s;
}
inline bool anyGate(const bool *gateAtJack, const bool *isTt2, int nTracks) {
    for (int t = 0; t < nTracks; ++t) if (isTt2[t] && gateAtJack[t]) return true;
    return false;
}
}
```

**Step 4: Run → pass. Step 5: Commit** `feat(engine): Tt2OutputMix sum/or helpers`.

---

## Phase 1 — engine integration

### Task 2: add the TT2 CV sum to each CV jack

**Files:** `src/apps/sequencer/engine/Engine.cpp` (CV precompute, the `applyModulatorOffset` write at `:754`, inside the `_cvOutputOverride` guard)

Precompute once per frame an `isTt2[CONFIG_TRACK_COUNT]` flag (`_project.track(t).trackMode() == Track::TrackMode::TeletypeV2`).

**Ownership rule, two edits (prevents the double-count + stray rotation):**
1. **Pool build (`:699`):** when building `cvPool`, **skip any jack whose `cvOutputTracks[jack]` is a TT2 track** — don't add it to the rotation pool (so rotation never shuffles a TT2-assigned jack).
2. **Base resolution (`:752`):** if `cvOutputTrack` is a TT2 track (`isTt2[cvOutputTrack]`), treat the base as **0** — do not read its `cvOutput`. TT2 reaches the jack only via the auto-index term below.

Then for each physical CV jack `j`, **inside the existing override guard**, after rotation has chosen the legacy source and after the (TT2-skipped) base+modulator value `v`:

```cpp
float cvAtJack[CONFIG_TRACK_COUNT];
for (int t = 0; t < CONFIG_TRACK_COUNT; ++t)
    cvAtJack[t] = isTt2[t] ? _trackEngines[t]->cvOutput(j) : 0.f;
v = clamp(v + Tt2OutputMix::sumCv(cvAtJack, isTt2, CONFIG_TRACK_COUNT), -5.f, 5.f);
_cvOutput.setChannel(j, v);   // unchanged write
```

So jack `j` = (non-TT2 base, else 0) + modulator + Σ TT2 `cvOutput(j)` — TT2 contributes exactly once. (Math headless-tested; wiring sim-verified.) **Commit** `feat(engine): TT2 CV n sums onto physical CV jack n (legacy base skips TT2)`.

### Task 3: OR the TT2 gates onto each gate jack

**Files:** `Engine.cpp` (gate precompute, the `setGate` at `:733`, inside the `_gateOutputOverride` guard)

**Ownership rule, two edits (same shape as CV):**
1. **Pool build (`:682`):** when building `gatePool`, **skip any jack whose `gateOutputTracks[jack]` is a TT2 track** — keep TT2-assigned jacks out of the gate rotation pool.
2. **Gate read (`:733`):** the legacy gate read (`_trackEngines[gateOutputTrack]->gateOutput(slot)`) must **skip** a TT2 `gateOutputTrack` (resolve `g = false` for it) — TT2 gates reach jacks only by auto-index, so a TT2 gate isn't also emitted on whatever jack Layout assigned it.

Inside the existing guard, after the legacy gate `g` is resolved (rotation already applied to the non-TT2 legacy source):

```cpp
bool gateAtJack[CONFIG_TRACK_COUNT];
for (int t = 0; t < CONFIG_TRACK_COUNT; ++t)
    gateAtJack[t] = isTt2[t] ? _trackEngines[t]->gateOutput(j) : false;
g = g || Tt2OutputMix::anyGate(gateAtJack, isTt2, CONFIG_TRACK_COUNT);
_gateOutput.setGate(j, g);   // unchanged write
```

Rotation (`:720`) still runs on the non-TT2 legacy gate before this OR. TT2 `TR n` is post-rotation and fixed to physical gate jack `n`. Sim-verified. **Commit** `feat(engine): TT2 TR n ORs onto physical gate jack n (legacy gate skips TT2)`.

---

## Verification

- Phase 0 helper tests green (`ctest --test-dir build -R Tt2OutputMix`).
- Full suite + STM32 release link clean (no model change → no size/serialization impact).
- Sim, the headline acceptance: a fresh project — user-facing **Track 1 = index 0**, whose default `_cvOutputTracks[0] = 0` (`Project.cpp:106`) — set Track 1 to TeletypeV2, write `CV 1 10000` → **physical CV jack 1 (channel index 0) reads exactly that value, not 2×** (proves the legacy-base skip kills the double-count); `TR 1 1` → **gate jack 1** fires once. No Layout/picker touched.
- With **no** TT2 track present, every jack is bit-identical to today (sum/OR terms 0/false; the legacy skip and pool-exclusion never trigger).
- A TT2 track and a non-TT2 track: the non-TT2 track's jack behaves as today plus any TT2 `CV n` layered on top; the TT2 track emits only by index.
- Modulators and overrides behave exactly as before. **Gate/CV rotation** is unchanged **for non-TT2 jacks**; TT2-assigned jacks are excluded from the pools (verify a TT2 track doesn't get rotated).
- Posture: Phase 0 test-first; engine sim-verified.

## Rollback

Two engine commits + one header; revert in reverse. With no TT2 tracks the added terms are inert, so a partial landing is a no-op over today's output, never a regression.
