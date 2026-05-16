# Generator Preview/Apply + Step Selection + 64-Step Context

## Goal

Implement A/B preview workflow for the Generator page: enter in ORIGINAL state, encoder re-roll creates preview, F0 toggles between original and preview, APPLY commits, CANCEL reverts. Add step selection and section (bank) navigation for 64-step context. Port as much code from the Vinx fork as possible.

## Key Files

- `src/apps/sequencer/engine/generators/SequenceBuilder.h` — add 3-copy state machine
- `src/apps/sequencer/engine/generators/Generator.h` — add delegate methods
- `src/apps/sequencer/engine/generators/RandomGenerator.h` — add Variation, randomizeParams, displayValue
- `src/apps/sequencer/ui/pages/GeneratorPage.h` — add state machine + step selection
- `src/apps/sequencer/ui/pages/GeneratorPage.cpp` — rewrite for A/B workflow

## Reference Files (Vinx)

- `temp-ref/vinx-performer/src/apps/sequencer/engine/generators/SequenceBuilder.h` — 3-copy impl with AcidSequenceBuilder, ChaosSequenceBuilder, applyEntropy
- `temp-ref/vinx-performer/src/apps/sequencer/engine/generators/Generator.h` — mode enum with Acid/Chaos/ChaosEntropy, delegate methods
- `temp-ref/vinx-performer/src/apps/sequencer/engine/generators/RandomGenerator.h` — Variation param, randomizeParams, randomizeSeed, randomizeContextParams, displayValue
- `temp-ref/vinx-performer/src/apps/sequencer/engine/generators/EntropyTargets.h` — 14-target enum + blendValue
- `temp-ref/vinx-performer/src/apps/sequencer/engine/generators/ChaosGenerator.h` — 14-target chaos (Phase 3 consumer)
- `temp-ref/vinx-performer/src/apps/sequencer/engine/generators/ChaosEntropyGenerator.h` — thin entropy wrapper (Phase 3 consumer)
- `temp-ref/vinx-performer/src/apps/sequencer/ui/pages/GeneratorPage.h` — state machine, step selection, section
- `temp-ref/vinx-performer/src/apps/sequencer/ui/pages/GeneratorPage.cpp` — full 2093-line workflow

## Phased Implementation Plan

Each phase ends with a hardware build + RAM check. If `.data + .bss` exceeds 122,880 bytes (120 KB warning zone), stop and restructure before continuing.

### Phase A: SequenceBuilder 3-copy state machine (model layer)

**Goal:** Add `_preview` copy + `apply()`/`showOriginal()`/`showPreview()`/`showingPreview()` to `SequenceBuilderImpl<T>`.

**RAM concern:** Adding `_preview` as an inline member would increase `sizeof(NoteSequenceBuilder)` by ~9.5 KB, which flows into `Container<NoteSequenceBuilder>` inside `NoteSequenceEditPage` (statically allocated in Pages struct). Current `.data + .bss = 118,648 (90.5%)`. A 9.5 KB spike pushes SRAM to ~128 KB, exceeding the 128 KB physical limit.

**Solution: Heap-allocated `_preview` with explicit lifecycle.**

