# Pattern Semantics Contract

Derived from `assumption-matrix.md` Direction 2. Answers: what does "pattern" mean per track family?

---

## Per-Track Pattern Semantics Matrix

| Track Family | Pattern Storage | Pattern Count | Pattern Semantics | Copy Semantics | Clear Semantics | Snapshot Semantics |
|---|---|---|---|---|---|---|
| **Note** | `NoteSequence` (grid of steps/gates/notes/curves) | 16 + 1 snapshot | X0X sequence grid. Each pattern is a complete independent sequence. | Value-copy (`operator=`) — copies all step data | `NoteSequence::clear()` — resets all steps to defaults | Snapshot is a full `NoteSequence` at index 16; revert/commit via value-copy |
| **Curve** | `CurveSequence` (grid of curve values per step) | 16 + 1 snapshot | Same model as Note. Each pattern is a complete independent sequence. | Value-copy (`operator=`) | `CurveSequence::clear()` | Same as Note |
| **Tuesday** | `TuesdaySequence` (algorithm + params generator state) | 16 + 1 snapshot | Generator algorithm index + parameters. No step grid, no stage map. | Value-copy (`operator=`) | `TuesdaySequence::clear()` | Same as Note/Curve |
| **DiscreteMap** | `DiscreteMapSequence` (stage-to-value mapping) | 16 + 1 snapshot | Stage-based output/value mapping table. Not a step grid, not a generator. | Value-copy (`operator=`) | `DiscreteMapSequence::clear()` | Same as Note/Curve |
| **Indexed** | `IndexedSequence` (step index + note value pairs) | 16 + 1 snapshot | Indexed step-to-note mapping. Not a grid, not a generator. | Value-copy (`operator=`) | `IndexedSequence::clear()` | Same as Note/Curve |
| **Teletype** | `PatternSlot` (×2 swap buffers) | 2 slots mapped to 16 patterns | **Not a sequence grid.** Each slot holds: scripts (slotScript + metro), patterns (`scene_pattern_t` ×4), I/O mapping, timing config. | **Slot-level copy** (`copyPatternSlot(src, dst)`) — copies entire `PatternSlot` including scripts and patterns | **Slot-level clear** (`clearPatternSlot`) — resets slot to default scripts + empty patterns | **BUG-RISK:** Snapshot index 16 aliases PatternSlot 1 (`clamp(16,0,15) % 2 = 1`). `createSnapshot()` copies current slot to slot 1. `commitSnapshot()` copies slot 1 to target pattern. This silently overwrites and restores slot 1, corrupting real data. |
| **MidiCv** | None | N/A | No pattern storage. Pattern switching is no-op. | N/A | N/A | N/A |

---

## Copy / Paste / Clear / Snapshot Behavior Rules

### 1. Per-Pattern Operations (Track::clearPattern / Track::copyPattern)

```
Note/Curve/Tuesday/DiscreteMap/Indexed:
    clearPattern(index)  → sequence(index).clear()
    copyPattern(src, dst)  → sequence(dst) = sequence(src)   [value-copy]

Teletype:
    clearPattern(index)  → clearPatternSlot(index % 2)
    copyPattern(src, dst) → copyPatternSlot(src % 2, dst % 2)   [slot-level copy]

MidiCv:
    clearPattern(index)  → no-op
    copyPattern(src, dst) → no-op
```

**Rule:** All grid-based families (Note/Curve/Tuesday/DiscreteMap/Indexed) use **value-copy at the sequence level**. Teletype uses **slot-level copy** because its pattern is not a grid — it is a VM configuration bundle.

### 2. ClipBoard Operations

#### ClipBoard::copyPattern (cross-track pattern copy)

```
Loop over all 8 tracks:
    Read track's current pattern at `patternIndex`
    Switch on track mode:
        Note:        copy note sequence (value-copy)
        Curve:       copy curve sequence (value-copy)
        Tuesday:     copy tuesday sequence (value-copy)
        DiscreteMap: copy discrete map sequence (value-copy)
        Indexed:     copy indexed sequence (value-copy)
        Teletype:    snapshot PatternSlot via patternSlotSnapshot()  [slot-level]
```

**Contract:** `ClipBoard::Pattern` stores a `Track::TrackMode` + tagged union per track. The union holds the largest sequence type (`NoteSequence`) but the tag discriminates. For Teletype, it holds a `PatternSlot`.

#### ClipBoard::pastePattern (cross-track pattern paste)

```
Loop over all 8 tracks:
    Only paste if target track mode == source track mode in clipboard
    Switch on track mode:
        Note:        track.noteTrack().sequence(patternIndex) = clipboard.data.note
        Curve:       track.curveTrack().sequence(patternIndex) = clipboard.data.curve
        Tuesday:     track.tuesdayTrack().sequence(patternIndex) = clipboard.data.tuesday
        DiscreteMap: track.discreteMapTrack().sequence(patternIndex) = clipboard.data.discreteMap
        Indexed:     track.indexedTrack().sequence(patternIndex) = clipboard.data.indexed
        Teletype:    track.teletypeTrack().setPatternSlotForPattern(patternIndex, clipboard.data.teletype)
```

**Rule:** Paste is **mode-gated**. A Note pattern cannot be pasted into a Teletype track. This is correct — the semantics are incompatible.

