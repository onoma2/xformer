# Launchpad Track Porting

## Task Summary

Extend Launchpad controller support to all grid-editable track types in PER|FORMER/XFORMER sequencer.

**Authoritative plan:** `PHASEDPLAN.md`
**Status:** Planning complete. Implementation not started.

## Hardware Constraint

**Only Launchpad Mini MK1 available for hardware testing.** All implementation prioritizes MK1 compatibility. Mini MK1 uses base `LaunchpadDevice` protocol (red/green/amber only, no RGB, no SysEx init). MK2+/Pro/X features deferred to Extra tier.

## Track Support Status

- ✅ **NoteTrack**: Fully supported with complete layer-based editing
- ✅ **CurveTrack**: Fully supported with complete layer-based editing
- ❌ **IndexedTrack**: Planned — Essential E4 (48-step grid editor)
- ❌ **DiscreteMapTrack**: Planned — Essential E3 (32-stage editor)
- ❌ **TuesdayTrack**: Planned — Essential E2 (step-key grid)
- ❌ **StochasticTrack**: Under development on `feat/stochastic` — will be available before launchpad work starts
- ❌ **MidiCvTrack**: Removed — no sequence data, arpeggiator only
- ❌ **TeletypeTrack**: Discarded — VM text-input, not grid-editable

## Tier Overview

| Tier | Effort | Cumulative | What |
|------|--------|------------|------|
| **Essential** | ~18h | ~18h | Pre-flight optimization (13 items) + NoteTrack fixes + Tuesday + DiscreteMap + Indexed + Performer |
| **Baseline** | ~10.5h | ~28.5h | Circuit keyboard, LP styles, visual feedback, button events, long-press, range fills |
| **User Decisions** | ~10.5h | ~39h | Generators, macros, undo, follow, locking, prob overlay, loop mod |
| **Extra** | ~13h | ~52h | Chaos params, RGB remapping, global config, other controllers |

**Recommended ship target:** Essential + Baseline (~28.5h).
**Absolute MVP:** Essential only (~18h).

## Essential Phases (Ship First)

| Phase | Track | Effort | Description | MK1 Testable |
|-------|-------|--------|-------------|--------------|
| E0 | Pre-flight | ~4.5h | 13 infrastructure items (file split, LedSemantic, syncLeds opt, VirtualGrid, etc.) | N/A |
| E1 | Note | ~1.5h | Fix 6 missing layers, step highlighting, Mode selector, Ikra bounds | ✅ |
| E2 | Tuesday | ~2.5h | 2×8 step-key grid — HW parity + parameter editor | ✅ |
| E3 | DiscreteMap | ~3h | 32-stage editor: 6 param views with VirtualGrid pagination | ✅ |
| E4 | Indexed | ~3.5h | 48-step grid editor (Pitch/Duration/GateSlide/Route). No macros. | ✅ |
| E5 | Performer | ~3h | SequenceLength + Overview layers, scene mute/solo/fill | ✅ |

## Baseline Phases (Mebitek Parity)

| Phase | Effort | Description | Source |
|-------|--------|-------------|--------|
| B1 | ~2h | Circuit keyboard + LP Note Style setting | Vinx |
| B2 | ~1h | LP Style setting (Classic/Blue; Classic default for MK1) | Vinx |
| B3 | ~2h | Visual feedback (octave lines, pattern viz, mute status, requested-pattern dim) | Modulove |
| B4 | ~1.5h | Enhanced button events (double-tap wiring, improved state tracking) | Modulove |
| B5 | ~1h | Interaction improvements (fill, range editing, run mode) | Modulove |
| B6 | ~1.5h | Long-press detection (timer-based, new gesture type) | Grid review |
| B7 | ~1.5h | Range fills + isEdited cache + visual polish | Grid review |

## Current Launchpad Architecture

### Core Files

- `/src/apps/sequencer/ui/controllers/launchpad/LaunchpadController.h/cpp`: Main controller
- `/src/apps/sequencer/ui/controllers/launchpad/LaunchpadDevice.h/cpp`: Base device class
- Model-specific implementations: Mk2, Mk3, Pro, Pro Mk3

### Key Features

- **Sequence Mode**: Layer selection, step range, run mode, sequence editing
- **Pattern Mode**: Pattern selection, latching, syncing, track mute
- **Performer Mode**: Stubbed (empty bodies with `// TODO`)

### Architecture Patterns

- Layer mapping arrays to associate track layers with grid coordinates
- Range mapping for value conversion
- Mode dispatch macros (`CALL_MODE_FUNCTION`)
- Template methods for type-safe operations
- Device abstraction for multiple Launchpad models

## Files to Modify

- `/src/apps/sequencer/ui/controllers/launchpad/LaunchpadController.cpp` (split into per-track files per E0.2)
- `/src/apps/sequencer/ui/controllers/launchpad/LaunchpadController.h`
- `/src/apps/sequencer/ui/controllers/launchpad/LaunchpadDevice.h/cpp` (E0.6, E0.7, E0.11, E0.12, E0.13)
- Model files for `isEdited()` additions (E0.1)

## Dependencies

| Task | Relationship | Notes |
|------|-------------|-------|
| `feat/stochastic` | Stochastic track will be ready before launchpad work | X6 rest-probability editing may become viable |
| `indexed-sequence-macro-refactor` | Blocks D2 Macro Grid for IndexedTrack | ~4.5h; can parallel E0–E5 |
| `performer-improvements` | Blocks D1 Generators, D3 Undo | External dependency |

## Status

Planning complete. `PHASEDPLAN.md` is authoritative. Ready to begin E0 pre-flight.

## Next Action

Begin E0 pre-flight tasks. Start with E0.11 (TX scratch buffer), then E0.2 (file split), then E0.1/E0.3/E0.4 (model accessors).
