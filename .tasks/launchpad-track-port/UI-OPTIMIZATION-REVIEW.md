# Launchpad UI Optimization & Refactoring Review

> Date: 2026-05-17  
> Scope: Current performer-phazer codebase + `temp-ref/mebitek-performer/` + `temp-ref/vinx-performer/`  
> Focus: UI rendering efficiency, MIDI overhead, code structure, and refactoring opportunities

---

## 1. Current Codebase Analysis

### 1.1 Frame Loop Inefficiency: Full Clear + Redraw Every 20ms

**Location:** `LaunchpadController.cpp:141-149`

```cpp
void LaunchpadController::update() {
    _device->clearLeds();           // zeros entire _ledState[80]
    CALL_MODE_FUNCTION(_mode, Draw) // redraws ALL LEDs from scratch
    globalDraw();                   // redraws function row again
    _device->syncLeds();            // diffs against _deviceLedState
}
```

**Problem:** Even when the sequencer is idle and no buttons are pressed, the code performs a full clear-draw-sync cycle at 50 FPS. `syncLeds()` does differential sending (good), but the preceding clear+redraw is unconditional.

**Cost per idle frame:**
- 80 memory writes (memset `_ledState` to 0)
- ~30–80 virtual `setLed()` calls to rebuild state
- 80 comparisons in `syncLeds()`

**Impact:** MIDI queue sees constant traffic. On a congested USB bus, the 128-slot TX ring buffer risks overflow during mode switches.

**Fix:** Add a dirty flag or model-change timestamp. Skip the entire `clearLeds()` + redraw if the model/engine state hasn't changed and no button events occurred since the last frame.

---

### 1.2 Navigation Recomputed Every Frame

**Location:** `LaunchpadController.cpp:302-334`

```cpp
void LaunchpadController::sequenceUpdateNavigation() {
    switch (_project.selectedTrack().trackMode()) {
    case Track::TrackMode::Note: {
        auto range = NoteSequence::layerRange(_project.selectedNoteSequenceLayer());
        _sequence.navigation.top = range.max / 8;
        _sequence.navigation.bottom = (range.min - 7) / 8;
        break;
    }
    ...
    }
}
```

**Problem:** Called inside `sequenceDraw()` every frame. Navigation bounds only change when track mode, selected layer, or first/last step changes. The `layerRange()` calls and divisions are redundant 49 out of 50 frames.

**Fix:** Cache navigation bounds and recompute only on explicit state changes (layer button press, track switch, step range edit).

---

### 1.3 Grid Overdraw

**`drawNoteSequenceNotes()`** (`LaunchpadController.cpp:850-869`):
- Draws octave lines across all columns first (up to 64 `setGridLed` calls)
- Then draws notes on top, overwriting some cells
- Worst case: 72 `setGridLed()` calls for an 8×8 grid

**`patternDraw()`** (`LaunchpadController.cpp:636-667`):
- Two passes: edited-pattern loop sets LEDs, then selected-pattern loop may overwrite the same `(row, col)`

**`drawBar()`** (`LaunchpadController.cpp:893-905`):
- Top cell is drawn twice: once in the `for` loop with `colorYellow()`, then overwritten with `stepColor()`
- `value < 0` branch draws downward from row 7, potentially extending below the visible grid

**Fix:** Combine passes. In `drawBar()`, loop to `value-1` and draw the top cell once. In `drawNoteSequenceNotes()`, skip octave-line drawing for columns that contain notes.

---

### 1.4 Modifier Guard Uses 7 Sequential Checks

**Location:** `LaunchpadController.cpp:288-295`

```cpp
if (!buttonState<Shift>() &&
    !buttonState<Navigate>() &&
    !buttonState<Layer>() &&
    !buttonState<FirstStep>() &&
    !buttonState<LastStep>() &&
    !buttonState<RunMode>() &&
    !buttonState<Fill>() &&
    button.isGrid()) {
```

**Problem:** Seven bitset lookups to verify "no modifier held." A single `_modifierMask` byte updated on button down/up would reduce this to one compare.

