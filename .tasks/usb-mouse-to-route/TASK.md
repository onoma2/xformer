# usb-mouse-to-route

## Goal
Make a USB mouse usable as a CV source inside the routing engine. Mouse X / Y / wheel / buttons would appear as new entries in `Routing::Source` alongside CvIn, Midi, Mod, etc., so they can drive any `Routing::Target` like Tempo, Pattern, octave, etc. Absolute-axis semantics: dx/dy deltas integrate into an accumulator that the routing engine reads as a normalized `[0,1]` value.

## Status
Paused. No code started. Design context captured from a session that walked the USB-keyboard git history to understand which diagnostic surfaces survived hardware testing.

## Worktree
- Path: `.worktrees/usb-mouse-to-route/`
- Branch: `feat/usb-mouse-to-route`
- Base: `master` @ `11b5dae0 Merge branch 'feat/generator'`
- Build directories are per-worktree (in `build/` which is gitignored); cmake bootstrap required before first `make sequencer`.

## Gate (must clear before any feature work)
The libusbhost driver currently rejects every mouse in `analyze_descriptor` (commit `0d2b0015 Reject USB mice at HID driver level to prevent host stack crash`). The commit message: *"Mice corrupt the USB host state machine during enumeration, causing permanent failure for all subsequent USB devices (keyboard, Launchpad) until power cycle."* Re-enabling mice without fixing the underlying enumeration bug reintroduces a known hardware regression. Investigation of the root cause is step 1 of this task and must succeed before anything else.

## Key files

### Driver layer
- `src/platform/stm32/libs/libusbhost/src/usbh_driver_hid.c:218-224` — the mouse rejection block (`if (hid->hid_type == HID_TYPE_MOUSE) { hid->state_next = STATE_INACTIVE; return false; }`).
- `src/platform/stm32/libs/libusbhost/src/usbh_driver_hid.c:138-149` — `parse_report_descriptor()` stub. Always sets `report_data_length = 1` regardless of input. Suspected interaction with mouse descriptor handling.
- `src/platform/stm32/libs/libusbhost/src/usbh_driver_hid.c:173-185` — `analyze_descriptor` USB_DT_INTERFACE branch: protocol 0x01 → KEYBOARD, 0x02 → MOUSE, else OTHER.
- `src/platform/stm32/libs/libusbhost/src/usbh_driver_hid.c:240-310` — state machine (`STATE_READING_COMPLETE_AND_CHECK_REPORT`, `STATE_GET_REPORT_DESCRIPTOR_READ_COMPLETE`, etc.). State machine assumes 8-byte boot keyboard report.
- `src/platform/stm32/libs/libusbhost/include/usbh_driver_hid.h:60-65` — `HID_TYPE` enum, already has `HID_TYPE_MOUSE`.

### Platform driver
- `src/platform/stm32/drivers/UsbH.h:80-118` — `enqueueHidReport`, `processHidReports`, `_hidReports` ring (size 8 × 8-byte reports), `_hidKeyEvents` ring (size 16). Keyboard-shaped today.
- `src/platform/stm32/drivers/UsbH.cpp:213-218` — `HidDriverHandler::messageHandler` — the libusbhost recv callback. Must remain non-blocking; only allowed action is `g_usbh->enqueueHidReport(...)`.
- `src/platform/stm32/drivers/UsbH.cpp:250-283` — `UsbH::process()`. This is the only safe place to touch OLED. Sequence: `usbh_poll(time_us)` → `processHidReports()` → walk `hid_is_connected()` for connect/disconnect synthesis → midi write drain.
- `src/platform/stm32/drivers/UsbH.cpp:294-361` — `enqueueHidReport` / `processHidReports`, currently hard-coded to length 8 (boot keyboard report).
- `src/platform/stm32/drivers/UsbH.h:32-35` — `setDebugMessageCallback`. Available but unused; only callable from post-poll context.

