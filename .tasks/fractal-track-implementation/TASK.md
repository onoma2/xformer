# FractalTrack Implementation — Task Status

Status: Paused
Priority: Low

## Overview

A new TrackMode inspired by Hallucigenia (VCV Rack) and Bloom v2. Reads from a parent NoteTrack (or other track types), applies smart mutation rules with evolutionary memory — mutations get "smarter" over time via history-based bias.

## Key Design

- **Parent-child inheritance**: FractalTrack links to any NoteTrack (or Curve/Indexed/Tuesday/DiscreteMap) and reads its step data as the base for mutation
- **Memory-based mutation**: `MutationHistory` buffer (16 records) + `SelectionPressure` tracks which mutations succeeded and biases future ones toward winning patterns
- **Rules constraints**: Scale-aware, leap-limited, position-aware, neighbor-aware. Extended rules: Markov chains, L-System, cellular automata, Fibonacci intervals, gravity wells, Perlin noise, echo memory, symmetry, tension/resolution
- **Bloom v2 features**: Ratchet, slew, trill per-step, 8-slot branch system
- **Full design doc**: `doc/fractal-track-research.md` (~12K words)

## Architecture

| Layer | New Files | Lines |
|-------|-----------|-------|
| Model | `model/FractalTrack.h`, `model/FractalSequence.h` | ~200 |
| Engine | `engine/FractalTrackEngine.h/.cpp` | ~400 |
| UI | `ui/pages/FractalTrackPage.h`, `ui/model/FractalTrackListModel.h` | ~350 |
| Integration | Modify `Track.h`, `Routing.h`, `Engine.cpp`, `Pages.h`, `TopPage.cpp`, `TrackPage.cpp` | ~80 |
| **Total** | | **~1030 new, ~80 modified** |

## Implementation Order

1. **Model**: `FractalSequence.h` (per-pattern params) + `FractalTrack.h` (track container)
2. **Track integration**: `Track.h` union/mode, `Routing.h` targets, `Engine.cpp` creation
3. **Engine**: `FractalTrackEngine.h/.cpp` — mutation logic, history, rules
4. **UI**: `FractalTrackListModel.h` (track setup) + `FractalTrackPage.h` (section editor)
5. **Page registration**: `Pages.h`, `TopPage.cpp`, `TrackPage.cpp`
6. **Parent source interface**: `FractalSourceInterface` on all parent-capable track engines
7. **Extended rules + Bloom features**: Markov, L-System, CA, ratchet, trill, slew, branches
