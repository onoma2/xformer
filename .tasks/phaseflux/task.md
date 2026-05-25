# phaseflux

## Goal

Implement PhaseFlux as a new Performer `TrackMode::PhaseFlux` — a 4×4 grid sequencer with stateless-ramp engine (DiscreteMap pattern), scale-degree pitch with Bell+vflip-style curve flips (NoteTrack chassis), per-stage MaskMelody/TiltMelody centrality filter (Stochastic borrow), and per-stage transforms (Flux PHAS + MASK borrows). Single CV + single Gate output; multi-jack routing mirrors. Engine designed around `_resetTickOffset` + integer `relativeTick` math; no heap, no RNG, all per-stage params bit-packed into 3 × uint32_t (93 bits, 3 spare).

Full design locked in `docs/phaseflux-spec.md` — single source of truth. Lessons from the design process extracted to `docs/spec-template.md`.

## Key files

**Spec & reference**
- `docs/phaseflux-spec.md` — locked MVP spec (18 sections, all 66 audit questions resolved)
- `docs/spec-template.md` — extracted process + architecture lessons for next track spec
- `ui-preview/pages_phaseflux.py` — rendered UI proposal (4×4 grid + dual scopes + 8-box edit-hold strip)
- `ui-preview/phaseflux-preview.png` — side-by-side default + edit-hold render

**Borrowed patterns (cite when implementing)**
- `src/apps/sequencer/engine/DiscreteMapTrackEngine.cpp:128-198` — stateless-ramp + `_resetTickOffset` pattern (spec §3)
- `src/apps/sequencer/engine/NoteTrackEngine.cpp:69-101` — `evalStepNote` pitch chassis + `Scale::noteToVolts` epilogue (spec §6.2)
- `src/apps/sequencer/engine/NoteTrackEngine.cpp:104, 144` — `reset()` vs `restart()` two-tier transport pattern (spec §11.1)
- `src/apps/sequencer/engine/StochasticTrackEngine.cpp:454-466` — MaskMelody/TiltMelody centrality + tilt math (spec §6.2.1)
- `src/apps/sequencer/engine/StochasticGenerator.cpp:261-311` — `evaluateBurst` clamp law (spec §6.4 pulse collision)
- `src/apps/sequencer/model/StochasticTrack.h:185-195` — Track-level chassis to borrow (spec §12.1)
- `src/apps/sequencer/model/NoteSequence.h:349-374` — BitField packing pattern (spec §16)
- `src/apps/sequencer/model/Bitfield.h` — `UnsignedValue<N>` / `SignedValue<N>` / `BitField<T, Index, Bits>` template
- `src/apps/sequencer/model/Curve.cpp:18-108` — LUT shapes (`RampUp`, `Bell`, `Triangle` reused; `Bounce` LUT TBD at impl time)

**Files created (Phase A — model layer)**
- `src/apps/sequencer/model/PhaseFluxTrack.h/.cpp` — track chassis (StochasticTrack borrow), serializes everything except `_lock`
- `src/apps/sequencer/model/PhaseFluxSequence.h/.cpp` — sequence chassis + 16-stage array; `Stage` is 3 × `uint32_t` bit-packed (24 fields, 93 bits, 3 spare in `_data2[29..31]`)
- `src/tests/unit/sequencer/TestPhaseFluxSequenceSerialization.cpp` — 7 round-trip cases (default + chassis + every stage field + min/max edges + bit-pack non-overlap + lock-not-persisted + Track-level mode round-trip)

**Files still to create (Phase B / C)**
- `src/apps/sequencer/engine/PhaseFluxTrackEngine.h/.cpp`
- `src/apps/sequencer/ui/pages/PhaseFluxEditPage.h/.cpp`
- `src/apps/sequencer/ui/pages/PhaseFluxSequencePage.h/.cpp`
- `src/apps/sequencer/ui/model/PhaseFluxSequenceListModel.h`
- `src/apps/sequencer/ui/model/PhaseFluxTrackListModel.h`

