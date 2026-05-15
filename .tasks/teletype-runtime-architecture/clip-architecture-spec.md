# Teletype Clip Architecture Spec

## Problem

Performer currently gives Teletype one strong back-link: pattern slots.
Teletype is a Performer track, and Performer tracks have patterns. The
Teletype version of pattern payload is currently `PatternSlot`: Teletype
material selected by the active Performer Pattern for that Teletype track.

The feature is musically useful, but the implementation is ambiguous. A
`PatternSlot` behaves partly like TeletypeTrack pattern payload, partly like a
live VM mirror, partly like generic saved material, and partly like file
import/export transaction state.

That ambiguity creates hidden lifecycle problems:

- `syncToActiveSlot()` can mutate slot storage from paths that look like reads
  or serialization.
- `applyPatternSlot()` and `syncToActiveSlot()` make the live VM and slot
  storage two mutable representations of overlapping state.
- File import/export has to defend against partial updates with large rollback
  buffers.
- Future TeletypeRuntime extraction is blocked because it is unclear whether
  the model or runtime owns the VM.

The goal is to keep the musical feature while making the ownership boundary
explicit.

## Terminology rule

This work has two different pattern concepts. Do not write "pattern"
unqualified in this spec or implementation plan.

```text
Performer Pattern
  XFORMER arrangement/pattern index.
  Selects each track's current pattern payload.

Teletype Pattern
  Teletype VM `scene_pattern_t` data.
  Edited/read by Teletype P/P.N/PN ops and by Teletype pattern UI.

Teletype Clip
  The TeletypeTrack pattern payload selected by a Performer Pattern.
  Contains slot script, metro script, Teletype Patterns, IO config,
  timing config, MIDI config, and CV config.
```

Preferred code vocabulary:

```cpp
int performerPatternIndex;
int teletypePatternIndex;
int teletypeClipIndex;

int clipIndexForPerformerPattern(int performerPatternIndex);
void loadClipForPerformerPattern(int performerPatternIndex);
void switchClipForPerformerPattern(int performerPatternIndex);

const scene_pattern_t &teletypePattern(int teletypePatternIndex) const;
void setTeletypePattern(int teletypePatternIndex, const scene_pattern_t &pattern);
```

Avoid ambiguous names in new APIs:

```text
patternSlotForPattern()
loadClipForPattern()
setPattern()
clipIndexForPattern()
```

## Core model

Define four separate concepts:

```text
TeletypeTrack
  A Performer TrackMode.
  Like other tracks, it participates in Performer Pattern selection,
  Performer Pattern copy/paste/clear, project save/load, and track engine
  execution.

TeletypeTrack Pattern Payload
  The TeletypeTrack-specific data owned by one Performer Pattern.
  For NoteTrack this role is played by note sequence data.
  For CurveTrack this role is played by curve sequence data.
  For TeletypeTrack this role is played by Teletype clip material.

Teletype Scene
  Full Teletype document/project-like unit.
  Upstream-style storage: scripts, patterns, grid/text/document material.

Teletype Clip
  The TeletypeTrack pattern payload.
  Performer Pattern selection chooses which clip material this track loads.
  A clip is not a global library object and not a full Teletype scene.

Teletype VM
  Live runtime interpreter state.
  Owns execution, delays, variables, stack, random state, active scripts,
  active patterns, and other transient runtime behavior.
```

The important Performer mapping is:

```text
NoteTrack Performer Pattern     -> NoteSequence / note payload
CurveTrack Performer Pattern    -> CurveSequence / curve payload
TeletypeTrack Performer Pattern -> TeletypeClip / Teletype material payload
```

The current implementation compresses this through two physical
`PatternSlot` buffers and `performerPatternIndex % PatternSlotCount`. That two-slot
mapping is an implementation/product constraint, not a license to treat slots
as global scenes or live VM truth.

The current `PatternSlot` payload is already close to a clip:

```text
slot script
metro script
patterns[4]
I/O routing config
CV output config
timing config
MIDI config
```

The proposal does not require a new payload first. It changes the contract:

```text
Old: PatternSlot is an implicit live mirror selected by pattern.
New: TeletypeClip is the TeletypeTrack's pattern payload, loaded into the VM
     explicitly when that track pattern becomes active.
```

## Ownership invariant

```text
scene_state_t is runtime truth while Teletype executes.
TeletypeClip is the TeletypeTrack pattern payload.
Performer Pattern selection chooses the active TeletypeClip for this track.
Clip -> VM happens only on explicit Performer Pattern / clip load.
VM -> Clip happens only on explicit capture/save for that Performer Pattern.
Read-only paths never capture VM state.
```

This is the central invariant. Any implementation that keeps hidden
VM-to-slot sync from read/export paths has not actually separated clips from
the VM.

