# Stochastic Lock — Design Deferred

_Drafted 2026-05-22. **Superseded by `PHASE14-TRUNK-MODEL.md` (also drafted 2026-05-22).** Under the trunk model, Lock collapses to a single bit on the sequence (or track) that refuses generator writes — no tape, no engine-state snapshot, no fractal substrate needed. The trunk IS the locked content. The flat-tape design and engine-flag (RNG-snapshot) alternative described below remain valid contingency designs in case the trunk model gets revised, but neither needs to be implemented if Phase 14 lands._

_Original status: engine implementation removed; redesign deferred until fractal-track work lands and a substrate decision is possible._

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

## Alternative: engine-flag Lock (RNG-state freeze) — likely simpler than the tape

After the tape design was drafted, a simpler option emerged: don't capture output at all, just freeze the engine state that drives the random rolls. Restore the snapshot at every cycle wrap. The engine re-evaluates each cycle from the same starting state → bit-identical audio without any tape.

**The codebase already proves this works.** `TuesdayTrackEngine::tick` lines 1870–1883:

```cpp
int loopLength = sequence.actualLoopLength();
uint32_t resetDivisor = (loopLength > 0) ? (loopLength * divisor) : 0;
uint32_t relativeTick = (resetDivisor == 0) ? tick : tick % resetDivisor;
if (resetDivisor > 0 && relativeTick == 0) {
    initAlgorithm();    // re-seeds _rng and _extraRng to loop-start state
    _stepIndex = ...;
}
```

Tuesday's entire loop mechanism is "re-seed the RNG on cycle wrap, advance steps deterministically from there." `generateStep()` is stateless w.r.t. the loop — purely a function of `_stepIndex` + current RNG state + sequence params. No `events()[]` buffer, no tape. The loop is bit-identical because the RNG is reset to a known state each cycle, and all 15 Tuesday algorithms (Test, Tritrance, Stomper, Aphex, Autechre, Stepwave, Markov, ChipArp1, ChipArp2, Wobble, Scalewalker, Window, Minimal, Blake, Ganz) read this contract.

**Same pattern applied to stochastic Lock:**

- On `ArmPending → Capturing` (at next `first()` visit), snapshot the engine state that drives randomness:
  ```cpp
  _snapshotRng           = _rng;
  _snapshotJumpRegister  = _jumpRegister;
  _snapshotLastDegree    = _lastDegree;
  _snapshotLastEvent     = _lastEvent;
  _snapshotLastEventValid = _lastEventValid;
  ```
- Cycle N plays from this state. Patience/Mutate/Live-writeback to `events()[]` are blocked while `Locked`.
- On every subsequent cycle wrap (`_patternIndex == first()`):
  ```cpp
  _rng           = _snapshotRng;
  _jumpRegister  = _snapshotJumpRegister;
  _lastDegree    = _snapshotLastDegree;
  _lastEvent     = _snapshotLastEvent;
  _lastEventValid = _snapshotLastEventValid;
  ```
- `triggerStep` runs normally inside the cycle. Within-cycle randomness (gate length, burst children, Repeat roll, Live regen) reads `_rng` — same state every cycle, so same rolls, same audio.
- Lock toggle off: just stop restoring on wrap. Natural evolution resumes from wherever the engine state happened to be at that moment.

**Per-track footprint (engine struct, no heap, no CCMRAM pool):**
- `Random _snapshotRng` — 4 B
- `int _snapshotJumpRegister` — 4 B
- `int _snapshotLastDegree` — 4 B
- `StochasticSourceEvent _snapshotLastEvent` — 6 B
- `bool _snapshotLastEventValid` — 1 B (padded)
- ≈ **20 B per track in the engine struct.** 8 tracks → 160 B. Fits under the existing 512 B engine size assert with comfortable headroom.

**No tape capacity limit.** Works on any pattern size — 16 steps, 64 steps, all-burst, doesn't matter. The lock doesn't store output, it stores the input conditions that produce the same output.

**Differences vs Tuesday:**

- Tuesday re-seeds from sequence params each cycle (`initAlgorithm()` derives the seed from `flow` and `ornament`). Always-deterministic by design — there's no "unlock" because there's no captured state to evolve from.
- Stochastic Lock would snapshot `_rng` at the moment of arm (not re-derive from params), then restore that snapshot at every cycle wrap until the user releases Lock. Releasing Lock just stops the restore — the engine continues with whatever state was there at that moment and natural evolution (patience, mutate, jump consuming `_rng` freely) resumes.

**Caveat that comes with the territory:** if the user turns a knob while Lock is on, the audio changes — the RNG state is frozen but the parameter inputs differ. That's arguably a feature ("perform on top of the locked pattern" — Repeat, Range, Variation knobs become mod controls over the frozen loop). For strict "Lock means everything is frozen" semantics you'd also snapshot model values, but that's an extension, not the base design.

**Engine determinism prerequisite (already true today):**
- `_rng` is a single `uint32_t` state. Deterministic LCG.
- All per-step rolls (`evaluateChildren`, `pickGateLength`, Repeat probability, Live-mode regen) consume `_rng.nextRange(N)` — integer math.
- CV pipeline uses `scale.noteToVolts(note)` — fixed-point lookup, deterministic on STM32.
- No floats from the audio API, no clock jitter affecting the audio path, no threading races in `triggerStep` (engine task only).

**Why this is probably the answer:**

- Lighter than the tape (20 B/track vs 768 B/track CCMRAM).
- No capacity cliff (no 128-event cap).
- Pattern of "snapshot + restore on cycle wrap" already shipping in Tuesday — same engine, same task, same audio path. Validated architecture.
- Sequence-owned 1-bit flag persists across save/load. Auto-arms on project load (1-cycle recapture delay).
- Park "tape" design as a fallback if engine determinism turns out to have hidden non-determinism we missed.

