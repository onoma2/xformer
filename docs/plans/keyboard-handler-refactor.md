---
id: keyboard-handler-refactor
schema: plan
date: 2026-05-11
topic: keyboard-manager-refactor
status: design-draft
branch: feat/global-keyboard
predecessor: .tasks/usb-keyboard-manager-refactor.md
---

# KeyboardHandler Refactor — Design Document

## What We're Building

Extract USB HID keyboard handling from `Ui` into a dedicated `KeyboardManager` class. This centralizes keycode translation (`hidKeycodeToAscii`, `hidKeycodeToStep`), ring buffering, and event synthesis into a single owned object, leaving `Ui` as a thin consumer that calls `KeyboardManager::process()` each frame.

**Note:** KeyboardManager is inspired by `ControllerManager`'s ownership pattern (owned by `Ui`, called from `Ui::update()`) but deliberately does NOT follow its internal architecture. ControllerManager is a parallel UI with direct model access; KeyboardManager is a page-stack-integrated translator with zero model knowledge. See the decision table below.

The refactor is motivated by three converging needs:
1. **Clean architecture** — keyboard logic (85+ lines of translation/dispatch) is scattered across `Ui.cpp` as static functions and private methods
2. **Bug-readiness** — the USB HID stack has been fragile (mouse corruption, press/release tracking, driver crashes); centralizing makes debugging faster
3. **Extensibility** — pluggable layout mappings (AZERTY, etc.) and future features need the pipeline to be modular

**Memory impact: zero net change** (~76 bytes moved from `Ui` to `KeyboardManager`, both in SRAM).

## Current Architecture (Before Refactor)

```
Data path (current):
  UsbH (ISR/Poll) → UsbH::process() → enqueueHidKey()
    → Engine::receiveKeyboard() [called from Engine::update()]
      → _keyboardReceiveHandler callback (lambda in Ui::init())
        → Ui::enqueueKeyboardEvent() → _receiveKeyboardEvents ring buffer
          → Ui::handleKeyboard() [called from Ui::update()]
            → hidKeycodeToAscii() (static in Ui.cpp)
            → hidKeycodeToStep() (static in Ui.cpp)
            → KeyState mutation + KeyEvent synthesis (for step keys Q-I/A-K)
            → KeyboardEvent dispatch → PageManager
```

### Code to Extract from Ui.cpp

| Item | Lines (Ui.cpp) | Type | Destination |
|------|----------------|------|-------------|
| `ReceiveKeyboardEvent` struct | Ui.h:63-67 | Type definition | `KeyboardManager.h` |
| `_receiveKeyboardEvents` ring buffer | Ui.h:68 | Member (56 bytes SRAM) | `KeyboardManager` private member |
| `hidKeycodeToAscii()` | Ui.cpp:217-247 | Static function (~30 lines) | `KeyboardManager.cpp` private static |
| `hidKeycodeToStep()` | Ui.cpp:249-260 | Static function (~12 lines) | `KeyboardManager.cpp` private static |
| `handleKeyboard()` | Ui.cpp:262-294 | Method (~33 lines) | `KeyboardManager::process()` |
| `enqueueKeyboardEvent()` | Ui.cpp:296-301 | Method (~6 lines) | `KeyboardManager::enqueue()` |
| `_keyboardReceiveHandler` lambda | Ui.cpp:67-69 | Init-time registration | `KeyboardManager` constructor or `init()` |
| `_hidConnectHandler` lambda | Ui.cpp:71-78 | Init-time registration | `KeyboardManager` (owns HID messages) |
| `_hidDisconnectHandler` lambda | Ui.cpp:80-83 | Init-time registration | `KeyboardManager` (owns HID messages) |

### Code to Update in Engine.cpp

| Item | Lines | Change |
|------|-------|--------|
| `Engine::receiveKeyboard()` | Engine.cpp:995-1002 | Call `_keyboardReceiveHandler` unchanged |
| `Engine::setKeyboardReceiveHandler()` | Engine.h:200 | Unchanged — Ui/Manager registers same callback shape |
| `Engine::setHidConnectHandler()` | Engine.h:201 | Unchanged — Manager registers directly |
| `Engine::setHidDisconnectHandler()` | Engine.h:202 | Unchanged — Manager registers directly |

