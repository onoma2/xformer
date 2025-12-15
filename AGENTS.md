# AGENTS Session Notes

## Codebase overview
- Firmware for PER|FORMER/XFORMER sequencer: STM32F405 hardware app plus macOS/Linux simulator.
- Main app `src/apps/sequencer/Sequencer.cpp` sets up drivers (ADC/DAC/gates/MIDI/LCD/SD), model/engine/UI, and FreeRTOS tasks.
- Engine/model/UI split: `engine` runs tracks/clock/algorithms, `model` holds project/tracks/settings/state, `ui` draws 128x64 LCD and handles keys/LEDs/MIDI.

## Resource footprint (current release build)
- Flash: `.text` ~413 KB (≈39% of 1 MB), ample headroom.
- SRAM (128 KB): `.data` 6 KB + `.bss` ~110 KB → ~15 KB free. Largest blocks:
  - `Model` ~93 KB: 8 tracks each with 17 `NoteSequence`s of 64 packed steps; includes project globals, settings, clipboard, harmony.
  - LCD driver buffer 8 KB.
- CCM RAM (64 KB): `.ccmram_bss` ~44.7 KB → ~21 KB free. Largest blocks:
  - `Ui` ~22.9 KB: framebuffer 8 KB, page objects, MIDI ring buffer, key/LED state.
  - FreeRTOS static stacks: UI/engine ~4 KB each; USBH/file/profiler ~2 KB; driver ~1 KB.
  - `Engine` ~6.5 KB.
- FreeRTOS heap disabled (static allocation only).

## RAM-heavy structures
- `Model`: per-track `NoteSequence` arrays (17 patterns/snapshots × 64 steps × 2x32-bit bitfields with pulse count, gate mode, accumulator, harmony overrides), plus routing/song/MIDI settings.
- `Ui`: framebuffer, `Pages` aggregate of all page instances and list/selection models, MIDI receive ring buffer.
- Misc: FS pools, USB/MIDI state, idle/timer stacks are small compared to above.

## Considerations
- RAM is the tight constraint; flash has plenty of margin.
- To free RAM: shrink note-step fields/pattern count/snapshots, reduce UI/page caches, or trim task stack sizes; prefer moving non-DMA data to CCM if SRAM pressure rises.