- `_preview` is a raw pointer `T* _preview = nullptr`, not `std::optional<T>`. On STM32, `std::optional` reserves inline storage for the contained value (it's an engaged flag + aligned storage sized for `T`). Using `std::optional<NoteSequence>` would still inflate the builder by ~9.5 KB in the Pages struct. A raw `T*` avoids this entirely: just a pointer (4 bytes) + bool flag.
- Allocation: `new T(_original)` in `showPreview()`. Deallocation: `delete _preview; _preview = nullptr` in `revert()` and destructor.
- Allocation failure: If `new` fails on STM32 (no heap memory), treat as CANCEL — set `_showingPreview = false`, leave `_edit` pointing at `_original`, proceed with generator in ORIGINAL state. The GeneratorPage checks `_preview != nullptr` before showing preview; if null, encoder param changes still work but toggle stays in ORIGINAL.
- Destroy: `~SequenceBuilderImpl()` calls `delete _preview`. This is mandatory — the builder owns the preview copy.
- `showOriginal()`: copies `_original` bytes into `_edit`'s target. Does NOT delete `_preview` — allows toggle back.
- `apply()`: copies `*_preview` into `_edit` and `_original`, marks `_showingPreview = true`. The `_preview` allocation stays alive (for future toggles).
- `revert()`: `_edit = _original; delete _preview; _preview = nullptr; _showingPreview = false;`. Destroys preview and resets to original state.

**Mutation ownership rule (critical):**

Current generators mutate `_edit` during `update()`. The Vinx A/B state machine works like this:
- `_edit` is a reference (`T&`) to the live sequence in the project model
- `_original` is a backup copy made when the generator enters
- `_preview` is a second copy allocated on first preview generation
- `showOriginal()` copies `_original` bytes into the live sequence through `_edit`
- `showPreview()` either allocates `_preview = new T(_original)` then applies the generator's result, or copies `*_preview` into the live sequence through `_edit`
- The generator's `update()` always writes through `_edit` (the live sequence reference)

This is safe: generators write through `_edit` → live sequence. `showOriginal()` and `showPreview()` swap content into the live sequence. `_original` is always a clean backup.

**Changes:**
1. `SequenceBuilder.h` — add 4 virtual methods to base: `apply()`, `showOriginal()`, `showPreview()`, `showingPreview()`
2. `SequenceBuilderImpl<T>` — add `T* _preview = nullptr` (raw pointer, heap-allocated) and `bool _showingPreview = false`
3. Add destructor: `~SequenceBuilderImpl() { delete _preview; }`
4. `SequenceBuilderImpl<T>::revert()` — `_edit = _original; delete _preview; _preview = nullptr; _showingPreview = false;`
5. `SequenceBuilderImpl<T>::apply()` — `_edit = *_preview; _original = *_preview; _showingPreview = true;`
6. `SequenceBuilderImpl<T>::showOriginal()` — copy `_original` bytes into `_edit` target; `_showingPreview = false;`
7. `SequenceBuilderImpl<T>::showPreview()` — `if (!_preview) { _preview = new (std::nothrow) T(_original); if (!_preview) { _showingPreview = false; return; } } copy *_preview into _edit target; _showingPreview = true;`
8. `Generator.h` — add delegate virtuals: `apply()`, `showOriginal()`, `showPreview()`, `showingPreview()`

**Hardware check A:** Build `build/stm32/release && make sequencer`. Verify:
- `.data + .bss` unchanged (should be same as baseline — `_preview` is heap-allocated, not in .bss)
- `sizeof(NoteSequenceBuilder)` should increase by only ~8 bytes (one pointer + one bool + padding)
- `sizeof(NoteSequenceEditPage)` should increase by the same ~8 bytes (the only new member is `_preview` pointer in the builder, but the builder is in a Container that's already sized for the max type)

---

### Phase B: RandomGenerator enhancements (engine layer)

**Goal:** Add Variation param, randomizeParams/displayValue to RandomGenerator. Wire through Generator delegate.

**Changes:**
1. `RandomGenerator.h` — add `Param::Variation` (blend 0-100%), widen seed to `uint32_t`, add `randomizeParams()`, `randomizeSeed()`, `randomizeContextParams()`, `displayValue(int index)`
2. `RandomGenerator.cpp` — implement the new methods. `randomizeSeed()` re-rolls `_params.seed`. `randomizeContextParams()` re-rolls smooth/bias/scale. `displayValue()` returns normalized 0-255 for graph rendering.
3. `Generator.h` — add `Mode::InitSteps` and virtual `randomizeParams()` with default no-op
4. `Generator::execute()` — handle `InitSteps` mode (clear all steps)

**Hardware check B:** Build. Verify `.data + .bss` hasn't grown beyond Phase A baseline by more than ~50 bytes (the `Variation` param + `uint32_t seed` widening).

---

### Phase C: GeneratorPage A/B workflow (UI layer — core state machine)

**Goal:** Rewire GeneratorPage with ORIGINAL/PREVIEW/APPLIED state machine, step selection integration, and bound track validation.

**StepSelection type:** Use `StepSelection<CONFIG_STEP_COUNT>*` — the concrete type used by both calling sites (`NoteSequenceEditPage::_stepSelection` and `CurveSequenceEditPage::_stepSelection`). `GeneratorPage::show()` receives this pointer from the calling page.

**Shift+Step conflict resolution:** In current XFORMER, `StepSelection::keyDown()` with shift held toggles persistent selection. In Vinx GeneratorPage, Shift+Step triggers immediate re-roll. Resolution: GeneratorPage consumes Shift+Step for re-roll ONLY when a generator is active. Precedence rule: if `_generator != nullptr && key.isStep() && key.shiftModifier()`, handle as re-roll and consume the event. StepSelection's shift-persist behavior is a different gesture (shift held before step press in edit pages, not generator pages).

**Changes:**
1. `GeneratorPage.h` — add `_previewArmed` (bool), `_applied` (bool), `_section` (int, 0-3), `StepSelection<CONFIG_STEP_COUNT>* _stepSelection`, `_boundTrackIndex` (int), `_boundTrackMode` (Track::TrackMode)`
2. `GeneratorPage.cpp`:
   - `show(Generator*, StepSelection<CONFIG_STEP_COUNT>*)` — store generator and step selection, reset state
   - `enter()` — `_generator->revert(); _generator->showOriginal(); _previewArmed = false; _applied = false; _section = current section from sequence`
   - `exit()` — `if (!_applied) { _generator->revert(); }`
   - `revert()` — `_generator->revert(); _applied = false; close();`
   - `commit()` — `_generator->apply(); _applied = true; close();`
   - `togglePreview()` — if `showingPreview()` then `showOriginal()` else `showPreview()`
   - `keyDown()`/`keyUp()` — delegate to `_stepSelection` for step button handling
   - `keyPress()` — Shift+Step consumed for re-roll, F0 toggles A/B
   - `encoder()` — on param change: `_generator->randomizeParams(); _generator->update(); _generator->showPreview();`
   - Context menu: NEW RAND / SMOOTH / RESEED / CANCEL / APPLY
   - `boundTrackContextValid()` / `ensureBoundTrackContext()` — cancel if track switches mid-generator
   - `stepOffset()` — returns `_section * StepCount`

**Hardware check C:** Build. Test on hardware: open generator, verify ORIGINAL state, turn encoder to generate preview, F0 toggles A/B, APPLY commits, CANCEL reverts. Check RAM delta is minimal (a few bools + pointers, ~80 bytes in Pages struct).

---

### Phase D: 64-step visualization (UI layer — bank navigation)

**Goal:** Add section navigation, bank separators, step selection highlighting, and dim non-active banks.

**64-step rendering strategy:** Current `drawRandomGenerator()` draws bars with `stepWidth = Width / steps` where steps = 16. With 64 steps on a 128px display, `stepWidth` drops from 8px to 2px — the step interior (`stepWidth - 2`) becomes 0 pixels, producing invisible bars. Solution: at >16 steps, switch to line/point rendering (connected line graph as in Vinx `drawProfile()`), with bank separators at 16-step boundaries. Active bank gets a frame. Non-active banks rendered at `Color::MediumLow`.

**Changes:**
1. `GeneratorPage.h` — add `stepOffset()` returning `_section * StepCount`
2. `GeneratorPage.cpp` — Left/Right buttons cycle `_section` 0-3
3. `drawRandomGenerator()` — if `previewStepCount() > 16`, switch to line rendering with bank separators
4. `drawBankSeparators()` — vertical lines at 16-step boundaries, brighter for current bank edges
5. `drawBankFrame()` — horizontal lines top/bottom of current bank section
6. `stepInCurrentBank(int step)` — checks if step falls in current section
7. `updateLeds()` — `LedPainter::drawSelectedSequenceSection(leds, _section)` for bank indicator
8. Step selection LED feedback: current step highlighted, selected steps bright

**Hardware check D:** Build. Test on hardware: verify bank separators at 16-step boundaries, Left/Right buttons navigate sections, bank frame highlights current section, non-active banks dimmed, 64-step graph is readable (line rendering, not zero-width bars).

---

### Phase E: Context menu expansion + randomizeSeed action

**Goal:** Expand generator context menu to 5 items, add Shift+Step re-roll, add randomizeSeed action.

**Changes:**
1. Context menu: NEW RAND / SMOOTH / RESEED / CANCEL / APPLY (5 items replacing current 3)
2. `keyPress()` — Shift+Step triggers `_generator->randomizeSeed(); _generator->update(); _generator->showPreview();` (precedence rule from Phase C)
3. Context action: RESEED calls `_generator->randomizeSeed()` then `showPreview()`

**Hardware check E:** Build. Test on hardware: verify all 5 context menu items work, Shift+Step re-rolls seed, RESEED randomizes seed in place. Verify double-click Page opens context menu (from Phase 1 submenu shortcuts).

---

## Decisions Log

- 2026-05-16: Squashed "Random generator preview/apply", "Generator preview/apply workflow", and "64-step context visualization" into one task. The A/B state machine is the core infrastructure that all three depend on; step selection and bank visualization are the UI layer on top of it.
- 2026-05-17: **Revised** `_preview` type from `std::optional<T>` to raw `T*` pointer. `std::optional` on STM32 reserves inline storage for the contained type, so `std::optional<NoteSequence>` would still inflate the builder by ~9.5 KB in the Pages struct. Raw pointer avoids this: 4 bytes + 1 byte bool.
- 2026-05-17: **Defined allocation failure behavior**: if `new T(_original)` fails, treat as CANCEL — stay in ORIGINAL state, no preview available. GeneratorPage checks `_preview != nullptr` before showing preview; if null, encoder param changes still work but toggle stays in ORIGINAL.
- 2026-05-17: **Clarified mutation ownership**: generators write through `_edit` (reference to live sequence). `showOriginal()` and `showPreview()` copy content INTO the live sequence through `_edit`. The Vinx design writes through `_edit` directly, which is correct because `_original` is always a separate backup.
- 2026-05-17: **Resolved StepSelection type**: Use `StepSelection<CONFIG_STEP_COUNT>*` — concrete type matching both calling sites. Generator takes pointer from calling page's existing `_stepSelection` member.
- 2026-05-17: **Resolved Shift+Step conflict**: GeneratorPage consumes Shift+Step for re-roll ONLY when generator is active. Precedence: if `_generator != nullptr && key.isStep() && key.shiftModifier()`, handle as reroll and consume event. StepSelection's shift-persist is a different gesture in edit pages.
- 2026-05-17: **Added 64-step rendering strategy**: At >16 steps, switch from filled rectangles to line/point rendering. Bank separators at 16-step boundaries. Active bank framed with horizontal lines. Non-active banks dimmed. Without this, 64 steps on 128px produces zero-width bars.

## Open Questions

- [ ] Should `EntropyTargets.h` be ported in Phase A or deferred to Phase 3 chaos/entropy task? Decision: defer. Phase A only adds the state machine shell.
- [ ] XFORMER-specific entropy layers (AccumulatorTrigger, PulseCount, GateMode, HarmonyRoleOverride, etc.) — how to map EntropyTarget to these? Needs custom `applyTarget<NoteSequence>` specialization. Deferred to chaos/entropy consumer task.
- [ ] **SequenceBuilder expansion for Tuesday/Indexed/Discrete** — current `SequenceBuilderImpl<T>` assumes `T::Layer` enum, `layerValue()/setLayerValue()`, `firstStep()/lastStep()` step range, and fixed-step iteration. Tuesday (no steps, algorithmic params), Indexed (bitpacked step, dynamic length 1-48), and Discrete (32 stages with threshold/direction/note, no Layer enum) each need hand-rolled SequenceBuilder subclasses before any generator/entropy system can apply. Deferred to post-Phase-E expansion task — the current preview/apply workflow targets NoteSequence and CurveSequence only.

## Completed Steps

- [x] Research: Vinx fork generator architecture fully analyzed
- [x] Research: XFORMER current state fully analyzed
- [x] Research: Delta between Vinx and XFORMER documented
- [x] Research: RAM footprint recorded (see RESEARCH.md)
- [x] Audit: 7 issues identified in initial plan, all addressed in revision
- [x] **Phase A: SequenceBuilder 3-copy state machine** — committed and hardware verified
  - Virtual destructor, apply(), showOriginal(), showPreview(), showingPreview() added to base
  - T* _preview (heap-allocated) + bool _showingPreview + destructor in SequenceBuilderImpl<T>
  - Allocation failure handled gracefully (std::nothrow, stays in ORIGINAL)
  - Generator base class: delegate methods added
  - updatePreview() base virtual + SequenceBuilderImpl implementation
  - RAM: .data + .bss unchanged at 118,648 (90.5%)
- [x] **Phase B: RandomGenerator enhancements** — committed and hardware verified
  - Variation param, randomizeParams, randomizeSeed, randomizeContextParams, displayValue
  - displayValue() is preview-aware (shows preview values when showing preview, original when showing original)
  - 32-bit seed
- [x] **Phase C: GeneratorPage A/B workflow** — committed and hardware verified
  - enter() in ORIGINAL state, encoder reroll creates preview, F0 toggles A/B
  - _previewArmed/_applied state machine with bound track validation
  - Step+Shift rerolls, F4 NEW RAND, encoder rerolls with function-key-aware detection
  - Fixed: updatePreview() now called on all param changes (even in ORIGINAL state), fixing stale-preview bug on A/B toggle
  - Fixed: updatePreview() now called before showPreview() in all reroll paths (F4, Shift+Step, RESEED)
  - SMOOTH context menu quick-edit calls updatePreview() after edit
- [x] **Phase D: 64-step visualization** — committed and hardware verified
  - Layer-specific draw routines: drawGateBlocks, drawNoteContour, drawSlideMarkers, drawBooleanMarkers, drawLengthBars, drawRepeatStripes, drawProfile (centered/offset)
  - Bank separators at 16-step boundaries with active-bank highlighting
  - Bank frame around current section
  - Playhead cursor (NoteTrackEngine/CurveTrackEngine)
  - Left/Right navigation for section 0-3
  - stepInCurrentBank for dimming non-active banks
  - LED feedback: current step + step selection + gate highlight
- [x] **Phase E: Context menu expansion** — committed and hardware verified
  - Random: empty slot / SMOOTH (quick-edit) / REGEN / CANCEL / COMMIT
  - Euclidean/others: NEW RAND / RESEED / REVERT / COMMIT
  - RESEED action calls randomizeSeed() + update() + updatePreview() + showPreview()

## Notes

RAM budget is tight. Current `.data + .bss = 118,648 (90.5%)`. Phase A adds ~8 bytes (pointer + bool) to Pages struct. Phase B adds ~50 bytes (Variation param, widened seed). Phases C-E add ~80 bytes. Total estimated persistent RAM: ~130 bytes. Heap allocation of `_preview` (~9.5 KB per generator instance) happens only while GeneratorPage is open and is freed on exit/revert. STM32 heap must have ~10 KB available at generator open time — verify with runtime monitoring.
