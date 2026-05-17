# Status

**Planning complete, reviewed** — Full implementation plan written and independently reviewed for feasibility. Phased plan (`PHASEDPLAN.md`) is the authoritative reference. Implementation not started.

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

## Hardware Constraint

**Only Launchpad Mini MK1 available for hardware testing.** This device uses the base `LaunchpadDevice` protocol with red/green/amber colors only (no RGB, no SysEx init). All plan phases prioritize MK1-verifiable work. MK2+/Pro/X-specific features (blue color styles, RGB remapping) are flagged and deferred to Extra tier.

## Plan Summary

See `PHASEDPLAN.md` for full details, acceptance criteria, and dependency graph.

| Tier | Effort | Cumulative | Description |
|------|--------|------------|-------------|
| **Essential** | ~18h | ~18h | Pre-flight optimization (13 items) + NoteTrack fixes + Tuesday + DiscreteMap + Indexed + Performer |
| **Baseline** | ~10.5h | ~28.5h | Circuit keyboard, LP styles, visual feedback, button events, long-press, range fills |
| **User Decisions** | ~10.5h | ~39h | Generators, macros, undo, follow, locking, prob overlay, loop mod — each needs explicit approval |
| **Extra** | ~13h | ~52h | Chaos params, RGB remapping, global config, other controllers |

**Recommended ship target:** Essential + Baseline (~28.5h).
**Absolute MVP:** Essential only (~18h).

## Removed from Plan

- **MidiCvTrack** — no sequence data, arpeggiator only. Lowest value for Launchpad grid.
- **TeletypeTrack** — VM text-input integration; not a grid-editable track type.

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
- **No model changes needed** — except `isEdited()` for 3 sequence types (~45 min total)
- **Split `LaunchpadController.cpp`** into per-track files before implementing
- **Indexed macros deferred** — extracted to separate task `indexed-sequence-macro-refactor` (~4.5h). Must complete before Macro Grid v2
- **MidiCvTrack removed** — no sequence data, lowest value for grid
- **TeletypeTrack discarded** — complex VM text-input integration; not a grid-editable track type
- **Vinx/Modulove LP improvements triaged** — style settings, circuit keyboard, performer mode, button events, visual feedback → Baseline tier; generators, undo/redo, track locking → User Decisions tier
- **Optimization pre-flight folded into E0** — LedSemantic, buffer-then-blast syncLeds, dirty-row mask, VirtualGrid, Modifier bitmask, DeviceCapabilities, TX scratch buffer, mapColor dedup, std::function→fn ptr

## Pre-implementation Checklist

From PHASEDPLAN E0 (13 items):

- [ ] E0.1: Add `isEdited()` to `DiscreteMapSequence`, `IndexedSequence`, `TuesdaySequence`
- [ ] E0.2: Split `LaunchpadController.cpp` into per-track files
- [ ] E0.3: Verify all `Project.h` accessors compile
- [ ] E0.4: Add `selectedLayer` to `_sequence` state struct
- [ ] E0.5: Add `LedSemantic` enum + `semanticColor()` helper
- [ ] E0.6: Refactor `syncLeds()` to buffer-then-blast MIDI (depends on E0.11)
- [ ] E0.7: Add `_dirtyRows` row-dirty mask to `LaunchpadDevice`
- [ ] E0.8: Extract `VirtualGrid` pagination helper
- [ ] E0.9: Add `Modifier` bitmask enum + `_modifierMask` byte
- [ ] E0.10: Add `DeviceCapabilities` struct to device classes
- [ ] E0.11: Add shared TX scratch buffer to `LaunchpadDevice`
- [ ] E0.12: Hoist `mapColor` table to base class + deduplicate
- [ ] E0.13: Replace `std::function` handlers with function pointers

## Dependencies

| Task | Relationship | Notes |
|------|-------------|-------|
| `feat/stochastic` | **Will be ready** before launchpad work starts | Stochastic track in development; X6 rest-probability editing may become viable |
| `indexed-sequence-macro-refactor` | **Blocks** D2 Macro Grid for IndexedTrack | ~4.5h separate task; can run in parallel with E0–E5 |
| `performer-improvements` | **Blocks** D1 Generators (A/B state machine), D3 Undo (undo stack) | External dependency |
| `teletype-file-reliability` | Independent | No overlap |
| `vinx-modulove-improvements` | LP items merged here | Non-LP items remain separate |

## Next Action

Begin E0 pre-flight tasks. Start with E0.11 (TX scratch buffer), then E0.2 (file split), then E0.1 / E0.3 / E0.4 (model accessors). Remaining E0.5–E0.13 can proceed in any order after E0.11 is done.