**Fix:** Maintain `uint8_t _modifierMask` where each bit = one function button state. Replace the 7-check guard with `if ((_modifierMask == 0) && button.isGrid())`.

---

### 1.5 Color Map Duplicated in 4 Headers

**Location:** `LaunchpadMk2Device.h`, `LaunchpadMk3Device.h`, `LaunchpadProDevice.h`, `LaunchpadProMk3Device.h`

The 16-entry `mapColor` lookup table is identical across all four derived classes:
```cpp
inline uint8_t mapColor(int red, int green) const {
    static const uint8_t map[] = { 0, 23, 25, 21, ... };
    return map[(red & 0x3) * 4 + (green & 0x3)];
}
```

**Problem:** 4 copies of the same read-only data in flash (~48 bytes) and 4 maintenance hazards.

**Fix:** Hoist to `LaunchpadDevice` as a single `static constexpr uint8_t colorMap[16]`.

---

### 1.6 `std::function` Handler Overhead

**Location:** `LaunchpadDevice.h`

```cpp
using SendMidiHandler = std::function<bool(uint8_t, const MidiMessage &)>;
using ButtonHandler   = std::function<void(uint8_t, const MidiMessage &)>;
```

**Problem:** `std::function` adds type-erased call overhead and ~32–48 bytes per handler. The handlers are never rebound after initialization — they always capture the same `ControllerManager`.

**Fix:** Replace with raw function pointers + `void *user` context:
```cpp
using SendMidiHandler = bool (*)(void *user, uint8_t cable, const MidiMessage &msg);
```

**Savings:** ~64–96 bytes RAM per device instance, removes indirection.

---

### 1.7 Virtual `setLed()` in Hot Path

**Problem:** `setLed()` is called ~30–80 times per frame through a vtable. All implementations are trivial (array write or 16-byte table lookup).

**Fix:** De-virtualize by storing a color-map function pointer in the base class and making `setLed()` a non-virtual inline. The only per-device variance is note/CC mapping and the color lookup table.

**Alternative:** Collapse the 5 device classes into a single `LaunchpadDevice` parameterized by a `DeviceTraits` struct (layout function, cable ID, CC ranges). Eliminates vtable entirely.

---

### 1.8 Button Struct Uses `int` Instead of `int8_t`

**Location:** `LaunchpadController.h:34-36`

```cpp
struct Button {
    int row = -1;
    int col = -1;
};
```

**Problem:** 8 bytes when 2 bytes suffice. Minor cache/ RAM impact.

**Fix:** `int8_t row = -1; int8_t col = -1;` — shrinks to 2 bytes.

---

## 2. Fork Feature Analysis

### 2.1 Mebitek — Baseline Reference (3240 lines)

**Adopted patterns (already good):**
- Differential LED sync via `_ledState` / `_deviceLedState` double buffer
- 1-byte packed `Color` union + compile-time velocity LUT
- `Container<...>` static polymorphism (no heap)
- Centralized bounds clipping in `setGridLed()` helpers
- `int8_t Navigation` struct (6 bytes)
- Tick-based double-press tracker (single struct, 13 bytes)
- `static const` layer maps with designated initializers

**Mebitek limitations:**
- Same full clear+redraw every frame as our codebase
- No dirty tracking beyond the device-level diff
- No long-press detection (only Down/Up/Press/DoublePress)
- No generator mode, no track locking

### 2.2 Vinx — Improvements Reference (3340 lines + 603 + 32 + 122)

**What vinx adds (feature-rich but not performance-optimized):**

| Feature | Files | Lines | Runtime Cost |
|---------|-------|-------|--------------|
| Generator mode | `LaunchpadControllerGeneratorsMode.cpp` | 603 | **Medium** — page-manager pointer chasing (5–10 compares per query) |
| Track selection locking | `LaunchpadControllerTrackSceneRouting.cpp` | 122 | **Low** — only on scene-button events |
| Circuit keyboard for Arp/Stochastic | `LaunchpadController.cpp` | +~60 | **Low** — per-frame `_noteStyle` int compare |
| Undo shortcut | `LaunchpadController.cpp` | +~23 | **Negligible** — only on Fn7 release |
| Visual feedback (mute, pattern) | `LaunchpadController.cpp` | +~50 | **Low** — 64 `isEdited()` checks/frame, no caching |