### Engine + UI
- `src/apps/sequencer/engine/Engine.cpp:149` — `receiveKeyboard()` call site inside `Engine::update()`. New mouse pump mirrors this.
- `src/apps/sequencer/engine/Engine.cpp:1044-1051` — `Engine::receiveKeyboard()` — drains `_usbH.recvKey()` and forwards to `_keyboardReceiveHandler`. Pattern for `Engine::receiveMouse()`.
- `src/apps/sequencer/engine/Engine.h:53-54` — handler typedefs (`KeyboardReceiveHandler`, `HidConnectHandler`).
- `src/apps/sequencer/engine/Engine.h:205-206, 239, 274-275` — handler setters, member storage.
- `src/apps/sequencer/ui/KeyboardManager.cpp:62-69` — HID-type connect toast. `MOUSE CONNECTED` branch already wired; becomes reachable as soon as the rejection is lifted.
- `src/apps/sequencer/ui/MessageManager.cpp:13-18` — `showMessage()` takes a FreeRTOS mutex (`os::LockGuard`). This is the diagnostic-surface fragility budget; explained below.

### Routing
- `src/apps/sequencer/model/Routing.h:444-482` — `Source` enum, currently `uint8_t`, ends at `Mod8`/`Last`. Plenty of headroom.
- `src/apps/sequencer/model/Routing.h:500-553` — `printSource()` switch.
- `src/apps/sequencer/engine/RoutingEngine.cpp:349-419` — `updateSources()` switch reads engine state into `_sourceValues[routeIndex]` as `[0,1]` floats. New mouse cases plug in here.

## Decisions log

### 2026-05-20: OLED diagnostic rule (load-bearing)
Three diagnostic surfaces were tried during USB keyboard bring-up; only one survived hardware testing. The rule for any mouse work:

- **Forbidden:** any call from inside `usbh_driver_hid.c` driver hooks (`init`, `analyze_descriptor`, `event`, `poll`, `remove`) to anything that takes a FreeRTOS mutex. Commit `41ade375 Add HID debug diagnostics` wired `MessageManager::showMessage` into those hooks. Commit `b32cf6cb Revert debug diagnostics that broke USB enumeration` reverted them with: *"caused all USB to break - Launchpad stuck in screensaver, mouse LED blink-die."* Same failure signature as the mouse rejection itself.
- **Forbidden:** OLED calls from `hid_in_message_handler` (the recv callback). Commit `80dd6d9c Add keypress debug display for HID polling verification` showed `K:XX M:XX` per keycode from there. Killed by the same `b32cf6cb` revert.
- **Allowed:** calls made from `UsbH::process()` after `usbh_poll()` returns, and everything downstream of it (Engine receive, KeyboardManager handlers, RoutingEngine update). The existing `KEYBOARD CONNECTED` toast survives because it runs from this post-poll reconciliation path.

Mechanism: `MessageManager::showMessage` takes `os::LockGuard lock(_mutex)` (`MessageManager.cpp:14`). Holding that mutex on the USB host task mid-enumeration makes control-transfer timing miss.

### 2026-05-20: Mouse-CV semantics
- Boot HID mouse report is `[buttons, dx, dy, (wheel)]` with `dx/dy` signed bytes. Engine integrates deltas into accumulators per axis.
- Axis output is normalized `[0,1]` to match `Routing::Source` convention. Clamp at edges (no wrap), with per-axis sensitivity scale.
- Default on disconnect: hold last value rather than reset to centre. Decision left open if user wants spring-back behavior.
- Wheel: per-tick discrete steps, gate-able. Not absolute angle. (Open question — see below.)
- Buttons: gate-style, 1.0 while pressed, 0.0 released. Expose buttons 1–3 (boot protocol).

### 2026-05-20: Source enum layout
- Adjacent to `Midi` reads better in the source-picker than appended at the end. Both are non-track-bound external inputs.
- `uint8_t` enum with current largest value ~33 has plenty of room.
- No `ProjectVersion` bump. Per `PROJECT.md`: adding source enum values is acceptable in dev firmware. Old projects read unknown source as `None`.

### 2026-05-20: No introspection of crash via UART or printf
User has no debugger; only OLED and observable USB-device behavior. The investigation must be designed around the post-poll-only diagnostic budget.