This is now the preferred implementation when the Lock work resumes. The tape design above is kept as a fallback.

## A third option: A/B preview paradigm borrowed from GeneratorPage

_Added 2026-05-24 during open design discussion._

`SequenceBuilderImpl<T>` (used by NoteSequence's GeneratorPage for Algo / Euclidean
preview) holds three states for a layer: `_original` (deep copy taken at entry),
`_edit` (reference to the live model), and `_preview` (allocated buffer with new
content). The "togglePreview" button swaps which copy lives at the `_edit`
reference. Engine plays whichever copy is live; UI signals via header which
state is on display.

The mechanic adapts cleanly to `StochasticSequence` despite it not being layered:
the whole sequence struct (~560 B, all knobs + events + window) is a single
snapshot unit. No layer-specific builder needed.

### Lightweight A/B (musical performance tool, ~1-2 days work)

State on `StochasticSequenceEditPage`:

```cpp
StochasticSequence _snapshot;   // ~560 B
bool _snapshotArmed = false;
bool _showingSnapshot = false;
```

Capture button (dedicated Fn combo): copies live → `_snapshot`. From that point,
a toggle swaps which is live in the model via `_track.sequence(idx) = _snapshot`
(or vice versa). After swap, fire `notifyStochasticWindowEdit` +
`notifyStochasticShapingEdit` so the engine cache + queues rebuild.

Use case: "compare current to a snapshot I took earlier". User dials a melody,
captures, experiments, toggles to A/B. Engine plays whichever's live.

Open issues at this lightweight tier:
- **Pattern switching invalidates the snapshot.** Either clear on pattern change
  (simplest) or per-pattern snapshot table (`560 × 16 ≈ 9 KB`, prohibitive for UI
  scratch state).
- **Edits while "showing snapshot" mutate the snapshot.** GeneratorPage's
  behavior — accept it (snapshot is just "the other copy") or freeze that side
  read-only (cleaner Lock semantic).
- **Auto-regen (Patience, Mutate, Jump) continues** modifying the live sequence
  regardless of which copy is on display. For pure A/B that's intended ("both
  timelines evolve"); for Lock semantic it leaks.

### True Lock built on top (full feature, bigger)

Lock = A/B + regen freeze + persistence:

- `notifyStochasticWindowEdit/ShapingEdit` already exist — re-use as the swap
  notification path.
- While `_showingSnapshot` is true, the cycle-end hooks (`rollCycleEndHooks`)
  must skip Jump/Sleep/Patience/Mutate for the locked side. Easiest: gate the
  hooks on `!isLocked()`.
- Engine state — `_currentStep`, `_pitchState`, `_jumpRegister`, `_sleepRemaining`,
  `_loopCycleCount` — needs to reset on swap so playback is reproducible. Today's
  `syncWindowEdit` only handles queues + cache. Need a richer "renew engine
  state" call.
- Persistence: snapshot must survive save/load. Either it becomes a stored
  pattern slot (the locked content IS one of the patterns; UI marks it as
  locked) or a dedicated locked-pattern field on the track (adds ~560 B to the
  track persistence footprint).
- UI affordance: pattern slot shows a lock icon when locked. Patience / Mutate /
  NewR / NewM operations bypass locked slots.

### How this relates to the other two designs above

| Design | Footprint | Determinism | Persistence | Notes |
|---|---|---|---|---|
| Tape replay | ~768 B/track CCMRAM | Replays captured ticks | Tape would persist | Capacity cliff at 128 events/cycle. Kept as fallback. |
| RNG snapshot | ~20 B/track | Re-derives from frozen RNG + live knobs | RNG state persists | Preferred. Knobs become "perform on top of frozen loop" controls. |
| A/B preview | ~560 B/page (UI) + persistence cost if Lock | Plays the swapped copy verbatim | Snapshot is a pattern slot or dedicated field | Different mental model — no "frozen RNG with live knobs"; whole sequence including knob values is the locked unit. |

The three are not exclusive. A/B preview is **content-level** lock: snapshot includes
knob positions. RNG snapshot is **state-level** lock: only the RNG seed is frozen,
knobs stay live. The musical character differs:

- **Content lock (A/B)**: "this melody is the keeper. Knobs are stored as part of
  the snapshot. Twiddling knobs while locked silently lands on the other copy
  (or is blocked, depending on UX choice)."
- **State lock (RNG snapshot)**: "this seed produces this loop. Knobs reshape
  the result deterministically against the locked seed. Knob-tweak-while-locked
  is the intended performance gesture."

A/B preview is musically more like a "scene save / scene compare" feature than a
performance Lock. Both have value; they answer different user questions.

### Path forward

- Land Lightweight A/B first as a near-term win. Proves the swap mechanic,
  surfaces engine-state-reset edge cases, immediate performance value.
- Then decide: extend to True Lock (content semantic) or implement RNG snapshot
  Lock (state semantic) alongside as separate features.
- The "outsource to fractal" question below remains separate — that's about
  whether the Lock infrastructure should live in stochastic or be generalized
  across track types.

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

- 2026-05-22 — removed heap-allocated `_lockedParents` cache, the `if (locked)` replay branch in `triggerStep`, `captureLockedParent`, `lockAvailable`, init/free helpers, destructor. Sim + STM32 release builds clean. `.text` shrunk by ~2.5 KB. Four stochastic test suites pass. PROJECT.md heap section updated. **Hardware-verified with 8 stochastic tracks — no crash.**
- 2026-05-22 — added the engine-flag (RNG-state freeze) alternative section, pointing at `TuesdayTrackEngine::tick:1870-1883` as the working precedent. This is now the preferred implementation when Lock work resumes; the flat-tape design is kept as a fallback.
