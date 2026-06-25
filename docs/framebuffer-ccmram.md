# Framebuffer / CCMRAM optimization

Findings on the largest CCMRAM consumer and a radical reclaim path. Not yet
implemented — this is a decision record from a RAM-optimization brainstorm
(2026-06-18).

## Current state (STM32 release, measured)

`arm-none-eabi-size -A` sections:

- `.data + .bss` = **121,956 B (~119.1 KB)** — ~900 B under the 122,880 B (120 KB) hard warning. **This is the binding constraint.**
- `.ccmram_bss` = **48,412 B (~47.3 KB)** of 56 KB — ~8.9 KB free. Comfortable.

CCMRAM composition (largest symbols):

| Symbol | Bytes | What |
|---|---|---|
| `ui` | 23,644 | the `Ui` object: framebuffer + `Pages` + managers |
| `engine` | 9,368 | `Engine` singleton |
| `uiTask` / `engineTask` stacks | 4,256 each | RTOS stacks |
| `usbhTask` / `profilerTask` stacks | 2,208 each | RTOS stacks |
| `driverTask` stack | 1,184 | RTOS stack |
| drivers (`usbMidi`/`midi`/`blm`/…) | ~1,300 | peripheral objects |

Inside `ui` (`src/apps/sequencer/ui/Ui.h`):

- `_frameBufferData[CONFIG_LCD_WIDTH * CONFIG_LCD_HEIGHT]` = 256×64 = **16,384 B** — the bulk.
- `Pages _pages` = **8,568 B** (54 page objects, all resident; fattest: TeletypeScriptViewPage 864, CurveSequenceEditPage 840, NoteSequenceEditPage 752).
- managers/canvas/leds/keystate ≈ remainder.

## The waste

The UI framebuffer is **8 bits per pixel** (`FrameBuffer8bit = FrameBuffer<uint8_t>`), but the OLED is **4-bit grayscale**. `Lcd::draw` (`src/platform/stm32/drivers/Lcd.cpp:134-139`) reads the 8-bit buffer, clamps every pixel to 0..15, and packs two pixels per byte into a separate 4bpp DMA buffer:

```
for (i in W*H/2):
    a = *src++; b = *src++;
    *dst++ = min(b,15) | (min(a,15) << 4);
```

So the top 4 bits of all 16,384 pixels are dead. The 8-bit buffer exists only so `Canvas` can address one plain `uint8_t` per pixel.

## Options

- **A — 4bpp packed framebuffer (recommended).** 16,384 → **8,192 B**, reclaim **8 KB CCMRAM**. 16 gray levels keep blend modes (Add/Sub) intact. Deletes the driver down-pack loop (framebuffer DMAs almost directly). Cost: every `Canvas` pixel op becomes a nibble read-modify-write (2 px/byte); ~2× per-pixel work, but frame draws are not the F405 bottleneck. Blast radius: `core/gfx` (FrameBuffer + Canvas) + both Lcd drivers (STM32 + sim).
- **B — 2bpp (4 levels).** 16,384 → 4,096 B, −12 KB. Rejected: kills grayscale/dimming and the Sub/Add blends the UI relies on (MediumLow/MediumBright); forces up-conversion on push.
- **C — page arena / union.** Placement-new pages into a shared buffer on navigation instead of keeping all 54 resident (mirrors the FileManager scene-load scratch union). Saves ~7 KB of the 8,568 B `Pages`. Rejected for now: high complexity (page lifetimes, state-loss on re-entry), real risk to the working UI nav path. Secondary lever if A is insufficient.

## Why this matters even though CCMRAM is comfortable

CCMRAM has 8.9 KB free; **main `.bss` is the constraint (~900 B)**. So 4bpp's value is *indirect*: freeing 8 KB CCMRAM lets fat `.bss` statics relocate into CCMRAM, directly buying back main-RAM headroom. The framebuffer shrink is the enabler; the `.bss → CCMRAM` relocation is what relieves the crunch.

Relocation candidates: the 3.6 KB file-load scratch (`gTeletypeLoadScratch` in `FileManager.cpp`), and other large file-scope statics — see the `.bss` research that follows this doc.

## Status

Brainstorm only. No code changed. Next step if pursued: scope Option A (4bpp `FrameBuffer` variant + `Canvas` nibble accessors + both Lcd drivers), measure, then relocate a `.bss` static into the reclaimed CCMRAM.
