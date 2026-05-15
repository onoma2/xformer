# Teletype Clip Architecture — Phased Implementation Plan

Based on `clip-architecture-spec.md` with source-grounded call-site mapping from `docs/slots-teletype.md`.

---

## Terminology rule

This plan must not use bare "pattern" for new names or behavior claims.

```text
Performer Pattern   — XFORMER arrangement/pattern index. Selects a Track's current pattern payload.
Teletype Pattern    — Teletype VM `scene_pattern_t`. Used by P/P.N/PN ops and Teletype pattern editing.
Teletype Clip       — TeletypeTrack pattern payload selected by a Performer Pattern.
                     Contains slot script, metro script, Teletype Patterns, IO/timing/MIDI/CV config.
```

Preferred new API names:

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

Avoid adding new ambiguous names such as `loadClipForPattern()`,
`clipIndexForPattern()`, or `setPattern()`.

---

## Clip Mapping Constraint

```text
Performer Pattern 1 -> Clip 0
Performer Pattern 2 -> Clip 1
Performer Pattern 3 -> Clip 0  (shares Clip 0)
Performer Pattern 4 -> Clip 1  (shares Clip 1)
```

`performerPatternIndex % 2`. Only two distinct Teletype clip states per track.
Switching between two Performer Patterns that share the same clip is a no-op.
Switching between different clips triggers capture + load. Out of scope:
expanding beyond two clips.

---

## Delayed Commands Across Clip Switches

Decision needed before Phase 3:

1. **Preserve (compatibility):** Old delayed commands continue against new VM state.
2. **Flush on switch:** Clear delay queue on clip load. Predictable, discards pending work.
3. **Tag with source clip:** Drop stale delays. Cleaner semantics, adds bookkeeping.

---

## State Before

| Fragile Point | Source |
|---|---|
| `const_cast` in `write()` and `patternSlotSnapshot()` | `TeletypeTrack.cpp:182-183, 331-332` |
| No UI/engine mutual exclusion during `syncToActiveSlot()` | `TeletypeScriptViewPage.cpp:830,867,879,898` vs `TeletypeTrackEngine.cpp:207` |
| Slot index 3 collision (slot script = manual trigger script) | `SlotScriptIndex = 3`, `kManualScriptCount = 4` |
| Metro period contamination across slot switches in Clock mode | `TeletypeTrackEngine.cpp:1141` writes derived `m` back to VM |
| File I/O rollback uses 4 global `PatternSlot` buffers (4.9 KB .bss) | `FileManager.cpp:630-633` |
| Boot script index changes with active slot | `TeletypeTrack.h:630-633` reads from `activeSlot()` |

---

## Phased Plan

Each phase ends with a hardware gate: STM32 release build + RAM check +
behavioral verification on hardware.

---

### Phase 0 — UI/Engine Race Fix (Standalone Correctness) ✓

**Goal:** Fix the race that exists right now. Not part of the clip
refactoring, but must land first because later phases depend on safe
UI/engine access.

**Key insight:** `Engine::suspend()` stops the transport/clock (calls
`_clock.masterStop()`). Short inline UI mutations must use
`Engine::lock()`/`Engine::unlock()` instead, which skip engine updates
without stopping playback. `suspend()`/`resume()` is only correct for
long operations like file I/O (already used correctly for script/track
save/load via `kSuspendEngineForScriptIO`).

**What:** Add `engine.lock()` / `engine.unlock()` around all VM state
mutations from UI, bracketing the full read-modify-write sequence (not
just the final sync/capture).

| File:Line | Mutation | Fix |
|-----------|----------|-----|
| `TeletypeScriptViewPage.cpp:828` | `ss_overwrite_script_command()` + optional `syncToActiveSlot()` | wrap in `_engine.lock()`/`_engine.unlock()` |
| `TeletypeScriptViewPage.cpp:863` | `ss_insert_script_command()` + optional `syncToActiveSlot()` | same |
| `TeletypeScriptViewPage.cpp:881` | `ss_toggle_script_comment()` + optional `syncToActiveSlot()` | same |
| `TeletypeScriptViewPage.cpp:901` | `ss_delete_script_command()` + optional `syncToActiveSlot()` | same |
| `TeletypePatternViewPage.cpp` (10 sites) | `ss_set_pattern_*()` + `syncPattern()` | wrap in `_engine.lock()`/`_engine.unlock()`, reads inside lock for RMW patterns |
| `TeletypePatternViewPage.cpp:621` | `toggleTurtle()` | wrap in `_engine.lock()`/`_engine.unlock()` |
| `TeletypePatternViewPage.cpp:530` | `backspaceDigit()` read | wrap VM read in `_engine.lock()`/`_engine.unlock()` |
| `TeletypePatternViewPage.cpp:612` | `negateValue()` read | wrap VM read in `_engine.lock()`/`_engine.unlock()` |
| `ListPage.cpp:119` | all Teletype config edits via `_listModel->edit()` | `_engine.lock()`/`_engine.unlock()` when selected track is Teletype |

