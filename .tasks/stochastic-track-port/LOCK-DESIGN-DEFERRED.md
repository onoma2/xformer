# Stochastic Lock — Design Deferred

_Drafted 2026-05-22. Status: engine implementation removed; redesign deferred until fractal-track work lands and a substrate decision is possible._

## Why this doc exists

The previous Lock implementation was removed in the 2026-05-22 cleanup commit. It crashed the firmware whenever two or more stochastic tracks were active simultaneously. Rather than ship a half-fix that survives 2 tracks but breaks at 4, the engine path was deleted entirely. The user-facing surface (the `_lock` flag on `StochasticTrack`, the Config-page toggle) was kept so projects don't lose state and so the future re-implementation can wire back in without UX churn.

This doc captures (a) why the old implementation was wrong, (b) the design we converged on before deferring, and (c) the open question about outsourcing the whole feature to the fractal-track trunk.

## What was removed

- Heap allocation: `new (std::nothrow) LockedParentEvent[CONFIG_STEP_COUNT]` per `StochasticTrackEngine` constructor. Each instance allocated 5,888 B from the heap.
- Per-event capture in `triggerStep` (the `captureLockedParent` helper).
- Lock-active read path in `triggerStep` (the `if (locked)` branch that replayed cached evaluated events).
- `LockedParentEvent`/`LockedChild` packed structs.
- `lockAvailable()` accessor.
- Destructor + `initLockedSteps`/`freeLockedSteps` helpers.

## Why it broke

The STM32F405RGT has 128 KB main SRAM. After `.data + .bss` (currently ~119 KB), only ~12 KB is free for libc heap and the call stack combined. The Cortex-M `_sbrk` on this build does not check against the stack pointer — it just walks `end` upward. `new` can therefore "succeed" while handing back memory that overlaps the live stack.

Per-track lock cache footprint: 64 `LockedParentEvent` slots × 92 B = **5,888 B per track**. Single track works (5,940 B left for stack). Two tracks consume 11,776 B → 52 B left for stack → hardfault as soon as anything deep enough runs. Worse, no `nullptr` from `new` because `_sbrk` didn't refuse — the corruption was silent.

See PROJECT.md "C++ Heap" section for the full picture.

## Reductions we considered and rejected

Listed shortest-to-longest:

- **Pack the existing struct (92 → 46 B).** Doubles the breakage threshold from 2 tracks to 4. Not a fix.
- **Move to a file-scope BSS pool sized for all 8 tracks × 64 steps.** Would consume ~47 KB BSS unpacked or ~24 KB packed. Neither fits in either main RAM (12 KB free) or CCMRAM (10 KB free). Would need to evict other state.
- **Cap stochastic-track count at 2 with a UX gate.** Crude. Doesn't scale.
- **Cap stochastic-track count at 1 + pack.** Same product cost, smaller margin.

