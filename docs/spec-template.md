# Spec Template

> Copy this file when starting a new track-type or major-feature spec. Replace bracketed prompts. Delete sections that don't apply.

## How to write a spec — lessons from PhaseFlux

**Process**

1. **Research codebase first.** Before any design discussion, grep existing tracks for the pattern. NoteTrack / CurveTrack / DiscreteMap / Stochastic / Tuesday almost always have a precedent. Cite `file:line` of the borrowed pattern in the spec — "this matches `DiscreteMapTrackEngine.cpp:128-198`." Surfacing precedent late wastes grilling rounds.
2. **Grill in 2–3 question batches.** Foundational decisions first (host class, identity model, grid shape). Resolve dependencies before drilling into derived choices. Use `/grillme`.
3. **Pick a position when contradictions surface.** Don't leave both options in the spec. If two events have different semantics, document the asymmetry and *why* — not "TBD".
4. **Headline contract violations.** When code or spec breaks a stated rule, lead with the fix, not a footnote.
5. **Major shifts are normal mid-grilling.** PhaseFlux pivoted from raw-voltage pitch → scale-degree, from stateful cursor → stateless ramp, from enum-per-curve → base + flips. Don't over-lock too early.
6. **External review at the end.** Spawn adversarial / coherence / feasibility reviewers (compound-engineering `ce-*-reviewer` agents) on the locked spec. A 66-question audit caught math errors and missing edge cases that the grilling missed.

**Architecture**

7. **Identity model first.** "Stored output vs recipe replay" drives storage, engine, reset semantics. Lock before per-field design.
8. **State ownership table.** Per field: Track / Sequence / Engine / UI. Lock before code (retro lesson #1).
9. **DiscreteMap stateless-ramp pattern** for any phase/cursor/ramp derivation: `_resetTickOffset = tick` + integer `relativeTick` math. No accumulated cursor state.
10. **NoteTrack pitch chassis** for any pitched track: scale-degree integer storage → `Scale::noteToVolts()` at output. Octave + transpose applied as `octave × notesPerOctave + transpose`.
11. **Stochastic borrow pattern** for track-level chassis: copy `_slideTime / _octave / _transpose / _playMode / _fillMode / _cvUpdateMode` verbatim with Routable wrappers; drop probability biases if no probability fields exist.

**Storage**

12. **Bit-pack per-cell records** with `BitField<T, Index, Bits>` + `UnsignedValue<N>` / `SignedValue<N>` from `model/Bitfield.h`. Unions with `raw` field. `raw == raw` equality. Version-gated migration.
13. **Bit budget tracked in comments.** Recount when fields change. Spare bits explicit.
14. **Container ceilings** (PROJECT.md §244-272). Model and Engine variants live in `Track::_container` / `Engine::TrackEngineContainer` arrays of 8, sized to largest variant. Adding ≤ ceiling = free. Exceeding = `8 × overshoot`.
15. **No heap, ever.** Every struct in `.bss`. `new (std::nothrow)` is not safety — `_sbrk` can overlap stack and hardfault.

**Embedded math**

16. **No RNG in MVP** unless explicitly justified. RNG forces seed state + reproducibility tests.
17. **Encoding-side clamp** for math degeneracies. `SignedValue<7>` maps via `/64` (not `/63`) so max-knob is `±0.984`, never hitting PowerBend `p = ±1` discontinuity.
18. **LUT + `pow()` only** in tick path. No `std::vector` / heap / `std::mt19937`.

**UI**

19. **Three surfaces, NoteTrack-modeled.** EditPage (BasePage) + SequencePage (ListPage backed by `*SequenceListModel`) + `*TrackListModel` for the shared TrackPage. Wire through three switches in TopPage (`setSequenceEditPage`, `setSequenceView`) and TrackPage (`setTrack`).
20. **Render proposals with `ui-preview/`.** ASCII sketches lie about pixel widths. Run the python tool, read the PNG, then ask for review (CLAUDE.md rule).
21. **Tag concepts.** `(code)`, `(ui)`, `(idea)` — every named term, every reply (CLAUDE.md rule).

---

## Spec body — fill in below

## 1. Class shape
[New TrackMode trio? Extension? Cite host classes. State why this shape, not the alternatives.]

## 2. Grid / traversal
[Cell count, cursor model, snake/linear/random, skip behavior.]

## 3. Timing model
[Cumulative duration table. Per-tick derivation. Reset Measure pattern. Knob-turn recompute side effects (clear gateTimer + pulseFired). Idle state (degenerate cycle).]

## 4. Identity / storage model
[Stored vs derived per field. Engine transient state list.]

## 5. Per-cell / per-step record
[Field table with widths + types. Bit budget sum. Defaults table.]

## 6. Curve math (if applicable)
[PowerBend formulas. Base shape + flips. Per-shape LUT mapping (cite `model/Curve.cpp` entries). Pulse-collision clamp rule (cite Stochastic `evaluateBurst`).]

## 7. Accumulator (if applicable)
[Counter math. Lifecycle-across-reset-events table. Per-stage vs sequence-global decision.]

## 8. Transforms (if applicable)
[List. Per-stage vs per-sequence scope. Order of application against masks.]

## 9. Outputs
[Jack count. Multi-jack mirroring rule. Quantization model.]

## 10. Modulation inputs
[Routing-via-global-Routing vs per-track subsystem. Defer policy.]

## 11. Performer integration
[Mute/Solo/Pattern/Snapshot/Fill participation table. Transport behavior (Stop / Start / Continue, NoteTrack two-tier).]

## 12. Performer alignment — Track / Sequence borrowed fields
[Stochastic borrow pattern: NoteTrack subset on Track; NoteSequence subset on Sequence. Routable-from-day-1 contract.]

## 13. Retro lesson gates
[10-row table mapping each `retro/stochastic-retro.md` lesson to how MVP satisfies it.]

## 14. Spec open questions — resolutions
[Q1..Qn table. Cite the grilling decision per row.]

## 15. Deferred to v2
[Explicit list. Reappearance requires written impact assessment.]

## 16. Engineering punch list
[Items not gating design — implementer-resolves. Bit layout, RAM measurement command. Do NOT include a serialization version policy — dev branch does not bump `ProjectVersion`; see `src/apps/sequencer/model/ProjectVersion.h` header comment.]

## 17. UI surfaces — modelled on NoteTrack
[Three files: EditPage, SequencePage, TrackListModel. Field tables. Wiring path with line refs to TopPage / TrackPage switches.]

## 18. Testing & MVP acceptance
[Phase A (math + storage foundations) / Phase B (first hw guard: boot + smoke + audible signal) / Phase C (remaining test families). MVP-done comprehensive gate list.]
