# Teletype Runtime Architecture

## Goal

Clarify Teletype ownership before any global runtime or RAM-motivated engine extraction work. The current system mixes a per-track Teletype VM with a Performer slot layer that copies VM state in and out of `_patternSlots[2]`. That dual-truth design is the main source of fragility. Do not implement a global Teletype VM or TeletypeRuntime extraction until the slot/VM ownership contract is explicit.

## Key files

- `docs/slots-teletype.md` — slot/pattern/VM synchronization contract and fragile points.
- `docs/global-teletype.md` — per-track VM vs global VM analysis; useful verdict, but RAM section needs correction under current `Track::_container` gates.
- `.tasks/teletype-runtime-architecture/clip-architecture-spec.md` — proposed slots-as-clips architecture where clips are TeletypeTrack pattern payloads, plus chunk model, lifecycle policy, and implementation slices.
- `src/apps/sequencer/model/TeletypeTrack.h` — persistent Teletype track data, `_state`, `_patternSlots`, slot accessors.
- `src/apps/sequencer/model/TeletypeTrack.cpp` — `applyPatternSlot()`, `syncToActiveSlot()`, `patternSlotSnapshot()`, `setPatternSlotForPattern()`, `onPatternChanged()`.
- `src/apps/sequencer/engine/TeletypeTrackEngine.h` — runtime adapter/state for scripts, CV/TR, metro, LFO/env/Geode, pulses, interpolation.
- `src/apps/sequencer/engine/TeletypeTrackEngine.cpp` — script execution, bridge scope, pattern-change handling, metro, delay/tick behavior.
- `src/apps/sequencer/engine/TeletypeBridge.h`
- `src/apps/sequencer/engine/TeletypeBridge.cpp` — current global active-engine callout bridge.
- `src/apps/sequencer/model/FileManager.cpp` — Teletype import/export globals and rollback behavior.

## Decision order

1. **Document/fix slot ownership first.**
   Decide whether the VM is truth or slots are truth. Current dual-truth sync is the real bug farm: `PatternSlot` is persistent storage, `_state` is live VM state, and `syncToActiveSlot()` / `applyPatternSlot()` copy mutable scripts/patterns between them.

2. **Make Teletype file import/export transaction-local.**
   This attacks the rollback globals and clarifies persistence semantics. The current file path snapshots both slots, clears/loads both slots, and restores both on failure. Any backup consolidation must preserve an explicit transaction boundary.

3. **Only then extract TeletypeRuntime.**
   A global scheduler/runtime with per-track contexts is reasonable, but it should follow a cleaner VM/slot boundary. Do not build a global runtime proof-of-concept while slot ownership is still ambiguous.

## Current verdict

- **One global Teletype VM is unsafe under current semantics.** Variables, patterns, delay queue, stack, random state, metro, turtle, and script execution are all mutable VM state with no partitioning.
- **Global TeletypeRuntime with per-track contexts is plausible.** It can centralize scheduling, bridge/I/O dispatch, and all-track commands while keeping one VM context per active Teletype track.
- **Do not sell global VM as a model RAM fix.** Current ARM probes: `TeletypeTrack=7104`, `NoteTrack=9544`, `Track=9560`. Removing Teletype model state does not automatically recover top-level model RAM because `Track::_container` is currently sized by `NoteTrack`.
- **Engine-side Teletype remains the measurable container target.** Current ARM probes: `TeletypeTrackEngine=912`, `Engine::TrackEngineContainer=912`, `NoteTrackEngine=588`. Shrinking or extracting TeletypeTrackEngine has a conditional direct gap of `(912 - 588) * 8 = 2,592 B` CCMRAM, but this should follow the slot/VM ownership work.
- **PatternSlot is partial storage, not a full VM scene.** Current ARM probes: `PatternSlot=1226`, `scene_state_t=4640`. Slot semantics must be defined before runtime extraction.
- **`bootScriptIndex` is clip-scoped permanently.** Storage (`TeletypeTrack.h:630-633`) reads/writes through `activeSlot()`, file IO stores `BOOT` inside each `SLOT` block. Moving it top-level is a breaking file-format change with no current justification.
- **Clip mapping is `% 2` — two clips, not per-Pattern.** `performerPatternIndex % PatternSlotCount` means Patterns 1 & 3 share Clip 0, Patterns 2 & 4 share Clip 1. Switching between patterns that share a clip is a no-op. Switching between different clips triggers capture + load.
- **`captureActiveClip()` is the only VM-to-clip primitive.** `captureVmToClip(int clipIndex)` does not exist — the VM represents the active clip only. Copying live VM into an arbitrary clip index is semantically wrong and dangerous.
- **UI/engine race on VM capture is fixed.** `TeletypeScriptViewPage`, `TeletypePatternViewPage`, and Teletype list edits now lock/suspend the engine around live VM/config mutations before capture.
- **Pattern Init stack hazard is fixed.** Phase 3 briefly exposed that `clearClipForPerformerPattern()` must not allocate a full `scene_state_t` on the UI stack; Pattern defaults are now reset directly inside the clip.
- **Two persistence contracts are cleaner than one.** Project save/load = whole model. Teletype text save/load = active clip + live VM only. Text load must not touch the inactive clip. This reduces text I/O buffers from 4 → 2 PatternSlots (~2,452 B .bss saved) and eliminates the two-slot backup/rollback complexity.
- **Approach A is now "slots are TeletypeTrack pattern clips."** Keep the feature, but stop treating slots as scenes or live VM mirrors. The live `scene_state_t` is runtime truth; a slot/clip is the TeletypeTrack pattern payload selected by Performer pattern state and explicitly loaded into/captured from the VM at defined boundaries.

