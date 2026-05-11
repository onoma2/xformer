# Performer Keyboard Shortcuts — Task Status

Status: Active
Priority: High

## Overview

Implement USB keyboard shortcuts for the full Performer UI — all ~50 page types beyond the already-implemented Teletype pages. The hardware has ~40 buttons; the goal is keyboard equivalents for as many actions as possible.

## Architecture Decision — TopPage::keyboard() as Global Catch-All

**Key insight**: TopPage sits at stack position 0 (always the bottom page). `PageManager::dispatchEvent()` sends KeyboardEvents top-to-bottom through the stack. Since only TeletypeScriptViewPage and TeletypePatternViewPage override `keyboard()`, all other pages are a clean slate — events fall through to TopPage.

**Model**: Exactly mirrors how TopPage already handles hardware buttons via `keyPress()`. Global nav (Alt+letter, Space, Escape, 1-8) goes in TopPage::keyboard(). Per-page shortcuts (F1-F5, context letters) go in each page's own keyboard() override. No conflicts because higher pages consume first.

**Why NOT Ui::handleKeyboard() interception**: Putting globals in Ui would bypass the consume mechanism. TopPage::keyboard() preserves the standard event flow — pages can override/consume, and TopPage only sees what falls through. Teletype pages that consume F1-F5, Escape, Enter, letters will never have those keys stolen by globals.

**Why NOT a separate global handler before dispatch**: Would need to duplicate consume logic. TopPage is the natural "catch-all" in the existing architecture.

## Architecture Decision — BasePage::keyboard() Chain for Shift+Alt

**Problem**: Context menu (Page+Shift on hardware) needs a keyboard equivalent on every page. Putting it in each page's `keyboard()` is 19 copy-pastes.

**Solution**: `BasePage::keyboard()` virtual method handles Shift+Alt → calls `contextShow()`. Pages that override `keyboard()` forward unconsumed events to `BasePage::keyboard()` (or `ListPage::keyboard()` which chains to it). `contextShow()` is virtual on BasePage (default no-op), overridden by the 19 pages that define context menus.

**ContextMenuPage** gets its own `keyboard()` for F1-F5 item selection and Escape to close.

## Architecture Decision — ListPage Arrow Navigation

**Problem**: The biggest keyboard efficiency gap wasn't F-buttons or letter shortcuts — it was that 20+ list-based pages had no keyboard navigation at all. You needed the encoder for everything.

**Solution**: Single `ListPage::keyboard()` implementation gives all 20 list pages arrow-key navigation:
- Up/Down = scroll rows
- Left/Right = edit value (always, no mode toggle needed — keyboard has separate keys for nav vs edit)
- Enter = toggle edit mode (for visual cursor feedback only)
- Shift+Left/Right = coarse edit (same as encoder+shift)

**Why not letter shortcuts (Phase 3)**: Mnemonic letter shortcuts are a hardware paradigm. On keyboard, the real bottleneck is encoder interaction. Arrows solve this universally for all 20 list pages with one implementation.

## Implementation Plan

### Phase 1: TopPage::keyboard() — Global Navigation ✅

|| Shortcut | Action | Maps to |
|----------|--------|---------|
| Alt+Q | Sequence page | Page+Step1 |
| Alt+W | Track page | Page+Step2 |
| Alt+E | Song page | Page+Step3 |
| Alt+A | Sequence Edit page | Page+Step0 |
| Alt+S | Monitor page | Page+Step7 |
| Alt+D | Overview page | Page+Left |
| Alt+F | Pattern page (modal) | Pattern button |
| Alt+G | Performer page (modal) | Performer button |
| Alt+T | Tempo page | Tempo button |
| Alt+C | Clock Setup page | Page+Tempo |
| Alt+R | Routing page | Page+Step4/5 |
| Alt+H | Harmony page | — |
| Alt+O | Overview page | Alt+D alias |
| Alt+1..8 | Track select 1-8 | Track1..8 buttons |
| Space | Toggle play/stop | Play button |
| Escape | Pop page / go back | Back navigation |

### Phase 2: Per-page F1-F5 Shortcuts ✅

|| Page | F1 | F2 | F3 | F4 | F5 |
|------|----|----|----|----|----|
| PatternPage | Latch† | Sync† | Snap | Commit | Cancel |
| NoteSequenceEditPage | Gate cycle | Retrig cycle | Length cycle | Note cycle | Condition |
| LayoutPage | Mode | Link | Gate | CV | Commit |
| RoutingPage | Prev route | Next route | Init | Learn | Commit |
| SongPage | Chain | Insert | Remove | Duplicate | Play/Stop |
| PerformerPage | Latch† | Sync† | Unmute | Fill† | Cancel |
| SystemPage | Cal | Info | Utils | Update | — |
| TrackPage | TI Preset | — | — | Sync Outs | — |
| MonitorPage | CV In | CV Out | MIDI | Stats | Version |
| CurveSequenceEditPage | — | — | — | — | — |
| IndexedSequenceEditPage | — | — | — | — | — |
| IndexedMathPage | — | — | — | — | — |
| IndexedRouteConfigPage | — | — | — | — | — |
| DiscreteMapSequencePage | — | — | — | — | — |
| TuesdayEditPage | — | — | — | — | — |

