# Generator Preview/Apply — Research Document

## Current RAM Footprint (2026-05-16, post Phase 1 improvements)

| Section | Size | vs Baseline |
|---------|------|-------------|
| `.data` | 6,320 B (6.2 KB) | +0 |
| `.bss` | 112,328 B (109.7 KB) | -1,312 |
| `.data + .bss` | 118,648 B (115.9 KB) | -1,312 |
| `.ccmram_bss` | 54,716 B (53.4 KB) | +620 |
| **SRAM %** | **90.5%** | |

Container gate: `.data + .bss` must stay ≤ ~120 KB (91.4% of 128 KB). Current headroom: ~1.5 KB.

---

## XFORMER Current State

### SequenceBuilder.h (2-copy model)

```
_edit (reference to active sequence) + _original (backup copy)
```

- `revert()` only: copies _original → _edit
- No `_preview`, no `apply()`, no `showOriginal()`, no `showPreview()`, no `showingPreview()`
- No `clearSteps(selected)` with selection mask — only `clearSteps()` (all)
- No `clearLayer(selected)` — only `clearLayer()` (all steps)
- No `applyEntropy()` — no entropy system
- Only `NoteSequenceBuilder` and `CurveSequenceBuilder` typedefs

### Generator.h (minimal)

```
Mode::InitLayer, Euclidean, Random, Last
```

- No `apply()`, `showOriginal()`, `showPreview()`, `showingPreview()`, `randomizeParams()`
- No `InitSteps` mode
- No Acid, Chaos, ChaosEntropy modes

### RandomGenerator.h (minimal)

- `Param::Seed, Smooth, Bias, Scale, Last`
- 16-bit seed (`uint16_t`)
- No `Variation` param
- No `randomizeParams()`, `randomizeSeed()`, `randomizeContextParams()`
- No `displayValue(int index)` for graph rendering

### GeneratorPage.h/cpp (270 lines, no state machine)

- No `_stepSelection`, no `_section`, no `_previewArmed`, no `_applied`
- `commit()` just calls `close()`
- `revert()` calls `_generator->revert()` then `close()`
- No bank separators, no step selection, no section navigation
- No A/B toggle, no preview state, no bound track validation

---

## Vinx Fork Architecture (3-copy model)

### SequenceBuilder.h — Core State Machine

Three copies of the sequence:
- `_edit` — reference to the active sequence (what the engine plays)
- `_original` — backup of the sequence when generator was entered
- `_preview` — the generated/mutated version

State transitions:
- **Enter**: `_generator->revert()` + `_generator->showOriginal()` — start in ORIGINAL state
- **Generate**: write to `_preview`, then `_generator->showPreview()` swaps `_edit ← _preview`
- **Toggle A/B**: `showOriginal()` swaps `_edit ← _original`, `showPreview()` swaps `_edit ← _preview`
- **APPLY**: `_edit = _preview; _original = _preview; _showingPreview = true;` — commit preview
- **CANCEL/exit**: `_generator->revert()` — restore `_edit ← _original`

Additional methods vs XFORMER:
- `clearSteps(const std::bitset<CONFIG_STEP_COUNT> &selected)` — selective step clearing
- `clearLayer(const std::bitset<CONFIG_STEP_COUNT> &selected)` — selective layer clearing
- `applyEntropy(uint32_t seed, int amount, const std::bitset<CONFIG_STEP_COUNT> &selected, uint16_t targetMask)` — entropy blending (Phase 3)

### Generator.h — Mode Extensions

```
Mode::InitLayer, InitSteps, Euclidean, Random, Acid, Chaos, ChaosEntropy, Last
```

Delegates to builder:
- `apply()` → `_builder.apply()`
- `showOriginal()` → `_builder.showOriginal()`
- `showPreview()` → `_builder.showPreview()`
- `showingPreview()` → `_builder.showingPreview()`
- `randomizeParams()` — virtual, default no-op

### RandomGenerator — Extensions

- `Param::Seed, Smooth, Bias, Scale, Variation, Last` (added Variation 0-100)
- `uint32_t seed` (widened from uint16_t)
- `randomizeParams()` — re-randomize seed + context
- `randomizeSeed()` — re-roll seed only
- `randomizeContextParams()` — re-roll smooth/bias/scale without changing seed
- `displayValue(int index)` — for graph drawing (normalized 0-255 output)

### GeneratorPage — State Machine (2093 lines)

Key state:
- `_previewArmed` — whether a preview has been generated (prevents toggle before first generation)
- `_applied` — whether commit was called (exit skips revert if applied)
- `_section` — current bank (0-3) for 64-step navigation
- `_stepSelection` — pointer to calling page's step selection
- `_boundTrackIndex` / `_boundTrackMode` — track switch guard

Context menu items: NEW RAND / SMOOTH X / RESETGEN / CANCEL / APPLY

