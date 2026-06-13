---
id: feat-tt2-trigger-input-firing
schema: plan
title: "feat: TT2 trigger-input script firing"
type: feat
status: completed
date: 2026-06-14
depth: standard
---

# feat: TT2 trigger-input script firing

## Summary

Make the native `TrackMode::TeletypeV2` engine **fire scripts on external
trigger/gate edges** — the defining Teletype-on-gates capability it currently
lacks (today it only self-runs via boot/metro/delay). Mirror the legacy
`TeletypeTrackEngine`'s input model: per-input source dispatch (CvIn threshold /
GateOut readback / LogicalGate), rising-edge detection, `runScript(i)` on the
edge. Add a minimal serialized per-input **source config** so the mechanism is
complete, but **default** it (CvIn1-4 → trigger scripts) — the editing UI for
that config is the editor plan's deferred I/O grid and stays deferred here.

This is the top "engine mechanics" item in `docs/teletype_v2.md` (Current State →
Remaining → engine mechanics). The legacy `TeletypeTrackEngine` is the reference;
the native engine already has `_engine` access + a working `runScript()`.

---

## Scope Boundaries

**In scope:** the native edge-detect → script-fire mechanism in `TT2TrackEngine`; the full source-dispatch (CvIn/GateOut/LogicalGate) mirroring the legacy `inputState`; a serialized per-input source config (defaulted, not yet user-editable); rising-edge firing for the 4 trigger inputs (TI-TR1-4 → scripts 0-3).

### Deferred to Follow-Up Work
- **I/O routing config UI** — editing which source feeds each trigger input lives in the editor's deferred I/O grid (`docs/plans/2026-06-14-001-feat-tt2-editor-repoint-plan.md`). This plan ships the config *data* (defaulted) so the grid later just edits existing fields.
- **Per-input polarity (falling-edge)** — upstream supports rising/falling via `script_pol & 1/2`; the legacy Performer engine is rising-only. Mirror rising-only now; falling-edge config rides with the I/O grid.
- **IN / PARAM CV sampling** and **output shaping** — separate roadmap items (`teletype_v2.md`).

---

## Key Technical Decisions

- **Mirror the legacy `inputState` dispatch, default the source.** Replicate the CvIn-threshold (`>1V`) / GateOut-readback / LogicalGate source types from `TeletypeTrackEngine::inputState`, driven by a per-input source enum. TT2 has no such config today, so add a small one and **default TI i → CvIn(i+1)**. Full mechanism, default routing, UI deferred.
- **4 trigger inputs → scripts 0-3**, matching the legacy `TriggerInputCount`. TT2 has 6 scripts; trigger scripts are 0-3. **Confirm at execution** that 0-3 don't collide with the special boot/metro indices (`TT2_INIT_SCRIPT` / `TT2_METRO_SCRIPT`) — if they do, define the trigger→script mapping to avoid the reserved slots.
- **Sample edges in `update(dt)`.** The native engine's per-refresh hook is `update(float dt)`; sample inputs + detect edges there (every main-loop pass), mirroring how the legacy engine polls. Rising edge = `now && !prev` per input, tracked in engine-local prev-state.
- **Source config is saved track data.** Add the per-input source enums to the serialized model (TT2Track / TeletypeProgram); bump the size assert, no migration (dev files break per project rule).

---

## Implementation Units

### U1. Trigger-input source config in the TT2 data model

**Goal:** A serialized, defaulted per-input source selector the engine reads and the future I/O grid will edit.
**Dependencies:** none.
**Files:** `src/apps/sequencer/model/TT2Track.h` (or `TeletypeProgram.h`), `src/tests/unit/sequencer/TestTeletypeV2TriggerConfig.cpp` (new) or fold into U2's test.
**Approach:** Add a `TT2TriggerInputSource` enum mirroring the legacy `TeletypeTrack::TriggerInputSource` (CvIn1-4, GateOut1-8, LogicalGate1-8, plus an Off/None). Store 4 of them (one per TI) in the saved data; `init()` defaults them to CvIn1-4. It serializes with the existing raw-struct write — update the `sizeof` static_assert. No migration.
**Patterns to follow:** `TeletypeTrack::TriggerInputSource` + its accessor `triggerInputSource(index)`; the existing `init(TeletypeProgram&)` defaults.
**Test scenarios:**
- After `init`, the 4 sources default to CvIn1, CvIn2, CvIn3, CvIn4.
- Round-trip: set a source, serialize→deserialize, value preserved (covered by existing program serialization tests or a small new one).

