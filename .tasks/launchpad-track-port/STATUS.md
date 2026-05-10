# Status

**Planning complete, reviewed** — Full implementation plan written and independently reviewed for feasibility. Implementation not started.

## What was done

- Investigated all Launchpad-related files (12 files, 2 core)
- Mapped all 14 dispatch points in LaunchpadController.cpp that switch on `trackMode()`
- Analyzed all 7 track types for grid-suitability
- Deep-dived into DiscreteMapSequence, IndexedSequence, TuesdaySequence, TeletypeTrack APIs
- Checked model drift since last Launchpad commit (`888f183c`)
- Examined `temp-ref/mebitek-performer/` reference implementation (3240-line .cpp)
- **Re-examined mebitek with fresh context** — extracted LayerMapItem pattern, Performer mode, circuit keyboard
- **Performed feasibility/priority/risk review** — found Indexed macros don't exist in model, several method name errors, file size risk
- **Removed MidiCvTrack** — no sequence data, arpeggiator only; lowest value for Launchpad grid
- **Created corrected implementation plan** with 6 track types, reordered phases, deferred items

## Plan Summary (Corrected)

| Phase | Track | Effort | Description | Status |
|-------|-------|--------|-------------|--------|
| 1 | Note | ~1.5h | Fix 6 missing layers, step highlighting, add Mode selector | Must-do |
| 2 | Tuesday | ~2.5h | **2×8 step-key grid** — 1:1 HW parity + parameter editor | Must-do |
| 3 | DiscreteMap | ~3h | 32-stage editor: threshold/direction/note/params + 16 macros | Must-do |
| 4 | Indexed | ~3.5h | 48-step grid editor (Pitch/Duration/GateSlide/Route). **No macros yet** | Must-do |
| 5 | Teletype | ~4-5h | Text input grid + script triggers + TR/CV output monitor | Must-do |
| 6 | Curve chaos | — | **DEFERRED** — low value, revisit later | Deferred |
| 7 | Macro Grid v2 | ~3h | Curve (12) + DiscreteMap (16). **Indexed macros need model refactor** | Phase 2.5 → v2 |

**Total: ~15-18h** (was 17h, corrected with realistic estimates and MidiCv removed)

## Removed: MidiCvTrack

MidiCvTrack has no step sequence, no per-step data, and no `isActiveSequence()`. The only thing to show on a Launchpad grid would be the Arpeggiator parameters (enabled, hold, mode, divisor, gateLength, octaves) and voice activity (8 voices, gate on/off). This is low-value compared to the effort — an arpeggiator has 5-6 parameters that are already editable via the device's LCD UI. **Removed from plan entirely.**

## Key Review Findings

| Finding | Severity | Action |
|---------|:--------:|--------|
| IndexedSequence macros don't exist in model — they're 500+ lines of UI code | 🔴 High | Defer Indexed macros. Ship grid editor first. |
| `transformSmooth`/`transformWalk`/`transformRandom` don't exist | 🟡 Medium | Use `transformSmoothWalk`. Fix in plan. |
| `DiscreteMapSequence::clearNotes()`/`clearDirs()` don't exist | 🟡 Medium | Use `clear()` or add trivial methods. |
| `isEdited()` missing on 3 sequence types | 🟡 Medium | Add ~15 min each. Safe (no serialization impact). |
| File will grow 974 → ~2600 lines | 🟡 Medium | **Split into per-track files BEFORE coding.** |
| Flash impact: +30–50 KB | 🟢 Low | Reaches ~460 KB / 1 MB. Safe. |
| RAM impact: ~16 bytes | 🟢 Low | Negligible. |

## Key design decisions

- **Layer button** stays consistent: LayerMapItem[] arrays for all track types, even those without native Layer enums
- **FirstStep/LastStep** available only on tracks that have those concepts
- **No model changes needed** — *except* `isEdited()` for 3 sequence types (~45 min total)
- **Split `LaunchpadController.cpp`** into per-track files before implementing
- **Indexed macros deferred** — they need model refactoring first
- **MidiCvTrack removed** — no sequence data, lowest value for grid

## Pre-implementation checklist

- [ ] Add `isEdited()` to DiscreteMapSequence, IndexedSequence, TuesdaySequence
- [ ] Fix method names in plan (`transformSmoothWalk`, not `transformSmooth`/`transformWalk`)
- [ ] Split `LaunchpadController.cpp` into per-track files
- [ ] Verify all `Project.h` accessors compile
- [ ] Add `selectedLayer` to `_sequence` state struct