## Approach A — Slots are TeletypeTrack pattern clips

This is the preferred slot/VM separation path to plan first.

**Contract:**

- `scene_state_t` is the live runtime truth while Teletype executes.
- `PatternSlot` should be renamed/conceptually treated as the TeletypeTrack pattern payload: `TeletypeClip`.
- Performer Pattern selection chooses which TeletypeClip this TeletypeTrack loads.
- Clip -> VM happens only on explicit pattern/clip load, currently `applyPatternSlot()`.
- VM -> Clip happens only on explicit capture/save boundaries for that track pattern, currently `syncToActiveSlot()`.
- No read-only path should secretly mutate clip storage.

**Implementation direction:**

1. Add contract wrappers before a large rename:
   - `loadClipIntoVm(int clipIndex)` around `applyPatternSlot()`.
   - `captureActiveClip()` around `syncToActiveSlot()`.
2. Migrate call sites away from direct `syncToActiveSlot()` so each caller declares whether it is loading a clip or capturing the live VM.
3. Remove automatic capture from read-only paths first:
   - `TeletypeTrack::write()` must not mutate through `const_cast`.
   - `patternSlotSnapshot()` must not silently capture VM state just to read/export.
4. Make TeletypeTrack Performer Pattern change behavior explicit:
   - load selected clip for the new Performer Pattern;
   - if auto-capture-on-leave is retained, make it a named policy branch, not a hidden side effect.
5. Only after this boundary is clear, revisit Teletype file import/export transaction buffers and later TeletypeRuntime extraction.

**Two persistence contracts:**

- **Project save/load** serializes the whole TeletypeTrack model (both clips
  + shared VM scripts via `TeletypeTrack::write()`/`read()`). Unchanged.
- **Teletype text save/load** operates on active clip + live VM only. Text
  save is a read-only live export of the thing the user is currently
  editing/hearing and must not call `captureActiveClip()`. Text load imports
  into the active clip only and does not touch the inactive clip.
  This eliminates the 4-buffer text I/O rollback problem (4 → 2 buffers,
  ~2,452 B .bss saved). Legacy two-slot text files are read by selecting
  the slot matching the active clip index and ignoring the other.

**UI/product wording:**

- Stop describing these as full scenes or global clip-library entries.
- Present them as clips: `PATTERN 1 -> CLIP 1`, `PATTERN 2 -> CLIP 2`.
- If live VM edits are captured back to a clip, expose that as a save/capture/discard boundary rather than invisible sync.

## Open questions

- [ ] Does Approach A auto-capture the live VM when leaving a clip, or require explicit capture/save?
      **Note:** This is a musical/product decision, not architecture truth. Auto-capture preserves
      current behavior; explicit-capture changes the mental model. Validate with user input.
- [ ] Should script/pattern mutations during execution persist immediately, only on pattern switch, or only on save?
- [ ] What happens to delayed commands when the active slot changes?
      **Options:** (1) preserve current behavior and document as compatibility,
      (2) flush delays on clip switch, (3) tag delays with source clip and drop if stale.
      Must be decided before Slice 3 ships.
- [ ] Should manual script index 3 intentionally mean "current slot script"?
- [ ] Is `resetMetroOnLoad` per-slot persistent config, transient run request, or both?
- [ ] What is the transaction boundary for Teletype file import failure?
- [ ] If TeletypeRuntime is extracted later, does it own only scheduling/bridge dispatch, or also runtime state pools?

## Completed steps

- [x] Source-grounded global VM analysis written in `docs/global-teletype.md`.
- [x] Slot/pattern/VM sync contract written in `docs/slots-teletype.md`.
- [x] Working order selected: slot ownership, then transaction-local file import/export, then runtime extraction.
- [x] Phase 0: UI/engine race fix — `Engine::lock()/unlock()` on all Teletype VM mutations from UI. Committed `9bce3e11`. Hardware verified.
- [x] Phase 1: Clip vocabulary wrappers — no behavior change. Committed `1980de8a`. Hardware verified.
- [x] Phase 2: Hidden capture removed from `write()`/`clipSnapshot()`; remaining call sites migrated to clip vocabulary. Committed `7cf6de4f`. Hardware verified.
- [x] Phase 3: Performer Pattern clip policy centralized via `switch/set/clear/copyClipForPerformerPattern()`; Pattern Init reboot fixed by removing full-scene stack allocation. Hardware verified.

## Next action

Execute phased plan per `tele-clip-plan.md`:

- **Phase 0:** ✓ Complete. Hardware verified.
- **Phase 1:** ✓ Complete. Hardware verified.
- **Phase 2:** ✓ Complete. Hardware verified.
- **Phase 3:** ✓ Complete. Hardware verified.
- **Phase 4:** Two persistence contracts — text S/L as active-only, 4→2 file I/O buffers (1 file, ~2,452 B .bss savings, hardware build + file roundtrip + RAM check).

Record RAM baseline before Phase 0. Refresh after each phase.

## Notes

- Keep this task separate from `resource-optimization`. RAM pressure motivated the investigation, but this is now a Teletype semantics/architecture task.
- Do not collapse this into `teletype-edit-page`; UI work depends on stable Teletype semantics, not the other way around.
- Do not implement global VM without a migration/compatibility story for existing per-track Teletype projects.
