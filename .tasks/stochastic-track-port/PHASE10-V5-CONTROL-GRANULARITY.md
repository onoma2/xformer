# Phase 10: V5 Control Granularity Spec

V5 redefines the Stochastic track around a user-facing control granularity model:

1. User wants generative music.
2. User picks how much control detail to expose.
3. The same generator + looper engine remains underneath every level.

This document is the implementation-facing spec. The vocabulary contract lives in
`.tasks/stochastic-track-port/PHASE7-DICTIONARY.md`.

## Goals

- Make the Stochastic track understandable as an instrument rather than a long
  mixed parameter list.
- Preserve the V4 ownership correction: pattern-defining stochastic material is
  sequence-owned.
- Separate generator density from loop playback masking.
- Expose full burst controls early because burst is audible engine work, not a
  hidden advanced detail.
- Add Level 3 duration tickets with the approved parent-duration set.
- Keep Marbles-style Shape/Spread/Bias/Steps as Level 1 macro controls only.

## Non-Goals

- No project file version bump during the development stage.
- No new module-name user modes such as MeloDICER, Proteus, SIG, or Marbles.
- No random playback order / Run mode.
- No new persistent lock banks.
- No `1 bar`, `1/64`, or `1/128` parent-duration tickets.
- No quintuplets until a specific UI and clocking contract exists.

## Core Topology

```text
Generator layer
  chooses parent event rhythm, pitch, articulation, rest state, and child hits

Looper layer
  stores generated source material, windows it, rotates it, masks it, mutates it,
  and regenerates it over time

Output layer
  schedules the evaluated CV and gate events
```

The engine must not become three engines. Levels are presentation and control
granularity over the same model.

## Development Direction

Implement from model truth upward, not from Level 1 UI downward.

```text
model truth -> Level 3 tables/transforms -> Level 2 direct controls -> Level 1 macros
```

The lowest truthful representation must be stored and persistent where it defines
pattern identity:
- pitch tickets
- duration tickets
- source modes
- generated source events
- mask ranks
- loop window
- patience/mutate/jump state inputs
- burst parameters

Ownership rule:
- Preserve the V4 ownership correction.
- New stochastic controls/entities are sequence-owned by default.
- Only standard Performer infrastructure stays track-owned: static octave,
  transpose, slide time, CV update mode, fill state, and comparable established
  track-wide offsets.
- Do not move stochastic generator, looper, ticket, mask, duration, patience, or
  mutation controls back to `StochasticTrack`.

Level 1 macro controls do not own separate hidden runtime systems. They are
control laws that derive, bias, initialize, or edit the same underlying model
truth exposed by Level 2 and Level 3.

Rules:
- If a lower-level control is active, it replaces or parameterizes the matching
  macro component.
- Lower levels must not be stacked as hidden post-processes unless the document
  explicitly says so.
- Calculated/generated source values must persist in the sequence source buffers
  when the domain is in `Loop`.
- `Live` domains may calculate fresh values without persisting them into loop
  source buffers.
- Lock/Hold persists only for the runtime session and stores evaluated output,
  not the reason that output existed.

## Level 1: Core

Purpose: immediate generative output with a small, playable surface.

### Generator Segment

| Control | Extends | Meaning |
| --- | --- | --- |
| `Complexity` | base | Macro richness for pitch, rhythm, and burst decisions. |
| `Density` | base | Generator-level sound/rest amount. |
| `Shape` | base | Macro distribution mode. |
| `Spread` | `Shape` | Width of the active distribution. |
| `Bias` | `Shape` | Center or lean of the active distribution. |
| `Steps` | `Shape` | Stepped/discrete character of the distribution. |
| `Range` | base | Generator register/octave spread probability. |
| `Burst` | base | Probability or amount of child hits. |
| `Burst Count` | `Burst` | How many child hits. |
| `Burst Rate` | `Burst` | How tightly or widely child hits are spaced. |
| `Burst Pitch` | `Burst` | Parent pitch or generated child pitches. |

### Looper Segment

| Control | Extends | Meaning |
| --- | --- | --- |
| `Mode` | base | Coupled `Live` or `Loop` source mode for rhythm and melody. |
| `Refresh` | `Mode=Loop` | Manual loop repopulation command. |
| `Size` | base | Number of parent events in the generated phrase. |
| `First` | `Size` | First parent event included in playback. |
| `Last` | `Size` | Last parent event included in playback. |
| `Rotate` | `First/Last` | Non-destructive loop read offset. |
| `Patience` | `Mode=Loop` | Automatic loop regeneration interval; 100 means never auto-regenerate. |