None of these change the fundamental architecture: the old design captured the full evaluated output per step (parent CV, duration, every child's CV/offset/gate/slide flags). That's a lot of data per step, and there are up to 64 steps × 8 tracks. Even with aggressive packing it doesn't fit.

## The design we converged on (before deferring)

A flat tick-event tape. Engine becomes a monkey replay machine while locked.

```cpp
struct LockedTick {
    uint16_t relTick;   // 0..capturedCycleTicks
    uint8_t  kind;      // 0 = cv, 1 = gate
    uint8_t  flags;     // gate: bit0 state. cv: bit0 slide.
    int16_t  value;     // cv only
};
// 6 B per event
```

- **Per-track tape**, 128 entries × 6 B = 768 B per track. File-scope `gLockTape[CONFIG_TRACK_COUNT][128]` in `.ccmram_bss` = 6,144 B total for 8 tracks.
- **Track-owned**, not sequence-owned. One tape per track at a time. Switching patterns under Lock invalidates the tape and triggers an auto-rearm capture on the next cycle of the new pattern.
- **Lock flag on `StochasticTrack`**, 1 bit, serialized via `write()`/`read()`. The tape itself is runtime-only — erased on lock-off, recaptured on lock-on or pattern switch.

**State machine (engine):** `Off → ArmPending → Capturing → Locked → Off`.

- Press Lock → state = `ArmPending`. Engine waits for next `_patternIndex == first()`.
- On `_patternIndex == first()` → state = `Capturing`. Engine sets `captureStartTick = currentTick`, `tapeWriteIdx = 0`. Existing `_cvQueue.push` / `_gateQueue.push` sites in `triggerStep` and `evaluateChildren` ALSO append to the tape with `relTick = absTick - captureStartTick`.
- When `_patternIndex` wraps back to `first()` (latch-aware to handle the user pressing Lock at `first` itself) → state = `Locked`. Snapshot `capturedCycleTicks`.
- While `Locked`: `triggerStep` is bypassed. Each tick, the engine pumps tape entries onto `_cvQueue` / `_gateQueue` with absolute ticks based on `lockStartTick + (cycle * capturedCycleTicks) + entry.relTick`. Existing `tick()` drain logic is unchanged.
- Tape full mid-capture (`tapeWriteIdx == 128`) → recording stops; the rest of the capture cycle plays normally but isn't recorded. Locked replay plays the recorded prefix and goes silent for the rest of the cycle.
- Toggle Lock off → `tapeWriteIdx = tapeReadIdx = 0`, state = `Off`.

**Reset Measure is opaque to Lock.** During Capturing, a reset that snaps `_patternIndex → first()` counts as the wrap (latch-aware) — the captured cycle may be truncated. During Locked, Reset Measure has no audio effect; replay is driven by `capturedCycleTicks`, not by source-tape cycles.

**Persistence:** project saves the 1-bit flag. On load, flag=true → engine auto-arms; one-cycle recapture delay before audio is locked again.

**Documented limits:**
- "Lock captures up to ~1–2 bars at 1/16 with moderate burst use."
- "Patterns with more queue events than 128 per cycle truncate; the truncated portion plays as silence under Lock."
- "Lock is per-track. Switching patterns auto-recaptures on the next cycle of the new pattern."

## Open question: outsource to fractal

The fractal-track design (see `.tasks/fractal-track-implementation/DICTIONARY.md`) has a "trunk" — an inline `uint16_t[]` buffer per step (11-bit CV + 4-bit gate length + 1 flag). Lock semantics in fractal: trunk content is captured during recording, and Lock blocks further trunk writes. Branches and ornamentation continue while locked.

This is the same primitive stochastic Lock needs. Two ways to share it:

1. **Generic "trunk + lock" layer that any track type can include.** Refactor the fractal trunk into a reusable component. Stochastic gets a trunk of its own. Lock semantics become a track-type-agnostic feature.
2. **Cross-track routing: dedicate a fractal track to capture another track's output.** Stochastic outputs go through fractal's record/playback path. No new code on the stochastic side; cost is one "trunk track" per "locked stochastic track" — a real product price but no architecture duplication.

Option (1) is cleaner but requires the fractal trunk to be implemented and stable first. Option (2) is more conservative but eats track slots.

**Decision deferred until fractal track is implemented.** Once we have a working trunk on hardware, we can evaluate whether (a) we copy the pattern into stochastic, (b) we generalize the trunk component, or (c) we wire stochastic through a fractal recording layer.

## What stays in the codebase right now

- `StochasticTrack::_lock` field exists. `lock()` / `setLock()` / `editLock()` / `printLock()` exist. The Config page Lock toggle still appears and is settable.
- `clear()` initializes `_lock = false`.
- `_lock` is NOT serialized (matches review #6 status — decision held until the new design lands).
- Engine **does not read** `_lock`. Toggling it has no audio effect today.
- A short comment block sits above `triggerStep` in `StochasticTrackEngine.cpp` pointing here.

When the redesign comes back, the flag is already wired and persistable; the engine just needs to start honoring it.

## Cleanup history

- 2026-05-22 — removed heap-allocated `_lockedParents` cache, the `if (locked)` replay branch in `triggerStep`, `captureLockedParent`, `lockAvailable`, init/free helpers, destructor. Sim + STM32 release builds clean. `.text` shrunk by ~2.5 KB. Four stochastic test suites pass. PROJECT.md heap section updated.