**TeletypeTrackListModel does not own Engine.** `ListPage::editSelectedRow()`
wraps `_listModel->edit()` with `lock()`/`unlock()` when the selected track
is Teletype. This covers all Teletype config field mutations (TI/TO/CV/
range/MIDI/BOOT/clock), not just the `MidiSource` case that calls
`syncToActiveSlot()`. **Intentional broad guard:** locks for any
list-model edit when the track is Teletype. Short synchronous operation,
acceptable overhead.

**Files changed:**
- `src/apps/sequencer/ui/pages/TeletypeScriptViewPage.cpp` — 4 edit ops wrapped with lock/unlock
- `src/apps/sequencer/ui/pages/TeletypePatternViewPage.cpp` — all pattern mutation+sync and VM reads wrapped with lock/unlock
- `src/apps/sequencer/ui/pages/ListPage.cpp` — added `editSelectedRow()` helper with lock/unlock guard
- `src/apps/sequencer/ui/pages/ListPage.h` — declared `editSelectedRow()`

**Hardware gate:**
- [x] STM32 release build: `cd build/stm32/release && make sequencer`
- [x] RAM: `.data + .bss` = 119,960 (flat). `.ccmram_bss` = 54,096 (flat).
- [x] Script editing: enter lines with metro running, verify sequencer continues
- [x] Pattern editing: edit TT pattern values with metro running, verify stability
- [x] Config editing: edit TI/TO/CV/MIDI settings from track page with metro running
- [x] Transport continuity: verify play/stop state unchanged after edits
- [x] Edge cases: metro S4/M edit, S1-S3 edit during trigger, pattern toggle
      during script pattern read, delete line during metro, insert row during
      metro, config edit during metro -- all pass, no crashes

---

### Phase 1 — Naming Wrappers (No Behavior Change) ✓

**Goal:** Establish clip vocabulary so every caller declares intent.
Zero runtime difference. Rename `PatternSlot` references to clip names.

**New methods on `TeletypeTrack`:**

```cpp
void loadClipIntoVm(int clipIndex);                    // wraps applyPatternSlot
void loadActiveClipIntoVm();                           // wraps applyActivePatternSlot
void loadClipForPerformerPattern(int performerPatternIndex); // mapping + load
void captureActiveClip();                              // wraps syncToActiveSlot
```

`captureActiveClip()` is the **only** VM-to-clip primitive. No
`captureVmToClip(int clipIndex)` — the VM represents the active clip only.

Wrappers call existing functions initially. Old functions remain as
compatibility wrappers.

**Engine call-site migration:**

| Old call | New call | File:Line |
|----------|----------|-----------|
| `applyPatternSlot(slot)` | `loadClipForPerformerPattern(pattern)` | `TeletypeTrackEngine.cpp:92` |
| `applyActivePatternSlot()` | `loadActiveClipIntoVm()` | `TeletypeTrackEngine.cpp:1417` (inside `loadScriptsFromModel()`) |

`onPatternChanged()` calls (lines 382, 384) deferred to Phase 3 — they pair
capture+load and belong in the centralized policy.

**Files changed:**
- `src/apps/sequencer/model/TeletypeTrack.h` — declare 4 new methods
- `src/apps/sequencer/model/TeletypeTrack.cpp` — implement wrappers
- `src/apps/sequencer/engine/TeletypeTrackEngine.cpp` — migrate 2 calls

**Hardware gate:**
- [x] STM32 release build
- [x] RAM: flat (wrappers are inline or thin, no new data)
- [x] Existing projects load without migration loss
- [x] Performer Pattern switching: switch between patterns on a Teletype
      track, verify scripts/patterns load correctly
