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

**Files to create**
- `src/apps/sequencer/model/PhaseFluxTrack.h/.cpp`
- `src/apps/sequencer/model/PhaseFluxSequence.h/.cpp` (with bit-packed Stage record)
- `src/apps/sequencer/engine/PhaseFluxTrackEngine.h/.cpp`
- `src/apps/sequencer/ui/pages/PhaseFluxEditPage.h/.cpp`
- `src/apps/sequencer/ui/pages/PhaseFluxSequencePage.h/.cpp`
- `src/apps/sequencer/ui/model/PhaseFluxSequenceListModel.h`
- `src/apps/sequencer/ui/model/PhaseFluxTrackListModel.h`

**Files to modify**
- `src/apps/sequencer/model/Track.h:41` — add `TrackMode::PhaseFlux` enum value
- `src/apps/sequencer/model/ProjectVersion.h:111` — add `Version_PhaseFlux_Pending = 36` placeholder constant
- `src/apps/sequencer/ui/pages/TopPage.cpp:97, 296-348, 354-380` — add PhaseFlux dispatch cases
- `src/apps/sequencer/ui/pages/TrackPage.cpp:152` — add PhaseFlux case to `setTrack()`
- `src/apps/sequencer/ui/pages/Pages.h:63` — register `phaseFluxEdit` + `phaseFluxSequence` page members

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

## Open questions

All 66 audit questions resolved. No open product decisions before implementation.

Implementer-resolved items (engineering punch list, §16):
- [ ] Exact byte-pack offsets within the 3 × uint32_t stage record (use `model/Bitfield.h` BitField template)
- [ ] Bounce temporal-curve LUT choice (ExpDown2x, ExpDown3x, or new `Bounce = 1 − (1−x)³` entry in `model/Curve.h`)
- [ ] Default `_resetMeasure` UI step granularity (powers-of-two from NoteSequence)
- [ ] Whether PhaseFlux participates in Project-level scale/rootNote inheritance
- [ ] Pre-build snake permutation lookup (`snakeOrder[16]`) — static const or computed once at engine init

## Completed steps

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

## Notes

**Implementation tier order** (spec §18.1):
1. **Phase A — math & storage foundations** before any engine beyond skeletons: serialization round-trip + cumulative duration table + per-tick derivation + PowerBend correctness
2. **Phase B — first hardware guard**: boot smoke + sub-stage curves audible + transforms audible (phaseShift, masks)
3. **Phase C — remaining test families**: skip mask, kMinCycleTicks floor, idle state, mask × shift, melody mask byte-for-byte, pulse collision clamp, accumulator lifecycle, pitch quantization, curve LUTs at φ=0/0.25/0.5/0.75/1, curve flips, cumulative-recompute side effects, transport behavior

**MVP-done gate list** (spec §18.2): 10 comprehensive gates — engine functional + UI surfaces complete + all test phases pass + STM32 RAM container ceilings respected + boot-to-sound + Pattern A/B/C + Snapshot + Routable-via-global-Routing verified + retro lesson gates passed + adversarial review clean on running firmware.

**Estimated RAM footprint** (§13): `PhaseFluxTrack ≈ 3,422 B` (vs NoteTrack 9,544 B ceiling — ~6,100 B headroom). `PhaseFluxTrackEngine ≈ 200 B` (vs TeletypeTrackEngine 912 B — ~700 B headroom). Adding the variant is **free** under container-gate analysis.