**Vinx optimizations over mebitek:**
- `drawBarH()` bounds safety: adds `i < 8` guard to prevent unbounded loop (mebitek could write past grid)
- Generator state cleanup on mode switch: explicit `cancelGeneratorMode()` before mode changes
- Code split into 4 TUs: better maintainability, neutral runtime impact

**Vinx does NOT optimize:**
- Still full clear+redraw every frame
- Still no LED dirty tracking at controller level
- Still no long-press event type (only DoublePress)
- Still recomputes navigation every frame
- Still 64 `isEdited()` checks every frame in pattern mode with no cache

**Verdict:** Vinx is a **feature superset** of mebitek, not a performance optimization. The extra ~857 lines are almost entirely generator mode + track-type branches. No performance TODOs or caching improvements were added.

---

## 3. UI Optimization Opportunities

### 3.1 LED Rendering Pipeline

**Current pipeline (per frame):**
```
clearLeds() → [mode draw: 30-80 setLed calls] → globalDraw() → syncLeds(): 80 compares + N MIDI sends
```

**Optimized pipeline:**
```
if (modelChanged || buttonEvent || engineStepAdvanced) {
    [mode draw: 30-80 setLed calls writing directly to _ledState]
    globalDraw()
}
syncLeds(): iterate only dirty LEDs → N MIDI sends
```

**Implementation:** Replace `clearLeds()` with a write-through model. Track changed LEDs via a 64-bit dirty mask (for grid) + 16-bit mask (scene/function). `syncLeds()` iterates set bits only.

**Estimated savings:**
- Idle frames: ~80 memset writes + ~80 virtual calls eliminated → **~0 MIDI traffic when idle**
- Active frames: same cost as today
- Implementation effort: ~2 hours, touches only `LaunchpadDevice` + subclasses

### 3.2 Pattern Mode `isEdited()` Caching

**Current:** 64 `isEdited()` calls per frame (8 tracks × 8 patterns).

**Optimized:** Maintain `uint64_t _patternEditedCache[8]` (one 64-bit mask per track). Invalidate cache on:
- Pattern switch
- Sequence edit
- Track mode change

**Savings:** 64 function calls → 0 calls on most frames. ~1 hour implementation.

### 3.3 Navigation Lazy Evaluation

**Current:** `sequenceUpdateNavigation()` runs every `sequenceDraw()`.

**Optimized:** Store `lastTrackMode`, `lastNoteLayer`, `lastCurveLayer` in `_sequence` state. Only recompute when any of these changed.

**Savings:** Removes `layerRange()` + division operations from 49/50 frames. ~30 min implementation.

### 3.4 SysEx Grid Batching for MK3/Pro MK3

**Current:** Each changed LED sends one `NoteOn`/`ControlChange` (up to 80 × 4-byte USB-MIDI events per frame).

**MK3/Pro Mk3 capability:** Novation supports "Set LED Grid" SysEx that updates multiple LEDs in one USB packet.

**Optimized:** Add `syncLedsSysEx()` path for MK3/ProMk3. Buffer changed LED RGB values into a single SysEx message.

**Savings:** Dramatic USB bandwidth reduction on mode switches. ~3 hours implementation. MK1 unaffected (falls back to NoteOn/CC).

---

## 4. Refactoring Recommendations

### 4.1 Architectural: Split Monolithic Controller

**Current:** `LaunchpadController.cpp` is 974 lines and will grow to ~2200+ after track porting.

**Recommended split (pre-implementation):**
```
LaunchpadController.h/cpp          — mode dispatch, global handlers, button tracking
LaunchpadNoteController.cpp        — NoteTrack draw/edit + circuit keyboard
LaunchpadCurveController.cpp       — CurveTrack draw/edit
LaunchpadTuesdayController.cpp     — TuesdayTrack step-key grid
LaunchpadDiscreteMapController.cpp — DiscreteMap stage editor
LaunchpadIndexedController.cpp     — Indexed step grid
LaunchpadPerformerController.cpp   — Performer mode
LaunchpadPatternController.cpp     — Pattern mode
```