- [x] Boot script: verify boot script runs on power-on / project load

---

### Phase 2 — Remove Hidden Capture + Rename Call Sites

**Goal:** No read-only or serialization path mutates clip storage.
Every `syncToActiveSlot()` call site is migrated to `captureActiveClip()`.
Every `patternSlotSnapshot()` migrated to `clipSnapshot()` (pure stored-data read).

#### Change 2a — `TeletypeTrack::write()`

Remove `const_cast` and `self->syncToActiveSlot()` at lines 182-183.

After this, `write()` is truly const. Project save serializes current `_state`
plus stored clip payloads. On project reload, stored clips take precedence
(`read()` deserializes clips, then `loadActiveClipIntoVm()` overwrites VM
patterns). A save-reload cycle returns to stored clip state. To persist edits,
call `captureActiveClip()` before save (or rely on auto-capture from Phase 3).

Longer-term: stop serializing `_state.patterns` from `write()`. Not in this
phase.

#### Change 2b — `patternSlotSnapshot()` → `clipSnapshot()`

Remove `const_cast` and `self->syncToActiveSlot()` at lines 331-332.
Pure stored-data read from `_patternSlots[slot]`.

#### Change 2c — `setPattern()` → `setTeletypePattern()`

Rename `setPattern()` to `setTeletypePattern()`, remove `syncToActiveSlot()`
from its body. Each caller must now explicitly call `captureActiveClip()`
afterwards if it wants persistence.

`TeletypePatternViewPage.cpp:627` calls `track.setPattern()`. Migrate to
`track.setTeletypePattern()` + `track.captureActiveClip()`.

#### Change 2d — `TeletypeScriptViewPage` (4 sites)

`track.syncToActiveSlot()` → `track.captureActiveClip()` at lines 830, 867,
879, 898.

#### Change 2e — Other call sites

| File:Line | Old | New |
|-----------|-----|-----|
| `TeletypeTrackListModel.h:249` | `syncToActiveSlot()` | `captureActiveClip()` |
| `ClipBoard.cpp:91` | `patternSlotSnapshot(p)` | `clipSnapshot(p)` |
| `ClipBoard.h:102` | field `.teletype` | `.teletypeClip` |
| `FileManager.cpp:715-716` | `patternSlotSnapshot(0/1)` | `clipSnapshot(0/1)` |
| `FileManager.cpp:755-756` | `patternSlotSnapshot(0/1)` | `clipSnapshot(0/1)` |
| `FileManager.cpp:766-767` | `patternSlotSnapshot(0/1)` | `clipSnapshot(0/1)` |
| `FileManager.cpp:1124,1129` | `applyActivePatternSlot()` | `loadActiveClipIntoVm()` |
| `Track.cpp:36` | `clearPatternSlot(p)` | `clearClipForPattern(p)` |
| `Track.cpp:63` | `copyPatternSlot(s,d)` | `copyClipForPattern(s,d)` |

#### Change 2f — Remove old compatibility wrappers

After all call sites are migrated, mark `applyPatternSlot()`,
`syncToActiveSlot()`, `patternSlotSnapshot()`, `setPattern()` as deprecated
or remove if no remaining callers.

**Files changed:**
- `src/apps/sequencer/model/TeletypeTrack.h` — rename `patternSlotSnapshot` to `clipSnapshot`; add `setClipForPattern()`, `clearClipForPattern()`, `copyClipForPattern()` wrappers; remove `const_cast` accessor
- `src/apps/sequencer/model/TeletypeTrack.cpp` — remove sync from `write()`, `setTeletypePattern()`, `clipSnapshot()`
- `src/apps/sequencer/model/ClipBoard.cpp, ClipBoard.h` — rename field
- `src/apps/sequencer/ui/pages/TeletypeScriptViewPage.cpp` — rename 4 calls
- `src/apps/sequencer/ui/pages/TeletypePatternViewPage.cpp` — add `captureActiveClip()` after `setTeletypePattern()`
- `src/apps/sequencer/ui/model/TeletypeTrackListModel.h` — rename 1 call
- `src/apps/sequencer/model/FileManager.cpp` — rename calls
- `src/apps/sequencer/model/Track.cpp` — rename calls
- `src/apps/sequencer/model/Track.h` — declare new Pattern methods