### Dependencies KeyboardManager Needs Access To

From the graphify-out call graph analysis:
- **`KeyState`** (from `Key.h`) — for step-key synthesis (mutates `_pageKeyState` and `_globalKeyState`)
- **`KeyPressEventTracker`** (from `KeyPressEventTracker.h`) — for multi-press detection on step keys
- **`MatrixMap`** (from `MatrixMap.h`) — for `fromStep()` mapping
- **`Screensaver`** (from `Screensaver.h`) — for `consumeKey()` on both `KeyEvent` and `KeyboardEvent`
- **`PageManager`** (from `PageManager.h`) — for `dispatchEvent()` on all synthesized events
- **`Engine`** — for `isSuspended()` check and handler registration

## Target Architecture (After Refactor)

```
Data path (refactored):
  UsbH (ISR/Poll) → UsbH::process() → enqueueHidKey()
    → Engine::receiveKeyboard() [unchanged]
      → _keyboardReceiveHandler callback (now lambda in KeyboardManager)
        → KeyboardManager::enqueue() → _receiveKeyboardEvents ring buffer
          → KeyboardManager::process() [called from Ui::update()]
            → hidKeycodeToAscii() (private static in KeyboardManager.cpp)
            → hidKeycodeToStep() (private static in KeyboardManager.cpp)
            → KeyState mutation + KeyEvent synthesis
            → returns synthesized events to Ui for dispatch
```

### Proposed Class Interface

```cpp
// KeyboardManager.h
class KeyboardManager {
public:
    KeyboardManager(Engine &engine, MessageManager &messageManager);

    void init();
    void process(KeyState &pageKeyState, KeyState &globalKeyState,
                 KeyPressEventTracker &tracker,
                 Screensaver &screensaver,
                 PageManager &pageManager);

    void enqueue(uint8_t keycode, uint8_t modifiers, uint8_t pressed);

private:
    static char hidKeycodeToAscii(uint8_t keycode, uint8_t modifiers);
    static int hidKeycodeToStep(uint8_t keycode);

    Engine &_engine;
    MessageManager &_messageManager;

    struct ReceiveKeyboardEvent {
        uint8_t keycode;
        uint8_t modifiers;
        uint8_t pressed;
    };
    RingBuffer<ReceiveKeyboardEvent, 16> _receiveKeyboardEvents;
};
```

### Key Design Decision: Why `process()` Takes References

`KeyboardManager::process()` needs to mutate `KeyState`, use `Screensaver`, and dispatch through `PageManager`. Two approaches:

**Approach A (chosen):** Pass references in `process()` call. `Ui` retains ownership of all state objects; `KeyboardManager` is a stateless translator.

**Approach B (rejected):** `KeyboardManager` holds pointers/references to all these objects, set during `init()`.

Approach A is better because:
- `KeyboardManager` has zero persistent state beyond its ring buffer (76 bytes)
- No hidden coupling — the dependency surface is visible at the call site
- Matches how `handleKeys()` already works (everything is in `Ui` scope)
- Easier to test (can pass mock objects per-call)

### Ownership Model

```
Ui owns:
  ├─ KeyboardManager (new)
  ├─ ControllerManager (existing pattern)
  ├─ KeyState (_pageKeyState, _globalKeyState)
  ├─ KeyPressEventTracker
  ├─ Screensaver
  ├─ PageManager
  └─ MessageManager

KeyboardManager owns:
  ├─ ReceiveKeyboardEvent ring buffer (moved from Ui)
  └─ Registration with Engine handlers (moved from Ui::init())
```

## Why KeyboardManager is NOT ControllerManager

Full comparison: `docs/plans/keyboard-vs-controller-comparison.md`

They share only surface similarity (both owned by `Ui`, both called from `Ui::update()`). Underneath, they solve fundamentally different problems. **Do not drift KeyboardManager toward the ControllerManager pattern.**

### Decision Record