**Benefits:**
- Parallel compilation
- Smaller merge conflict surface
- Easier to gate features (e.g., `#ifdef GENERATORS_MODE`)
- Per-track files mirror the OLED page structure

**Cost:** ~1 hour to split + fix linkage. Already planned as pre-flight task E0.2.

### 4.2 Device Layer: Traits-Based De-virtualization

**Current:** 5 device classes with virtual `setLed()`/`syncLeds()`/`recvMidi()`.

**Recommended:** Single `LaunchpadDevice` class parameterized at construction:
```cpp
struct LaunchpadTraits {
    uint8_t vendorId, productId;
    uint8_t (*noteNumber)(int row, int col);
    uint8_t (*sceneNumber)(int col);
    uint8_t (*functionCc)(int col);
    uint8_t (*mapColor)(int red, int green);
    bool needsSysExInit;
    const uint8_t *sysExInitPayload;
    size_t sysExInitSize;
};
```

**Benefits:**
- Eliminates vtable dispatch from hot path
- One class instead of 5 (+ headers)
- `Container<...>` can be replaced with direct member
- `mapColor` table deduplicated automatically

**Cost:** ~2 hours. Low blast radius — only touches device layer.

### 4.3 Button Event System: Modifier Mask

**Current:** 7 sequential `buttonState<T>()` checks for modifier guard.

**Recommended:** `uint8_t _modifierMask` updated atomically in `buttonDown()`/`buttonUp()`:
```cpp
enum ModifierMask : uint8_t {
    ShiftMask     = 1 << 0,
    NavigateMask  = 1 << 1,
    LayerMask     = 1 << 2,
    FirstStepMask = 1 << 3,
    LastStepMask  = 1 << 4,
    RunModeMask   = 1 << 5,
    FillMask      = 1 << 6,
};
```

**Benefits:** Single `if ((_modifierMask & ~ShiftMask) == 0)` check. Faster and more maintainable.

**Cost:** ~30 minutes.

### 4.4 Extract Pure Logic for Testability

**Current:** `LaunchpadController` is not unit-testable (needs full MIDI stack, device, model).

**Recommended:** Extract pure functions into `LaunchpadLogic.h/.cpp`:
- `Button` struct methods (`isGrid()`, `gridIndex()`)
- `Navigation` clamping math
- `RangeMap::map()` / `unmap()`
- `stepColor()` boolean algebra
- Layer map coordinate lookup

**Benefits:** Immediately testable with existing `UNIT_TEST` framework. No pybind11 bindings needed.

**Cost:** ~1 hour. Already planned in STATUS.md TDD section.

---

## 5. Prioritized Action List

### Before Any Feature Work (Pre-Flight)

| # | Action | Effort | Impact | Files |
|---|--------|--------|--------|-------|
| 1 | Split `LaunchpadController.cpp` into per-track files | ~1h | High (maintainability) | Controller layer |
| 2 | Add `isEdited()` to 3 sequence types | ~45min | Medium (pattern viz) | Model |
| 3 | Extract pure logic to `LaunchpadLogic.h/.cpp` | ~1h | Medium (testability) | New files |
| 4 | Replace `std::function` handlers with function pointers | ~30min | Low (RAM + speed) | `LaunchpadDevice.h` |
| 5 | Hoist `mapColor` table to base class | ~15min | Low (flash hygiene) | Device headers |

### During Feature Work ( Opportunistic )

| # | Action | Effort | Impact | When |
|---|--------|--------|--------|------|
| 6 | Add `_modifierMask` for button guards | ~30min | Medium | While adding new button handlers |
| 7 | Cache `sequenceUpdateNavigation()` bounds | ~30min | Medium | While adding track types |
| 8 | Shrink `Button` to `int8_t` | ~15min | Low | While touching header |
| 9 | Fix `drawBar()` top-cell overdraw | ~15min | Low | While adding bar draw for new tracks |
| 10 | Fix `drawNoteSequenceNotes()` octave-line overdraw | ~30min | Low | While fixing NoteTrack |