### U2. Native edge-detect + script firing in TT2TrackEngine

**Goal:** Fire the matching script on a rising edge of each trigger input.
**Dependencies:** U1.
**Files:** `src/apps/sequencer/engine/TT2TrackEngine.h`, `src/tests/unit/sequencer/TestTeletypeV2TriggerInput.cpp` (new), `src/tests/unit/sequencer/CMakeLists.txt`.
**Approach:** Add engine-local `_inputState[4]` / `_prevInputState[4]`. Add `inputState(i)` mirroring the legacy dispatch: resolve the U1 source enum → read `_engine.cvInput().channel(n) > 1.0f` (CvIn), `_engine.gateOutput()` bit (GateOut), or `_engine.trackEngine(n).gateOutput(0)` (LogicalGate); Off → false. Add `updateInputTriggers()`: for each of the 4 inputs, `now = inputState(i)`; if `now && !prev[i]` → `runScript(triggerScriptIndex(i))`; store prev. Call it from `update(dt)`. Factor the pure edge-decision (`now`, `prev` → fire?) into a testable free function so the firing logic is unit-coverable without an `Engine`. Respect `mute()` consistent with output gating.
**Execution note:** Test-first on the pure edge-decision helper; the source-read + wiring is integration (sim/on-device).
**Patterns to follow:** `TeletypeTrackEngine::updateInputTriggers` + `inputState` (the reference); the engine's existing `runScript()` + `update(dt)` structure.
**Test scenarios:**
- Edge helper: low→high fires once; high held across calls does **not** refire; high→low does not fire; low stays no-fire.
- Four inputs are independent (edge on input 2 fires only script for input 2).
- `runScript` is invoked with the correct trigger→script index for each input; reserved boot/metro indices are not clobbered.
- (Integration, sim/device) a CV-in crossing 1V fires its script; a held-high input fires once, not every tick.

### U3. Wire-up verification + roadmap status

**Goal:** Confirm the mechanism end-to-end and reflect it in the roadmap.
**Dependencies:** U1, U2.
**Files:** `docs/teletype_v2.md` (flip trigger-input firing from "Remaining" to "Done"), full `TestTeletypeV2*` suite + STM32 release.
**Approach:** Run the suite + STM32 build; on sim/device, drive a trigger input and confirm the script fires (CV/gate output changes). Update the roadmap's Current State.
**Test scenarios:** `Test expectation: none (verification + docs)` — covered by U1/U2 tests + the on-device check.

---

## System-Wide Impact

- `TT2TrackEngine` gains an input-sampling pass in `update(dt)` and a few prev-state bytes (watch the `sizeof(TT2TrackEngine) <= 944` assert).
- The saved TT2 model grows by 4 source enums per track (small); bump its size assert.
- Reads `_engine.cvInput()` / `gateOutput()` / `trackEngine()` — same surfaces the legacy engine already uses; no new engine plumbing.
- No change to the op layer, runtime, or output state.

---

## Risks & Mitigations

- **Trigger→script index collision** with `TT2_INIT_SCRIPT`/`TT2_METRO_SCRIPT`. Mitigation: confirm the indices at execution; choose a mapping that avoids reserved slots (Key Decisions).
- **Edge sampling rate** — `update(dt)` runs per main-loop pass; a gate shorter than one pass could be missed. Mitigation: matches the legacy engine's behavior (acceptable); note it, don't over-engineer.
- **`sizeof(TT2TrackEngine)` / model size asserts** trip on the new fields. Mitigation: bump asserts; verify STM32 release links (post-`-Os` headroom is ample).
- **Testability** — full path needs an `Engine`. Mitigation: factor the edge-decision into a pure, unit-tested helper; integration via sim/device.

---

## Verification

- U1/U2 unit tests green (config defaults/round-trip; edge-decision helper; per-input independence; correct script index).
- Full `TestTeletypeV2*` suite + STM32 release green.
- On sim/device: a trigger/CV input crossing threshold fires its script once per rising edge; held-high does not refire; output reflects the script.
- `docs/teletype_v2.md` Current State updated (trigger-input firing → Done).
- Execution posture: edge-decision helper test-first; source-read + wiring integration-verified.