### 3. Per-Sequence Copy/Paste (step-level)

| Source | Destination | Behavior |
|---|---|---|
| NoteSequence (full) | NoteSequence | Value-copy all steps |
| NoteSequenceSteps (selected) | NoteSequenceSteps (selected) | `ModelUtils::copySteps` — aligns selected source steps into selected destination steps |
| CurveSequence (full) | CurveSequence | Value-copy all steps |
| CurveSequenceSteps (selected) | CurveSequenceSteps (selected) | `ModelUtils::copySteps` |
| IndexedSequence (full) | IndexedSequence | Value-copy |
| IndexedSequenceSteps (selected) | IndexedSequenceSteps (selected) | Manual per-step copy (no `steps()` accessor for `ModelUtils::copySteps`) |
| DiscreteMapSequence (full) | DiscreteMapSequence | Value-copy |
| TuesdaySequence (full) | TuesdaySequence | Value-copy |

**Gap:** `IndexedSequence` lacks a public `steps()` accessor, so step-level paste is implemented with a manual loop rather than `ModelUtils::copySteps`. This is a minor inconsistency.

### 4. Snapshot Mechanism

```
PlayState::createSnapshot():
    For each track:
        Save current pattern index to _snapshot.lastTrackPatternIndex[track]
    Save global selected pattern to _snapshot.lastSelectedPatternIndex
    Mark _snapshot.active = true

PlayState::revertSnapshot(targetPattern = -1):
    For each track:
        Restore pattern from _snapshot.lastTrackPatternIndex[track]
    If targetPattern != -1:
        Also copy snapshot pattern (index 16) into targetPattern for all tracks

PlayState::commitSnapshot(targetPattern = -1):
    If targetPattern != -1:
        Copy snapshot pattern (index 16) into targetPattern for all tracks
    Clear _snapshot.active
```

**Rule:** Snapshot is a **global operation** across all tracks. It copies the current pattern data into the snapshot index (16), then switches all tracks to pattern 16. Revert restores the original pattern indices. Commit copies snapshot data back to target patterns.

**Source:** `PlayState::createSnapshot()` calls `Track::copyPattern(trackPatternIndex, SnapshotPatternIndex)` for each track. This is a **full data copy**, not just index capture. Index capture happens in `_snapshot.lastTrackPatternIndex[]`.

---

## What Should Be Uniform in UI

| Operation | Uniform Behavior | Type-Specific Detail |
|---|---|---|
| Pattern switch | Immediate / Synced / Latched request semantics | None — all families use `PlayState::selectTrackPattern()` |
| Pattern copy (full) | Copy current pattern to clipboard | Teletype copies `PatternSlot`; others copy sequence |
| Pattern paste (full) | Paste clipboard into current pattern | Mode-gated: only paste if track mode matches clipboard |
| Pattern clear | Reset current pattern to defaults | Teletype clears slot; others clear sequence |
| Snapshot create | Save current pattern indices globally | All families participate |
| Snapshot revert | Restore saved pattern indices | All families participate |
| Snapshot commit | Copy snapshot pattern into target | Note/Curve/Tuesday/DiscreteMap/Indexed copy sequence 16; Teletype no-op |

---

## What Must Stay Type-Specific

| Aspect | Why Type-Specific |
|---|---|
| Pattern data structure | `NoteSequence` has 64 steps with gates/notes/curves; `PatternSlot` has scripts + VM patterns + I/O mapping. Fundamentally different. |
| Copy/paste internal implementation | Teletype uses slot-level copy; others use sequence value-copy. |
| Clear implementation | Teletype clears slot scripts + metro + patterns; others clear step arrays. |
| Edit UI | Note/Curve show step grid; Teletype shows script text; Tuesday shows algorithm parameters. |
| Pattern count | Note/Curve/Tuesday/DiscreteMap/Indexed have 16+1; Teletype has 2 slots; MidiCv has 0. |
| Snapshot data | Note/Curve/Tuesday/DiscreteMap/Indexed have full sequence at index 16; Teletype has no snapshot sequence. |

---

## Contract Violations

1. **UI assumes all families have 16 patterns.** Teletype has 2 slots. The UI maps pattern index 0-15 to `patternIndex % 2`. This is an implicit contract that should be explicit.

2. **Snapshot index 16 is universal but not universal.** Note/Curve/Tuesday/DiscreteMap/Indexed use it. Teletype does not. If `CONFIG_SNAPSHOT_COUNT` ever changes, Teletype remains fixed at 2 slots.

3. **ClipBoard Pattern union uses a tagged union with `NoteSequence` as one member.** The struct declares a union of all sequence types plus `TeletypeTrack::PatternSlot`, with `Track::TrackMode` as the discriminator. **Source-verify needed:** confirm that the `Container` usage in `ClipBoard` correctly handles the `PatternSlot` size before calling any architectural inconsistency claim.

4. **IndexedSequence step-level paste has no `steps()` accessor.** Uses manual loop instead of `ModelUtils::copySteps`. Minor maintenance debt.

---

## Where Findings Go

- Pattern count / snapshot semantics divergence → architecture research (Direction 2)
- ClipBoard union size / `Container` usage → `ram-recovery-experiments.md` (only if redesigning ClipBoard)
- IndexedSequence `steps()` accessor gap → code cleanup, not architecture