## Clip chunks

Initially, keep the current fixed `PatternSlot` layout for compatibility.
Treat it as the full TeletypeTrack pattern payload for the mapped Performer
Pattern.

Later, clips can become chunked:

```text
Script chunk
  slot script
  metro script

Pattern chunk
  Teletype patterns[0..3]

I/O chunk
  TI-* input mappings
  TO-* output mappings
  CV range / offset / quantize / root
  MIDI port/channel

Timing chunk
  timebase
  clock divisor/multiplier
  reset-metro behavior
```

Chunking lets TeletypeTrack patterns arrange meaningful Teletype material
without requiring a full scene load. It also lets future import/export glue
Teletype scenes into TeletypeTrack pattern payloads:

```text
scene -> selected chunks -> TeletypeTrack pattern clip
TeletypeTrack pattern clip -> selected chunks -> live VM/config
```

Chunking is a later format decision. The first implementation should keep the
existing binary/storage layout and only make the lifecycle explicit.

## Clip mapping constraint

The current implementation maps `performerPatternIndex % 2` to two physical
clip buffers. Performer Patterns 1 and 3 both map to Clip 0; Patterns 2 and 4
both map to Clip 1. Switching from Pattern 3 to Pattern 1 is a no-op (same
clip). Switching from Pattern 1 to Pattern 3 overwrites Clip 0 with auto-
captured VM state and reloads it — effectively a round-trip.

This `% 2` mapping is the current product/implementation constraint. It means
only two distinct Teletype clip states exist per track regardless of how many
Performer Patterns the arrangement uses. Any future expansion beyond two clips
requires storage and UI changes.

```text
Performer Pattern 1 -> Clip 0
Performer Pattern 2 -> Clip 1
Performer Pattern 3 -> Clip 0  (shares Clip 0)
Performer Pattern 4 -> Clip 1  (shares Clip 1)
```

## API direction

Add contract names before large renames:

```cpp
void loadClipIntoVm(int clipIndex);
void loadActiveClipIntoVm();
void loadClipForPerformerPattern(int performerPatternIndex);

void captureActiveClip();
```

`captureActiveClip()` is the only VM-to-clip primitive. The VM represents the
active clip only; capturing into an arbitrary clip index would be semantically
wrong (it would copy live execution state into a clip that is not currently
loaded, overwriting stored material without a clear user intent). Do not add
`captureVmToClip(int clipIndex)` unless there is an explicit product requirement
for "copy live VM into another clip," which is dangerous.

These wrappers can initially call existing functions:

```text
loadClipIntoVm()    -> applyPatternSlot()
captureActiveClip() -> syncToActiveSlot()
```

Then migrate call sites away from direct `applyPatternSlot()` and
`syncToActiveSlot()` so every caller declares its intent.

Eventual rename:

```text
PatternSlot        -> TeletypeClip / TeletypePatternPayload
applyPatternSlot   -> loadClipIntoVm / loadClipForPerformerPattern
syncToActiveSlot   -> captureActiveClip
```

## Delayed commands across clip switches

The Teletype delay queue is mutable VM state. When a clip switch occurs
(auto-capture + load), the delay queue may contain commands queued by scripts
in the old clip. The current likely behavior is that these delayed commands
continue executing against the newly loaded VM state (scripts, patterns,
variables from the new clip). This is a semantic hazard that needs an
explicit decision, not just documentation:

1. **Preserve current behavior (compatibility):** Old delayed commands
   continue running against the new VM state. Document as known behavior.
2. **Flush delays on clip switch:** Clear the delay queue on clip load.
   Predictable but discards pending work.
3. **Tag delays with source clip:** Track which clip queued each delay;
   drop stale delays when their source clip is no longer active. Cleaner
   semantics but adds bookkeeping overhead.

This decision affects musical behavior and must be made before Slice 3
(centralized pattern-change policy) ships.

## Lifecycle policy

Two valid product policies remain. This is a musical/product decision, not
an architecture truth. The code must choose one explicitly.

### Policy 1: Auto-capture on clip leave

```text
TeletypeTrack Performer Pattern / clip leave:
  capture active VM into old clip
  load new clip into VM
```

This is closest to current behavior, but must be named and centralized.

Pros:
- Preserves the feeling that editing/running a clip updates that clip.
- Lower behavior-change risk for existing users.

Cons:
- Script execution can still persist changes unless clearly documented.
- Delayed commands crossing a clip switch remain a hard semantic edge case.

### Policy 2: Explicit capture only

```text
TeletypeTrack Performer Pattern / clip leave:
  load new clip into VM

user save/capture:
  capture active VM into selected clip
```

Pros:
- Clean document semantics: load, edit, save/discard.
- Easier file transactions and undo/rollback.

