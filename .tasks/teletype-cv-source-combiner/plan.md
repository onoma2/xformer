# Teletype CV Source Combiner Plan

## Goal
Create one explicit raw/value-domain CV source pipeline per Teletype CV output. The first implementation should remove accidental update-order dominance while preserving raw `0..16383` Teletype CV semantics and deferring user-facing op design.

## Non-goals
- No Telex VCA behavior.
- No voltage-domain mixer in the first implementation.
- No new user-facing Teletype ops in the first implementation.
- No parser or string enum work.
- No RAM savings claim.

## Chosen V0 Source Law
Use an explicit priority mux:

```text
ENV active
  else GEODE routed/active
  else LFO active
  else CV held value
```

This law intentionally preserves `E.*` as a raw CV envelope and gives Env a clear musical role: a triggered envelope owns the output while it is active, then the output returns to the next active source.

## Future Combiner Law
Keep this law internal until op definition is intentionally designed.

```text
source raw domain: 0..16383
center raw value: 8192
default logic threshold: 8192
```

Operations:

```text
MUX     choose A or B
XFADE   linear raw interpolation A..B using X 0..16383
ADD     clamp((A - 8192) + (B - 8192) + 8192)
SUB     clamp((A - 8192) - (B - 8192) + 8192)
MIN     min(A, B)
MAX     max(A, B)
AND     A > threshold && B > threshold ? 16383 : 0
OR      A > threshold || B > threshold ? 16383 : 0
```

User-facing op names and mode/source numeric IDs are deferred until after the internal pipeline is working and hardware-tested.

## Phase 1: Source Cache, No Behavior Change Target

Files:
- `src/apps/sequencer/engine/TeletypeTrackEngine.h`
- `src/apps/sequencer/engine/TeletypeTrackEngine.cpp`

Steps:
- Add a private per-output source state struct.
- Seed it from current CV defaults.
- Keep existing flat arrays initially.
- Add helper methods to set source raw values:

```cpp
void setCvSourceRaw(uint8_t index, int16_t raw);
void setEnvSourceRaw(uint8_t index, int16_t raw);
void setLfoSourceRaw(uint8_t index, int16_t raw);
void setGeodeSourceRaw(uint8_t index, int16_t raw);
```

Expected result:
- Build passes.
- No behavior change yet.

## Phase 2: Split Raw Render From Source Mutation

Files:
- `src/apps/sequencer/engine/TeletypeTrackEngine.cpp`

Steps:
- Split current `handleCv(index, value, slew)` behavior into:

```cpp
void setRawTarget(uint8_t index, int16_t value, bool slew);
void renderRawToPerformerCv(uint8_t index, int16_t raw, bool slew);
```

- Keep `handleCv()` as a wrapper temporarily.
- Ensure raw-to-voltage mapping remains byte-for-byte equivalent in meaning:

```text
raw + CV.OFF
-> rawToVolts()
-> remap from Bipolar5V to configured output range
-> output offset volts
-> clamp -5..+5
-> quantize
-> Performer output slot
```

Expected result:
- Build passes.
- Existing `CV`, `CV.SET`, and `E.T` behavior unchanged.

## Phase 3: Env Source Ownership

Files:
- `src/apps/sequencer/engine/TeletypeTrackEngine.cpp`

Steps:
- Change Env transitions so they update Env source state rather than final output directly.
- Preserve current raw semantics:

```text
E.O = attack start raw
E   = attack target raw
E.T = jump to E.O, slew to E
decay = slew back to E.O
```

- Add `renderCvOutputs()` at the end of update, initially rendering the Env source when Env is active.

Expected result:
- Hardware check: `E.O 1 0; E 1 16383; E.A 1 500; E.T 1` sweeps the full configured range.

## Phase 4: LFO Source Ownership

Files:
- `src/apps/sequencer/engine/TeletypeTrackEngine.cpp`

Steps:
- Change `updateLfos()` so it writes `_cvSources[i].lfoRaw` rather than calling `handleCv()`.
- Mark LFO source active/inactive through `setLfoStart()`.
- In `renderCvOutputs()`, use V0 priority law so Env overrides LFO only while Env is active.

Expected result:
- Hardware check: LFO outputs normally when Env is idle.
- Hardware check: triggering Env while LFO runs makes Env own output, then output returns to LFO after Env completes.

## Phase 5: Geode Source Ownership

Files:
- `src/apps/sequencer/engine/TeletypeTrackEngine.cpp`

Steps:
- Change Geode routing so `updateGeode()` writes `_cvSources[i].geodeRaw` rather than calling `handleCv()`.
- Mark Geode source active only when that output is routed and Geode is producing a meaningful routed value.
- Apply V0 priority law: Env overrides Geode while active; Geode overrides LFO/CV when routed/active.

Expected result:
- Hardware check: Geode routed output works.
- Hardware check: Env trigger temporarily takes over a Geode-routed CV output and returns to Geode afterward.

## Phase 6: Remove Old Direct-Writer Paths

Files:
- `src/apps/sequencer/engine/TeletypeTrackEngine.h`
- `src/apps/sequencer/engine/TeletypeTrackEngine.cpp`

Steps:
- Remove direct `handleCv()` calls from Env, LFO, and Geode update paths.
- Keep `handleCv()` only as the `CV`/`CV.SET` bridge entry if that remains clearer, or rename it after all call sites are updated.
- Remove flat source arrays only when the per-output struct owns equivalent state cleanly.

Expected result:
- One final CV render pass owns output writes.
- No accidental update-order source competition remains.

## Phase 7: Deferred Op Surface Design

Files:
- `teletype/src/ops/hardware.c`
- `src/apps/sequencer/engine/TeletypeBridge.cpp`
- `src/apps/sequencer/engine/TeletypeTrackEngine.h`
- `src/apps/sequencer/engine/TeletypeTrackEngine.cpp`

Steps:
- Design numeric Teletype ops for source selection and combiner modes only after Phases 1-6 pass hardware checks.
- Use the future combiner law above as the initial candidate.
- Keep op design separate from the pipeline refactor review.

## Verification
- Build `build/stm32/release` target first.
- Hardware-test Env-only full sweep.
- Hardware-test LFO-only output.
- Hardware-test Env-over-LFO temporary ownership.
- Hardware-test Geode routed output.
- Hardware-test Env-over-Geode temporary ownership.
- Confirm `.data + .bss` and `.ccmram_bss` do not regress materially; this task is not expected to save RAM.