### Post-Feature (Polish)

| # | Action | Effort | Impact | Risk |
|---|--------|--------|--------|------|
| 11 | Add dirty-mask LED tracking (remove `clearLeds()`) | ~2h | **High** — eliminates idle-frame MIDI spam | Low — isolated to device layer |
| 12 | Cache `isEdited()` in pattern mode | ~1h | Medium — 64 calls → 0/frame | Low — cache invalidation is straightforward |
| 13 | De-virtualize device layer (traits-based) | ~2h | Medium — removes vtable from hot path | Medium — touches all device classes |
| 14 | SysEx grid batching for MK3/ProMk3 | ~3h | High — bulk LED updates in one packet | Low — MK1 fallback path stays intact |

---

## 6. Fork Feature Assessment Summary

| Feature | Mebitek | Vinx | Worth Porting? | Optimization Note |
|---------|---------|------|----------------|-------------------|
| Differential LED sync | ✅ | ✅ | Already have it | Could be improved with dirty mask |
| Circuit keyboard | ✅ | ✅ | **Yes** — Baseline | Add bounds safety (vinx fix) |
| LP Style / Note Style | ✅ | ✅ | **Yes** — Baseline | Default Classic for MK1 |
| Performer mode | ✅ | ✅ | **Yes** — Essential | Same code in both |
| Generators mode | ❌ | ✅ | **Deferred** — User Decision | Adds ~600 lines flash + page-manager coupling |
| Track selection locking | ❌ | ✅ | **Deferred** — User Decision | Low runtime cost, high coupling |
| Undo/redo shortcut | ❌ | ✅ | **Deferred** — User Decision | Needs core undo stack first |
| Visual feedback (mute, pattern) | ✅ | ✅ | **Yes** — Baseline | No caching — consider `isEdited()` cache |
| Enhanced button events | ✅ | ✅ | **Yes** — Baseline | No actual long-press in either fork |
| Double-tap detection | ✅ | ✅ | Already have it | Keep current 300ms window |
| Stochastic/Logic/Arp tracks | ✅ | ✅ | **No** — Not in target project | Target has Tuesday/DiscreteMap/Indexed instead |
| Launch Control XL | ❌ | ✅ | **No** — Extra tier | Different hardware, out of scope |

---

## 7. Key Finding: No Long-Press Implementation Exists

Despite both task documentation and user requests mentioning "long-press," **neither mebitek nor vinx actually implements a timed long-press event.** Both forks only support:
- `Down` — button pressed
- `Up` — button released
- `Press` — emitted on first down
- `DoublePress` — emitted on second down within 300ms

**"Long press" behavior in both forks is achieved by holding a modifier (Shift) and checking `buttonState<Shift>()` inside handlers.** There is no timer-based long-press gesture.

**If true long-press is desired**, it requires adding:
- A per-button timer or timestamp array
- A `ButtonAction::LongPress` enum value
- Timer expiry handling in `update()` or a periodic callback

**Estimated effort:** ~1.5 hours.

---

## 8. Resource Impact of Optimizations

| Optimization | RAM Δ | Flash Δ | Runtime Δ |
|--------------|-------|---------|-----------|
| Dirty-mask LED tracking | +16 bytes (bitmask) | +~50 bytes | **Eliminates idle-frame work** |
| `std::function` → function pointers | **-64 bytes** | -~100 bytes | Removes indirection |
| `Button` → `int8_t` | **-6 bytes** | 0 | Negligible |
| `mapColor` deduplication | 0 | **-48 bytes** | Negligible |
| `isEdited()` cache | +64 bytes (8× uint64_t) | +~80 bytes | **-64 calls/frame in pattern mode** |
| Traits-based device layer | **-~40 bytes** (no vptr) | -~200 bytes | Removes virtual dispatch |
| **Net conservative estimate** | **-~30 bytes** | **-~170 bytes** | Significant idle-frame reduction |

All optimizations are **RAM-negative or RAM-neutral** while improving performance. Safe to apply on the STM32F4 budget.
