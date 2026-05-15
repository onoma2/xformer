# Teletype Slot/Pattern/VM Synchronization Contract

---

## Data Layout

Three storage layers, data moves between them:

```
Layer 1: _patternSlots[2]        (PatternSlot structs, in TeletypeTrack model)
Layer 2: _state                  (scene_state_t VM, in TeletypeTrack model)
Layer 3: TeletypeTrackEngine      (runtime synthesis state: metro timer, LFOs,
                                   envelopes, Geode, pulses, CV slew, etc.)
```

Current ARM size anchors:
- `PatternSlot = 1226 B`
- `scene_state_t = 4640 B`
- `TeletypeTrack = 7104 B`
- `TeletypeTrackEngine = 912 B`

This confirms a PatternSlot is not a full `scene_state_t` copy. It is a partial
slot/shadow record around scripts, metro, patterns, and routing config, while
`_state` remains the full live VM state.

Each `PatternSlot` stores a **shadow copy** of the VM-meaningful fields for one
slot:
- `slotScript[6]` (maps to VM script index 3)
- `metro[6]` (maps to VM script index METRO_SCRIPT = 4)
- `patterns[4]`
- `resetMetroOnLoad`

Each `PatternSlot` also stores **routing-only** config that never enters the VM:
I/O mappings, timing config, CV output range/offset/quantize, MIDI source,
boot script index.

The VM (`_state`) is the **live authoritative copy** during execution.
`_patternSlots` are the **persistent storage.**
Data flow:
- Slot → VM on `applyPatternSlot()`
- VM → Slot on `syncToActiveSlot()`

`_activePatternSlot` (0 or 1) determines which slot is "live."

### Script Numbering (TELETYPE_TRACK_LITE = 1)

| Index | Name | Scope |
|-------|------|-------|
| 0-2   | S1-S3 | Shared across both slots |
| 3     | SlotScript | Per-slot (copy of whichever slot is active) |
| 4     | METRO_SCRIPT | Per-slot (metro commands) |
| 5     | DELAY_SCRIPT | Internal (delay queue execution) |
| 6     | TOTAL_SCRIPT_COUNT | End sentinel |

---

## The Two Synchronization Functions

### `applyPatternSlot(int slotIndex)` — TeletypeTrack.cpp:387-401

Copies the selected slot's scripts and patterns INTO the VM:

```
_state.scripts[3].l          = slot.slotScriptLength
_state.scripts[3].c          = slot.slotScript (memcpy)
_state.scripts[METRO_SCRIPT].l = slot.metroLength
_state.scripts[METRO_SCRIPT].c = slot.metro (memcpy)
_state.patterns[0..3]        = slot.patterns[0..3]
_resetMetroOnLoad             = slot.resetMetroOnLoad
_activePatternSlot            = slotIndex
```

### `syncToActiveSlot()` — TeletypeTrack.cpp:407-418

Copies the VM's current scripts and patterns back INTO the active slot:

```
slot.slotScriptLength  = _state.scripts[3].l
slot.slotScript        = _state.scripts[3].c (memcpy)
slot.metroLength       = _state.scripts[METRO_SCRIPT].l
slot.metro             = _state.scripts[METRO_SCRIPT].c (memcpy)
slot.patterns[0..3]    = _state.patterns[0..3]
slot.resetMetroOnLoad  = _resetMetroOnLoad
```

---

## Call Graph — All Call Sites

### UI Thread → Model

| File:Line | Call | When |
|-----------|------|------|
| `TeletypeTrackListModel.h:249` | `syncToActiveSlot()` | Track list operations |
| `TeletypeScriptViewPage.cpp:830` | `syncToActiveSlot()` | After editing slot script (script 3) or metro script |
| `TeletypeScriptViewPage.cpp:867` | `syncToActiveSlot()` | After duplicating line in slot/metro script |
| `TeletypeScriptViewPage.cpp:879` | `syncToActiveSlot()` | After toggling comment in slot/metro script |
| `TeletypeScriptViewPage.cpp:898` | `syncToActiveSlot()` | After deleting line in slot/metro script |
| `ClipBoard.cpp:91` | `patternSlotSnapshot(p)` | Copying a pattern (read path, has const_cast) |
| `Track.cpp:36` | `clearPatternSlot(p)` | Clearing a pattern (calls sync+apply if active slot) |
| `Track.cpp:63` | `copyPatternSlot(s,d)` | Copying pattern between slots (calls sync+apply) |
| `ClipBoard.cpp:250` | `setPatternSlotForPattern(p,s)` | Pasting a pattern (calls sync+apply if active slot) |