† held-state buttons use toggle semantics on keyboard (press = toggle on/off, not hold)

### Phase 2b: ListPage Arrow Navigation ✅

Single `ListPage::keyboard()` implementation covers 20 pages:
- Up/Down = scroll rows
- Left/Right = edit value (always, no mode toggle needed)
- Enter = toggle edit mode (visual cursor feedback)
- Shift+Left/Right = coarse edit

All ListPage subclasses inherit this. Pages with existing `keyboard()` (Layout/Routing/System/Track) forward unconsumed events to `ListPage::keyboard()`. FileSelectPage adds Enter=OK, Escape=Cancel.

### Phase 2c: Shift+Alt Context Menu + ContextMenuPage Keyboard ✅

- Shift+Alt on any page → calls `contextShow()` (opens Init/Copy/Paste/etc menu)
- F1-F5 in context menu → select menu item
- Escape in context menu → close menu

### Phase 3: Per-page Letter Shortcuts (DEFERRED)

Replaced by ListPage arrow navigation (Phase 2b) which covers 20+ pages with a single implementation. Letter shortcuts may still be added for specific high-value actions (e.g., Performer mute/solo) but are not a priority.

### Phase 4: Step Type-In (NoteSequenceEditPage)

`5N E4` syntax command line for direct step value entry. Modeled on TeletypeScriptViewPage's `_editBuffer`. Not yet started.

### Phase 5: Form Mode (List Pages) (DEFERRED)

Arrow navigation in Phase 2b covers list page interaction. Direct value type-in could be added later but is not currently needed.

## Key Files

| File | Role |
|------|------|
| `src/apps/sequencer/ui/Event.h` | KeyboardEvent class with HID keycode constants, modifiers, ASCII decode |
| `src/apps/sequencer/ui/Page.h/cpp` | Page base class, `keyboard()` virtual (default no-op), `dispatchEvent()` routing |
| `src/apps/sequencer/ui/PageManager.h/cpp` | Page stack, `dispatchEvent()` top-to-bottom with consume |
| `src/apps/sequencer/ui/Ui.h/cpp` | `handleKeyboard()`, `enqueueKeyboardEvent()`, `hidKeycodeToAscii()` |
| `src/apps/sequencer/ui/pages/BasePage.h/cpp` | `keyboard()` virtual — Shift+Alt context menu, `contextShow()` virtual (default no-op), `pressFunctionButton()` |
| `src/apps/sequencer/ui/pages/ListPage.h/cpp` | `keyboard()` — arrow nav (Up/Down/Left/Right/Enter) for all 20 list pages |
| `src/apps/sequencer/ui/pages/TopPage.h/cpp` | Global catch-all — Escape, Space, Alt+letter, digits |
| `src/apps/sequencer/ui/pages/ContextMenuPage.h/cpp` | `keyboard()` — F1-F5 item select, Escape close |
| `src/apps/sequencer/ui/Screensaver.h/cpp` | `consumeKey()` — runs before page dispatch |

## Event Dispatch Reference

```
KeyboardEvent flow:
  Engine HID callback → Ui::enqueueKeyboardEvent() → ring buffer
  → Ui::update() → Ui::handleKeyboard()
    → hidKeycodeToAscii() → KeyboardEvent(keycode, modifiers, ch)
    → screensaver.consumeKey(kbEvent)        [first chance]
    → PageManager::dispatchEvent(kbEvent)
        ├─ If top page is modal: only top page gets event
        └─ Else: top→bottom, stop at event.consumed()
            TopPage (stack[0]) is LAST to see events
            Teletype pages (stack[1+]) see events FIRST
```

Keyboard dispatch chain for page-specific handling:
```
Page::keyboard() (virtual, default → BasePage::keyboard())
  ├─ Shift+Alt? → contextShow()   ← handled by BasePage
  └─ (unconsumed) → falls through to TopPage::keyboard() for globals
ListPage::keyboard() (overrides BasePage::keyboard())
  ├─ Arrow keys / Enter → list nav & edit
  └─ BasePage::keyboard() for Shift+Alt
Pages with their own keyboard() (Layout, Routing, System, Track, etc.)
  ├─ F1-F5 handling
  └─ ListPage::keyboard() for arrows → BasePage::keyboard() for Shift+Alt
```

**Key property**: TopPage::keyboard() only sees events that NO higher page consumed. This guarantees zero conflict with Teletype or any future page-specific handler.

## Decisions log