| Decision | Rationale |
|----------|-----------|
| **No `Model &` in constructor** | ControllerManager takes `Model &` because it directly edits sequences/play state/track selection (bypasses page stack). KeyboardManager is a pure translator — it converts HID bytes into events and lets the page stack handle the rest. Zero model knowledge. |
| **No `Container<>` or device hierarchy** | ControllerManager needs polymorphic `LaunchpadDevice` variants (Mk2, Mk3, Pro, etc.) with different LED protocols. All USB keyboards speak the same HID protocol — already handled by `UsbH`. No device abstraction needed. |
| **No `connect()`/`disconnect()` lifecycle** | ControllerManager creates/destroys a controller object on USB MIDI device plug/unplug. KeyboardManager is always-on; the ring buffer is always allocated (56 bytes). HID connect/disconnect just changes user-visible messages. |
| **No direct model mutation** | ControllerManager calls `sequence.step(i).toggleGate()`, `_engine.togglePlay()`, etc. directly. KeyboardManager synthesizes `KeyEvent`/`KeyboardEvent` and dispatches through `PageManager` — pages handle the model changes. This is the core architectural difference. |
| **`process()` takes external references** | ControllerManager's `update()` is self-contained (only draws LEDs to Launchpad). KeyboardManager must mutate `KeyState`, use `Screensaver`, and dispatch through `PageManager` — all owned by `Ui`. Passing by reference makes the dependency surface explicit at the call site. |
| **No LED/output feedback** | ControllerManager is bidirectional (receives button presses, sends LED colors). KeyboardManager is unidirectional (receives keycodes, dispatches events inward). No feedback path to the keyboard. |

### Architectural Difference in One Diagram

```
ControllerManager: Direct Model Manipulation (PARALLEL to page stack)
  Launchpad button → MIDI → ControllerManager → model/engine directly
  ControllerManager → MIDI → Launchpad LEDs

KeyboardManager: Page Stack Integration (EMBEDDED in page stack)
  USB HID key → KeyboardManager → KeyEvent/KeyboardEvent
    → Screensaver.consumeKey()
    → PageManager.dispatchEvent() → pages consume
      → TopPage (globals), BasePage (context menu),
        ListPage (arrows), ContextMenuPage (F1-F5)
```

### Drift Warning Signs

If during implementation you find yourself doing any of these, **stop and re-read this section**:

- Adding `Model &` to `KeyboardManager`
- Creating a `KeyboardDevice` base class or `Container<>`
- Adding `connect(vendorId, productId)` / `disconnect()` methods
- Bypassing `PageManager::dispatchEvent()` to edit the model directly
- Adding LED or display output from `KeyboardManager`
- Holding persistent references to `KeyState`, `Screensaver`, or `PageManager` as member variables

## Phased Implementation Plan

Each phase is designed to be a single commit that compiles cleanly and can be hardware-tested independently. No phase should leave the firmware in a broken state.

---

### Phase 0: Baseline Hardware Validation

**Before any code changes.** Verify current keyboard behavior is stable.

**Hardware check procedure:**
1. Flash current `feat/global-keyboard` branch to STM32
2. Connect USB keyboard — verify "KEYBOARD CONNECTED" message
3. Test Q-I / A-K step toggle/clear on NoteSequenceEditPage
4. Test global shortcuts: Escape (back), Space (play/stop), Alt+letter (nav), 1-8 (track select)
5. Test Tab opens context menu, second Tab/Escape closes it
6. Test ListPage arrow navigation (Up/Down/Left/Right/Enter)
7. Test Launchpad enumeration after keyboard use (USB host stack integrity)
8. Disconnect keyboard — verify "DEVICE REMOVED" message

**Pass criteria:** All 8 checks pass. Document any anomalies as known issues.

---

### Phase 1: Create Skeleton KeyboardManager

**Goal:** Create `KeyboardManager.h/.cpp` as a passthrough wrapper. All logic still lives in `Ui.cpp`, but the class exists and is wired in.

