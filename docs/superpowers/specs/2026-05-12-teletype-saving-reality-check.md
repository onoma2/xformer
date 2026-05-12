# Teletype File Saving: Actual Problems & Minimal Fixes

**Date**: 2026-05-12
**Source**: Graphify analysis of 17,340-node codebase graph + source audit
**Status**: Replaces `2026-05-12-teletype-shadow-binary-design.md` — that spec solves imaginary problems and introduces unbuildable features. This document solves real ones.

---

## What's Actually Wrong

Three problems. Not twelve. Not a new file format. Three.

### Problem 1: Legacy Duplication in Binary Serialization (~46 bytes wasted)

**Where**: `TeletypeTrack::write()` lines 188-215, `TeletypeTrack::read()` lines 273-328

`write()` serializes the active slot's I/O mapping as a standalone block (lines 188-215), then serializes **the same data again** inside the per-PatternSlot loop (lines 228-267). On read, the legacy block is read into `_patternSlots[0]`, then immediately overwritten by the slot loop.

**Impact**: ~46 wasted bytes per save. Not a crash risk. Not corruption. Just mess.
**Risk of removing it**: Old `.PRO` files that predate the PatternSlot system would break. But the existing plan (`docs/plans/2026-05-12-teletype-save-load-fixes.md`) already proposed this removal and it's unchecked. So the backward-compat concern has already been accepted.

**Fix**: Remove the legacy I/O block from `write()`. Remove the legacy read from `read()`. This is exactly what the existing plan already specifies — just do it.

### Problem 2: `readTeletypeTrack` Silently Drops Bad Lines

**Where**: `FileManager::readTeletypeTrack()` lines 1002-1012, 1021-1031

When parsing a `.TXT` track file, if `parse()` or `validate()` fails on any script line, the line is silently `continue`d. No error flag. No count. The UI shows "TRACK LOADED" regardless. A typo in the text file causes invisible data loss — lines shift up, scripts silently shrink.

**Contrast**: `readTeletypeScript()` (line 471) does the same `continue` but tracks `success = false` and returns `fs::INVALID_CHECKSUM`. At least the user sees "INVALID SCRIPT FILE".

**Fix**: Add the same `success` tracking to `readTeletypeTrack`. Return `fs::INVALID_DATA` (not `INVALID_CHECKSUM` — wrong error code for this) if any line failed to parse. The UI already has a handler for `INVALID_CHECKSUM` showing "INVALID TRACK FILE" — wire `INVALID_DATA` to the same message.

Additionally: insert a blank line at the failed position to preserve script geometry, instead of silently shifting everything up.

### Problem 3: No Rollback on Failed Text Load

**Where**: `FileManager::readTeletypeTrack()` line 735, UI callbacks in `TeletypePatternViewPage.cpp:388`

`readTeletypeTrack` starts by calling `track.clear()` (line 735). If the load then fails (bad file, parse errors, truncated file), the track is already wiped. No rollback. The user's working track is destroyed.

`readTeletypeScript` does **not** clear the track first — it only clears the specific script index. So it's less destructive but still overwrites the script on partial success.

**Fix**: Snapshot the track state before loading. On failure, restore from snapshot. The `ttSlot1`/`ttSlot2` buffers already exist for this purpose — copy the current track's slots there before `clear()`, and restore on error.

---

## What's NOT Wrong (Spec Claims Debunked)

| Spec Claim | Reality |
|---|---|
| "Massive Teletype VM state corrupts `.PRO` stream" | Total TT payload is ~4.5KB out of a much larger project file. The `VersionedSerializedWriter` has FNV hash verification that catches corruption. No evidence of stream corruption in git history. |
| "Stack overflows in `VersionedSerializedWriter`" | The shared `ttSlot1`/`ttSlot2`/`ttLineBuffer` globals explicitly prevent this (comment: "avoid large stack usage in file task"). No stack overflow reported. |
| "RTOS race conditions on shared globals" | The task model is cooperative single-threaded (`_taskPending` flag, no preemption). No race is possible — only one file operation runs at a time. |
| "Need a Shadow Binary `.T9B` file" | The binary serialization works. The FNV hash catches corruption. A parallel file format adds complexity (missing-file handling, version mismatch, two files to manage) for zero benefit. |
| "`scene_state_t` as raw binary dump" | `scene_state_t` contains pointers and VM state that can't be safely raw-dumped. The current structured serialization is correct. |
| "Mutex needed for `ttSlot1`/`ttSlot2`" | No concurrency exists. The single-task model makes this unnecessary. |

---

## Proposed Plan (3 Phases, Hardware Checks at Each Gate)

Each phase ends with: **sim compile → hardware flash → smoke test → commit**.

### Phase 0: Streamline Binary Write/Read (Existing Plan)

**This is already written** in `docs/plans/2026-05-12-teletype-save-load-fixes.md`.

- Remove legacy I/O block from `TeletypeTrack::write()`
- Remove legacy read from `TeletypeTrack::read()`
- Compile sim, verify existing projects still load (backward compat accepted)

**Hardware check**: Flash to STM32. Load a project with a Teletype track. Verify scripts, patterns, and I/O mappings are intact. Save and reload — verify round-trip.

### Phase 1: Fix Silent Parse Failures in Text Loader

**Files**:
- Modify: `src/apps/sequencer/model/FileManager.cpp` — `readTeletypeTrack()`

**Changes**:
1. Add `bool success = true` tracking (like `readTeletypeScript` already does)
2. On `parse()` or `validate()` failure: set `success = false`, insert blank line to preserve geometry
3. After the parse loop: if `!success`, return `fs::INVALID_DATA`
4. In `TeletypePatternViewPage.cpp`: handle `fs::INVALID_DATA` → show "INVALID TRACK FILE"

**Hardware check**: Flash to STM32. Create a `.TXT` track file with a deliberate typo. LOAD TT — verify error message appears. Load a valid file — verify "TRACK LOADED".

### Phase 2: Add Rollback on Failed Text Load

**Files**:
- Modify: `src/apps/sequencer/model/FileManager.cpp` — `readTeletypeTrack()`

**Changes**:
1. Before `clear()`: snapshot current track state (copy `_patternSlots` + `_activePatternSlot`)
2. On parse failure: restore from snapshot instead of leaving track wiped
3. Same for `readTeletypeScript`: snapshot the single script before clearing, restore on failure

**Hardware check**: Flash to STM32. Set up a track with known scripts. Attempt to LOAD TT from a corrupt file. Verify original scripts are intact after the error message. Load a valid file — verify it replaces correctly.

---

## What About the `#S4P1`/`#S4P2` Labels?

The spec says rename `#S4P1` to `#SLOT_A_SCRIPT`. This is a valid UX concern but:
- It's a **breaking change** for every existing `.TXT` file users have saved
- The reader already supports aliases (`#S1P1` for `#S4P1`)
- It doesn't affect correctness

**Recommendation**: Defer. If we must do it, add new names as write-format and keep old names as read-aliases. Not worth a phase on its own.

---

## Token Budget

| Phase | Files Changed | Risk | Estimated Effort |
|-------|---------------|------|------------------|
| Phase 0 | 1 (`TeletypeTrack.cpp`) | Low — existing plan with code provided | Small |
| Phase 1 | 2 (`FileManager.cpp`, `TeletypePatternViewPage.cpp`) | Low — additive error tracking | Small |
| Phase 2 | 1 (`FileManager.cpp`) | Medium — snapshot/restore logic | Medium |

Total: ~4 files, no new files, no new formats, no new architecture.
