# teletype-performer-ecosystem-redesign — TASK.md

## Goal
Full architectural understanding of the Teletype→Performer pipeline from script input through CV/gate generation. Map layer boundaries, static vs dynamic ownership, and data flow to inform redesign decisions — where the TeletypeEditPage, LayoutPage, and ScriptViewPage should split responsibilities.

## Status
**paused** — 6-layer pipeline mapped. See `OVERVIEW.md` for full helicopter view.

## Next action
Design boundary decisions — what goes on TeletypeEditPage vs LayoutPage vs ScriptViewPage

## See also
- `savings-plan.md` — RAM optimization proposals (redundancies, conflicts, stale patterns with impact/effort/savings)

## Key files
- `src/apps/sequencer/ui/pages/TeletypeScriptViewPage.h/.cpp` — script editing UI + display-only I/O grid
- `src/apps/sequencer/ui/pages/TeletypeEditPage.h/.cpp` — empty placeholder, target for full rewrite
- `src/apps/sequencer/ui/pages/TeletypePatternViewPage.h/.cpp` — pattern data editing UI
- `src/apps/sequencer/ui/pages/LayoutPage.h/.cpp` — track mode + routing config
- `src/apps/sequencer/model/TeletypeTrack.h/.cpp` — persistent model: VM state, PatternSlots, I/O routing, CV config, timing
- `src/apps/sequencer/engine/TeletypeTrackEngine.h/.cpp` — runtime engine: tick, metro, envelopes, LFOs, CV slew, Geode, pulse timing
- `src/apps/sequencer/engine/TeletypeBridge.h/.cpp` — C→C++ callback bridge, ScopedEngine singleton
- `teletype/src/` — C VM (parser, op tables, state structs, tele_io callbacks)
- `src/apps/sequencer/engine/Engine.h/.cpp` — top-level update loop, output collection
- `src/apps/sequencer/engine/CvOutput.h/.cpp` — calibration + DAC write
- `src/platform/stm32/drivers/Dac.h/.cpp` — DAC8568 SPI driver
- `src/platform/stm32/drivers/GateOutput.h/.cpp` — 74HC595 shift register driver
