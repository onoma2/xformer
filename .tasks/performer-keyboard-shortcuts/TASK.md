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

## Implementation Plan

### Phase 1: TopPage::keyboard() — Global Navigation

Add `keyboard()` override to TopPage.h/cpp. This is the catch-all at the bottom of the page stack.

| Shortcut | Action | Maps to |
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
| Alt+Shift | Open context menu | Page+Shift |

**Conflict safety**: Alt+key combos never conflict with Teletype text entry because `hidKeycodeToAscii()` strips Alt modifier — Teletype's char handler sees alt-modified keys as their base letter (or `\0`), which means Alt+letter is always "unclaimed" by Teletype pages.

**Space caution**: TeletypeScriptViewPage doesn't handle Space (it uses Tab for indentation). TeletypePatternViewPage uses Space for toggle 0/1 — it consumes Space, so TopPage never sees it there. On non-Teletype pages Space is safe as toggle play.

**Escape caution**: TeletypeScriptViewPage consumes Escape (clears edit buffer). Pages that need Escape will consume it; TopPage only sees unconsumed Escapes.

**Files to modify**:
- `TopPage.h` — add `void keyboard(KeyboardEvent &event) override;`
- `TopPage.cpp` — implement keyboard() with Alt+letter, Space, Escape, 1-8

### Phase 2: Per-page F1-F5 Shortcuts

Each page adds `keyboard()` override mapping F1-F5 to its hardware button equivalents. Higher pages consume first, so no conflicts with Phase 1 globals.

| Page | F1 | F2 | F3 | F4 | F5 |
|------|----|----|----|----|----|
| PatternPage | Latch | Sync | Snap | Commit | Cancel |
| NoteSequenceEditPage | Gate cycle | Retrig cycle | Length cycle | Note cycle | Condition |
| LayoutPage | Mode | Link | Gate | CV | Commit |
| RoutingPage | Prev route | Next route | Init | Learn | Commit |
| SongPage | Chain | Insert | Remove | Duplicate | Play/Stop |
| PerformerPage | Latch | Sync | Unmute | Fill | Cancel |
| SystemPage | Cal | Info | Utils | Update | — |
| TrackPage | TI Preset | — | — | Sync Outs | — |
| MonitorPage | CV In | CV Out | MIDI | Stats | Version |

**Files to modify**: Each page's .h (add `keyboard()` override) and .cpp (implement F-key mapping).

### Phase 3: Per-page Letter Shortcuts

Context-sensitive letter keys for modal actions. Each page's `keyboard()` handles letters specific to its mode.

| Page | Key | Action |
|------|-----|--------|
| PerformerPage | M | Toggle mute |
| PerformerPage | S | Solo |
| LayoutPage | M | Track mode |
| LayoutPage | L | Track link |
| LayoutPage | G | Gate assign |
| LayoutPage | V | CV assign |
| RoutingPage | L | MIDI learn toggle |

### Phase 4: Step Type-In (NoteSequenceEditPage)

`5N E4` syntax command line for direct step value entry. Modeled on TeletypeScriptViewPage's `_editBuffer`.

### Phase 5: Form Mode (List Pages)

Tab through list fields, type values directly. Requires `setAbsolute()` on ListModel or direct model setter calls.

## Key Files

| File | Role |
|------|------|
| `src/apps/sequencer/ui/Event.h` | KeyboardEvent class with HID keycode constants, modifiers, ASCII decode |
| `src/apps/sequencer/ui/Page.h/cpp` | Page base class, `keyboard()` virtual (default no-op), `dispatchEvent()` routing |
| `src/apps/sequencer/ui/PageManager.h/cpp` | Page stack, `dispatchEvent()` top-to-bottom with consume |
| `src/apps/sequencer/ui/Ui.h/cpp` | `handleKeyboard()`, `enqueueKeyboardEvent()`, `hidKeycodeToAscii()` |
| `src/apps/sequencer/ui/pages/TopPage.h/cpp` | **Primary target** — add `keyboard()` override for global shortcuts |
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

**Key property**: TopPage::keyboard() only sees events that NO higher page consumed. This guarantees zero conflict with Teletype or any future page-specific handler.

## Decisions log

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

## Next actions

1. Add `keyboard()` override to TopPage.h/cpp with Alt+letter navigation, Space=play, Escape=back
2. Test on hardware that Alt+letter works on Teletype pages (should not intercept — Teletype consumes chars first)
3. Add Space=toggle play handling, verify PatternView's Space (toggle 0/1) still works (it consumes first)