# PhaseFlux Flash Reduction Notes

Snapshot: 2026-06-05.

PhaseFlux edit-page code is one of the largest remaining custom flash contributors after Asteroids was removed from the build.

## Current Culprit

Primary target:

```text
PhaseFluxEditPage::editSlot(...) = ~9,636 B
```

Source:

```text
src/apps/sequencer/ui/pages/PhaseFluxEditPage.cpp
```

Main bloat cause:

```cpp
auto forEachCell = [&](const std::function<void(PhaseFluxSequence::Stage&)> &fn) { ... };
```

`editSlot()` then calls that helper through many capturing lambdas:

```cpp
forEachCell([&](Stage &s) { s.setTemporalWarp(...); });
forEachCell([&](Stage &s) { s.setPitchWarp(...); });
forEachCell([&](Stage &s) { s.setMaskMelody(...); });
```

`std::function` plus many lambdas inside one giant branch tree is expensive on ARM. Disassembly shows many repeated branch islands inside the same function.

## Proposal

Split edit application into small non-capturing helpers.

Example shape:

```cpp
enum class PhaseFluxStageEdit : uint8_t {
    TemporalCurve,
    TemporalWarp,
    TemporalResponse,
    PulseCount,
    StageLen,
    TemporalMask,
    TemporalWindow,
    TemporalRepeat,
    PitchCurve,
    PitchWarp,
    PitchResponse,
    BasePitch,
    PitchRange,
    PitchDirection,
    PitchWindow,
    PitchRepeat,
    MaskMelody,
    TiltMelody,
    AccumStep,
    PulseAccumStep,
    AccumTrigger,
    PulseAccumTrigger,
};
```

Then:

```cpp
void PhaseFluxEditPage::editSelectedCells(PhaseFluxStageEdit edit, int value);
void PhaseFluxEditPage::editStage(PhaseFluxSequence::Stage &stage, PhaseFluxStageEdit edit, int value);
```

`editSlot()` becomes mostly mapping:

```cpp
case 1:
    editSelectedCells(PhaseFluxStageEdit::TemporalWarp, value);
    break;
```

This keeps the UI dispatch in `editSlot()` while moving repeated cell-iteration mechanics into one ordinary helper.

## Expected Benefit

This may save more than the `Routing::writeTarget(...)` ownership split.

Reasons:

- Removes `std::function` from `editSlot()`.
- Removes many unique lambda closure call sites.
- Shrinks one large branch tree.
- Keeps behavior local to PhaseFlux.

Expected net win: measurable KB-scale reduction, exact amount unknown until STM32 release rebuild.

## Behavior Risks

Preserve exactly:

- Multi-cell selection semantics.
- `_selectedCell` fallback when no selection is active.
- Global Pitch edits that hit `masterStage`, not selected cells.
- Accum N/P split.
- `shift` only where currently used: pitch rate and macro nudges.
- Press-only toggles remain in `togglePressSlot()`.

## Second Pass

Apply the same idea to:

```text
PhaseFluxEditPage::togglePressSlot(...) = ~1,578 B
```

It repeats selected-cell iteration with another `std::function` helper.

Do not start with draw pages. Draw functions are larger, but behavior and OLED layout risk are higher. `editSlot()` is isolated, measurable, and easier to verify.

## Verification

Before and after:

```bash
make -C build/stm32/release sequencer
/opt/homebrew/bin/arm-none-eabi-size -A build/stm32/release/src/apps/sequencer/sequencer
/opt/homebrew/bin/arm-none-eabi-nm --print-size --size-sort --radix=d \
  build/stm32/release/src/apps/sequencer/sequencer \
  | /opt/homebrew/bin/arm-none-eabi-c++filt \
  | rg 'PhaseFluxEditPage::editSlot|PhaseFluxEditPage::togglePressSlot|PhaseFluxStageEdit'
```

Functional checks:

- Single-cell edit still changes only selected cell.
- Multi-cell edit changes all selected cells.
- Global Pitch page still edits master stage where required.
- Accum N and Accum P edit their own fields.
- Macro nudges still honor `shift`.
- Press-only Flip/Snap/Zero behavior is unchanged.