### UI Thread → File I/O

| File:Line | Call | When |
|-----------|------|------|
| `FileManager.cpp:715-716` | `patternSlotSnapshot(0..1)` | Export — captures slot state to global buffer |
| `FileManager.cpp:755-756` | `patternSlotSnapshot(0..1)` | Import backup — snapshots state before clear |
| `FileManager.cpp:766-767` | `patternSlotSnapshot(0..1)` | Import — captures cleared slot state |
| `FileManager.cpp:1122-1124` | `setPatternSlotForPattern()` + `applyActivePatternSlot()` | Import commit — writes parsed data back |
| `FileManager.cpp:1127-1129` | `setPatternSlotForPattern()` + `applyActivePatternSlot()` | Import rollback — restores from backup |

### Engine Thread → Model

| File:Line | Call | When |
|-----------|------|------|
| `TeletypeTrackEngine.cpp:92` | `applyPatternSlot(...)` | Engine `reset()` — after ss_init() |
| `TeletypeTrackEngine.cpp:207` | `onPatternChanged(currentPattern)` | `update()` every frame — if pattern != cached |
| `TeletypeTrackEngine.cpp:1417` | `applyActivePatternSlot()` | `loadScriptsFromModel()` — during boot |
| `TeletypeTrack.cpp:183` | `syncToActiveSlot()` | `write()` — serialize (via const_cast) |
| `TeletypeTrack.h:627` | `syncToActiveSlot()` | `setPattern()` — model mutation (via const_cast) |

### Engine Thread — pattern change detection flow

```
update() every frame:
  if currentPattern != _cachedPattern:
    _teletypeTrack.onPatternChanged(currentPattern):
      slot = patternSlotForPattern(currentPattern)
      if slot == _activePatternSlot: return (no-op)
      syncToActiveSlot()       // save old slot's state from VM
      applyPatternSlot(slot)   // load new slot's state into VM
    _cachedPattern = currentPattern
```

This runs BEFORE the rest of `update()` (script execution, metro, CV updates) on
the same frame. So pattern changes are effective for the same frame in which
they are detected.

---

## Invariants

1. **Slot-stored state is stale after script execution.**
   Any script that modifies patterns (`P 0 0`, `P.N 0`, `P.I`, turtle ops),
   slot-bound scripts (index 3), or the metro script (index 4) mutates the
   VM only. Until `syncToActiveSlot()` is called, the PatternSlot copy is
   stale. This means: if a slot-bound script runs, and a pattern change
   switches to the other slot, then back, the modifications are lost unless
   something called `syncToActiveSlot()` before switching away.

2. **Only scripts 3 and 4 are per-slot.**
   Scripts 0, 1, 2 are shared across both slots. `applyPatternSlot()` and
   `syncToActiveSlot()` do NOT touch them. Slot switching only swaps
   script 3 content, script 4 content, and patterns.

3. **Engine runtime state is not synced.**
   Metro timer phase (`_metroRemainingMs`), LFO phase, envelope state,
   Geode state, pulse timers, CV slew values — live in
   `TeletypeTrackEngine` and are NEVER synced to the model. A slot switch
   does not preserve these. The engine's metro timer derives from
   `_state.variables.m` via `syncMetroFromState()` but the remaining-time
   counter is engine-local and reset on pattern change only if the new
   slot's `resetMetroOnLoad` flag is set (checked during
   `runBootScriptNow()`).

4. **`resetMetroOnLoad` is stored in three places.**
   - `PatternSlot::resetMetroOnLoad` (model — persistent)
   - `TeletypeTrack::_resetMetroOnLoad` (model — transient, copied from
     slot during `applyPatternSlot()`)
   - Checked during `runBootScriptNow()` via
     `consumeMetroResetOnLoad()` which clears the flag after consumption