**Changes:**
- Create `src/apps/sequencer/ui/KeyboardManager.h` — class declaration with stub methods
- Create `src/apps/sequencer/ui/KeyboardManager.cpp` — stub implementations that delegate to current logic
- Add `#include "KeyboardManager.h"` to `Ui.h`
- Add `KeyboardManager _keyboardManager;` member to `Ui`
- Construct in `Ui` constructor (same ownership as `ControllerManager`)
- In `Ui::init()`: call `_keyboardManager.init()` which registers handlers with Engine
- In `Ui::update()`: call `_keyboardManager.process(...)` instead of `handleKeyboard()`

**Critical:** At this phase, `KeyboardManager` is a thin shell. The actual logic is still in `Ui.cpp` methods, called from `KeyboardManager` stubs. This proves the wiring works without moving any logic.

**Simulator test:** Build sim/debug, verify keyboard still works.
**STM32 build test:** Build stm32/release, verify no compile errors.

**Hardware check Phase 1:**
1. Flash to hardware
2. Run all Phase 0 checks (1-8)
3. **Regression check:** Verify no new latency on step key response (Q-I/A-K should feel identical)

---

### Phase 2: Extract Ring Buffer + enqueue()

**Goal:** Move `ReceiveKeyboardEvent` struct, `_receiveKeyboardEvents` ring buffer, and `enqueueKeyboardEvent()` into `KeyboardManager`.

**Changes:**
- Move `ReceiveKeyboardEvent` struct from `Ui.h` to `KeyboardManager.h`
- Move `RingBuffer<ReceiveKeyboardEvent, 16>` from `Ui` to `KeyboardManager`
- Move `enqueueKeyboardEvent()` logic into `KeyboardManager::enqueue()`
- Update `Ui::init()` lambda to call `_keyboardManager.enqueue()` instead
- Remove `enqueueKeyboardEvent()` and `_receiveKeyboardEvents` from `Ui`
- Remove `ReceiveKeyboardEvent` from `Ui.h`

**RAM impact:** 56 bytes moved from `Ui` SRAM to `KeyboardManager` SRAM. Net zero.
**Flash impact:** Negligible (function pointer indirection).

**Hardware check Phase 2:**
1. Flash to hardware
2. Run all Phase 0 checks (1-8)
3. **Buffer integrity test:** Hold a key down for 10 seconds — verify no buffer overflow crash
4. **Rapid key test:** Mash Q-I/A-K rapidly — verify no dropped events
5. **USB hot-plug:** Unplug and replug keyboard during key hold — verify clean recovery

---

### Phase 3: Extract HID Translation Functions

**Goal:** Move `hidKeycodeToAscii()` and `hidKeycodeToStep()` into `KeyboardManager.cpp` as private static methods.

**Changes:**
- Move `hidKeycodeToAscii()` from `Ui.cpp` (line 217-247) to `KeyboardManager.cpp`
- Move `hidKeycodeToStep()` from `Ui.cpp` (line 249-260) to `KeyboardManager.cpp`
- Update `KeyboardManager::process()` to call these directly
- Remove the static functions from `Ui.cpp`

**No behavioral change.** Pure code relocation.

**Hardware check Phase 3:**
1. Flash to hardware
2. Run all Phase 0 checks (1-8)
3. **Translation accuracy test:**
   - Type every letter a-z, A-Z (with shift) — verify correct ASCII in Teletype editor
   - Type digits 0-9 — verify track select works
   - Type punctuation: `-=[]\;',./` and shifted versions — verify Teletype input
4. **Step mapping test:** Q-I maps to steps 0-7, A-K maps to steps 8-15 on NoteSequenceEditPage

---

### Phase 4: Extract Core process() Logic

**Goal:** Move the entire `handleKeyboard()` dispatch logic into `KeyboardManager::process()`. This is the largest migration — the step-key synthesis, KeyState mutation, and event dispatch.

**Changes:**
- Move `handleKeyboard()` body into `KeyboardManager::process(KeyState&, KeyState&, KeyPressEventTracker&, Screensaver&, PageManager&)`
- `process()` now directly:
  - Drains ring buffer
  - Calls `hidKeycodeToAscii/Step`
  - Mutates `pageKeyState` / `globalKeyState` (passed by reference)
  - Creates `KeyEvent` / `KeyboardEvent`
  - Calls `screensaver.consumeKey()`
  - Calls `pageManager.dispatchEvent()`