**Hardware gate:**
- [ ] STM32 release build
- [ ] RAM: flat (no new data; removed `const_cast` may enable minor optimizer wins)
- [ ] `const_cast` removed: verify `write()` is truly `const` (compile with warnings)
- [ ] Captured clip edit survives project save/load: edit S4, call
      `captureActiveClip()`, project save, project reload → S4 edit present
- [ ] Uncaptured VM-only edit is not silently captured by project save:
      edit S4 without capture, project save, project reload → S4 edit absent
      (stored clip state takes precedence on reload)
- [ ] Script persistence: edit S4 on hardware, switch Pattern away and back, verify S4 restored
- [ ] Pattern persistence: edit TT pattern values on hardware, switch Pattern away and back, verify restored
- [ ] Clipboard: copy/paste Teletype clip, verify correct data
- [ ] File export: use TT text save on hardware, re-import, verify roundtrip

---

### Phase 3 — Centralize Pattern-Change Policy

**Goal:** One clearly-named function for Performer Pattern / clip switching.
Auto-capture-on-leave is made explicit (current behavior). Delay queue
policy is implemented.

**Product decision gate (before implementing):**
- [ ] Auto-capture or explicit-capture on clip leave? Default: auto-capture
      (current behavior). Validate with user input.
- [ ] Delay queue behavior on clip switch? Default: preserve current behavior
      (compatibility). Decide before shipping.

**New method:**

```cpp
void TeletypeTrack::switchClipForPerformerPattern(int performerPatternIndex) {
    const int newClip = clipIndexForPerformerPattern(performerPatternIndex);
    if (newClip == _activeClipIndex) return;

    captureActiveClip();              // save current VM into old clip
    loadClipIntoVm(newClip);          // load new clip into VM
}
```

`clipIndexForPerformerPattern()` replaces `patternSlotForPattern()` — same
`% 2` logic.

#### Call site migration

| Old | New | Location |
|-----|-----|----------|
| `onPatternChanged()` body | `switchClipForPerformerPattern()` | `TeletypeTrack.cpp:377-385` |
| `setPatternSlotForPattern()` body | `setClipForPerformerPattern()` | `TeletypeTrack.cpp:337-343` |
| `clearPatternSlot()` body | `clearClipForPerformerPattern()` | `TeletypeTrack.cpp:346-361` |
| `copyPatternSlot()` body | `copyClipForPerformerPattern()` | `TeletypeTrack.cpp:364-374` |

Each refactored method uses `captureActiveClip()` + `loadClipIntoVm()` explicitly.

**Out of scope:**
- `bootScriptIndex` stays clip-scoped permanently. Storage and file IO bind it
  to clips. Top-level move is a breaking file-format change.
- `_resetMetroOnLoad` dual copy (transient + per-slot) stays. Needs deeper
  lifecycle analysis.

**Files changed:**
- `src/apps/sequencer/model/TeletypeTrack.h` — add `switchClipForPerformerPattern()`, `clipIndexForPerformerPattern()`
- `src/apps/sequencer/model/TeletypeTrack.cpp` — implement; refactor the 4 methods above
- `src/apps/sequencer/engine/TeletypeTrackEngine.cpp:207` — call `switchClipForPerformerPattern()`
- `src/apps/sequencer/model/Track.cpp:36,63` — call renamed functions

**Hardware gate:**
- [ ] STM32 release build
- [ ] RAM: flat
- [ ] Pattern switching: switch between all 4 Performer Patterns on a TT
      track, verify scripts/patterns/presets load for each clip
- [ ] Auto-capture: edit S4 on Clip 0, switch to Clip 1, switch back to
      Clip 0, verify S4 edits preserved
- [ ] Delay queue: schedule DEL commands, switch pattern during delay
      execution, verify chosen policy behavior
- [ ] Performer copy/paste/clear: copy clip, clear clip, paste clip on
      hardware, verify correct behavior for each
- [ ] Boot script: verify boot script index per-clip persists across
      project save/reload

---

### Phase 4 — Two Persistence Contracts

**Goal:** Separate project save/load from Teletype text save/load. Text
files operate on active clip + live VM only. 4 global `PatternSlot` buffers
reduce to 2.

