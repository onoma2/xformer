# Status

**Planning complete, reviewed** — Full implementation plan written and independently reviewed for feasibility. Phased plan (`PHASEDPLAN.md`) is the authoritative reference. Implementation not started.

**Design exploration (2026-07-05, in `GRID-IDIOMS-REFERENCE.md` + `native-scheme-mirror.html`):** settled the surface model — the pads **mirror each engine's own physical control scheme** (not a borrowed/generic interface), one page at a time, since 6 of 7 new engines drive the panel physically (± keypads, masks, sub-page/topic systems) with the encoder + OLED holding value-precision the pads can't. Plus per-engine control-surface *extensions* (the circuit-keyboard analog) with verified feasibility: DiscreteMap transfer-map / Stochastic density pad / PhaseFlux phase-accum / Indexed group-launcher = feasible + new; Fractal + Curve = feasible but low novelty; Tuesday variation deck = partial (morph is new); TT2 programmable grid = hard (no grid device in the VM). Not yet folded into PHASEDPLAN phases — kept as design input to think through.

**Doc map:** see `DOCS-INDEX.md` for recency + current status of every doc in this folder (what's authoritative vs design-input vs stale/historical).

## What was done

- Investigated all Launchpad-related files (12 files, 2 core)
- Mapped all 14 dispatch points in LaunchpadController.cpp that switch on `trackMode()`
- Analyzed all 5 track types for grid-suitability
- Deep-dived into DiscreteMapSequence, IndexedSequence, TuesdaySequence APIs
- Checked model drift since last Launchpad commit (`888f183c`)
- Examined `temp-ref/mebitek-performer/` reference implementation (3240-line .cpp)
- Re-examined mebitek with fresh context — extracted LayerMapItem pattern, Performer mode, circuit keyboard
- Merged vinx-modulove Launchpad improvements — LP Style/Note Style, circuit keyboard, generators mode, undo/redo, performer mode, enhanced button events, visual feedback
- Performed feasibility/priority/risk review — found Indexed macros don't exist in model, several method name errors, file size risk
- Removed MidiCvTrack — no sequence data, arpeggiator only; lowest value for Launchpad grid
- Created corrected implementation plan with 5 track types, reordered phases, deferred items
- Discarded TeletypeTrack — VM text-input integration is 2–3× underscoped; no Launchpad grid value
- Created PHASEDPLAN.md — organized into Essential → Baseline → User Decisions → Extra tiers based on Mini MK1 testing constraint
- Reviewed external grid patterns (GRID-APPROACHES-REVIEW.md) and UI optimizations (UI-OPTIMIZATION-REVIEW.md)
- Folded optimization pre-flight items into PHASEDPLAN E0 (grew from 4 to 13 items)
- Revised plan for the full 11-mode TrackMode enum (2026-07-04): PhaseFlux → Essential E6 (16 model-resident stages, OLED grid precedent, Project accessor exists); Stochastic → D8 and TT2 pattern bank → D9 (owner decisions); Fractal excluded (no per-cell model data — cell content is volatile engine state)
- Re-reviewed adversarially + for feasibility, findings folded: E0.6 buffer-then-blast + E0.11 TX scratch dropped (UsbMidi ring buffer already batches; no bulk-transfer API); DiscreteMap grid layers cut to the 3 per-stage params (GateLength/SlewTime/Pluck are sequence-wide); Performer mode scoped to Note/Curve rows; PhaseFlux GateLength sentinel rows specified; tier arithmetic re-derived; stale D1/D2 dependencies cleared

## Hardware Constraint

**Only Launchpad Mini MK1 available for hardware testing.** This device uses the base `LaunchpadDevice` protocol with red/green/amber colors only (no RGB, no SysEx init). All plan phases prioritize MK1-verifiable work. MK2+/Pro/X-specific features (blue color styles, RGB remapping) are flagged and deferred to Extra tier.

## Plan Summary

See `PHASEDPLAN.md` for full details, acceptance criteria, and dependency graph.

| Tier | Effort | Cumulative | Description |
|------|--------|------------|-------------|
| **Essential** | ~22h | ~22h | Pre-flight optimization (11 items) + NoteTrack fixes + Tuesday + DiscreteMap + Indexed + PhaseFlux + Performer |
| **Baseline** | ~10.5h | ~32.5h | Circuit keyboard, LP styles, visual feedback, button events, long-press, range fills |
| **User Decisions** | ~19.5h | ~52h | Generators, macros, undo, follow, locking, prob overlay, loop mod, Stochastic grid, TT2 pattern grid — each needs explicit approval |
| **Extra** | ~13–15h | ~65–67h | Chaos params, RGB remapping, global config, other controllers |

**Recommended ship target:** Essential + Baseline (~32.5h).
**Absolute MVP:** Essential only (~22h).

## Removed from Plan

- **MidiCvTrack** — no sequence data, arpeggiator only. Lowest value for Launchpad grid.
- **TeletypeTrack** — VM text-input integration; not a grid-editable track type. Verdict carries to TeletypeV2/Mini scripts; their 4×64 tt-pattern banks are the D9 owner decision.
- **FractalTrack** — no per-cell model data; cell content is volatile engine state (`FractalTrackEngine::_trunk`, CCMRAM). Sequence holds only window brackets + branch-pool params.

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

## Key Design Decisions

- **Layer button** stays consistent: LayerMapItem[] arrays for all track types, even those without native Layer enums
- **FirstStep/LastStep** available only on tracks that have those concepts
- **No model changes needed** — except computed `isEdited()` for 4 sequence types (DiscreteMap, Indexed, Tuesday, PhaseFlux; ~1h total)
- **Split `LaunchpadController.cpp`** into per-track files before implementing
- **Indexed macros deferred** — extracted to `indexed-sequence-macro-refactor`, which has since landed (model macros at `IndexedSequence.h:542-552`); Macro Grid v2 (D2) is unblocked, owner-decision-only
- **MidiCvTrack removed** — no sequence data, lowest value for grid
- **TeletypeTrack discarded** — complex VM text-input integration; not a grid-editable track type
- **Vinx/Modulove LP improvements triaged** — style settings, circuit keyboard, performer mode, button events, visual feedback → Baseline tier; generators, undo/redo, track locking → User Decisions tier
- **Optimization pre-flight folded into E0** — LedSemantic, dirty-row mask + compare-on-write, VirtualGrid, Modifier bitmask, DeviceCapabilities (lands with B2), mapColor dedup, std::function→fn ptr. Buffer-then-blast + TX scratch buffer are NOT in the plan (UsbMidi ring buffer already batches; see PHASEDPLAN E0 note)
- **PhaseFlux in Essential** — the one new mode that is grid-ready today (16 model-resident stages, `PhaseFluxEditPage::drawGrid` precedent, `selectedPhaseFluxSequence()` exists)
- **Stochastic + TT2 pattern grids are owner decisions (D8/D9)** — Stochastic steps are generator outputs (grid painting fights regeneration); TT2 int16 cells need a value-entry gesture decision

## Pre-implementation Checklist

From PHASEDPLAN E0 (11 items):

- [ ] E0.1: Add computed `isEdited()` to `DiscreteMapSequence`, `IndexedSequence`, `TuesdaySequence`, `PhaseFluxSequence`
- [ ] E0.2: Split `LaunchpadController.cpp` into per-track files
- [ ] E0.3: Fill `Project.h` accessor gaps (Stochastic/TT2 only if D8/D9 approved)
- [ ] E0.4: Add `selectedLayer` to `_sequence` state struct
- [ ] E0.5: Add `LedSemantic` enum + `semanticColor()` helper
- [ ] E0.7: Add `_dirtyRows` mask + compare-on-write `setLed()` to `LaunchpadDevice`
- [ ] E0.8: Extract `VirtualGrid` pagination helper
- [ ] E0.9: Add `Modifier` bitmask enum + `_modifierMask` byte
- [ ] E0.10: Add `DeviceCapabilities` struct (lands with B2, not pre-flight)
- [ ] E0.12: Hoist `mapColor` table to base class + deduplicate
- [ ] E0.13: Replace `std::function` handlers with function pointers

## Dependencies

| Task | Relationship | Notes |
|------|-------------|-------|
| `stochastic-track-port` | Landed | Stochastic model is in; grid scope is the D8 owner decision (absorbs old X6 rest-probability) |
| `indexed-sequence-macro-refactor` | Done — dependency cleared | Indexed model macros exist (`IndexedSequence.h:542-552`); D2 is owner-decision-only |
| `generator-preview-apply` | Done — A/B foundation landed | D1 Generators is owner-decision-only |
| `performer-improvements` | **Blocks** D3 Undo (general undo stack) | External dependency |
| `teletype-file-reliability` | Independent | No overlap |
| `vinx-modulove-improvements` | LP items merged here | Non-LP items remain separate |

## Next Action

Begin E0 pre-flight tasks. Start with E0.2 (file split), then E0.1 / E0.3 / E0.4 (model accessors). Remaining E0 items can proceed in any order; E0.10 waits for B2.