Cons:
- More visible workflow change.
- Existing users may expect edits to survive pattern changes.

Recommended first step: implement wrappers and remove hidden read-path capture
before deciding the policy. After the hidden captures are gone, both policies
can be tested deliberately. Either policy must account for delayed-command
behavior (see above).

**Note:** The auto-capture vs. explicit-capture decision is musical behavior.
It should be validated with user/workflow input, not closed as an architecture
decision. Auto-capture-on-leave preserves current user expectations;
explicit-capture changes the mental model of what a pattern switch does.

## Read/write rules

These paths must not capture the VM implicitly:

```text
TeletypeTrack::write()
patternSlotSnapshot()
clipboard reads
file export reads
UI redraw/list reads
```

If a caller needs current live VM state stored into a clip, it must call a
capture API first.

Project serialization should write TeletypeTrack pattern payloads. Runtime VM
state should be serialized only if there is a separate, explicit runtime-state
feature.

## Two persistence contracts

Teletype material is persisted through two different mechanisms that should
not be conflated. The current code blurs these by making the Teletype text
file format a two-slot full backup, which is both unnecessary and costly.

### Project save/load

Project serialization owns the **whole** persistent model:

```text
TeletypeTrack
  shared scripts S1-S3
  TeletypeClip[2]
    slot script
    metro script
    Teletype patterns[4]
    IO / CV / timing / MIDI / BOOT
  active clip index
```

Before project save, behavior depends on the chosen capture policy:

```text
auto-capture policy:
  captureActiveClip()
  serialize all clips

explicit policy:
  serialize all stored clips only
```

Either way, project save/load remains whole-model. This is unchanged from
current behavior — `TeletypeTrack::write()` and `TeletypeTrack::read()`
already serialize both clips plus shared VM scripts.

### Teletype text save

Text export is a **read-only live export** for PC editing / sharing /
roundtrip. It saves the thing the user is currently editing and hearing, but
it does not commit runtime state into the Performer project model.

It must not call `captureActiveClip()`.

One text file contains:

```text
S1-S3        from live VM
S4           from live VM active slot script
M            from live VM active metro script
PATS         from live VM Teletype patterns[0..3]
IO/TIMING    from active clip config
BOOT         from active clip config
```

No `SLOT 1` / `SLOT 2`, no backup pair, no exporting the inactive clip.
This maps to how a Teletype user thinks: "save the thing I am editing/
hearing now."

The split source is intentional:

```text
Script/pattern material comes from live VM.
IO/timing/BOOT/CV/MIDI config comes from active clip config.
Text save does not make the live VM and stored active clip consistent.
```

Consistency with the Performer project is created only by explicit project
save/capture policy or by text load, not by text export.

Current write format exports both slots via `patternSlotSnapshot(0)` and
`patternSlotSnapshot(1)`, writing `S4P1`/`M1` and `S4P2`/`M2` sections plus
two `SLOT` blocks in `#PATS` and `#IO`. The new format exports only the
active working material with no slot disambiguation tags.

### Teletype text load

Text import loads into **active Teletype Clip + live VM only**:

```text
parse file into temp active clip document
apply temp clip to active clip
load active clip into VM
```

It must not touch the inactive clip:

```text
Load TT text while Performer Pattern 3 is active
  -> resolves active clip = clipIndexForPerformerPattern(3)
  -> imports into that clip only
  -> live VM is updated from that clip
  -> other clip remains untouched
```

This eliminates the 4-buffer rollback problem:

```text
current:
  snapshot clip1 + clip2
  parse clip1 + clip2
  rollback clip1 + clip2

clean:
  snapshot active clip only
  parse active clip only
  commit/rollback active clip only
```

RAM effect: text import/export scratch reduces from 4 `PatternSlot` buffers
(~4.9 KB .bss) to 2 (active temp + active backup), saving ~2,452 B .bss.
Potential further reduction to 1 buffer if the transaction is structured to
reuse the backup as parse target then re-snapshot on commit.

### Legacy two-slot text files

The old two-slot text file format becomes compatibility-only. When loading a
legacy file:

```text
If file contains SLOT 1 / SLOT 2 (or S4P1/S4P2, M1/M2):
  import only the SLOT matching the active clip index
  ignore the other slot
```

This is predictable and keeps the operation "current active material only."
It avoids a UI choice prompt: the active clip index is known from the current
Performer Pattern.

Legacy write path: for the transition period, keep the ability to write the
old two-slot format under a compatibility flag, or simply always write the
new single-clip format. Existing projects are not affected because project
save/load uses binary serialization, not the text format.

### File import/export model (replaced)