**Files modified (Phase A — Track infrastructure wiring)**
- `src/apps/sequencer/model/Track.h` — `TrackMode::PhaseFlux` (serialize ID 8), `phaseFluxTrack()` accessor pair, Container template parameter, union member, `setTrackIndex` + `initContainer` switch cases
- `src/apps/sequencer/model/Track.cpp` — `clearPattern` / `copyPattern` / `gateOutputName` / `cvOutputName` / `write` / `read` switch cases
- `src/apps/sequencer/model/ClipBoard.h/.cpp` — Pattern union member + copy/paste switch cases
- `src/apps/sequencer/model/Routing.cpp` — `writeTarget` PhaseFlux dispatch (track + sequence Routables via generic `isTrackTarget` / `isSequenceTarget` checks)
- `src/apps/sequencer/model/ProjectVersion.h` — `Version_PhaseFlux_Pending = 36` placeholder constant outside the enum chain (Latest stays at Version35, no per-save version churn)
- `src/apps/sequencer/ui/model/TrackModeListModel.h` — temporary gate skipping PhaseFlux in the forward direction (removed when engine + UI land)
- `src/apps/sequencer/CMakeLists.txt` + `src/tests/unit/sequencer/CMakeLists.txt` — register new sources + test target

**Files to modify in Phase B / C (still pending)**
- `src/apps/sequencer/engine/Engine.cpp:527-554` — add `case TrackMode::PhaseFlux:` that instantiates `PhaseFluxTrackEngine`
- `src/apps/sequencer/ui/pages/TopPage.cpp:97, 296-348, 354-380` — PhaseFlux dispatch cases
- `src/apps/sequencer/ui/pages/TrackPage.cpp:152, 188-190` — PhaseFlux case in `setTrack()` + remove the `Last` assert hole
- `src/apps/sequencer/ui/pages/Pages.h:63` — register `phaseFluxEdit` + `phaseFluxSequence` page members
- `src/apps/sequencer/ui/model/TrackModeListModel.h` — drop the temporary skip once PhaseFlux is selectable
- `src/apps/sequencer/model/Project.h` — add `selectedPhaseFluxSequence()` accessor (first UI consumer triggers this)

## Decisions log

Reverse-chronological — append-only.

- 2026-05-25: 66-question external audit closed in single 1-by-1 pass. Critical math errors (§6.2 direction × curve table) and missing rules (§3.4 cumulative-recompute side effects) fixed. Remaining contradictions (§7 vs §11 accumulator-state) resolved via §7.1 asymmetry-by-design rationale. Spec now internally consistent.
- 2026-05-25: Adopted refined curve model — base shape (Linear/Bell/Bounce temporal, Ramp/Bell/Triangle pitch) + vflip + hflip bits. Exp/Log absorbed by `temporalWarp`. Mirror = Bell+vflip. Stage record 90 → 93 bits, still fits 3 × uint32_t.
- 2026-05-25: Pitch chassis rewritten scale-degree based (NoteTrack-aligned). `basePitch` = `SignedValue<7>` scale degrees. `pitchRange` = {0.5, 1, 2, 3} octaves of active scale. `pitchDirection` = Up/Down/Bipolar-Centered. `accumulatorStep` moved from ±1V to ±15 scale degrees. Output via `Scale::noteToVolts()`.
- 2026-05-25: Per-stage MaskMelody + TiltMelody added (Stochastic centrality-filter borrow, byte-for-byte math match). Per-cell bitmask alternative rejected. Caveat: function is 12-EDO-tuned; documented as known limitation in §6.2.1.
- 2026-05-25: Pulse-collision clamp law from Stochastic `evaluateBurst` — drops PowerBend collisions, enforces `kMinPulseGateTicks = 6` audible floor + 1-tick retrigger gap. `pulseCount` becomes max-cap, not strict.
- 2026-05-25: Transport behavior locked — NoteTrack two-tier (`reset()` full / `restart()` light). Stop preserves all state, Start full-resets, Continue resumes mid-tick.
- 2026-05-25: Engine = stateless-ramp DiscreteMap pattern. Cursor + intra-stage phase derived per tick from `relativeTick`. `cumulativeTicks[17]` table recomputed on stageDivisor/skip edit. Reset Measure preserves accumulator counters; pattern switch clears them (§7.1).
- 2026-05-25: PowerBend ±1 degeneracy guarded by encoding-side clamp — `SignedValue<7>` mapped via `/64` (not `/63`), max-knob lands at ±0.984.
- 2026-05-25: Per-cell Routables deferred to v2. All per-stage fields plain bitfields in MVP.
- 2026-05-25: Serialization version policy: dev iterates at `Version35` with placeholder constant; bump to `Version36` on ship.
- 2026-05-25: UI surfaces locked — three files modelled on NoteTrack pattern. EditPage rendered + verified in `ui-preview/phaseflux-preview.png`.
- 2026-05-25: Locked grid traversal as snake permutation (boustrophedon). All-cells-skipped → §3.5 idle state. `kMinCycleTicks = 1/32 × measureDivisor` floor with proportional scaling.
- 2026-05-25: **Phase A landed.** Stage bit-pack layout: `_data0` = basePitch / pitchRange / pitchDirection / pitchCurve / pitchFlipV/H / pitchWarp / pitchResponse / pulseCount (32 used); `_data1` = temporalCurve / temporalFlipV/H / temporalWarp / temporalResponse / maskMelody / tiltMelody (32 used); `_data2` = phaseShift / mask / maskShift / accumulatorStep / accumulatorLength / gateLength / stageDivisor / skip (29 used, 3 spare bits 29..31). `stageDivisor` enum mapping: `{Div1_32, Div1_16T, Div1_16, Div1_8T, Div1_8, Div1_4T, Div1_4, Div1_2}` (3 bits, 8 slots, default `Div1_16`). `_lock` field present on PhaseFluxTrack but intentionally not serialized (Stochastic parity). `_edited` declared as transient UI dirty bit only — not serialized (NoteSequence parity).
- 2026-05-25: PhaseFlux participates in Project-level scale/rootNote inheritance via `_scale` / `_rootNote` int8_t fields on PhaseFluxSequence, default −1 (inherit). Pattern follows StochasticSequence.
- 2026-05-25: No new `Routing::Target::` enum values for PhaseFlux. Existing `SlideTime` / `Octave` / `Transpose` / `Divisor` / `ClockMult` / `Scale` / `RootNote` targets dispatch through `phaseFluxTrack().writeRouted(...)` and `phaseFluxTrack().sequence(p).writeRouted(...)` like Stochastic. Per-cell routing remains deferred to v2.
- 2026-05-25: ProjectVersion stays at `Version35`. `Version_PhaseFlux_Pending = 36` lives as a standalone `constexpr int` after the enum's closing brace, NOT as a new enum entry — adding it inside the enum would auto-bump `Latest` and cause version-guard churn on every save. Rename + insert into the enum at ship time.
- 2026-05-25: Temporary gate in `TrackModeListModel` skips PhaseFlux during forward cycling. Without it, the user could land on PhaseFlux from the Track Setup page; Engine.cpp has no per-mode case yet, so the old TrackEngine instance would keep ticking with the wrong `_trackMode`, triggering `SANITIZE_TRACK_MODE` asserts or release-build UB. Gate gets removed when `PhaseFluxTrackEngine` + UI page land.
- 2026-05-25: `Project::selectedPhaseFluxSequence()` accessor deferred to Phase B (UI is the only consumer). No tests or model-layer code in this commit reference it; adding it now would be cruft.

