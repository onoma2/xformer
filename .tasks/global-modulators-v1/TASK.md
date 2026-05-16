# global-modulators-v1

## Goal
Port Modulove-style global LFO modulators into XFORMER as a standalone feature, starting with a smaller V1: project-level modulators that can offset physical CV outputs after track/routing selection. This fills a real gap between CurveTrack sequenced CV, Teletype script-local LFOs, and RoutingEngine shapers.

## Status
**Complete on `feat/modulators`.** V1 engine + full Modulove-matching UI (including ADSR shape and MIDI CC routing) implemented and building cleanly. All non-goals from original V1 plan have been resolved. Branch ready for hardware verification or merge.

## V1 scope
- 8 global modulators (`CONFIG_MODULATOR_COUNT = 8`).
- Waveforms: sine, triangle, saw up, saw down, square, random.
- Modes: free-running and gate-sync/retrigger (matching Modulove — phase always advances, gate resets on rising edge only).
- Output target: physical CV outputs only.
- Modulator value applied as post-track/post-routing CV offset in volts.
- Project serialization (unguarded — reads unconditionally per project rule).
- Full Modulove-matching UI: waveform viz with playhead, level bar, dynamic footer, track LED selection, context menu for CV routing.

## Explicit non-goals for V1 (ALL RESOLVED in current implementation)
- ~~No MIDI CC output routing.~~ — Ported: `FirstModulator`/`LastModulator` in `ControlSource`, `sendModulator()` via throttled `OutputState`, routing overlay with TARGET/EVENT/CCNUM.
- ~~No ADSR shape.~~ — Ported: model fields + engine state machine + pagination.
- ~~No full Modulove routing overlay.~~ — Ported: 5-function Shift+Page overlay (MODE/GATE/TARGET/EVENT/CCNUM).
- ~~No quick-map popup beyond context menu Route toggle.~~ — Unchanged; context menu still does CV routing only.

## V2 scope (all original items now resolved; next steps are enhancements, not gaps)
- ~~ADSR shape~~ — Done.
- ~~MIDI CC output routing~~ — Done.
- ~~Shift+Page routing overlay~~ — Done.

### Possible next enhancements (not required for parity)
- Quick-map popup for one-shot MIDI output assignment.
- Monitor page modulator readout.
- Additional LFO waveforms (variable pulse, S&H, chaos).
- Per-output attenuverter beyond global Depth.
- MIDI Note mode for modulators (unimplemented in Modulove too).
- Multi-target routing (one modulator → multiple outputs).

## Key files
- `temp-ref/modulove-performer/src/apps/sequencer/model/Modulator.h` — reference model and serialization.
- `temp-ref/modulove-performer/src/apps/sequencer/engine/ModulatorEngine.h` — reference runtime engine; measured around 256 B.
- `temp-ref/modulove-performer/src/apps/sequencer/engine/WaveformGenerator.h` — shared waveform generation for engine/UI.
- `temp-ref/modulove-performer/src/apps/sequencer/ui/pages/ModulatorPage.h`
- `temp-ref/modulove-performer/src/apps/sequencer/ui/pages/ModulatorPage.cpp` — reference UI, too large for V1 direct port.
- `temp-ref/modulove-performer/src/apps/sequencer/engine/Engine.cpp` — reference integration: tick modulators and add offset in `updateTrackOutputs()`.
- `src/apps/sequencer/Config.h` — add `CONFIG_MODULATOR_COUNT`.
- `src/apps/sequencer/model/Project.h` / `.cpp` — likely home for modulator array and per-CV-output routing.
- `src/apps/sequencer/model/FileManager.cpp` / serialization helpers — project save/load integration.
- `src/apps/sequencer/engine/Engine.h` / `.cpp` — tick/update `ModulatorEngine`, apply physical CV offset after output selection.
- `src/apps/sequencer/engine/CvOutput.h` / `.cpp` — final DAC channel write target, not first choice for ownership.

