# Arrow Keys → Encoder Rotation — Feature (implemented)

Status: Done
Priority: High
Parent: performer-keyboard-shortcuts

## What Changed

Mapped **Up/Down arrow keys** to **encoder rotation events** in `BasePage::keyboard()`. Every page that has an `encoder()` handler now gets arrow key support for free — no per-page code needed.

## How It Works

In `BasePage::keyboard()` (`src/apps/sequencer/ui/pages/BasePage.cpp:80-87`):

- `KeyUp` → `EncoderEvent(+1, pressed)` (same as one encoder click clockwise)
- `KeyDown` → `EncoderEvent(-1, pressed)` (same as one encoder click counter-clockwise)
- `pressed` reads the current encoder button state from `pageKeyState[Key::Encoder]`

This calls the current page's `encoder()` virtual method directly, which is exactly what `Ui::handleEncoder()` does for the hardware encoder.

## Why This Works Globally

- Pages that **already handle** Up/Down in their own `keyboard()` override (ListPage, Teletype pages) consume the event before it reaches BasePage
- Pages that **don't** handle Up/Down (NoteSequenceEditPage, CurveSequenceEditPage, etc.) fall through to BasePage and get encoder rotation
- Shift+Up/Down works naturally — `KeyboardEvent::shift()` is available if any page's `encoder()` checks `globalKeyState()[Key::Shift]`
- Encoder button state (`pressed`) is respected — same behavior as hardware

## Key Design Decisions

1. **Direct `encoder()` call, not via PageManager dispatch**  
   BasePage calls `this->encoder()` directly because `keyboard()` is already executing in the correct page context. No need for PageManager since we're not on the event stack — we're *in* an event handler already.

2. **`pageKeyState[Key::Encoder]` for pressed state**  
   Matches how `Ui::handleEncoder()` reads the encoder button: `_pageKeyState[Key::Encoder]`. This means if the user holds the encoder button and presses Up, the encoder handler sees `pressed=true`.

3. **No Shift augmentation for step size**  
   Unlike Left/Right which map to hardware key events with Shift in KeyState, the encoder approach is simpler — each keypress = ±1. The existing encoder handlers already check `globalKeyState()[Key::Shift]` for fine/coarse control.

## Files Changed

| File | Change |
|------|--------|
| `src/apps/sequencer/ui/pages/BasePage.cpp` | Added Up/Down → EncoderEvent mapping in `keyboard()` |

## No Longer Needed from numeric-entry.md

The 530-line "numeric entry" plan with text buffer, parsers, and draw overlay is **superseded**. Arrow keys now give the same "hold step, turn encoder" experience via keyboard. The complexity budget of typed value entry (note name parser, enum parser, drawDetail overlay, text buffer) is eliminated.

## Replaced numeric-entry.md v2

That plan's value proposition was: "type a value instead of scroll to it." Arrow keys achieve the same goal:
- Hold step(s) with mouse/keyboard → select steps
- Press Up/Down arrows → change values one step at a time
- Shift+Left/Right → section navigation (already existed)

This is **functionally identical** to how hardware users "hold step, turn encoder" but via keyboard.