Bank visualization:
- `drawBankSeparators()` — vertical lines at 16-step boundaries, brighter for current bank edges
- `drawBankFrame()` — horizontal lines top/bottom of current bank
- `stepInCurrentBank(int step)` — checks if step falls in current section
- Non-active banks rendered dim (`Color::MediumLow`)

Step selection:
- `keyDown()` delegates to `_stepSelection->keyDown(event, stepOffset())`
- `keyUp()` delegates to `_stepSelection->keyUp(event, stepOffset())`
- Encoder with step held: Shift+step = immediate re-roll without full preview cycle
- F0 (function key 0): toggle between ORIGINAL and PREVIEW

---

## RAM Impact Analysis

### Phase A: SequenceBuilder 3-copy

The `_preview` member in `SequenceBuilderImpl<T>` would be a full copy of the sequence type T.

| Type | sizeof(T) | _preview as inline member cost |
|------|-----------|----------------------------------|
| NoteSequence | 9,544 B | +9.5 KB per instance in Container |
| CurveSequence | ~4,800 B | +4.8 KB per instance in Container |

**Problem:** `Container<NoteSequenceBuilder>` sits inside `NoteSequenceEditPage`, which is statically allocated in the `Pages` struct. Adding `_preview` as an inline member would increase `sizeof(NoteSequenceEditPage)` by ~9.5 KB, flowing directly into `.bss`.

**Decision: Use raw `T* _preview = nullptr` with heap allocation.** Not `std::optional<T>` — on STM32, `std::optional` reserves inline storage for the contained value (engaged flag + aligned storage sized for T), so `std::optional<NoteSequence>` would inflate the builder by ~9.5 KB in the Pages struct. A raw `T*` is just 4 bytes + 1 byte bool.

- `_edit` (reference) and `_original` (copy) remain as members — no size change from current code.
- `_preview` is `T* _preview = nullptr` — heap-allocated on `showPreview()`, freed on `revert()` or destructor.
- `_showingPreview` is `bool _showingPreview = false`.
- Total persistent increase: ~8 bytes (pointer + bool + padding) per builder type.
- Runtime heap allocation: ~9.5 KB per active generator instance (freed on exit/revert).

**Allocation failure behavior:** If `new (std::nothrow) T(_original)` fails, `_preview` stays null, `_showingPreview` stays false. GeneratorPage checks `_preview != nullptr` and `showingPreview()` before allowing toggle. Generator stays in ORIGINAL state. Encoder changes still work but preview toggle is disabled.

**Lifecycle ownership:** `SequenceBuilderImpl<T>` destructor calls `delete _preview`. `revert()` calls `delete _preview; _preview = nullptr`. `apply()` leaves `_preview` alive for future toggles. `showOriginal()` leaves `_preview` alive for toggle back.

**Estimated persistent RAM delta for Phase A: ~8 bytes** (pointer + bool in Pages struct, not 9.5 KB — the preview copy is on the heap only while generator page is open).

### Phase B: Generator + RandomGenerator

Adding `Variation` param (1 byte), widening seed to `uint32_t` (+2 bytes from `uint16_t`), a few method pointers. Estimated: **~50 bytes** in `.bss` (from widened seed in `RandomGenerator::Params` and the `Variation` field).

### Phase C: GeneratorPage state machine

Adding `_previewArmed` (1B), `_applied` (1B), `_section` (4B), `_stepSelection` ptr (4B), `_boundTrackIndex` (4B), `_boundTrackMode` (1B). Total: **~15 bytes** in Pages struct.

### Phase D: Bank visualization

No persistent RAM — all rendering is stack-local.

### Phase E: Context menu expansion

No persistent RAM — context menu items are static const arrays.

### Total estimated persistent RAM increase: ~65 bytes

Well within the ~2.3 KB headroom before the 120 KB warning zone.

---

## Porting Strategy

Port Vinx code as directly as possible. The key differences:

1. **Sequence types**: XFORMER has NoteSequence, CurveSequence, IndexedSequence, TuesdaySequence, DiscreteMapSequence, MidiSequence, TeletypeScript. Vinx has NoteSequence, CurveSequence, StochasticSequence, LogicSequence, ArpSequence. Only NoteSequence and CurveSequence overlap. Entropy specializations for Indexed, Tuesday, DiscreteMap are XFORMER-specific.

2. **Container sizing**: XFORMER uses `Container<NoteSequenceBuilder>` with a max-size buffer. Verify that adding `_preview` doesn't inflate this beyond the Track container gate (9,544 B).

3. **Step selection**: XFORMER already has `StepSelection<CONFIG_STEP_COUNT>` used in edit pages. GeneratorPage just needs a pointer to it, same as Vinx.

4. **Section navigation**: XFORMER already uses `_section` + `stepOffset()` in edit pages. Port the same pattern to GeneratorPage.

5. **Bound track validation**: Port `boundTrackContextValid()` and `ensureBoundTrackContext()` to prevent applying generator to wrong track after switching.
