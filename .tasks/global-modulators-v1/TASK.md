# global-modulators-v1

## Goal
Port Modulove-style global LFO modulators into XFORMER as a standalone feature, starting with a smaller V1: project-level modulators that can offset physical CV outputs after track/routing selection. This fills a real gap between CurveTrack sequenced CV, Teletype script-local LFOs, and RoutingEngine shapers.

## Status
Active. V1 engine + full Modulove-matching UI implemented on `feat/modulators`. Phase 6 (hardware verification) pending.

## V1 scope
- 8 global modulators (`CONFIG_MODULATOR_COUNT = 8`).
- Waveforms: sine, triangle, saw up, saw down, square, random.
- Modes: free-running and gate-sync/retrigger if cheap to preserve from Modulove.
- Output target: physical CV outputs only.
- Modulator value applied as post-track/post-routing CV offset in volts.
- Basic model serialization for project save/load.
- Basic UI page can come after model/engine/output integration is measured.

## Explicit non-goals for V1
- No MIDI CC output routing.
- No ADSR until the core LFO/random path is working and measured.
- No quick-map popup.
- No full Modulove page clone before engine/model proof.
- No interaction with Teletype CV source combiner. This is a global output layer; Teletype combiner is local Teletype source ownership.

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

## Open questions
- [x] Should modulation be additive only in V1, or should there be bipolar attenuverter/depth per output assignment? → Additive offset only per V1 plan.
- [x] Should modulator routing attach to physical CV output channel or logical output before CV rotation? → Physical CV output channel per V1 plan.
- [x] Should CV route lanes be modulatable the same way as track CV outputs? → Yes per V1 plan; targets physical CV channel.
- [x] How should modulation interact with `_cvOutputOverride`? → No modulation while override is active per V1 plan.
- [x] Should random smoothing be included in V1, or should random be stepped until the core path is stable? → Include smooth/slew per V1 plan (matches Modulove reference).

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

## Remaining gaps to full Modulove parity (not in V1 scope)
- ADSR shape (7th waveform with attack/decay/sustain/release/amplitude)
- MIDI CC output routing (routing overlay with target selection, CC number, Note vs CC)
- Pagination (ADSR needs 2 pages — not needed without ADSR)
- Waveform cache only invalidates on parameter change (could add engine tick invalidation)
- [ ] Phase 6: Hardware verification and RAM gate check.

## RAM measurements (feat/modulators branch)
- `.data` = 6,320 bytes (unchanged from baseline)
- `.bss` = 112,248 bytes (was 113,640 baseline; branch includes other optimizations)
- `.data + .bss` = 118,568 bytes (90.4% of 128 KB, within 120 KB hard warning)
- `.ccmram_bss` = 54,332 bytes
- `.text` = 790,244 bytes (flash has ample headroom)
- Delta from this feature: ~364 bytes BSS (8 Modulators × ~15B model + 8B cvOutputModulator + ~192B ModulatorEngine + UI state)
- Track container gate: NoteTrack=9544B unchanged. Modulator does not inflate Track::_container.
- Engine container: TeletypeTrackEngine=912B unchanged. ModulatorEngine is separate from the engine container.

## Notes
- This is a feature task, not a RAM recovery task. It must still pass the current RAM budget gates.
- Do not start with the 956-line Modulove `ModulatorPage.cpp`. First prove model, engine, project serialization, and output injection.
- Keep the implementation independent from `.tasks/teletype-cv-source-combiner`.
