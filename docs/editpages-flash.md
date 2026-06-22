# Edit Page Flash Triage

> **Update 2026-06-13 — superseded by `docs/ui-editpages-flash.md`** (consolidated
> research with current symbol sizes; the numbers below are stale). Two corrections:
> the Stochastic descriptor-table target is `contextAction` (3,448 B, where the
> 16-slot map is triplicated), NOT `editLiveStep` (now only 748 B); and the dominant
> flash lever is **compiler flags** (`-funroll-loops` removal, `-Os`, `-flto`) — far
> larger than any single page — which none of these per-page notes covered.

Snapshot: 2026-06-05.

This note excludes the detailed PhaseFlux proposal in `docs/pf-flash.md` and ranks the other edit-page flash contributors.

## Object Ranking

Fresh STM32 release object sizes:

```text
32.9K  IndexedSequenceEditPage.cpp.obj
30.4K  StochasticSequenceEditPage.cpp.obj
30.2K  CurveSequenceEditPage.cpp.obj
21.3K  NoteSequenceEditPage.cpp.obj
11.9K  TuesdayEditPage.cpp.obj
 4.6K  IndexedStepsPage.cpp.obj
 1.4K  AccumulatorStepsPage.cpp.obj
 0.8K  QuickEditPage.cpp.obj
 0.4K  TeletypeEditPage.cpp.obj
```

## Best Target: Stochastic

Large symbols:

```text
5,048  StochasticSequenceEditPage::drawLivePage
4,312  StochasticSequenceEditPage::drawLoopPage
3,440  StochasticSequenceEditPage::contextAction
3,028  StochasticSequenceEditPage::drawPitchPage
1,772  StochasticSequenceEditPage::updateLeds
1,028  StochasticSequenceEditPage::handleLiveFunction
1,028  StochasticSequenceEditPage::handleLoopFunction
1,006  StochasticSequenceEditPage::editLiveStep
```

Shape problem:

- Page-specific code repeats slot maps.
- `editLiveStep`, `contextAction` Init, `contextAction` Random, labels, and LED handling all encode the same 16 live slots separately.
- There are many local lambdas, but no `std::function` issue like PhaseFlux.

Proposal:

- Introduce static live/loop slot descriptor tables.
- Each descriptor owns label, formatter type, init value, random range, edit behavior, and notify behavior.
- Use the same table in draw labels, edit, init, random, and LED paths.

Expected benefit:

- Likely KB-scale win.
- Behavior risk medium because the table must preserve per-slot defaults, random ranges, and notification side effects exactly.

## Second Target: Curve

Large symbols:

```text
3,884  CurveSequenceEditPage::draw
3,448  CurveSequenceEditPage::encoder
1,596  CurveSequenceEditPage::keyPress
1,062  CurveSequenceEditPage::macroContextAction
1,062  CurveSequenceEditPage::transformContextAction
  884  CurveSequenceEditPage::gatePresetsContextAction
  856  CurveSequenceEditPage::updateLeds
  796  CurveSequenceEditPage::drawDetail
```

Shape problem:

- `AdvancedGateMode` name switches are duplicated in encoder and draw detail.
- Wavefolder and chaos row dispatch repeat the same shape.
- Layer switches are repeated across draw, encoder, keyPress, and drawDetail.

Proposal:

- Add shared `advancedGateModeShortName()` and `advancedGateModeLongName()` helpers.
- Add small row edit helpers for wavefolder and chaos rows.
- Consider a layer metadata table later if the first pass measures well.

Expected benefit:

- Smaller win than Stochastic.
- Lower behavior risk.

## Third Target: Indexed

Large symbols:

```text
3,588  IndexedSequenceEditPage::draw
2,832  IndexedSequenceEditPage::encoder
1,444  IndexedSequenceEditPage::rotateToFirstSelected
1,208  IndexedSequenceEditPage::applyVoicing
1,172  IndexedSequenceEditPage::splitStepInto
1,004  IndexedSequenceEditPage::keyDown
```

Shape problem:

- Feature-rich page with less obvious dead duplication.
- Draw path has timeline math and a local `drawSegment` lambda.
- Encoder repeats edit-mode and layer decisions.

Proposal:

- Do not start here.
- Extract timeline draw helper only if the remaining flash pressure justifies it.
- Treat as higher risk than Stochastic and Curve.

## Note Page

Large symbols:

```text
2,904  NoteSequenceEditPage::draw
1,592  NoteSequenceEditPage::drawDetail
1,088  NoteSequenceEditPage::encoder
  946  NoteSequenceEditPage::keyPress
```

Shape problem:

- Mostly normal layer switches and text labels.
- Some repeated enum-label switches for gate mode, harmony role, inversion, and voicing.

Proposal:

- Possible label-table cleanup.
- Not a first-pass flash target.

Expected benefit:

- Modest.

## Tuesday Page

`TuesdayEditPage.cpp.obj` is about 11.9K.

It has repeated param switch tables, but the total object is already comparatively small.

Proposal:

- Do not prioritize unless later flash work needs small cleanup wins.

## Recommended Order

1. PhaseFlux `editSlot()` helper refactor. See `docs/pf-flash.md`.
2. Stochastic live/loop slot descriptor table.
3. Curve gate-mode names and row helpers.
4. Routing ownership split. See `docs/routing-flash.md`.
5. Indexed only if still flash-constrained.

## Measurement Commands

Object ranking:

```bash
/opt/homebrew/bin/arm-none-eabi-size build/stm32/release/src/apps/sequencer/libsequencer_shared.a \
  | rg 'EditPage|SequenceEditPage|StepsPage|TrackPage' \
  | awk '{print $1+$2, $0}' \
  | sort -n
```

Large edit-page symbols:

```bash
/opt/homebrew/bin/arm-none-eabi-nm --print-size --size-sort --radix=d \
  build/stm32/release/src/apps/sequencer/sequencer \
  | /opt/homebrew/bin/arm-none-eabi-c++filt \
  | rg 'EditPage::|SequenceEditPage::|StepsPage::|TrackPage::' \
  | tail -140
```
