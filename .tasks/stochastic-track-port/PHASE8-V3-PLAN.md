# Phase 8 V3 Stochastic Core Rebuild Plan

## Purpose

Phase 8 replaces the current single-mode stochastic core with a clearer two-layer design:

```text
generator domains -> loop source playback/evolution -> evaluated lock replay
```

This document is the controlling implementation plan for Phase 8. Where this plan conflicts with the older Phase 7 dictionary, this plan wins for Phase 8. Do not rewrite the Phase 7 dictionary during implementation unless the user explicitly asks for a dictionary topology amendment.

Phase 8 is engine/model/internal-access work. Final visual UI is Phase 9.

## Product Contract

- No `New` control.
- No user-facing `Dice`.
- No user-facing `Realtime`.
- Use `Loop` and `Live` as the source-mode vocabulary.
- Rhythm and Melody each have their own source mode.
- `Patience` is the automatic source-refresh rate for Loop domains only:
  - `0`: refresh constantly.
  - `100`: never refresh automatically.
  - middle values: slower/faster automatic refresh by the selected patience curve.
- `Mutate` edits Loop source material only. It does not affect Live domains.
- `Rest` creates generator-level rhythm silence.
- `Density` deterministically thins loop playback using stable priority/rank.
- `Lock` is evaluated-output replay above generator, loop, patience, mutation, density, and source edits.
- Add runtime-only `Lock A/B`; do not persist evaluated lock buffers in Phase 8.

## Mental Model

User-facing explanation:

> Rhythm and Melody can each be Live or Loop. Patience refreshes Loops. Mutate edits Loops. Rest creates rhythm silence. Density thins rhythm playback. Lock A/B captures exactly what came out.

Internal ownership:

- Generator owns how new rhythm and melody material is invented.
- Loop source owns repeatable rhythm/melody material and playback windowing.
- Evolution owns source refresh, source mutation, and density thinning.
- Lock owns final evaluated event replay.

## Key Files

Model:
- `src/apps/sequencer/model/StochasticTypes.h`
- `src/apps/sequencer/model/StochasticSequence.h`
- `src/apps/sequencer/model/StochasticTrack.h`
- `src/apps/sequencer/model/StochasticTrack.cpp`
- `src/apps/sequencer/model/ClipBoard.h`
- `src/apps/sequencer/model/ClipBoard.cpp`

Engine:
- `src/apps/sequencer/engine/StochasticGenerator.h`
- `src/apps/sequencer/engine/StochasticGenerator.cpp`
- `src/apps/sequencer/engine/StochasticTrackEngine.h`
- `src/apps/sequencer/engine/StochasticTrackEngine.cpp`

Routing / temporary hardware access:
- `src/apps/sequencer/model/Routing.h`
- `src/apps/sequencer/model/Routing.cpp`
- `src/apps/sequencer/ui/model/StochasticPerformanceListModel.h`
- `src/apps/sequencer/ui/model/StochasticConfigListModel.h`

Docs:
- `.tasks/stochastic-track-port/TASK.md`
- `.tasks/stochastic-track-port/PHASE8-V3-PLAN.md`

Do not perform Phase 9 visual UI work in Phase 8.

## Phase 8.0 Baseline and Guard Rails

Before editing code:

1. Read `PROJECT.md` for version bump and RAM policy.
2. Read this plan.
3. Read current stochastic files listed above.
4. Build STM32 release if the environment is available:

```bash
cd build/stm32/release
make sequencer
```

5. Record:
   - `.data`
   - `.bss`
   - `.ccmram_bss`
   - `sizeof(StochasticTrack)`
   - `sizeof(StochasticSequence)`
   - `sizeof(StochasticTrackEngine)`
   - `sizeof(Engine::TrackEngineContainer)`

Hard constraints:

- No STL containers in the tick path.
- No tick-path heap allocation.
- No large inline engine buffers that exceed the engine container gate.
- No file-format version bump unless explicitly requested.
- No final custom visual UI in Phase 8.
- Do not touch unrelated dirty files.

## Phase 8.1 Source Mode Topology

### Files

- Modify `src/apps/sequencer/model/StochasticTypes.h`
- Modify `src/apps/sequencer/model/StochasticTrack.h`
- Modify `src/apps/sequencer/model/StochasticTrack.cpp`
- Modify `src/apps/sequencer/model/Routing.h`
- Modify `src/apps/sequencer/model/Routing.cpp`
- Modify `src/apps/sequencer/ui/model/StochasticPerformanceListModel.h`