## Open questions

All 66 audit questions resolved. No open product decisions before implementation.

Implementer-resolved items (engineering punch list, §16):
- [x] Exact byte-pack offsets within the 3 × uint32_t stage record — see Phase A decision log entry above
- [x] Whether PhaseFlux participates in Project-level scale/rootNote inheritance — yes, via `_scale` / `_rootNote = -1 = inherit`
- [ ] Bounce temporal-curve LUT choice (ExpDown2x, ExpDown3x, or new `Bounce = 1 − (1−x)³` entry in `model/Curve.h`) — engine-side, deferred to Phase B
- [ ] Default `_resetMeasure` UI step granularity (powers-of-two from NoteSequence) — UI-side, deferred to Phase B/C
- [ ] Pre-build snake permutation lookup (`snakeOrder[16]`) — static const or computed once at engine init — engine-side, deferred to Phase B

## Completed steps

**Design**
- [x] Spec design via grilling — 8 rounds, ~25 product decisions locked
- [x] DiscreteMap stateless-ramp pattern adopted for engine
- [x] NoteTrack pitch chassis adopted (scale-degree integer storage)
- [x] Stochastic borrow pattern for Track-level chassis (§12.1)
- [x] Flux PHAS + MASK transforms borrowed (§6 transforms)
- [x] Stochastic MaskMelody + TiltMelody centrality filter borrowed (§6.2.1)
- [x] Refined curve model (base + vflip + hflip) replacing per-variant enums
- [x] UI surfaces designed and rendered (`ui-preview/pages_phaseflux.py`)
- [x] External 3-reviewer audit run (adversarial / coherence / feasibility) — all clean
- [x] 66-question external audit completed and resolved
- [x] Spec template extracted to `docs/spec-template.md`

