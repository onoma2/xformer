# Curve Track 16-Step Capacity Refactor

Goal: make Curve track use 16 steps capacity only (not global `CONFIG_STEP_COUNT`). No legacy projects to preserve.

## Plan

1) **CurveSequence step count constant**
- Add `static constexpr int MaxSteps = 16;` in `src/apps/sequencer/model/CurveSequence.h`.
- Replace `CONFIG_STEP_COUNT` with `CurveSequence::MaxSteps` for:
  - `StepArray` size
  - `setLastStep` clamp
  - `editFirstStep/editLastStep` clamps
  - any bounds checks/loops in the model
- Update `src/apps/sequencer/model/CurveSequence.cpp` LFO generator bounds and clamps to use `CurveSequence::MaxSteps`.

2) **Engine bounds**
- `src/apps/sequencer/engine/CurveTrackEngine.h`: use `CurveSequence::MaxSteps` in `setMonitorStep` bounds.

3) **UI selection + LFO actions**
- `src/apps/sequencer/ui/pages/CurveSequenceEditPage.h`: change `StepSelection<CONFIG_STEP_COUNT>` to `StepSelection<CurveSequence::MaxSteps>`.
- `src/apps/sequencer/ui/pages/CurveSequenceEditPage.cpp`: replace `CONFIG_STEP_COUNT - 1` with `CurveSequence::MaxSteps - 1` where curve steps are iterated.
- `src/apps/sequencer/ui/pages/CurveSequencePage.cpp`: update LFO context actions to use `CurveSequence::MaxSteps - 1`.

4) **Launchpad controller**
- `src/apps/sequencer/ui/controllers/launchpad/LaunchpadController.cpp`: change curve sequence loops from `CONFIG_STEP_COUNT` to `CurveSequence::MaxSteps`.
- Decide mapping for 16 steps (simplest: first 16 pads).

5) **Clipboard / Selected steps**
- Add `using CurveSelectedSteps = std::bitset<CurveSequence::MaxSteps>;` in `src/apps/sequencer/model/ClipBoard.h`.
- Update `CurveSequenceSteps` to use `CurveSelectedSteps` instead of `SelectedSteps`.
- Update `copyCurveSequenceSteps()` / `pasteCurveSequenceSteps()` signatures and uses in curve UI.
- `ModelUtils::copySteps` already supports templated N.

6) **Python bindings**
- `src/apps/sequencer/python/project.cpp`: curve sequence `steps` property should iterate `CurveSequence::MaxSteps`.

7) **Tests**
- `src/tests/unit/sequencer/TestCurveTrackLfoShapes*.cpp`: replace `CONFIG_STEP_COUNT` with `CurveSequence::MaxSteps`.

## Notes
- Serialization will now write/read only 16 steps. With no legacy projects, no migration needed.
- Global `CONFIG_STEP_COUNT` remains 64 for other tracks.