- Remove `Ui::handleKeyboard()` — replace with `_keyboardManager.process(...)` call in `Ui::update()`
- Remove `KeyboardManager` stubs from Phase 1

**This is the highest-risk phase.** It moves the KeyState mutation and event synthesis. The `process()` signature makes all dependencies explicit.

**Hardware check Phase 4 (comprehensive):**
1. Flash to hardware
2. **Full regression — all Phase 0 checks (1-8)**
3. **Step key state integrity:**
   - Press Q (step 0 gate on) → verify LED and display update
   - Release Q → verify no change (gate stays on, as expected)
   - Press Q again (step 0 gate off) → verify step clears
   - Hold Shift + Q → verify NOT treated as step key (modifier strip check)
   - Test all 16 step keys (Q-I, A-K) in sequence
4. **Screensaver interaction:**
   - Enable screensaver, let display sleep
   - Press any keyboard key → verify display wakes
   - Verify step keys and regular keys both wake screensaver
5. **Engine suspend interaction:**
   - Trigger file operation (engine suspended)
   - Press keyboard keys → verify events are buffered, not lost
   - After file operation completes → verify buffered events fire
6. **Page stack integrity:**
   - Open modal page (Pattern, Performer)
   - Test keyboard shortcuts → verify modal page consumes correctly
   - Close modal → verify normal shortcuts resume
7. **Launchpad coexistence:**
   - Connect Launchpad + Keyboard simultaneously
   - Use both inputs — verify no interference
   - Disconnect/reconnect each — verify other still works

---

### Phase 5: Extract HID Connect/Disconnect Handlers

**Goal:** Move HID connect/disconnect message handling from `Ui::init()` lambdas into `KeyboardManager`.

**Changes:**
- Add `MessageManager &` reference to `KeyboardManager`
- Move `_engine.setHidConnectHandler()` lambda into `KeyboardManager::init()`
- Move `_engine.setHidDisconnectHandler()` lambda into `KeyboardManager::init()`
- Move `_engine.setKeyboardReceiveHandler()` lambda into `KeyboardManager::init()`
- Remove the three handler registrations from `Ui::init()`

**Hardware check Phase 5:**
1. Flash to hardware
2. **Connect/disconnect messaging:**
   - Plug in keyboard → verify "KEYBOARD CONNECTED" message (3000ms duration)
   - Unplug keyboard → verify "DEVICE REMOVED" message
   - Plug in mouse → verify "MOUSE CONNECTED" message (mouse should be rejected at driver level)
   - Unplug mouse → verify "DEVICE REMOVED" message
3. **Run all Phase 0 checks (1-8)** — final comprehensive regression

---

### Phase 6: Cleanup + CMakeLists Registration

**Goal:** Final cleanup. Remove dead code from `Ui.h`/`Ui.cpp`. Register new files in build system.

**Changes:**
- Remove `handleKeyboard()` declaration from `Ui.h`
- Remove `enqueueKeyboardEvent()` declaration from `Ui.h`
- Remove `#include "core/utils/RingBuffer.h"` from `Ui.h` if no longer needed
- Add `KeyboardManager.cpp` to `src/apps/sequencer/ui/CMakeLists.txt` (both stm32 and sim)
- Verify `Ui.h` no longer has any keyboard-specific members

**Final hardware check Phase 6:**
1. **Full production validation** — run ALL checks from Phase 0 through Phase 5
2. **Long-running stability test:** Leave keyboard connected for 30 minutes, use periodically — verify no memory leak or USB stack corruption
3. **Power cycle test:** Power off/on module with keyboard connected — verify keyboard enumerates on boot

---

## Risk Assessment

### Per-Phase Risk Matrix