### Required Types

Replace the single global source mode concept with independent domain modes:

```cpp
enum class StochasticSourceMode : uint8_t {
    Loop,
    Live,
    Last
};
```

Add to `StochasticTrack`:

```cpp
StochasticSourceMode rhythmMode() const;
void setRhythmMode(StochasticSourceMode mode);

StochasticSourceMode melodyMode() const;
void setMelodyMode(StochasticSourceMode mode);
```

Storage:

```cpp
StochasticSourceMode _rhythmMode;
StochasticSourceMode _melodyMode;
```

Defaults:

```text
Rhythm Mode = Loop
Melody Mode = Loop
```

Remove or quarantine the old global `StochasticMode::Dice/Realtime` path:

- It must not drive engine behavior after Phase 8.
- If serialization layout requires keeping the byte, rename the internal field to reserved storage and do not expose it.
- UI labels must show `Loop` / `Live`, not `Dice` / `Realtime`.

Routing:

- Add dedicated routing targets only if needed for hardware testing:
  - `StochasticRhythmMode`
  - `StochasticMelodyMode`
- If routing IDs are scarce or risky, defer routability and expose only in temporary list model.

Acceptance:

- User can independently set rhythm source to `Loop` or `Live`.
- User can independently set melody source to `Loop` or `Live`.
- No visible page or routing menu says `Dice` or `Realtime`.

## Phase 8.2 Split Source Buffers

### Files

- Modify `src/apps/sequencer/model/StochasticTypes.h`
- Modify `src/apps/sequencer/model/StochasticSequence.h`
- Modify `src/apps/sequencer/model/StochasticTrack.h`
- Modify `src/apps/sequencer/model/ClipBoard.h`
- Modify `src/apps/sequencer/model/ClipBoard.cpp`

### Required Shape

Keep one parent playback concept, but split stored source material by domain.

Add compact domain records:

```cpp
struct StochasticRhythmEvent {
    uint8_t rate;
    uint8_t length;
    uint8_t densityRank;
    uint8_t childFirst;
    uint8_t childCount;
    uint8_t flags; // rest, legato, slide, accent, valid
};

struct StochasticMelodyEvent {
    uint8_t degree;
    int8_t octave;
    uint8_t flags; // valid
};
```

Keep `StochasticChildHit`, but make clear whether child pitch is inherited or generated:

- Child timing belongs to Rhythm.
- Child generated pitch belongs to Melody.
- If `Burst Pitch = Parent`, child melody data may be omitted or ignored.
- If `Burst Pitch = Generate`, child melody data must be generated and lock-captured.

Sequence storage:

```cpp
std::array<StochasticRhythmEvent, CONFIG_STEP_COUNT> _rhythmEvents;
std::array<StochasticMelodyEvent, CONFIG_STEP_COUNT> _melodyEvents;
std::array<StochasticChildHit, CONFIG_STEP_COUNT * 4> _children;
bool _rhythmValid;
bool _melodyValid;
uint32_t _rhythmSeed;
uint32_t _melodySeed;
```

Keep existing `Size`, `First`, `Last`, and `Rotate` as a shared loop window for Phase 8.

Do not expose separate rhythm/melody sizes in Phase 8. Internally, write helper functions so separate sizes can be added later without rewriting the engine.

Serialization:

- Preserve read/write symmetry exactly.
- If old event arrays exist, either migrate them deterministically into split arrays or ignore them only after ensuring no `end_of_file`.
- Do not serialize active runtime engine lock caches.
- Clipboard must copy/paste the split source buffers and seeds.

Acceptance:

- Rhythm Loop can repeat rhythm material while Melody Live generates fresh pitch.
- Melody Loop can repeat pitch material while Rhythm Live generates fresh timing/rest/burst structure.
- Project save/load reconstructs Loop-domain material deterministically.

## Phase 8.3 Generator Split

### Files

- Modify `src/apps/sequencer/engine/StochasticGenerator.h`
- Modify `src/apps/sequencer/engine/StochasticGenerator.cpp`
- Modify `src/apps/sequencer/engine/StochasticTrackEngine.cpp`

### Required API

Replace whole-parent generation as the only public path with domain-specific generation:

```cpp
static void generateRhythmPattern(
    StochasticSequence &sequence,
    const StochasticTrack &track,
    uint32_t seed);

static void generateMelodyPattern(
    StochasticSequence &sequence,
    const StochasticTrack &track,
    const Scale &scale,
    int rootNote,
    uint32_t seed);

static StochasticRhythmEvent generateRhythmEvent(
    const StochasticTrack &track,
    Random &rng);

static StochasticMelodyEvent generateMelodyEvent(
    const StochasticTrack &track,
    const Scale &scale,
    int rootNote,
    int &lastDegree,
    Random &rng);

static void mutateRhythmOne(
    StochasticSequence &sequence,
    const StochasticTrack &track,
    Random &rng);

static void mutateMelodyOne(
    StochasticSequence &sequence,
    const StochasticTrack &track,
    const Scale &scale,
    int rootNote,
    Random &rng);
```

Rhythm generation owns:

- `Rate`
- `Variation`
- `Rest`
- `Legato`
- `Slide`
- `Accent`
- `Burst`
- `Burst Count`
- `Burst Rate`
- `DensityRank`

Melody generation owns:

- `Tickets`
- `Range`
- `Spread`
- `Bias`
- `Complexity`
- `Contour`
- `Linearity`
- `Jump`
- scale/root conversion inputs

Rules:

- `Rest` is rolled only when generating rhythm material.
- `DensityRank` is generated only for rhythm material and only inside `Size`.
- `Density` itself is applied at playback, not generation.
- `Spread` and `Bias` reshape ticket mass but never enable excluded degrees.
- `Complexity` limits explored melodic vocabulary, not event audibility.
- `Jump` is bounded `-1..+1` octave displacement at melody evaluation time.

Acceptance:

- Generator helpers can produce rhythm-only and melody-only material independently.
- Existing all-loop behavior still works when both domains are `Loop`.

## Phase 8.4 Engine Playback Rewrite

### Files

- Modify `src/apps/sequencer/engine/StochasticTrackEngine.h`
- Modify `src/apps/sequencer/engine/StochasticTrackEngine.cpp`

### Source Resolution

At each parent event:

```text
rhythm = Rhythm Loop ? sequence.rhythmEvents[readIndex] : generateRhythmEvent()
melody = Melody Loop ? sequence.melodyEvents[melodyReadIndex] : generateMelodyEvent()
evaluated = combine rhythm + melody + Performer pitch/output settings
```

For Phase 8:

- `readIndex` uses shared `First..Last..Rotate`.
- `melodyReadIndex` equals `readIndex`.
- Leave helper seam for future separate melody size/index.

Pattern material validity:

- If `Rhythm Mode = Loop` and rhythm source invalid, generate rhythm pattern.
- If `Melody Mode = Loop` and melody source invalid, generate melody pattern.
- If a domain is `Live`, do not read or mutate its stored source buffer for sounding output.

Cycle boundary:

- `Patience` and `Mutate` affect only domains whose mode is `Loop`.
- `Patience = 0` should refresh Loop domains at the highest defined cadence.
- `Patience = 100` should never automatically refresh Loop domains.
- `Mutate` should edit one narrow cell in Loop domains:
  - Melody mutation changes one melody event.
  - Rhythm mutation changes one rhythm event.
  - Do not regenerate the whole combined parent event for a Proteus-style melody mutation.

Density:

- Apply after rhythm gate/rest/condition wants to produce output.
- Use `densityRank`.
- Do not consume RNG.
- If density hides parent, do not schedule child hits.

Burst:

- Child hits are scheduled inside parent duration.
- Child hits do not advance parent index.
- Child generated pitches must be audible in `CV Update = Gate`.
- Lock must capture evaluated child CV/timing/gate lengths.

Acceptance matrix:

| Rhythm | Melody | Expected |
|---|---|---|
| Loop | Loop | repeating rhythm and pitch |
| Loop | Live | repeating rhythm, changing pitch |
| Live | Loop | changing rhythm, repeating pitch cycle |
| Live | Live | fully fresh generation |

## Phase 8.5 Lock A/B

### Files

- Modify `src/apps/sequencer/model/StochasticTypes.h`
- Modify `src/apps/sequencer/model/StochasticTrack.h`
- Modify `src/apps/sequencer/model/StochasticTrack.cpp`
- Modify `src/apps/sequencer/engine/StochasticTrackEngine.h`
- Modify `src/apps/sequencer/engine/StochasticTrackEngine.cpp`
- Modify `src/apps/sequencer/ui/model/StochasticPerformanceListModel.h`

### Required Types

```cpp
enum class StochasticLockMode : uint8_t {
    Off,
    A,
    B,
    Last
};
```