**Phase A — math & storage foundations (done)**
- [x] `Version_PhaseFlux_Pending = 36` placeholder constant landed outside the enum chain
- [x] `PhaseFluxSequence` storage class — chassis (`_divisor`, `_clockMultiplier`, `_resetMeasure`, `_scale`, `_rootNote`, `_edited`) + 16-stage array
- [x] `PhaseFluxSequence::Stage` bit-packed record — 3 × `uint32_t`, 24 fields, 93 bits used, 3 spare
- [x] `PhaseFluxTrack` storage class — chassis (`_slideTime`, `_octave`, `_transpose`, `_playMode`, `_fillMode`, `_cvUpdateMode`, `_lock`) + pattern slot array, `static_assert(sizeof(...) <= 9544)`
- [x] All required `Routable<>` wrappers (`_slideTime`, `_octave`, `_transpose`, `_divisor`, `_clockMultiplier`) — day-1 contract per spec §12.3
- [x] Serialization round-trip test — 7 cases (default, every chassis field, every stage field, every numeric min/max edge, bit-pack non-overlap, `_lock` non-persistence, Track-level mode round-trip)
- [x] `TrackMode::PhaseFlux` enum value (serialize ID 8) wired through `Track.h` + `Track.cpp` (6 switch sites each)
- [x] `Track::phaseFluxTrack()` accessor pair, `Container<...>` template parameter, union member, `#include`
- [x] `ClipBoard` pattern copy/paste wired
- [x] `Routing` `writeTarget` PhaseFlux dispatch wired (no new `Routing::Target::` entries)
- [x] `TrackModeListModel` temporary gate so PhaseFlux is unreachable via Track Setup page until engine + UI land
- [x] Spec-compliance + code-quality reviewer subagents passed on both commits
- [x] Sim release build (`make -C build/sim/release sequencer`) clean; regression tests (Stochastic / Accumulator / NoteSequence serialization) green
- [x] Pre-existing failure `TestStochasticDurationDictionary::direct_history_event_is_compact_ui_truth` noted (unrelated to PhaseFlux)

## Notes

**Implementation tier order** (spec §18.1):
1. ~~**Phase A — math & storage foundations**~~ ✅ done. Storage round-trip is the only Phase A test that can land before engine code; cumulative duration table + per-tick derivation + PowerBend correctness all need engine code and live with Phase B/C.
2. **Phase B — first hardware guard (next)**: boot smoke + sub-stage curves audible + transforms audible (phaseShift, masks). Requires `PhaseFluxTrackEngine` + minimal UI (Track page dispatch + EditPage skeleton) so the user can actually select PhaseFlux and hear it.
3. **Phase C — remaining test families**: skip mask, `kMinCycleTicks` floor, idle state, mask × shift, melody mask byte-for-byte, pulse collision clamp, accumulator lifecycle, pitch quantization, curve LUTs at φ=0/0.25/0.5/0.75/1, curve flips, cumulative-recompute side effects, transport behavior.

**Phase B entry checklist** (when resuming):
1. Build `PhaseFluxTrackEngine.h/.cpp` modelled on `DiscreteMapTrackEngine` (stateless-ramp + `_resetTickOffset`).
2. Add `case TrackMode::PhaseFlux:` in `Engine.cpp:527-554` instantiating the new engine.
3. Wire UI: `PhaseFluxTrackListModel`, `PhaseFluxSequenceListModel`, `PhaseFluxEditPage`, `PhaseFluxSequencePage`, register in `Pages.h`, dispatch in `TopPage.cpp` and `TrackPage.cpp`.
4. Add `Project::selectedPhaseFluxSequence()` accessor (UI consumer triggers it).
5. **Remove the `TrackModeListModel` PhaseFlux skip** (search for `// PhaseFlux has no engine/UI yet`).
6. STM32 release build + container-size measurement (RAM gate) before flashing.
7. Boot smoke + sub-stage curves + phaseShift/mask audibility on hardware.

**MVP-done gate list** (spec §18.2): 10 comprehensive gates — engine functional + UI surfaces complete + all test phases pass + STM32 RAM container ceilings respected + boot-to-sound + Pattern A/B/C + Snapshot + Routable-via-global-Routing verified + retro lesson gates passed + adversarial review clean on running firmware.

**Estimated RAM footprint** (§13): `PhaseFluxTrack ≈ 3,422 B` (vs NoteTrack 9,544 B ceiling — ~6,100 B headroom). `PhaseFluxTrackEngine ≈ 200 B` (vs TeletypeTrackEngine 912 B — ~700 B headroom). Adding the variant is **free** under container-gate analysis.
