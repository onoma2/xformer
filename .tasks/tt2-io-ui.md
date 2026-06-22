# tt2-io-ui

## Goal
Display polish on the TT2 I/O config page and the script-view live HUD: readable 2-letter source
codes, full-width spread, a hardware float-printf fix, and the HUD laid out as one clean rectangle.

## Key files
- `src/apps/sequencer/ui/pages/TT2IoConfigPage.cpp` — source codes, INPUTS spread, OFFST cell
- `src/apps/sequencer/ui/pages/TeletypeScriptViewPage.cpp` — `drawHud` (CV/TR/TI squares + IN/PARAM/BUS bars)
- `ui-preview/render_tt2_io.py`, `ui-preview/render_tt2_script.py` — OLED mocks

## Decisions log
- 2026-06-21: Script HUD — TI trigger-input squares grew 4→8 and overflowed the IN/PARAM/BUS bars. Re-laid as 2 rows of 4 (cols 0-3, x192-222) at y38/y45; IN/PARAM/BUS bars extended to span the same y38-50 band so the dashboard reads as one rectangle. Measured: bottom band x192-254, y38-50; bars h 12→13 to reach the TI bottom. Commit `f76cee25`.
- 2026-06-21: TT2 I/O page — 2-letter source codes (IN/CV/GT/RT/LC/LG, None=--) replacing ambiguous single letters; INPUTS view spread full-width (TRIG 2-up / CV IN / MIDI); OFFST column formats via `stbsp_snprintf` not `std::snprintf`; renamed OFF→OFFST; fixed latent bug where trigger inputs 6-8 were never drawn (single column capped at 5 rows). Commits `4ea0a833`.
- 2026-06-21: TT2 I/O enum audit — inputs are complete (superset of master TT1: trigger sources, CV-input sources, IN/PARAM/X/Y/Z/T roles all match). Output destination routing (master's TriggerOutputDest/CvOutputDest) intentionally absent — owner: TT2 outputs are fixed + additive. Not a gap.

## Open questions
- [ ] Hardware audition of the OFFST column (the float fix) and the HUD rectangle.

## Completed steps
- [x] I/O page 2-letter codes + spread + OFFST float fix + OFF→OFFST. STM32 + sim build clean. Commit `4ea0a833`.
- [x] Script HUD 2-row TI squares + aligned bottom band. Sim clean, ui-preview verified. Commit `f76cee25`.

## Notes
**HW float-printf gotcha (reusable):** STM32 links `--specs=nano.specs` with no `-u _printf_float`,
so raw `std::snprintf("%f", …)` produces nothing on hardware (renders fine in the sim's glibc). Use
`FixedStringBuilder` / `stbsp_snprintf` (stb_sprintf) for any fractional/voltage display — that's
why every other voltage renders on HW.