## Current source facts
- Modulove has a `ModulatorEngine` runtime cost around 256 B, documented in `.tasks/engine-subobject-size-comparison.md`.
- Modulove applies modulation in `Engine::updateTrackOutputs()` after obtaining the track CV value and before `_cvOutput.setChannel()`.
- XFORMER `Engine::updateTrackOutputs()` has CV rotation, interpolated rotation, CV route lanes, and output override. Therefore the port must define physical-output ownership carefully instead of copy-pasting Modulove.
- XFORMER currently has no global `ModulatorEngine`.

## Decisions log
- 2026-05-15: Extracted LFO modulators from broad `performer-improvements` into this dedicated task.
- 2026-05-15: V1 target is physical CV output modulation only. This avoids track/container semantics and keeps the integration point after routing/rotation.
- 2026-05-15: MIDI CC routing, ADSR, quick-map UI, and full Modulove page are deferred until model/engine/output integration is measured.
- 2026-05-15: Modulator application law for V1: compute modulator output as a voltage offset and add it to the final physical CV value before DAC channel write, then clamp to DAC output range.
- 2026-05-16: Chaos shapes (Lorenz/Latoocarfian) ported. Parabolic parameter curves (`p1curve = p1n*(2-p1n)`) expand sweet spot. Default `None` target in routing overlay. Slew (`smooth`) applied to chaos output via shared `_lastRandomValue` buffer. Rate display fixed (`<1000` cutoff).

## Open questions
- [x] Should modulation be additive only in V1, or should there be bipolar attenuverter/depth per output assignment? → Additive offset only per V1 plan.
- [x] Should modulator routing attach to physical CV output channel or logical output before CV rotation? → Physical CV output channel per V1 plan.
- [x] Should CV route lanes be modulatable the same way as track CV outputs? → Yes per V1 plan; targets physical CV channel.
- [x] How should modulation interact with `_cvOutputOverride`? → No modulation while override is active per V1 plan.
- [x] Should random smoothing be included in V1, or should random be stepped until the core path is stable? → Include smooth/slew per V1 plan (matches Modulove reference).



## Remaining gaps to full Modulove parity (ALL RESOLVED)
- ~~ADSR shape~~ — Done. Model + engine + pagination implemented.
- ~~MIDI CC output routing~~ — Done. `FirstModulator`/`LastModulator` in `ControlSource`, `sendModulator()` via throttled `OutputState`, 5-function Shift+Page overlay.
- ~~Pagination~~ — Done. ADSR uses 2 pages (SHAPE/ATTACK/DECAY/SUSTAIN/RELEASE on Pg 1, AMPLITUDE/DEPTH/OFFSET on Pg 2).
- ~~Waveform cache only invalidates on parameter change~~ — Unchanged from V1; sufficient for now.
- [x] Phase 6: RAM gate check passed. Build clean.
- Hardware verification: pending user testing.

## RAM measurements (feat/modulators branch)
- `.data` = 6,320 bytes (unchanged from baseline)
- `.bss` = 112,248 bytes (was 113,640 on master; branch includes other optimizations)
- `.data + .bss` = 118,568 bytes (90.5% of 128 KB, within 120 KB hard warning)
- `.ccmram_bss` = 54,436 bytes (83.1% of 64 KB)
- `.text` = 774,020 bytes (flash has ~250 KB headroom in 1 MB)
- Feature BSS delta: ~328 bytes (model arrays + engine state, not in any container)
- Feature CCMRAM delta: ~112 bytes (ModulatorPage waveform cache)
- Feature flash delta: ~6 KB (ModulatorPage code + model code + WaveformGenerator inlines)
- Track container gate: NoteTrack=9544B unchanged. Modulator is direct Project member, not in Track::_container.
- Engine container: TeletypeTrackEngine=912B unchanged. ModulatorEngine is direct Engine member, not in TrackEngineContainer.