5. **Engine reset preserves scripts 0-2 across `ss_init()`.**
   `TeletypeTrackEngine::reset()` saves scripts 0-2 to stack locals,
   calls `ss_init()` (zeroes everything), restores scripts 0-2, then
   calls `applyPatternSlot()` to restore slot-bound state. This is the
   only path where scripts 0-2 survive a VM reinit.

---

## Fragile Points

### Fragile 1: `const_cast` in read paths

`syncToActiveSlot()` mutates slot storage but is called from conceptually
read-only operations:

- `patternSlotSnapshot()` — TeletypeTrack.cpp:331-332 uses `const_cast` to
  call `syncToActiveSlot()` on a `const` track
- `write()` — TeletypeTrack.cpp:182-183 uses `const_cast` to call
  `syncToActiveSlot()` during serialization
- `setPattern()` — TeletypeTrack.h:627 calls `syncToActiveSlot()` then
  writes `_state.patterns[clamped]` — both are mutations hidden inside a
  `const`-by-reference getter pattern

The `const_cast` is a sign that the sync direction is wrong for these call
sites. The caller expects a read but gets a write.

### Fragile 2: UI/engine thread concurrency

`TeletypeScriptViewPage.cpp:830,867,879,898` — UI thread calls
`track.syncToActiveSlot()` directly on the model. If the engine thread is
concurrently executing `onPatternChanged()` (which also calls
`syncToActiveSlot()` then `applyPatternSlot()`), there is no mutual
exclusion.

Scenario:

```
Time | Engine thread                     | UI thread
-----+-----------------------------------+--------------------------
T1   | update(): onPatternChanged(0→1)   |
T2   | syncToActiveSlot() saves slot 0   |
T3   |                                   | syncToActiveSlot() writes
     |                                   |   slot 1's state from VM
T4   | applyPatternSlot(1) overwrites    |
     |   slot 1's state from PatternSlot |
```

At T4, the UI thread's edit (saved at T3 into slot 1) is overwritten by
`applyPatternSlot(1)` which copies from the same slot's PatternSlot
storage — which still has the old state because `syncToActiveSlot()` at T2
saved slot 0 (not slot 1).

Per the `Engine` lock/suspend protocol, the UI should call
`engine.lock()` before model mutations. The Teletype script view page
may or may not hold this lock.

### Fragile 3: Pattern change within frame execution

`onPatternChanged()` runs in `update()` before `advanceTime()` /
`runMetro()` but AFTER `TeletypeTrackEngine::reset()` boot path. On a
given frame:

1. `update()` is entered
2. `onPatternChanged()` fires → syncs old slot, applies new slot
3. `advanceTime()` → `tele_tick()` → processes delays (which may reference
   the *old* script via `DELAY_SCRIPT`)
