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

Each phase ends with a hardware build + RAM check. If RAM exceeds the 120 KB warning zone (`.data + .bss > 122,880`), stop and restructure before continuing.

### Phase A: SequenceBuilder 3-copy state machine (model layer)

**Goal:** Add `_preview` copy + `apply()`/`showOriginal()`/`showPreview()`/`showingPreview()` to `SequenceBuilderImpl<T>`.

**RAM concern:** Adding `_preview` as a member doubles the builder size.
`Container<NoteSequenceBuilder>` sits inside `NoteSequenceEditPage` (statically allocated in Pages).
`NoteSequence` ≈ 9544 B. Adding a second copy means ~9.5 KB more per page that has a builder.

**Solution:** Use heap allocation for `_preview`. The `_edit` reference and `_original` copy stay as members
(unchanged from current code). `_preview` is `std::optional<T>` or a raw pointer allocated with `new` on
`showPreview()` and freed on `revert()`. This way the Container size doesn't change and `_preview` only
exists while the generator page is open (~transient, not in .bss).

**Changes:**
1. `SequenceBuilder.h` — add virtual methods: `apply()`, `showOriginal()`, `showPreview()`, `showingPreview()`
2. `SequenceBuilderImpl<T>` — add `std::optional<T> _preview` and `bool _showingPreview = false`
3. `SequenceBuilderImpl<T>::revert()` — `_edit = _original; _preview.reset(); _showingPreview = false;`
4. `SequenceBuilderImpl<T>::apply()` — `_edit = *_preview; _original = *_preview; _showingPreview = true;`
5. `SequenceBuilderImpl<T>::showOriginal()` — `_edit = _original; _showingPreview = false;`
6. `SequenceBuilderImpl<T>::showPreview()` — if (!_preview) _preview.emplace(_original); _edit = *_preview; _showingPreview = true;`
7. `Generator.h` — add delegate virtuals: `apply()`, `showOriginal()`, `showPreview()`, `showingPreview()`

**Hardware check A:** Build `build/stm32/release && make sequencer`. Verify `.data + .bss ≤ 120 KB`.
Check `sizeof(NoteSequenceBuilder)` hasn't changed (it shouldn't — `_preview` is `std::optional` which is heap-allocated on emplacement). Check `sizeof(NoteSequenceEditPage)` delta.

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

**Changes:**
1. `GeneratorPage.h` — add `_previewArmed`, `_applied`, `_section`, `_stepSelection` pointer, `_boundTrackIndex`, `_boundTrackMode`
2. `GeneratorPage.cpp`:
   - `enter()` — `_generator->revert(); _generator->showOriginal(); _previewArmed = false; _applied = false; _section = ...`
   - `exit()` — `if (!_applied) _generator->revert();`
   - `revert()` — `_generator->revert(); _applied = false; close();`
   - `commit()` — `_generator->apply(); _applied = true; close();` (or `_applied = true; close();` if no preview was generated)
   - `togglePreview()` — if `showingPreview()` then `showOriginal()` else `showPreview()`
   - `keyDown()`/`keyUp()` — delegate to `_stepSelection` for step button handling
   - `encoder()` — on param change: `_generator->randomizeParams(); _generator->update(); _generator->showPreview();`
   - Context menu: NEW RAND / SMOOTH / RESEED / CANCEL / APPLY
   - `boundTrackContextValid()` / `ensureBoundTrackContext()` — cancel if track switches mid-generator
3. `show(Generator*, StepSelection*)` — accept step selection pointer from calling page

**Hardware check C:** Build. Test on hardware: open generator, verify ORIGINAL state, turn encoder to generate preview, F0 toggles A/B, APPLY commits, CANCEL reverts. Check RAM delta is minimal (a few bools + pointers on the stack-like Pages struct).

---

### Phase D: 64-step visualization (UI layer — bank navigation)

**Goal:** Add section navigation, bank separators, step selection highlighting, and dim non-active banks.

**Changes:**
1. `GeneratorPage.h` — `stepOffset()` method returning `_section * StepCount`
2. `GeneratorPage.cpp` — Left/Right cycle `_section` 0-3
3. `drawRandomGenerator()` — port `drawBankSeparators()` and `drawBankFrame()` from Vinx
4. `drawRandomGenerator()` — dim non-active bank steps with `Color::MediumLow`
5. `updateLeds()` — `LedPainter::drawSelectedSequenceSection(leds, _section)` for bank indicator
6. Step buttons highlight current selection using `_stepSelection`

**Hardware check D:** Build. Test on hardware: verify bank separators appear at 16-step boundaries, Left/Right buttons navigate sections, bank frame highlights current section, non-active banks are dimmed.

---

### Phase E: Context menu expansion + randomizeSeed action

**Goal:** Expand generator context menu to 5 items, add Shift+Step re-roll, add randomizeSeed action.

**Changes:**
1. Context menu: NEW RAND / SMOOTH / RESEED / CANCEL / APPLY (5 items replacing current 3)
2. `keyPress()` — Shift+Step triggers `_generator->randomizeSeed(); _generator->update(); _generator->showPreview();`
3. Context action: RESEED calls `_generator->randomizeSeed()` then `showPreview()`

**Hardware check E:** Build. Test on hardware: verify all 5 context menu items work, Shift+Step re-rolls seed, RESEED randomizes seed in place.

---

## Decisions Log

- 2026-05-16: Squashed "Random generator preview/apply", "Generator preview/apply workflow", and "64-step context visualization" into one task. The A/B state machine is the core infrastructure that all three depend on; step selection and bank visualization are the UI layer on top of it.
- 2026-05-16: Decided to use `std::optional<T>` for `_preview` instead of inline member to avoid inflating `Container<NoteSequenceBuilder>` by ~9.5 KB. `_preview` is heap-allocated only while generator page is open.

## Open Questions

- [ ] Should `EntropyTargets.h` be ported in Phase A or deferred to Phase 3 chaos/entropy task? Decision: defer. Phase A only adds the state machine shell.
- [ ] XFORMER-specific entropy layers (AccumulatorTrigger, PulseCount, GateMode, HarmonyRoleOverride, etc.) — how to map EntropyTarget to these? Needs custom `applyTarget<NoteSequence>` specialization. Deferred to chaos/entropy consumer task.

## Completed Steps

- [x] Research: Vinx fork generator architecture fully analyzed
- [x] Research: XFORMER current state fully analyzed
- [x] Research: Delta between Vinx and XFORMER documented
- [x] Research: RAM footprint recorded (see RESEARCH.md)

## Notes

RAM budget is tight. Current `.data + .bss = 118,648 (90.5%)`. Phase A adds ~0 bytes to persistent RAM (optional/heap-allocated _preview). Phase B adds ~50 bytes (Variation param, widened seed). Phases C-E add ~80 bytes (state flags, pointers). Total estimated persistent RAM increase: ~130 bytes. Well within budget.