## TDD Infrastructure (2026-05-04)

### Existing Test Framework

**C++ unit tests** (custom framework, `src/test/UnitTest.h`):
- 43+ active test files in `src/tests/unit/sequencer/` and `src/tests/unit/core/`
- `UNIT_TEST(name) { CASE("desc") { expect(...); } }` pattern with `setjmp/longjmp` per-case isolation
- Built via CMake, registered with CTest via `register_sequencer_test()` — CI runs `make test`
- Tests link against `core` + `sequencer_shared` (full model+engine static lib)
- Pattern: stack-construct `Model`/`Project`/`Engine`, configure, call methods, `expectEqual()`

**Python UI tests** (pybind11 + SDL simulator):
- `src/apps/sequencer/tests/testframework/` — `UiTest` base class + `Controller` fluent API
- `Controller`: button press/release, encoder, ADC, MIDI input, screenshots — all via `Simulator` pybind11 bindings
- Full model access: `env.sequencer.model.project.track(i).noteTrack.sequence(j).step(k).gate`
- **CRITICAL GAP**: `TargetState` (LEDs, gates, DAC, LCD) is NOT Python-bound — cannot verify hardware output from Python tests

**Integration tests** (`src/tests/integration/drivers/`):
- 11 driver tests using `IntegrationTest` base + `Simulator` + SDL frontend
- Only built as standalone executables, NOT registered with CTest
- Run on-device via STM32 `IntegrationTestRunner`

### Launchpad Testability Assessment

**LaunchpadController is NOT directly unit-testable** because:
1. Constructor needs `ControllerManager`, `Model`, `Engine`, `ControllerInfo` — solvable (same as engine tests)
2. Device selection (Mk2/Mk3/Pro) sends MIDI SysEx on `initialize()` — needs real or mock device
3. `sendMidi()` routes through `ControllerManager` to `MidiPort` — needs full MIDI stack
4. `LaunchpadDevice` has no virtual interface — cannot inject a mock
5. Button state read via `_device->buttonState(row, col)` — needs device
6. LED writes go to `_device->setLed()` → `_ledState[]` → `syncLeds()` sends MIDI — same dependency

### Recommended TDD Approach

**Option C (Pragmatic): Test pure logic separately, then integration-test through simulator.**

1. **Extract pure-logic helpers** into `LaunchpadLogic.h/.cpp`:
   - `Button` struct methods (`isGrid()`, `isFunction()`, `isScene()`, `gridIndex()`)
   - `Navigation` calculations
   - `RangeMap::map()`/`unmap()`
   - `stepColor()` and LED-value computation
   - These are pure functions needing no device — immediately unit-testable

2. **Add TargetState Python bindings** (P0 blocker for verification):
   - Bind `LedState`, `GateOutputState`, `DacState`, `TargetState` to pybind11
   - Enables: simulate Launchpad MIDI input → verify LED output from Python
   - ~1-2h effort, high leverage

3. **Add MIDI output capture** (P0 for Launchpad-specific testing):
   - Ring buffer of recent `MidiEvent` outputs accessible from Python
   - Enables: verify Launchpad LED feedback MIDI messages are correct

4. **Write C++ model/engine unit tests** for each phase:
   - Phase 1 (Note): Test `NoteSequence` layer selection, step highlighting math, `noteFirstStep`/`noteLastStep` in Ikra mode
   - Phase 3 (DiscreteMap): Test `DiscreteMapSequence` stage navigation, `isEdited()` (add it first)
   - Phase 4 (Indexed): Test `IndexedSequence` step access, `isEdited()` (add it first)

5. **Write Python integration tests** for each phase after TargetState bindings:
   - Simulate Launchpad MIDI note-ons → verify correct LED MIDI note-offs
   - Simulate encoder rotations → verify model state changes
   - Verify layer navigation → verify LED row changes

### Pre-implementation TDD Checklist (adds ~2-3h)

| Task | Effort | Blocks |
|------|--------|--------|
| Add `TargetState` Python bindings (LedState, GateOutputState, DacState) | ~1.5h | All Python verification tests |
| Add MIDI output capture to Python bindings | ~0.5h | Launchpad LED feedback tests |
| Extract pure-logic helpers from `LaunchpadController.cpp` | ~1h | C++ unit tests for Button/Navigation/stepColor |
| Add `isEdited()` to 3 sequence types | ~0.75h | DiscreteMap/Indexed/Tuesday pattern page tests |
| Add C++ unit test for `NoteSequence` layer/step math | ~0.5h | Phase 1 verification |