### Detailed BSS breakdown (modulators only)
| Member | Size | Location |
|--------|------|----------|
| Modulator × 8 (model array) | 8 × ~15B = ~120B | Project, .bss |
| cvOutputModulator[8] | 8B | Project, .bss |
| selectedModulatorIndex | 4B | Project, .bss |
| ModulatorEngine state | ~184B | Engine, .bss |
| ModulatorPage cache | 112B | Pages, .ccmram_bss |
| **Total feature RAM** | **~428B** | |

### Container gate verification
- `Track::_container` max arm: `NoteTrack = 9544B` — **no inflation**
- `Engine::TrackEngineContainer` max arm: `TeletypeTrackEngine = 912B` — **no inflation**
- `ModulatorEngine` is a direct `Engine` member, **not** in any variant container
- `Modulator[]` is a direct `Project` member, **not** in any variant container
- Feature adds ~428B total across SRAM + CCMRAM, well within budget

## Completed steps
- [x] Task wiki scan found existing Modulove LFO modulator analysis in `performer-improvements`.
- [x] Reference files confirmed present in `temp-ref/modulove-performer`.
- [x] Dedicated task folder and V1 plan created.
- [x] Phase 1: Modulator model (Modulator.h/Modulator.cpp), CONFIG_MODULATOR_COUNT, Project serialization (Version 35), cvOutputModulator array.
- [x] Phase 2: WaveformGenerator.h (header-only, 5 waveforms), ModulatorEngine.h (header-only, phase accumulator + random state).
- [x] Phase 3: Modulator tick in Engine::update() tick loop, gate source from configured track.
- [x] Phase 4: applyModulatorOffset() in Engine::updateTrackOutputs() for all three CV output paths (rotation, track-CV, route-lane). No modulation during override.
- [x] Phase 5: ModulatorListModel + ModulatorPage (ListPage), TopPage Mode::Modulator navigation, Pages.h integration.
- [x] Phase 5b: Full UI rewrite — ModulatorPage as BasePage with waveform viz, playhead, level bar, dynamic footer, track LED selection, context menu for CV routing. selectedModulatorIndex in Project. Removed ModulatorListModel.
- [x] Phase 5c: ADSR shape — model fields (`attack`/`decay`/`sustain`/`release`/`amplitude`), engine state machine (Idle→Attack→Decay→Sustain→Release), 2-page ModulatorPage UI.
- [x] Phase 5d: MIDI CC routing — `ControlSource::FirstModulator..LastModulator`, `sendModulator()` in `OutputState`, `applyModulatorOffset()` in Engine::tick.
- [x] Phase 5e: Routing overlay — Shift+Page toggle, 5 functions (MODE/GATE/TARGET/EVENT/CCNUM), TARGET cycles MIDI 1-16 + CV 1-8, default None target.
- [x] Phase 5f: Chaos shapes (Lorenz/Latoocarfian) — added to Shape enum, `isChaosShape()` helper, Hz-mode rate editing, 2-page UI (Pg1: RATE/P1/P2/DEPTH, Pg2: SLEW/OFFSET), live horizontal-line waveform, parabolic parameter curves.
- [x] Phase 5g: None target for routing overlay — default unassigned, clear-on-apply with deduplication, prevents Modulove leak of simultaneous MIDI+CV assignment.
- [x] Phase 5h: Slew on chaos — `_lastRandomValue` reused as shared smoothed buffer (Random and Chaos mutually exclusive), cleared on retrigger.
- [x] Phase 5i: Rate display fix — `hzTenths < 1000` shows decimal instead of dropping it at `< 100`.
- [x] Phase 6: RAM gate check passed. Build clean.
- [x] Hardware verified — user confirmed chaos shapes, slew, None target working.

## Notes
- This is a feature task, not a RAM recovery task. It must still pass the current RAM budget gates.
- Do not start with the 956-line Modulove `ModulatorPage.cpp`. First prove model, engine, project serialization, and output injection.
- Keep the implementation independent from `.tasks/teletype-cv-source-combiner`.