### Level 1 Rules

- `Mode = Live` means `Rhythm Live + Melody Live`.
- `Mode = Loop` means `Rhythm Loop + Melody Loop`.
- `Refresh` repopulates current Loop source domains immediately, like a
  momentary `Patience=0` action without changing the stored Patience value.
- `Shape`, `Spread`, `Bias`, and `Steps` are macro controls here.
- `Range` is always available and means the same thing at every level.
- Full burst controls are visible here.
- `Density` means generator sound/rest amount, not deterministic loop thinning.

## Level 2: Direct

Purpose: hands-on MeloDICER-like control without exposing every probability
table.

### Generator Segment

| Control | Extends | Meaning |
| --- | --- | --- |
| `Rhythm` | Level 1 `Mode` | Rhythm source mode, `Live` or `Loop`. |
| `Melody` | Level 1 `Mode` | Melody source mode, `Live` or `Loop`. |
| `Rate` | `Complexity` | Explicit base parent-event duration. |
| `Variation` | `Complexity` | Rhythmic deviation around `Rate`. |
| `Rest` | `Density` | Explicit stochastic rest probability. |
| `Legato` | rhythm articulation | Chance to continue gate instead of retriggering. |
| `Slide` | pitch articulation | Chance or amount of glide. |
| `Accent` | articulation | Chance or amount of accent. |
| `Low Degree` | `Range` | Lower generated pitch boundary. |
| `High Degree` | `Range` | Upper generated pitch boundary. |
| `Pitch Tickets` | `Shape` | Explicit per-degree pitch weights. |

### Looper Segment

Level 1 looper controls remain available. The new Level 2 looper capability is
split source control:

```text
Rhythm Loop + Melody Loop = full phrase loop
Rhythm Loop + Melody Live = stable groove, fresh notes
Rhythm Live + Melody Loop = fresh timing, stable motif
Rhythm Live + Melody Live = continuous generation
```

### Level 2 Rules

- Pitch tickets replace hidden macro pitch distribution control.
- If Shape/Spread/Bias/Steps remain visible while tickets are visible, the UI
  must make the relationship explicit. They must not secretly compete with
  edited tickets.
- `Rest` is direct generator silence and should be musically distinct from
  Level 1 `Density`.

## Level 3: Weights / Advanced

Purpose: explicit probability programming and advanced loop operations.

### Generator Segment

| Control | Extends | Meaning |
| --- | --- | --- |
| `Duration Tickets` | `Rate/Variation` | Explicit parent-duration weights. |
| `Rest Tickets` | `Rest` | Optional weighted rest material if implemented. |
| `Degree Rotate` | `Pitch Tickets` | Shifts generated degrees through the active scale. |
| `Ticket Rotate` | `Pitch Tickets` | Rotates ticket weights/mask while exclusions stay excluded. |
| `Linearity` | `Complexity` | Local melodic smoothing toward nearby pitches. |
| `Contour` | `Complexity` | Upward/downward motion bias. |

### Looper Segment

| Control | Extends | Meaning |
| --- | --- | --- |
| `Mask` | Level 1 `Density` | Deterministic playback thinning of stored loop events. |
| `Tilt` | `Mask` | Early/late priority bias for what survives masking. |
| `Mutate` | `Patience` | Gradual stored-source change without full regeneration. |
| `Jump` | loop evolution | Loop-level bounded register movement. |
| `Lock` | output layer | Hard runtime evaluated-output freeze. |

### Level 3 Rules

- `Mask` is approved as the deterministic loop-thinning name.
- Do not call `Mask` density.
- `Tilt` belongs to `Mask`.
- `Degree Rotate` and `Ticket Rotate` are Level 3 controls.
- `Mutate` and `Jump` are loop-level behavior, not generator macros.

## Duration Tickets

Duration tickets select parent-event duration. They do not select burst timing
and they do not make the track slower than its clock/divisor.

Approved table:

| Slot | Duration | Role |
| --- | --- | --- |
| 0 | `1/2` | slow parent event |
| 1 | `1/4` | quarter parent event |
| 2 | `1/8` | eighth parent event |
| 3 | `1/16` | sixteenth parent event |
| 4 | `3/16` | dotted/three-step parent event |
| 5 | `5/16` | irregular parent event |
| 6 | `7/16` | irregular parent event |
| 7 | `1/8T` | triplet parent event |