Track storage:

```cpp
StochasticLockMode _lockMode;
```

Engine storage:

- Two runtime evaluated lock buffers:
  - `LockSlot A`
  - `LockSlot B`
- Each slot stores evaluated parent events and evaluated child hits.
- Keep buffers runtime-only in Phase 8.

Behavior:

- `Lock Off`: evaluate normally and allow capture/update policy.
- `Lock A`: replay slot A if valid.
- `Lock B`: replay slot B if valid.
- If selected lock slot has no valid captured event for the read index, evaluate normally and capture that event into the selected slot, or explicitly show/use empty behavior. Prefer current lock behavior if already established on hardware.
- Lock replay bypasses:
  - source buffer reads
  - Live generation
  - Patience
  - Mutate
  - RNG consumption
  - Density edits after capture

User-facing labels:

- `Lock: Off`
- `Lock: A`
- `Lock: B`

Non-goal:

- Do not persist Lock A/B buffers in project files.
- Do not call Lock A/B "Pattern Banks".

Acceptance:

- User can capture/replay two evaluated phrases during one runtime session.
- Switching A/B does not alter source material.
- Project save/load does not attempt to serialize lock buffers.

## Phase 8.6 Temporary Hardware Access

### Files

- Modify `src/apps/sequencer/ui/model/StochasticPerformanceListModel.h`
- Modify `src/apps/sequencer/ui/model/StochasticConfigListModel.h`

Temporary list-model access is allowed in Phase 8 only to verify the engine on hardware.

Performance list should expose:

- `Rhythm`: `Loop` / `Live`
- `Melody`: `Loop` / `Live`
- `Patience`
- `Mutate`
- `Density`
- `Tilt`
- `Rest`
- `Rate`
- `Variation`
- `Legato`
- `Slide`
- `Burst`
- `Count`
- `Rate` for burst if label context is clear; otherwise `B Rate`
- `Pitch` for burst if label context is clear; otherwise `B Pitch`
- `Lock`: `Off` / `A` / `B`
- `Size`
- `First`
- `Last`
- `Rotate`

Config list should keep Performer infrastructure:

- `Clock/Div`
- `Reset`
- `Scale`
- `Root`
- `Octave`
- `Trans`
- `CV`
- `Slide Time`

Do not implement final graphics, grids, visual lock-bank pages, or custom ticket pages in Phase 8. That is Phase 9.

## Phase 8.7 Verification

Build:

```bash
cd build/stm32/release
make sequencer
```

Record:

- `.data`
- `.bss`
- `.ccmram_bss`
- `sizeof(StochasticTrack)`
- `sizeof(StochasticSequence)`
- `sizeof(StochasticTrackEngine)`
- `sizeof(Engine::TrackEngineContainer)`

Hardware smoke tests:

1. Create Stochastic track; verify no boot/load crash.
2. `Rhythm Loop + Melody Loop`: confirm repeating full phrase.
3. `Rhythm Loop + Melody Live`: confirm rhythm repeats while pitch changes.
4. `Rhythm Live + Melody Loop`: confirm rhythm changes while pitch repeats.
5. `Rhythm Live + Melody Live`: confirm full live generation.
6. `Patience = 100`: confirm Loop material does not auto-refresh.
7. `Patience = 0`: confirm Loop material refreshes at maximum cadence.
8. `Mutate > 0`: confirm only Loop domains mutate.
9. `Density` down/up: confirm same rhythm skeleton disappears/reappears.
10. `Lock A`: capture and replay final evaluated output.
11. `Lock B`: capture a different output and switch A/B.
12. While locked, edit tickets, density, patience, mutate, burst, and source modes; output should not change.
13. Save/load project; confirm no `end_of_file`.
14. Confirm lock buffers are not expected to persist after load.

## Commit Slices

1. Model/source-mode topology.
2. Split source buffers and serialization.
3. Generator split.
4. Engine source-resolution and loop/evolution semantics.
5. Lock A/B runtime buffers.
6. Temporary list access and routing cleanup.
7. Verification notes and task status update.

## Phase 9 Handoff

Phase 9 should build the real UI around the stable Phase 8 model:

- visual Rhythm/Melody source state
- ticket editor sized by active scale degrees
- rhythm density/rest-order view
- generated source loop view
- Lock A/B capture/replay view
- burst child-hit visualization

Do not start Phase 9 until Phase 8 hardware semantics are verified.
