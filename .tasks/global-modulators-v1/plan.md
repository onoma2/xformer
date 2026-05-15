# Global Modulators V1 Plan

## Goal
Implement a reduced Modulove-style global modulator system for XFORMER: 8 project-level LFO/random modulators that can offset physical CV outputs after track/routing selection.

## Architecture

```text
Project::modulators[8]
Project::cvOutputModulator[8]
        |
Engine ticks ModulatorEngine once per clock/update
        |
Engine::updateTrackOutputs()
  track/routed CV value
  + optional modulator voltage offset
  -> _cvOutput.setChannel()
```

V1 deliberately targets the physical CV output layer. It should not alter track engines, Teletype internals, RoutingEngine shaper state, or CV route source semantics.

## Phase 0: Reference Diff and Size Gate

Files to inspect:
- `temp-ref/modulove-performer/src/apps/sequencer/model/Modulator.h`
- `temp-ref/modulove-performer/src/apps/sequencer/engine/ModulatorEngine.h`
- `temp-ref/modulove-performer/src/apps/sequencer/engine/WaveformGenerator.h`
- `temp-ref/modulove-performer/src/apps/sequencer/engine/Engine.cpp`
- `temp-ref/modulove-performer/src/apps/sequencer/model/Project.h`

Steps:
- Confirm all reference code compiles under XFORMER includes or list required adaptation points.
- Record current STM32 release size before code changes.
- Record current MonitorPage values if size probes are still present.

Expected output:
- Short implementation note with exact reference members to port and exact members to defer.

## Phase 1: Add Model Types Without Engine Integration

Files:
- Create `src/apps/sequencer/model/Modulator.h`
- Modify `src/apps/sequencer/Config.h`
- Modify `src/apps/sequencer/model/Project.h`
- Modify project serialization files if `Project` has explicit read/write helpers outside `Project.h`

Steps:
- Add `CONFIG_MODULATOR_COUNT 8`.
- Port a reduced `Modulator` model:
  - shape
  - mode
  - random mode
  - rate
  - depth
  - offset
  - phase
  - smooth
  - gate track
- Defer ADSR fields if they force UI/engine scope.
- Add `std::array<Modulator, CONFIG_MODULATOR_COUNT>` to `Project`.
- Add per-physical-CV-output modulator assignment:

```cpp
// 0 = none, 1..CONFIG_MODULATOR_COUNT = modulator index + 1
std::array<uint8_t, CONFIG_CHANNEL_COUNT> _cvOutputModulators{};
```

Expected result:
- STM32 release build passes.
- Project default initializes with no modulation assigned.
- RAM delta measured.

## Phase 2: Port Waveform and Runtime Engine

Files:
- Create `src/apps/sequencer/engine/WaveformGenerator.h`
- Create `src/apps/sequencer/engine/ModulatorEngine.h`
- Modify `src/apps/sequencer/engine/Engine.h`
- Modify `src/apps/sequencer/engine/Engine.cpp`

Steps:
- Port `WaveformGenerator` from Modulove with only required V1 shapes.
- Port `ModulatorEngine` with:
  - phase accumulator
  - current value
  - random target/current state
  - last gate state
  - per-modulator RNG
- Defer ADSR state unless included intentionally.
- Add `ModulatorEngine _modulatorEngine;` to `Engine`.
- Reset it during engine/model setup.

Expected result:
- STM32 release build passes.
- Runtime RAM delta measured; expected order is a few hundred bytes, not KB.

## Phase 3: Tick Modulators

Files:
- Modify `src/apps/sequencer/engine/Engine.cpp`

Steps:
- Tick each modulator from the main engine timing path.
- Gate source for Sync/Retrigger comes from selected track's first gate output, matching Modulove's first implementation.
- Make clock/tick-rate assumptions explicit in comments if XFORMER timing differs from Modulove.

Expected result:
- Modulator values change over time.
- No output behavior change yet if no CV output assignment is active.

## Phase 4: Apply to Physical CV Outputs

Files:
- Modify `src/apps/sequencer/engine/Engine.cpp`
- Possibly modify `src/apps/sequencer/model/Project.h`

Steps:
- In `Engine::updateTrackOutputs()`, after track/routed CV value is selected and before `_cvOutput.setChannel(i, value)`, apply assigned modulator:

```cpp
float Engine::applyCvOutputModulator(int channel, float cvValue) const;
```

- V1 law:

```text
mod value 0..127
center 64
offset volts = (value - 64) / 64.0
cvValue += offset volts
clamp to -5..+5
```

- Apply to both track-sourced CV and CV route lane output because the target is physical output channel.
- Do not apply while `_cvOutputOverride` is active.

Expected result:
- With no assignment, behavior is unchanged.
- With assignment, physical CV output is modulated by approximately ±1V.

## Phase 5: Minimal UI Access

Files:
- Create a small `ModulatorPage` or add a temporary list page if that matches current UI patterns better.
- Modify `src/apps/sequencer/ui/pages/Pages.h`
- Modify navigation/page selection where needed.

Steps:
- Do not port full Modulove `ModulatorPage.cpp` first.
- Provide enough UI to edit:
  - selected modulator
  - shape
  - rate
  - depth
  - offset
  - target physical CV output assignment
- Waveform preview can be added after the engine path is hardware-confirmed.

Expected result:
- User can configure one modulator and assign it to a CV output from hardware UI.

## Phase 6: Hardware Verification

Tests:
- No-assignment regression: all CV outputs behave exactly as before.
- Sine modulator assigned to CV1: CV1 moves around the underlying track/routed value.
- Depth zero: assignment produces no audible/visible modulation.
- Different physical output assignment: CV2 can be modulated while CV1 remains unmodulated.
- CV route lane source: routed CV output can be modulated at the physical channel.
- CV output override: override value is not modulated.

## Deferred V2
- MIDI CC routing.
- Full ADSR.
- Full Modulove waveform preview page.
- Quick-map popup.
- More output combine modes than additive voltage offset.
- Per-target attenuverter beyond existing depth/offset if V1 proves insufficient.