Implementation rules:
- Store eight compact weights on `StochasticSequence`.
- Start with `0..100` weights unless exclusion semantics are explicitly needed.
- When duration tickets are inactive, use `Rate + Variation`.
- When duration tickets are active, choose the parent duration from the table and
  ignore `Rate + Variation` for duration selection.
- The selected parent duration is the container that burst child hits must fit
  inside.
- Burst owns faster sub-events. Do not add `1/64` or `1/128` parent durations.

## Rename / Migration Rules

Current implementation names may lag the V5 vocabulary. The next implementation
phase should converge them carefully:

- Current `Density` deterministic thinning must become `Mask`.
- Current `Tilt` remains `Tilt`, but is attached to `Mask`.
- New V5 `Density` must become generator-level sound/rest amount.
- Current `Rest` remains direct rhythm-generator rest probability.
- Existing `Shape`, `Spread`, `Bias`, and `Steps` map to Marbles macro shaping.
- Existing `rhythmMode` and `melodyMode` map to Level 2 `Rhythm` and `Melody`.
- Level 1 `Mode` is a UI macro over both source modes, not a third stored mode.
- Existing `Range` remains the same user-facing control across all levels and
  must not be redefined as a Shape sub-control.

## Macro Decomposition Rules

Level 1 macros are shortcuts/control laws over lower-level model truth:

- `Mode` writes both `rhythmMode` and `melodyMode`.
  - `Live` sets both to `Live`.
  - `Loop` sets both to `Loop`.
  - If Level 2 splits them, Level 1 display may later show `Split`/`Mixed`;
    wording is a UI decision.
- `Complexity` derives or biases pitch movement, rhythm vocabulary, and burst
  likelihood, but it must act through the same event-generation inputs that
  Level 2/3 expose.
- `Density` derives generator sound/rest amount; Level 2 `Rest` is the direct
  rest component of that law.
- `Shape/Spread/Bias/Steps` derive or reshape pitch distribution; once explicit
  pitch tickets are edited, their relationship must be explicit.
- `Burst` macro derives or edits the same `Burst Count`, `Burst Rate`, and
  `Burst Pitch` parameters exposed at Level 1/3.
- `Rate/Variation` are Level 2 controls over parent duration; `Duration Tickets`
  are the Level 3 truth table for parent duration.
- `Refresh` is a momentary command that repopulates Loop source material using
  current generator settings.

Persistence rules:
- Loop-mode generated parent rhythm, melody, and child-hit material must be
  stored in the sequence source buffers.
- Macro edits should invalidate or regenerate affected Loop source domains
  deliberately; do not let stale generated buffers silently keep old macro math.
- Live-mode generated material is scratch/evaluation material and must not
  overwrite Loop source buffers.

## Patience and Refresh

`Patience` is the automatic loop refresh interval. It is not lock and it is not
mutation.

Patience rule:
- `Patience = 0`: refresh Loop domains every completed loop cycle.
- `Patience = 100`: never auto-refresh.
- `Patience 1..99`: map exponentially to musical loop counts.

Initial mapping:

```cpp
if (patience >= 100) {
    loopsBeforeRefresh = Never;
} else {
    loopsBeforeRefresh = 1 << clamp(patience / 14, 0, 7);
}
```

Approximate loop-count range:

```text
0       -> 1 loop
1..13   -> 1 loop
14..27  -> 2 loops
28..41  -> 4 loops
42..55  -> 8 loops
56..69  -> 16 loops
70..83  -> 32 loops
84..97  -> 64 loops
98..99  -> 128 loops
100     -> never
```

`Refresh` is a manual command:
- It immediately repopulates Loop source domains from current generator settings.
- It behaves like a momentary `Patience=0` event.
- It does not alter the stored Patience value.
- It does not affect Live domains.
- It must be ignored by locked evaluated-output replay.

## Engine Acceptance

- Level 1 default state must produce audible output without ticket editing.
- Full burst should be audible from Level 1.
- Split Rhythm/Melody source modes must still work at Level 2.
- Range affects register/octave spread consistently at all levels.
- `Density` changes generator sound/rest amount.
- `Mask` down/up removes/restores the same stored loop events.
- `Patience` regenerates only Loop domains according to the exponential
  loop-count rule.