#### Contracts

```text
Project save/load — whole model, unchanged.
  TeletypeTrack::write()/read() serializes both clips + shared VM scripts.

Teletype text save — read-only live export, must not call captureActiveClip().
  S1-S3 + S4 + M + PATS from live VM.
  IO/TIMING/BOOT from active clip config.
  No SLOT tags. No inactive clip.

Teletype text load — active clip only.
  Parse into active clip + live VM.
  Inactive clip untouched.
  Legacy files: select slot matching active clip index, ignore other.
```

#### Write: `writeTeletypeTrack()` refactored

Current (2-slot, `FileManager.cpp:705-735`):

```text
ttSlot1/2 = patternSlotSnapshot(0/1)
writeSlotIo(0, ttSlot1); writeSlotIo(1, ttSlot2)
#S4P1, #M1, #S4P2, #M2
#S1, #S2, #S3
writePatterns(0, ttSlot1); writePatterns(1, ttSlot2)
```

New (active-only, read-only):

```text
const auto &activeClip = track.activeClipConfig()  // const ref, no copy
const scene_state_t &state = track.state()

#IO                          // active clip IO/config, no SLOT tags
#S4                          // state.scripts[SlotScriptIndex]
#M                           // state.scripts[METRO_SCRIPT]
#S1, #S2, #S3                // state.scripts[0..2]
#PATS                        // state.patterns[0..3], no SLOT tags
```

Must not call `captureActiveClip()`. No `PatternSlot` scratch buffer needed
for write (reads config by const ref).

#### Read: `readTeletypeTrack()` refactored

Text load mutates **both** active clip data (via the clip transaction) and
shared scripts S1-S3 (directly in `scene_state_t`). If parse fails after
modifying S1-S3, rolling back only `activeClipBackup` is insufficient. The
transaction must cover shared scripts too.

**Transaction struct (complete):**

```cpp
struct TeletypeFileTransaction {
    // Active clip transaction
    TeletypeTrack::PatternSlot activeClipTemp;    // parse target
    TeletypeTrack::PatternSlot activeClipBackup;  // rollback

    // Shared scripts S1-S3 transaction
    struct {
        tele_script_t scripts[3];                 // EditableScriptCount minus SlotScriptIndex
        // or: uint8_t lengths[3]; tele_command_t commands[3][ScriptLineCount];
    } sharedScriptsBackup;

    void begin(TeletypeTrack &track);   // snapshot active clip + shared S1-S3
    void commit(TeletypeTrack &track);  // write activeClipTemp -> active clip,
                                        // load VM (applies S1-S3 + S4 + M + patterns)
    void rollback(TeletypeTrack &track);// restore active clip + S1-S3 from backups,
                                        // load VM
};
```

`sharedScriptsBackup` stores the 3 shared scripts (S1, S2, S3) that text
load parses directly into `scene_state_t`. On rollback, these are restored.

Size estimate: `activeClipTemp` + `activeClipBackup` = 2 × 1226 B. Shared
scripts backup = 3 scripts × (`ScriptLineCount` × `sizeof(tele_command_t)` +
length byte). Exact size measured after implementation; expect ~300-600 B.
Total transaction: ~3.1-3.6 KB, down from 4.9 KB.

**Parse flow:**

```text
1. transaction.begin(track)    // snapshot active clip + S1-S3
2. clear S1-S3 script buffers in scene_state_t
3. clear activeClipTemp script/pattern buffers
4. parse file:
     #S1/#S2/#S3 lines → scene_state_t (shared VM scripts)
     #S4 lines          → activeClipTemp.slotScript
     #M lines           → activeClipTemp.metro
     #PATS lines        → activeClipTemp.patterns[]
     #IO lines          → activeClipTemp config fields
5. if valid:   transaction.commit(track)
6. if invalid: transaction.rollback(track)
```

Inactive clip never touched.

#### FileManager and the active clip index

`FileManager::readTeletypeTrack(TeletypeTrack &track, const char *path)`
receives only `track` and `path` — no Performer Pattern index. The plan
previously wrote `setClipForPerformerPattern(activePerformerPattern, ...)`,
but this is not implementable unless the Performer Pattern is passed in.

The clean fix: text load operates by **active clip index**, not Performer
Pattern index. Use APIs that talk in clip-index terms:

```text
commit:   track.setActiveClip(activeClipTemp) + track.loadActiveClipIntoVm()
rollback: track.setActiveClip(activeClipBackup) + track.loadActiveClipIntoVm()
```

Where `setActiveClip(const PatternSlot &clip)` writes
`_patternSlots[_activePatternSlot] = clip`. This is already the active slot;
no Performer Pattern mapping needed. If a `setClipForPerformerPattern()`
API is desired, call it from the page that has the Performer Pattern, not
from FileManager.

#### Section header mapping

```text
New format         Legacy format (detected)
----------         -----------------------
#IO                #IO + SLOT 1/SLOT 2 sub-headers
#S4                #S4P1 or #S4P2
#M                 #M1 or #M2
#S1                #S1
#S2                #S2
#S3                #S3
#PATS              #PATS + SLOT 1/SLOT 2 sub-headers
```

Legacy read: if `S4P1`/`S4P2`/`M1`/`M2`/`SLOT` sections detected, parse only
the slot matching `track.activePatternSlot()`, ignore the other. `S1`/`S2`/`S3`
always parsed into `scene_state_t`, covered by `sharedScriptsBackup` in the
transaction struct.

**Files changed:**
- `src/apps/sequencer/model/FileManager.cpp` — rewrite `writeTeletypeTrack()`
  for read-only live export; rewrite `readTeletypeTrack()` for active-clip-
  only with full transaction (active clip + shared S1-S3 backup); add legacy
  two-slot detection and selective parsing; replace 4 globals with
  `TeletypeFileTransaction`; replace `writeSlotIo()` 2-slot call with
  single-slot IO write; replace `writePatterns()` 2-slot call with
  single-slot patterns write
- `src/apps/sequencer/model/TeletypeTrack.h` — add `setActiveClip(const PatternSlot &)`
  for FileManager to write active clip by index without Performer Pattern dependency

**Hardware gate:**
- [ ] STM32 release build
- [ ] RAM: `.bss` reduced by ~2,452 B (4 → 2 `PatternSlot` globals in
      `FileManager`). Record exact `.data + .bss` and `.ccmram_bss`
- [ ] New format roundtrip: text save → text load on hardware, verify active
      clip material preserved
- [ ] Inactive clip untouched: note inactive clip state, text load, verify
      inactive clip unchanged
- [ ] Legacy format: load an old two-slot .tt file, verify active slot
      imported, other slot ignored
- [ ] Project save/load: binary project save/reload still works for both clips
- [ ] Text save does not mutate: after text save only (no text load), inspect
      active clip project state — unchanged. Then text load same file:
      active clip changes to file contents.
- [ ] Error rollback (active clip): load a corrupted .tt file, verify active
      clip restored from `activeClipBackup`, inactive clip untouched
- [ ] Error rollback (shared scripts): load a corrupted .tt file that fails
      after modifying S1-S3, verify S1-S3 restored from
      `sharedScriptsBackup`
- [ ] IO config roundtrip: text save with custom TI/TO/CV settings, reload,
      verify config preserved

---

### Phase 5 — Chunked Clips (Future, Deferred)

**Status:** Explicitly deferred until lifecycle is stable.

Sketch only:

```cpp
enum class ClipChunk : uint8_t {
    Script  = 1 << 0,   // slot script + metro script
    Pattern = 1 << 1,   // Teletype patterns[0..3]
    IO      = 1 << 2,   // input/output mappings + CV config + MIDI
    Timing  = 1 << 3,   // timebase, clock div/mult, reset-metro
    All     = 0xFF,
};
```

Enables "script swap" or "routing swap" without full scene loads.

---

## Dependency Order

```text
Phase 0 (UI/engine race fix, standalone)
  └─> Phase 1 (naming wrappers, no behavior change)
        └─> Phase 2 (remove hidden capture, rename call sites)
              └─> Phase 3 (centralize pattern-change policy)
                    └─> Phase 4 (two persistence contracts)
                          └─> Phase 5 (chunked clips — deferred)
```

Phases 0–1 can ship as separate commits.
Phase 2 can be one commit (mechanical renames + const removal).
Phase 3 needs the product decisions first.
Phase 4 depends on Phase 3's lifecycle being stable.

---

## Files Touched (All Phases)