- 2026-05-11: **A9 reverted / A10 debug path**: HID-driver press/release tracking in `usbh_driver_hid.c` was treated as unsafe after USB crash reports. New debug build keeps `usbh_driver_hid.c` at the stable reverted path, copies raw 8-byte keyboard reports in `UsbH::messageHandler()`, diffs press/release in `UsbH::process()`, and lets `Ui::handleKeyboard()` synthesize real hardware step `KeyDown/KeyPress/KeyUp` events for Q-I/A-K.
- 2026-05-10: Ring buffer approach for HID keys (ISR-safe, no rendering in USB context)
- 2026-05-10: Fixed `hid_get_type()` inversion
- 2026-05-11: Keyboard keypresses verified end-to-end
- 2026-05-10: KeyboardEvent class, Page dispatch, TeletypeScriptViewPage handler
- 2026-05-10: Fixed type confusion crash (Ui* vs MessageManager* context)
- 2026-05-10: Route keyboard through Engine (mirrors MIDI pattern)
- 2026-05-10: Uppercase letter conversion for Teletype parser
- 2026-05-10: Analyzed hardware Teletype shortcuts — single-line clipboard safe, multi-line needs arch change
- 2026-05-10: Added Ctrl+C/V/X, Shift+Enter, [/] to ScriptViewPage
- 2026-05-10: Added full keyboard handler to PatternViewPage matching hardware
- 2026-05-10: **BUG**: USB mouse corrupts USB host state machine. Fix: reject mice at driver level.
- 2026-05-10: **Hardware test PASS**: Keyboard works, mouse rejected, Launchpad works, messages confirmed.
- 2026-05-11: **BUG fix**: Removed UsbH.cpp debug HUD message callback that overwrote "KEYBOARD CONNECTED" with "HID 0 t=3". Also removed DBG() prints from connect/disconnect handlers.
- 2026-05-11: **Architecture decision**: TopPage::keyboard() as global catch-all (stack position 0). No separate global handler needed. Preserves consume mechanism. Zero-conflict with Teletype pages by design.
- 2026-05-11: **Phase 1 hardware test PASS**: Escape=back, Space=play/stop, Alt+letter nav, digits 1-8 track select, Alt+digits track select all working. Alt+digits kept as redundant but safe fallback (Alt strips printability from Teletype). No conflicts with Teletype pages.
- 2026-05-11: **Phase 2 hardware test PASS**: F1-F5 working on all 15 pages. Held-state buttons (Latch/Sync/Fill) on PerformerPage and PatternPage use toggle semantics instead of instant keyDown→keyUp.
- 2026-05-11: **Bug fix**: PerformerPage F1(Latch), F2(Sync), F4(Fill) and PatternPage F1(Latch), F2(Sync) were held-state buttons — `pressFunctionButton()` did instant keyDown→keyPress→keyUp which immediately undid the state. Fixed to use toggle semantics (`_latching = !_latching` with commit on release).
- 2026-05-11: **Phase 2b decision**: ListPage arrow navigation replaces per-page letter shortcuts. Arrow keys are universal metaphor (no memorization), covers 20 pages with one implementation, and solves the real bottleneck (encoder dependency). Phase 3 deferred.
- 2026-05-11: **Phase 2c decision**: Shift+Alt for context menu. BasePage::keyboard() virtual method with contextShow() virtual (default no-op). Pages with context menus override it. ContextMenuPage gets keyboard() for F1-F5 and Escape. All existing keyboard() overrides forward unconsumed to BasePage::keyboard() for Shift+Alt.
- 2026-05-11: **Arrow nav design**: Up/Down always scrolls rows. Left/Right always edits (no mode toggle — keyboard has dedicated keys unlike hardware encoder which doubles up). Enter toggles edit mode for visual cursor feedback only.

## Completed steps

- [x] HID driver fixes (hid_get_type, dedup, ring buffer)
- [x] KeyboardEvent class, page dispatch, Engine routing
- [x] Ui keyboard ring buffer, handleKeyboard(), ASCII mapping
- [x] TeletypeScriptViewPage::keyboard() — char input, Ctrl+C/V/X, Shift+Enter, [/], etc.
- [x] TeletypePatternViewPage::keyboard() — arrow nav, Ctrl+C/V/X, Enter, digits, Space, [/], etc.
- [x] Screensaver wake on keyboard
- [x] Human-readable HID connect/disconnect messages (bug fix: removed debug override)
- [x] Mouse rejection at driver level
- [x] Hardware test confirmed — everything working
- [x] Architecture analysis for global shortcuts (TopPage::keyboard catch-all)
- [x] Phase 1: TopPage::keyboard() — Escape=back, Space=play, Alt+letter nav, 1-8 track select, Alt+digits track select
- [x] Phase 2: F1-F5 on all 15 page types (with toggle semantics for held-state buttons)
- [x] Phase 2b: ListPage::keyboard() — Up/Down/Left/Right/Enter arrow nav for all 20 list pages
- [x] Phase 2b: FileSelectPage — Enter=OK, Escape=Cancel
- [x] Phase 2c: BasePage::keyboard() — Shift+Alt context menu trigger (virtual contextShow)
- [x] Phase 2c: ContextMenuPage::keyboard() — F1-F5 select, Escape close
- [x] Bug fix: PerformerPage/PatternPage held-state F-buttons use toggle semantics

## Next actions

1. Hardware test: keyboard still shows "KEYBOARD CONNECTED"
2. Hardware test: Tab still opens quick menu
3. Hardware test: Q-I/A-K press/release toggles/clears steps on NoteSequenceEditPage
4. Hardware test: Launchpad/MIDI still enumerate after keyboard step-key use