- `Refresh` immediately repopulates only Loop domains.
- `Mutate` mutates only Loop domains.
- `Lock` replay bypasses generator, looper, RNG consumption, patience, mutation,
  mask, and jump changes.

## Agent-Focused Implementation Plan

Implement V5 in model/generator/engine first. Keep visual UI work minimal until
the sound model is verified on hardware.

### Phase 10.0: Baseline and Invariants

Read before editing:
- `PROJECT.md`
- `.tasks/stochastic-track-port/PHASE7-DICTIONARY.md`
- `.tasks/stochastic-track-port/PHASE10-V5-CONTROL-GRANULARITY.md`
- `src/apps/sequencer/model/StochasticTypes.h`
- `src/apps/sequencer/model/StochasticSequence.h`
- `src/apps/sequencer/model/StochasticTrack.h`
- `src/apps/sequencer/engine/StochasticGenerator.h`
- `src/apps/sequencer/engine/StochasticGenerator.cpp`
- `src/apps/sequencer/engine/StochasticTrackEngine.h`
- `src/apps/sequencer/engine/StochasticTrackEngine.cpp`
- `src/apps/sequencer/ui/model/StochasticPerformanceListModel.h`
- `src/apps/sequencer/ui/model/StochasticConfigListModel.h`
- `src/apps/sequencer/ui/pages/StochasticSequenceEditPage.h/.cpp`

Before behavior edits:
- Capture current STM32 release RAM and size probes.
- Confirm `StochasticTrack` remains under the `NoteTrack` gate.
- Confirm current default Stochastic output behavior on simulator or hardware if
  possible.
- Do not bump project version.

Hard invariants:
- No STL containers in tick path.
- No tick-path allocation.
- No new persistent lock banks.
- Keep existing ticket page.
- Do not start final Level 1/2/3 visual UI until model and engine semantics are
  verified.

### Phase 10.1: Model Truth and Persistence

Files:
- `src/apps/sequencer/model/StochasticTypes.h`
- `src/apps/sequencer/model/StochasticSequence.h`
- `src/apps/sequencer/model/StochasticTrack.h`
- `src/apps/sequencer/model/StochasticTrack.cpp`

Tasks:
1. Preserve the V4 sequence-owned ownership model.
   - New stochastic entities are sequence-owned unless they are standard
     Performer track infrastructure.
   - Do not move pattern-defining fields back to `StochasticTrack`.
2. Treat Level 3-ish persistent data as the source of truth:
   - pitch tickets
   - duration tickets
   - source modes
   - generated source event buffers
   - mask ranks
   - loop window
   - burst parameters
   - patience/mutate/jump inputs
3. Rename or alias current deterministic thinning storage toward `Mask`.
   - Existing deterministic rank/thinning behavior becomes `mask`.
   - `Tilt` remains attached to `mask`.
   - Keep serialization symmetric; dev projects may break, but no
     `end_of_file`.
4. Add new generator-level `Density`.
   - Range: `0..100`.
   - Meaning: generator sound/rest amount.
   - It is not deterministic playback thinning.
5. Add Level 3 duration-ticket storage on `StochasticSequence`.
   - Eight compact weights for:
     `1/2`, `1/4`, `1/8`, `1/16`, `3/16`, `5/16`, `7/16`, `1/8T`.
   - Start with `0..100` weights.
   - Add an enable/active policy:
     either explicit `durationTicketsEnabled`, or active when any ticket table
     differs from its default. Prefer explicit if it is cheaper and clearer.
6. Keep `range` as a persistent generator register/octave spread control.
   - It must mean the same thing at every level.
   - Do not tie it to Shape as a sub-control.
7. Keep `rhythmMode` and `melodyMode` as the stored source modes.
   - Level 1 `Mode` is UI/editing macro only.
   - Do not add a third stored source mode.
8. Ensure `clear()`, `write()`, and `read()` initialize and serialize all new
   fields symmetrically.
9. Add a non-persistent/manual `Refresh` command path.
   - It may be represented as a UI action rather than stored state.
   - It must repopulate Loop source domains using current sequence-owned
     generator settings.
10. Define which macro edits invalidate cached Loop source domains.
   - Macro edits must not leave stale persisted source buffers silently using old
     math.
   - Live-mode scratch evaluation must not overwrite Loop source buffers.