| File | P0 | P1 | P2 | P3 | P4 |
|------|----|----|----|----|-----|
| `TeletypeScriptViewPage.cpp` | add lock/unlock ✓ | — | rename 4 calls | — | — |
| `TeletypePatternViewPage.cpp` | add lock/unlock (10 sites) ✓ | — | add `captureActiveClip()` after `setTeletypePattern()` | — | — |
| `ListPage.cpp` | add `editSelectedRow()` with lock ✓ | — | — | — | — |
| `ListPage.h` | declare `editSelectedRow()` ✓ | — | — | — | — |
| `TeletypeTrack.h` | — | +4 wrapper methods | rename, remove `const_cast`, add `clipSnapshot()` | +`switchClipForPerformerPattern()`, `clipIndexForPerformerPattern()` | +`setActiveClip()` |
| `TeletypeTrack.cpp` | — | +wrapper bodies | remove sync from `write()`/`setTeletypePattern()`/`clipSnapshot()` | +`switchClipForPerformerPattern()` body | — |
| `TeletypeTrackEngine.cpp` | — | migrate 2 calls | — | migrate `onPatternChanged()` call | — |
| `ClipBoard.cpp` | — | — | rename call + field | — | — |
| `ClipBoard.h` | — | — | rename field | — | — |
| `Track.cpp` | — | — | rename calls | rename calls | — |
| `Track.h` | — | — | declare new Pattern methods | — | — |
| `FileManager.cpp` | — | — | rename calls | — | rewrite `writeTeletypeTrack()` + `readTeletypeTrack()`; 4→2 globals; legacy read; section headers |

---

## Open Decisions

1. **Delay queue behavior (Phase 3):** Preserve (compatibility), flush on
   switch, or tag with source clip? Default: preserve + document.

2. **Auto-capture vs. explicit-capture (Phase 3):** Musical/product decision.
   Default: auto-capture (current behavior). Validate with user input.

3. **`write()` redundancy cleanup (post-Phase 4):** `write()` serializes
   both `_state.patterns` and clip payload patterns. Decide later whether
   to stop serializing `_state.patterns`. Clip patterns take precedence on
   reload.

4. **Transaction buffer further reduction (post-Phase 4):** 2→1 `PatternSlot`
   buffer if parse errors tolerated as partial model mutations with rollback.
   `sharedScriptsBackup` could also be eliminated if scripts are parsed into
   temporary buffers first and only committed to `scene_state_t` on success,
   but this requires restructuring the line-by-line parse loop or using a
   staging `scene_state_t`.

---

## RAM Baseline (record before Phase 0, refresh after each phase)

| Metric | Before P0 | After P0 | After P1 | After P2 | After P3 | After P4 |
|--------|-----------|----------|----------|----------|----------|----------|
| `.data` | | | | | | |
| `.bss` | | | | | | |
| `.data + .bss` | | | | | | |
| `.ccmram_bss` | | | | | | |
| `Track` | | | | | | |
| `TeletypeTrack` | | | | | | |
| `TeletypeTrackEngine` | | | | | | |
| FM globals | 4 × 1226 | | | | | 2 × 1226 + shared scripts backup |

**Note:** Phase 4 `.bss` savings from 4→2 `PatternSlot` globals (~2,452 B)
are partially offset by `sharedScriptsBackup` (~300-600 B). Net savings
~1.8-2.1 KB. If the transaction struct is later moved from anonymous-
namespace globals to file-task stack, `.bss` may drop further but stack
pressure increases — verify stack depth on STM32 if that change is made.

---

## Performer↔Teletype Influence Preserved

All existing influence paths unchanged:

- **Performer Pattern clip control:** `selectTrackPattern()`, `switchClipForPerformerPattern()` still control which TeletypeTrack payload is active.
- **Note-track read/write:** `noteGateGet/Set`, `noteNoteGet/Set` on `TeletypeTrackEngine`.
- **Transport/tempo/bus CV/CV-TR routing/MIDI/trigger inputs:** unchanged.
- **Clipboard:** `ClipBoard` copy/paste on clip data.
- **Project save/load:** `TeletypeTrack::write()`/`read()` serializes both clips (whole-model).
- **Teletype text file:** read-only live export of active material; load imports into active clip only. Inactive clip untouched.