The previous "File import/export model" section was subsumed by the two
persistence contracts above. Key change: `ttSlot1`, `ttSlot2`,
`ttSlot1Backup`, `ttSlot2Backup` are no longer needed for text import/export.
They reduce to `activeClipTemp` and `activeClipBackup` (2 buffers, or 1 with
careful structuring). Project serialization does not use these globals — it
operates through `TeletypeTrack::write()`/`read()`.

## UI language

Use clip wording for TeletypeTrack-owned pattern material:

```text
TRACK 5 TELETYPE
PATTERN 1 -> CLIP 1
PATTERN 2 -> CLIP 2
```

Avoid calling clips "scenes." A scene is closer to a full Teletype
project/document. A clip is this TeletypeTrack's pattern payload: a
Performer-addressable subset of Teletype material owned by a track pattern.

If capture is explicit, expose commands such as:

```text
Capture Clip
Reload Clip
Discard VM Edits
Copy Clip
Clear Clip
```

If auto-capture is retained, expose that policy in settings or documentation
instead of hiding it behind `syncToActiveSlot()` calls.

## Implementation slices

### Slice 1: Naming wrappers, no behavior change

- Add `loadClipIntoVm()` / `loadActiveClipIntoVm()` wrappers.
- Add `captureActiveClip()` wrapper.
- Keep old functions temporarily or mark them as compatibility wrappers.
- Build STM32 release.

### Slice 2: Remove hidden capture from reads

- Remove `const_cast` capture from `TeletypeTrack::write()`.
- Make `patternSlotSnapshot()` a pure stored-clip read, or split it into:
  - `clipSnapshot()` for stored clip data.
  - `captureVmSnapshot()` for explicit VM capture.
- Audit clipboard/file export/list model reads.
- Add focused tests or simulator assertions if existing harness supports it.
- **Fix UI/engine race condition independently first:** `TeletypeScriptViewPage`
  calls `syncToActiveSlot()` (4 sites) and `TeletypePatternViewPage` calls
  `setPattern()` (1 site) without `engine.suspend()`/`engine.lock()`. This
  race exists now, not just after refactoring. Fix as a standalone correctness
  issue before bundling into the clip rename work.

### Slice 3: Centralize pattern-change policy

- Make TeletypeTrack Performer Pattern change call one clear function:
  `switchClipForPerformerPattern(performerPatternIndex)`.
- Inside that function, implement either:
  - auto-capture-on-leave + load; or
  - load-only with explicit capture elsewhere.
- Decide and implement delayed-command policy across clip changes (see
  "Delayed commands across clip switches" section).

### Slice 4: Two persistence contracts

- Refactor `writeTeletypeTrack()` to export active live VM material plus active
  clip config only (no
  `SLOT 1`/`SLOT 2`, no `S4P2`/`M2`, no second `SLOT` block in `#PATS`/`#IO`).
  This is read-only and must not call `captureActiveClip()`.
- Refactor `readTeletypeTrack()` to parse into active clip only, with 2-buffer
  transaction (active temp + active backup) replacing the current 4-buffer
  scheme.
- Add legacy two-slot read path: if file contains `SLOT 1`/`SLOT 2` sections,
  import only the slot matching the active clip index, ignore the other.
- Verify project save/load (`TeletypeTrack::write()`/`read()`) is unchanged —
  it continues to serialize all clips through binary serialization.
- Evaluate RAM reduction: 4 → 2 `PatternSlot` buffers (~2,452 B .bss saved).
  Further reduction to 1 buffer possible if backup is reused as parse target.

### Slice 5: Optional chunked clips

- Add a chunk mask or versioned clip format only after the lifecycle is stable.
- Preserve legacy `PatternSlot` layout migration.
- Support script-only, pattern-only, I/O-only, timing-only, and full clips if
  product workflow justifies it.

## Non-goals

- Do not implement one global Teletype VM in this work.
- Do not remove Teletype's ability to mutate Performer through existing ops.
- Do not replace Teletype scripts with a Performer action language.
- Do not promise RAM savings from this architecture alone.
- Do not consolidate file backup buffers before the transaction boundary is
  clear.

## Verification gates

- STM32 release build passes.
- Existing Teletype projects load without migration loss.
- Pattern switching preserves current behavior unless the chosen policy
  intentionally changes it.
- Saving a project does not mutate clip state except through explicit capture
  or the chosen auto-capture policy.
- Teletype text file contains active live VM script/pattern material plus
  active clip config only; inactive clip is not exported.
- Teletype text load modifies only the active clip; inactive clip unchanged.
- Legacy two-slot text files load correctly (active slot selected, other
  ignored).
- File import failure leaves previously committed clips intact.
- Script 3 / metro edits have predictable persistence behavior.
- Delayed commands across clip changes follow the chosen policy and are
  hardware-tested.