Acceptance:
- `StochasticSequence` size stays within the V4 budget.
- `StochasticTrack` stays under the `NoteTrack` container gate.
- Existing ticket page still compiles and edits pattern-owned pitch tickets.

### Phase 10.2: Level 3 and Level 2 Generator Semantics

Files:
- `src/apps/sequencer/engine/StochasticGenerator.h`
- `src/apps/sequencer/engine/StochasticGenerator.cpp`

Tasks:
1. Implement/verify Level 3 truth selection first.
   - Pitch tickets select degree material.
   - Duration tickets select parent duration when active.
   - Burst Count/Rate/Pitch shape child hits.
   - Mask/Tilt shape deterministic loop playback thinning.
2. Implement Level 2 controls as parameterized access to that truth.
   - `Rate + Variation` produce parent-duration selection when duration tickets
     are inactive.
   - `Rest` is direct generator silence.
   - `Range` controls register/octave spread consistently.
   - `Rhythm` and `Melody` directly edit source modes.
3. Rename internal deterministic thinning concepts from density to mask where
   practical.
   - If full renaming is too large, add comments and wrappers so the public
     behavior is `Mask`.
4. Apply new generator `Density` during rhythm generation.
   - Density controls sound/rest amount at generation time.
   - `Rest` remains direct explicit rest probability.
   - Define the combination clearly in code comments. Preferred initial law:
     sound chance is constrained by Density first, then Rest can still create a
     rest as the direct rhythm control.
5. Keep `Mask` deterministic and RNG-free at playback time.
   - Changing Mask must not consume RNG.
   - Mask down/up restores the same stored loop skeleton.
6. Add duration-ticket duration selection.
   - When duration tickets are disabled, use existing `Rate + Variation`.
   - When enabled, select parent duration from the approved 8-slot table.
   - Store evaluated duration bucket in `StochasticSourceEvent`; never store raw
     `rate 1..400` in packed event cache.
7. Keep `Shape/Spread/Bias/Steps` as Level 1 macro generation controls.
   - They must not enable excluded pitch degrees.
   - If explicit pitch tickets are edited, do not let Shape secretly fight
     tickets unless the current engine already has a documented combining rule.
8. Keep full Burst generation available at Level 1.
   - Burst probability, count, rate, and pitch mode must all affect generated
     child hits.

### Phase 10.2b: Level 1 Macro Control Laws

Files:
- `src/apps/sequencer/model/StochasticSequence.h`
- `src/apps/sequencer/engine/StochasticGenerator.cpp`
- temporary UI/list files only if needed for hardware access

Tasks:
1. Add macro setter/evaluator behavior after the lower-level truth paths work.
2. `Mode` macro writes both stored source modes.
3. `Complexity` biases the same pitch/rhythm/burst selection inputs used by
   Level 2/3.
4. `Density` writes/derives generator sound/rest amount, not Mask.
5. `Shape/Spread/Bias/Steps` derive or reshape pitch distribution through the
   same pitch ticket/selection law.
6. `Burst` macro edits the same Burst Count/Rate/Pitch parameters that are
   exposed directly.
7. Macro edits must invalidate/regenerate affected Loop source domains
   deliberately.

Acceptance:
- Default generation can produce notes without editing tickets.
- Density low/high audibly changes generated sound/rest amount.
- Duration tickets select only the approved durations.
- Burst child generation still works with parent and generated child pitch.
- Macro edits and lower-level edits do not stack as contradictory hidden systems.

### Phase 10.3: Engine Playback

Files:
- `src/apps/sequencer/engine/StochasticTrackEngine.h`
- `src/apps/sequencer/engine/StochasticTrackEngine.cpp`

Tasks:
1. Apply source modes as currently stored:
   - `Rhythm Loop/Live`
   - `Melody Loop/Live`
2. Treat Level 1 `Mode` only as a UI macro over source modes.
3. Parent event duration comes from:
   - generated event duration bucket from `Rate + Variation`, or
   - generated event duration bucket from `Duration Tickets`.
4. Burst child hits must always fit inside the chosen parent duration.
   - Keep the minimum audible low-gap constraints from earlier hardware fixes.
   - No child hit may cross into the next parent event.
5. Apply `Mask` after loop source read/evaluation.
   - A masked parent schedules silence/gate-off and no children.
   - Mask changes must not regenerate source material.