## Open questions
- [ ] **Root cause of mouse enumeration crash.** Empty `parse_report_descriptor` always returns `report_data_length = 1`. Does the SET_REPORT in `hid_set_report` then send malformed data and corrupt the bus? Or does `wLength = hid->report0_length` (clamped to `USBH_HID_BUFFER`) in `STATE_GET_REPORT_DESCRIPTOR_READ_SETUP` mismatch mouse descriptor sizes? Or is the `event()` callback re-entering during a pending mouse interrupt transfer? Needs targeted instrumentation via state-only flags + post-poll OLED readout.
- [ ] **Wheel semantics.** Per-tick gate vs cumulative position normalised to wraparound?
- [ ] **Spring-back on idle?** Mouse axes hold last value, or decay to 0.5 over time?
- [ ] **Acceleration curve.** Linear sensitivity vs ballistics (slow precision, fast jumps).
- [ ] **Source enum position.** Insert between `Midi` and `GateOut1`, or at the new end before `Last`? Adjacent to Midi shifts later enum values — acceptable because routes serialize by source-name (verify in `Route::read/write`).

## First steps (when resumed)

1. **Reproduce the crash on hardware.** Build a branch with the `analyze_descriptor` mouse rejection removed and nothing else changed. Plug a mouse with a known-good keyboard and Launchpad already attached via a hub. Observe: do keyboard/Launchpad stop responding? Does the mouse LED come on, blink, then die (the signature in `b32cf6cb`)? This establishes the failure boundary.
2. **Add post-poll-safe state instrumentation.** No callbacks across the libusbhost boundary. Instead: write a `volatile uint8_t mouse_enum_stage` inside `usbh_driver_hid.c` that the C driver updates on each `STATE_*` transition. In `UsbH::process()` post-poll, read this byte and once-only emit a `MOUSE STAGE N` toast via the existing `_debugMsgCallback`. This is the only diagnostic that does not violate the OLED rule.
3. **Identify the failing transition.** Stage values will pinpoint where mouse enumeration diverges from keyboard. Likely candidates: stub `parse_report_descriptor`, `wLength` mismatch in descriptor read, or interrupt-IN timing differences.
4. **Fix the actual bug.** Either fill in `parse_report_descriptor` properly, or fix the `wLength` size handling, or correct the `STATE_*` ordering. Then re-enable mice in `analyze_descriptor`. Confirm `MOUSE CONNECTED` toast at `KeyboardManager.cpp:66` and that keyboard/Launchpad survive.
5. **Add mouse report path in `UsbH`.** New ring `_hidMouseReports`, parser `processMouseReports()` that handles 3- and 4-byte boot mouse reports, integrates dx/dy into per-axis accumulators clamped to `[0,1]`. New accessor `recvMouse(MouseEvent*)` mirroring `recvKey`.
6. **Engine plumbing.** Add `Engine::receiveMouse()` called from `Engine::update()` adjacent to `receiveKeyboard()` at line 149. Engine owns mouse state (`_mouseX`, `_mouseY`, `_mouseWheel`, `_mouseButtons`) so `RoutingEngine::updateSources()` can read it the same way it reads `cvInput()`/`busCv()`.
7. **Routing integration.** Add `MouseX`, `MouseY`, `MouseWheel`, `MouseButton1`, `MouseButton2`, `MouseButton3` to `Routing::Source` enum at `Routing.h:464` (just after `Midi`). Add cases in `RoutingEngine::updateSources()` at `RoutingEngine.cpp:349`. Add cases in `printSource()` at `Routing.h:500`.
8. **Source-picker UI.** Add the new entries to `RoutableListModel` source list. Confirm name strings render and selection persists in serialization round-trip.

## Notes

- **Diagnostic budget.** The only OLED-safe place is `UsbH::process()` post-poll. Plan diagnostic granularity accordingly: a single integer state byte + once-per-transition toast is the most you can afford. Per-event display is forbidden.
- **Reference forks.** Mebitek/Vinx forks have not been checked for mouse support; quick survey could either save work or be ruled out fast.
- **RAM impact.** Mouse accumulator state: ~8–16 bytes in Engine. Mouse report ring: ~24 bytes (3 reports × 8 bytes) in normal SRAM. Routing source enum is `uint8_t`; new values are zero-cost in storage. Negligible.
- **History of how the keyboard landed.** Commits `6b0e407e` (driver lift) → `41ade375`/`80dd6d9c` (failed in-driver OLED diagnostics) → `b32cf6cb` (revert proving the rule) → `82e64080` (Engine pump) → `e242c178…7f3b268a` (KeyboardManager extraction) → `0d2b0015` (mouse rejection added because mice still broke the bus). Walk this sequence in git before starting; it encodes the failure modes the next attempt must avoid.
