# usb-keyboard-manager-refactor

## Goal
Refactor the USB HID Keyboard handling towards a `KeyboardManager` inspired by the existing `ControllerManager`. This achieves better separation of concerns by moving keycode translation (ASCII/Step) out of the main `Ui` class and provides a consistent interface for external controllers.

## Key files
- `src/apps/sequencer/ui/KeyboardManager.h` — Manager interface and event definitions
- `src/apps/sequencer/ui/KeyboardManager.cpp` — Translation logic (ASCII/Step mapping)
- `src/apps/sequencer/ui/Ui.cpp` — Consumer of the new manager
- `src/apps/sequencer/engine/Engine.cpp` — Bridging UsbH to KeyboardManager
- `src/platform/stm32/drivers/UsbH.cpp` — Low-level HID driver

## Preliminary Plan
1. **Define Interface**: Create `KeyboardManager` mirroring `ControllerManager`'s structure.
2. **Migrate Logic**: Move `hidKeycodeToAscii` and `hidKeycodeToStep` from `Ui.cpp` to `KeyboardManager`.
3. **Internal Buffering**: Move `_receiveKeyboardEvents` ring buffer from `Ui` to `KeyboardManager`.
4. **Bridge Engine**: Update `Engine` to pass events directly to the manager.
5. **UI Integration**: Update `Ui::update()` to call `KeyboardManager::update()` and dispatch translated events.

## Technical Nuances & Optimization
- **Latency (Direct Polling)**: Instead of the current "push" model where `Engine` uses a callback to notify `Ui`, the `KeyboardManager` will directly poll the raw `UsbH` events via `Engine`. This reduces call-stack depth and ensures translation happens only when the UI is ready to process, avoiding unnecessary overhead in the Engine task.
- **Layout Strategy**: The `KeyboardManager` will be designed to support pluggable layout mappings. This anticipates future support for AZERTY/other layouts via a system setting without requiring UI refactors.
- **Event Integrity**: `KeyEvent` (hardware) and `KeyboardEvent` (USB) will remain distinct types in the dispatch layer. This prevents regressions in the complex hardware-button shortcut logic while allowing the keyboard to "emulate" physical buttons through targeted translation.

## Full Design Document

→ `docs/plans/keyboard-handler-refactor.md` — Complete phased implementation plan with hardware validation procedures.

## Brainstorm Materials
### Proposed Architecture
- **Ownership**: `Ui` owns a `KeyboardManager` (constructed with `Model &` and `Engine &`).
- **Data Path**: `UsbH` (IRQ/Poll) → `Engine` (Receive) → `KeyboardManager` (Translate/Buffer) → `Ui` (Dispatch).
- **Events**: Standardize on a unified event type that carries both raw HID info and translated state.

### Adversarial Analysis
- **Latency**: Risk of "double buffering" if not careful with ring buffer handoffs.
- **Memory**: Manager adds class overhead and its own event queue.
- **Input Lag**: Translation must be fast; avoid complex lookups in the hot path.
- **Regression**: Ensure no loss of existing shortcut functionality (Context menus, step editing).

## Decisions log
- 2026-05-11: Refined data path to use direct polling to minimize input lag.
- 2026-05-11: Proposed refactor inspired by `ControllerManager`. Synthesized brainstorm results from Goose and OpenCode. Aiming to centralize translation logic and standardize event dispatching.

## Open questions
- [ ] Should `KeyboardManager` be owned by `Ui` or `Engine`? (ControllerManager is owned by `Ui`).
- [ ] How to handle different keyboard layouts (QWERTY/AZERTY) within the manager?
- [ ] Can we reuse the `KeyEvent` / `KeyboardEvent` types or should they be unified?

## Completed steps
- [x] Initial research into current keyboard data flow
- [x] Brainstorm and adversarial analysis of the refactor plan
- [x] **Phase 1**: Create skeleton `KeyboardManager` — class created, wired into `Ui`, delegates `process()` via callback to `Ui::handleKeyboard()`. Builds cleanly for sim target.

## Notes
- Reference `src/apps/sequencer/ui/ControllerManager.cpp` for pattern matching.