6. Keep `Patience` and `Mutate` Loop-domain-only.
7. Implement the new Patience rule.
   - `0` refreshes every loop cycle.
   - `100` never auto-refreshes.
   - `1..99` uses the exponential loop-count mapping.
8. Add manual `Refresh`.
   - It repopulates current Loop source domains immediately.
   - It does not alter Patience.
9. Keep `Jump` loop-level, not per-event random overflow.
10. Keep `Lock` above all generator/looper behavior.
   - Locked replay must bypass source reads, RNG, patience, mutation, mask,
     jump, and live edits.

Acceptance:
- `Rhythm Loop + Melody Live` and `Rhythm Live + Melody Loop` still work.
- Masked/rest parents produce no burst leakage.
- Lock replay is stable when Density, Mask, tickets, source modes, and Burst
  controls are edited.

### Phase 10.4: Temporary Hardware Access

Files:
- `src/apps/sequencer/ui/model/StochasticPerformanceListModel.h`
- `src/apps/sequencer/ui/model/StochasticConfigListModel.h`
- `src/apps/sequencer/ui/pages/StochasticSequenceEditPage.h/.cpp`

Tasks:
1. Keep the existing ticket page.
   - Do not delete or replace `StochasticSequenceEditPage`.
   - It remains the pitch-ticket editor for now.
   - It may receive only narrow fixes needed for correctness, such as bank
     normalization or event handling.
2. Update temporary list models only enough to hardware-test V5 semantics.
3. Rename user-facing deterministic thinning label to `Mask`.
4. Add user-facing generator `Density`.
5. Add access to duration-ticket controls, even if initially list-based.
6. Keep `Shape`, `Spread`, `Bias`, and `Steps` together.
7. Keep `Burst`, `Burst Count`, `Burst Rate`, and `Burst Pitch` together and
   accessible early.
8. Add a temporary hardware-accessible `Refresh` action/button/menu item.
9. Do not build final Level 1/2/3 visual pages in this phase.

Acceptance:
- Hardware can edit Density, Mask, Tilt, Burst controls, source modes, and
  Duration Tickets.
- Hardware can trigger Refresh and hear Loop source material repopulate.
- Existing pitch ticket page still edits the selected pattern's pitch tickets.

### Phase 10.5: Verification

Required checks:
- STM32 release build.
- `.data`, `.bss`, `.ccmram_bss`.
- `sizeof(StochasticSequence)`.
- `sizeof(StochasticTrack)`.
- `sizeof(StochasticTrackEngine)`.
- Save/load a project with a Stochastic track; no `end_of_file`.
- Hardware default output.
- Hardware Density versus Mask distinction.
- Hardware Burst audibility.
- Hardware duration-ticket selection.
- Hardware Patience `0`, mid, and `100` behavior.
- Hardware Refresh command.
- Hardware split Rhythm/Melody Loop/Live cases.
- Hardware Lock stability.

Commit guidance:
- Slice commits by model, generator, engine, temporary UI access, and docs/status.
- Do not stage unrelated `ui-preview` files or generated PNGs unless explicitly
  requested.

## UI Acceptance

- UI is segmented as Generator and Looper, not one long mixed list.
- Each lower-level page makes clear which higher-level parameter it extends.
- `Shape/Spread/Bias/Steps` appear together.
- `Burst/Burst Count/Burst Rate/Burst Pitch` appear together from Level 1.
- `Duration Tickets` use the approved eight-slot table.
- `Degree Rotate` and `Ticket Rotate` appear with advanced pitch tickets, not
  Level 1.

## Verification

Hardware checks for V5:

1. Default Level 1 produces sound.
2. `Density` low/high changes generator note/rest amount.
3. `Burst` with generated pitch is audible.
4. `Mode=Loop` repeats both rhythm and melody.
5. `Mode=Live` continuously changes both rhythm and melody.
6. Level 2 split source modes produce stable-groove/fresh-melody and fresh-rhythm/stable-motif cases.
7. Duration tickets choose only the approved durations.
8. Burst child hits never escape the chosen parent duration.
9. `Mask` down/up restores the same loop skeleton.
10. `Patience=0` refreshes every loop; `Patience=100` never auto-regenerates.
11. `Refresh` immediately repopulates Loop source domains without changing Patience.
12. `Lock` freezes final evaluated CV/gate output.
