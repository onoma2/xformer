# global-modulators-v1

## Goal
Port Modulove-style global LFO modulators into XFORMER as a standalone feature, starting with a smaller V1: project-level modulators that can offset physical CV outputs after track/routing selection. This fills a real gap between CurveTrack sequenced CV, Teletype script-local LFOs, and RoutingEngine shapers.

## Status
Paused. Extracted from `.tasks/performer-improvements/TASK.md`; no code started.

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
- [ ] Should modulation be additive only in V1, or should there be bipolar attenuverter/depth per output assignment?
- [ ] Should modulator routing attach to physical CV output channel or logical output before CV rotation?
- [ ] Should CV route lanes be modulatable the same way as track CV outputs? V1 answer should probably be yes, because the target is physical CV channel.
- [ ] How should modulation interact with `_cvOutputOverride`? V1 answer should probably be no modulation while override is active.
- [ ] Should random smoothing be included in V1, or should random be stepped until the core path is stable?

## Completed steps
- [x] Task wiki scan found existing Modulove LFO modulator analysis in `performer-improvements`.
- [x] Reference files confirmed present in `temp-ref/modulove-performer`.
- [x] Dedicated task folder and V1 plan created.

## Notes
- This is a feature task, not a RAM recovery task. It must still pass the current RAM budget gates.
- Do not start with the 956-line Modulove `ModulatorPage.cpp`. First prove model, engine, project serialization, and output injection.
- Keep the implementation independent from `.tasks/teletype-cv-source-combiner`.