| Phase | Risk Level | What Could Break | Rollback Strategy |
|-------|-----------|-----------------|-------------------|
| 0 | None | N/A (baseline) | N/A |
| 1 | Low | Wiring error, include path | Revert single commit |
| 2 | Low | Ring buffer move breaks ISR safety | Ring buffer is same size, same type — revert |
| 3 | Very Low | Pure code relocation | Revert single commit |
| 4 | **Medium** | KeyState mutation, event dispatch | Most complex phase — revert and re-examine |
| 5 | Low | Handler registration order | Check init() call order matches original |
| 6 | Very Low | Build system, dead code removal | Revert single commit |

### Known Fragile Areas (from history)

From the decisions log in `.tasks/performer-keyboard-shortcuts/TASK.md`:
1. **USB HID driver press/release tracking** — previously caused crashes, was reverted to stable path. `KeyboardManager` must not change the press/release semantics.
2. **Mouse corruption** — mice corrupt USB host state machine. Driver-level rejection must remain untouched.
3. **Held-state buttons** — F1(Latch), F2(Sync), F4(Fill) on PerformerPage/PatternPage use toggle semantics. The `pressFunctionButton()` synthesis path is not affected by this refactor.
4. **Step key modifier strip** — `(event.modifiers & 0x55)` check for Ctrl/Alt/Gui must be preserved exactly.

### Memory Safety

| Concern | Analysis |
|---------|----------|
| Ring buffer ISR safety | `enqueue()` called from Engine callback (same context as current). Ring buffer is lock-free single-producer/single-consumer. No change. |
| KeyState mutation | `process()` runs in UI task context (same as current `handleKeyboard()`). No threading concern. |
| Static allocation | No heap usage. All storage is class members with static lifetime. FreeRTOS heap is disabled. |
| CCM RAM eligible | `KeyboardManager` data is not DMA-accessed. Could be placed in CCM if SRAM pressure rises. |

## Open Questions

- [ ] **Layout strategy:** Should `KeyboardManager` accept a layout table at construction, or should layout be a runtime setting? (YAGNI says defer — current QWERTY-only is fine)
- [ ] **Unit test strategy:** `hidKeycodeToAscii` and `hidKeycodeToStep` are pure functions that can be easily tested once extracted. Should we add `TestKeyboardManager.cpp` in Phase 3?
- [ ] **`Ui::handleMidi()` pattern:** MIDI handling uses the same ring-buffer-and-handler pattern as keyboard. Should we also extract a `MidiManager` in a future refactor? (Out of scope for this task, but noting the symmetry)

## Success Criteria

1. All keyboard shortcuts work identically to pre-refactor behavior
2. No RAM increase (measured via `.map` file comparison)
3. No measurable input latency change (qualitative hardware test)
4. USB host stack stability (Launchpad + Keyboard coexistence)
5. `Ui.cpp` has zero keyboard-specific code (all moved to `KeyboardManager`)
6. `KeyboardManager.cpp` is self-contained and testable

## Key Files Summary

| File | Role | Change Type |
|------|------|-------------|
| `src/apps/sequencer/ui/KeyboardManager.h` | New — Manager class declaration | Created (Phase 1) |
| `src/apps/sequencer/ui/KeyboardManager.cpp` | New — Translation and dispatch logic | Created (Phase 1), filled (Phase 2-5) |
| `src/apps/sequencer/ui/Ui.h` | Remove keyboard members | Modified (Phase 2, 6) |
| `src/apps/sequencer/ui/Ui.cpp` | Remove keyboard functions, add Manager calls | Modified (Phase 1-6) |
| `src/apps/sequencer/engine/Engine.h` | No changes needed | Unchanged |
| `src/apps/sequencer/engine/Engine.cpp` | No changes needed | Unchanged |
| `src/apps/sequencer/ui/Event.h` | No changes needed | Unchanged |
| `src/apps/sequencer/ui/Key.h` | No changes needed | Unchanged |
| `src/apps/sequencer/ui/ControllerManager.h` | Pattern reference only | Unchanged |
| `src/platform/stm32/drivers/UsbH.cpp` | No changes needed | Unchanged |
| `src/platform/sim/drivers/UsbH.h` | No changes needed | Unchanged |