4. `updateInputTriggers()` → triggers scripts using current VM state
   (now the new slot's scripts)
5. `runMetro()` → fires metro script using current VM state
   (now the new slot's metro)

If the old slot had delayed commands queued, they will fire step 3 using
whatever VM state exists after the slot switch. The delayed command's
`origin_script` field references the old slot context, but the variables
and patterns have already been replaced.

### Fragile 4: Slot index 3 collision with manual scripts

`SlotScriptIndex = 3`. Manual script trigger (`triggerManualScript()`,
`kManualScriptCount = 4`) cycles scripts 0-3. This means manual trigger
script 3 fires whatever slot-bound script is currently loaded. If the
user changes slots and triggers script 3 manually, they get the new
slot's script without necessarily intending to.

### Fragile 5: Boot script index is slot-scoped but timing is ambiguous

`bootScriptIndex()` reads from `activeSlot().bootScriptIndex`. The boot
script runs in `runBootScriptNow()`, called from:
- `consumeBootScriptRequest()` (triggered by UI setting)
- Engine reset path (`_bootScriptPending → tick() → runBootScript()`)
- Pattern-dependent `onPatternChanged()` does not re-trigger boot

If a user changes patterns (and thus slots) between requesting boot and
the boot executing, the wrong script runs.

### Fragile 6: File I/O rollback uses global buffers

`FileManager.cpp:628-635`:

```cpp
namespace {
    TeletypeTrack::PatternSlot ttSlot1;
    TeletypeTrack::PatternSlot ttSlot2;
    TeletypeTrack::PatternSlot ttSlot1Backup;
    TeletypeTrack::PatternSlot ttSlot2Backup;
    char ttLineBuffer[256];
}
```

These are namespace-scope globals used by the file task. If two file
operations interleave (e.g., SD card operations enqueued rapidly), they
corrupt each other's backups. The file manager uses a single-shot
task mechanism (`_taskPending` flag at FileManager.cpp:1259), so this
should not happen in practice, but the buffer lifetime is tied to
`FileManager` static state, not the task itself.

### Fragile 7: Metro period contamination in Clock mode

In Clock timebase mode, `runMetro()` computes a derived metro period
from BPM, clock divisor, and clock multiplier, then writes it back into
`_state.variables.m` (`TeletypeTrackEngine.cpp:1141`):

```cpp
state.variables.m = derived;
```

On the next slot switch, `applyPatternSlot()` does NOT restore `m` from
the PatternSlot (since `m` is not stored in PatternSlot — it is a
runtime variable). So the derived value from Clock-mode computation
persists across a slot switch, even if the new slot is in Ms timebase
mode. The metro period continues at the derived value until `runMetro()`
recomputes it for the new mode.

---

## What Would It Take to Eliminate the Sync Dance

The root cause: the VM is the execution engine but `PatternSlot` is the
storage. They are two representations of the same state, with no
automatic write-through.

### Approach A: Slots are Teletype clips

This is the preferred separation path.

The current slot feature is musically valid, but the name and contract are
wrong. Slot-owned scripts/patterns/routing are not full Teletype scenes and
not independent VMs. They are Performer-owned **Teletype clips**: saved
Teletype material that can be loaded into the live VM and captured back at
explicit boundaries.

Contract:

- `scene_state_t` is the live runtime truth while Teletype executes.
- `PatternSlot` becomes, conceptually and eventually by name, `TeletypeClip`.
- `applyPatternSlot()` becomes `loadClipIntoVm()`.
- `syncToActiveSlot()` becomes `captureVmToClip()` / `captureActiveClip()`.
- Clip -> VM happens only on explicit clip load.
- VM -> Clip happens only on explicit capture/save boundaries.
- Read-only paths must not mutate clip storage.

First implementation slice:

- Add wrapper methods with the new names before doing a large rename.
- Migrate direct `syncToActiveSlot()` callers to explicit capture calls.
- Remove hidden capture from `TeletypeTrack::write()` and
  `patternSlotSnapshot()`.
- Make pattern-change behavior explicit: either load-only, or named
  auto-capture-on-leave followed by load.
- Defer file import/export backup consolidation until this ownership boundary
  is clear.

UI wording:

- Do not present slots as full scenes.
- Present them as clips: `PATTERN 1 -> CLIP 1`,
  `PATTERN 2 -> CLIP 2`.
- If live VM edits are captured into a clip, expose that as a
  save/capture/discard boundary rather than invisible sync.

### Approach B: VM-is-truth, slots are write-only storage

`applyPatternSlot()` is the only sync direction (slot → VM on load).
Never call `syncToActiveSlot()`.

- `patternSlotSnapshot()` reads from VM directly instead of slot
- `write()` serializes from VM directly
- UI edits scripts on the VM directly (requires lock if engine running)
- Slots are written only during project save (copy VM → slot)
- Eliminates all `const_cast` uses
- Cost: clipboard copy/paste must reads from VM, not slot. The
  `ClipBoard.cpp:91` `patternSlotSnapshot()` becomes `VM → clipboard`
  instead of `slot → clipboard`.

### Approach C: Slots-are-truth, VM is ephemeral snapshot

`syncToActiveSlot()` runs after every script execution. The VM is
always freshly loaded from the active slot before execution.

- No stale slot state possible
- Adds overhead: every script run needs `applyPatternSlot()` before
  and `syncToActiveSlot()` after
- Changes to scripts 0-2 (shared) need special handling since they
  are not per-slot

### Approach D: Runtime owns the VM (Architecture C natural fit)

The runtime/engine holds the VM. Slots are a serialization boundary.

- On pattern change: serialize current VM → slot buffer, deserialize
  new slot → VM
- `syncToActiveSlot()` constrained to pattern-change boundaries only
- In the per-frame hot path the VM is the sole authoritative copy
- UI edits go through the runtime (which can enforce locking)
- The PatternSlot struct becomes a "document format" not a "live
  mirror"
