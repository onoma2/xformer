# BUG REPORT: Accumulator Trigger Mode Issues

**Date**: 2025-11-18
**Reporter**: Hardware Testing
**Status**: 🔴 CRITICAL - Feature not working as designed
**Affected Version**: Branch `claude/stm-eurorack-cv-oled-01BX54cUuKHJrHRTHbUeC4TG`

---

## Bug #1: GATE and STEP Trigger Modes Not Working

### Description
Accumulator increments on every gate pulse regardless of the selected TRIG mode setting. Both STEP and GATE modes appear to behave identically to RTRIG mode (ticking on every pulse/gate).

### Expected Behavior

**STEP Mode:**
- Should tick **once per step** when accumulator trigger is enabled
- Independent of pulse count, gate mode, and retrigger settings
- Example: 4 steps with triggers → 4 increments total

**GATE Mode:**
- Should tick **once per gate pulse fired**
- Respects pulse count and gate mode settings
- Example: `pulseCount=3, gateMode=ALL` → 4 ticks (pulses 1, 2, 3, 4)
- Example: `pulseCount=3, gateMode=FIRST` → 1 tick (pulse 1 only)

**RTRIG Mode:**
- Should tick **N times for N retriggers**
- Example: `retrig=3` → 3 ticks
- All ticks fire immediately at step start (known behavior)

### Actual Behavior
All three modes (STEP, GATE, RTRIG) appear to increment the accumulator on every gate pulse/repetition, ignoring the TRIG mode setting.

### Steps to Reproduce

1. Navigate to ACCUM page
2. Set TRIG mode to **STEP**
3. Enable accumulator with Direction=Up, Min=0, Max=10, Step=1
4. Navigate to ACCST page (accumulator steps)
5. Enable accumulator trigger on step 1
6. Set step 1 with `pulseCount=3` (4 pulses total)
7. **Expected**: Accumulator increments by 1 (once per step)
8. **Actual**: Accumulator increments by 4 (once per pulse)

### Impact
- 🔴 **CRITICAL**: Core feature completely broken
- Users cannot control when accumulator increments
- STEP and GATE modes unusable
- Feature documentation is misleading

### Suspected Root Cause

**Location**: `src/apps/sequencer/engine/NoteTrackEngine.cpp` in `triggerStep()` method

Likely issue: Trigger mode checking logic is incorrect or missing. The code may be:
- Not checking `triggerMode()` at all
- Checking it in the wrong place
- Logic inverted or conditions wrong

**Code to Investigate** (lines ~350-430):
```cpp
// STEP mode tick logic (around line 353)
// GATE mode tick logic (around line 392)
// RTRIG mode tick logic (around line 410)
```

Possible issues:
1. All three conditions might be evaluating to true
2. Mode checking might be bypassed
3. Wrong sequence pointer being checked
4. Fill sequence logic interfering

---

## Bug #2: Missing Playhead in Accumulator Steps Page

### Description
The ACCST page (accumulator steps configuration) does not show a running playhead indicator to show which step is currently playing, unlike all other layer pages (GATE, NOTE, RETRIG, etc.).

### Expected Behavior
- When sequence is playing, ACCST page should show visual indicator of current step
- Playhead should move in real-time as steps advance
- Consistent with other layer pages (NOTE, GATE, RETRIG, LENGTH, etc.)

### Actual Behavior
- ACCST page shows static step triggers (STP1-STP16)
- No visual indication of which step is currently playing
- User cannot see playback position while editing accumulator triggers

### Steps to Reproduce
1. Start sequence playback (PLAY button)
2. Navigate to NOTE page → playhead visible ✓
3. Navigate to GATE page → playhead visible ✓
4. Navigate to ACCST page → playhead missing ✗

### Impact
- 🟡 **MODERATE**: UX inconsistency
- Makes editing during playback more difficult
- User must switch pages to see playback position

### Suspected Root Cause

**Location**: `src/apps/sequencer/ui/pages/AccumulatorStepsPage.cpp`

Likely missing:
- Playhead rendering logic in `draw()` method
- Real-time update subscription
- Step state tracking

**Code to Investigate**:
- Compare `AccumulatorStepsPage::draw()` with `NoteSequenceEditPage::draw()`
- Check if page subscribes to step updates
- Verify if `_currentStep` or equivalent is tracked

---

## Related Files

**Model Layer:**
- `src/apps/sequencer/model/Accumulator.h/cpp` - Trigger mode enum and logic
- `src/apps/sequencer/model/NoteSequence.h` - Step accumulator trigger flags

**Engine Layer:**
- `src/apps/sequencer/engine/NoteTrackEngine.cpp` - triggerStep() method (lines ~350-430)
  - Line ~353: STEP mode logic
  - Line ~392: GATE mode logic
  - Line ~410: RTRIG mode logic

**UI Layer:**
- `src/apps/sequencer/ui/pages/AccumulatorStepsPage.h/cpp` - Missing playhead
- `src/apps/sequencer/ui/pages/NoteSequenceEditPage.cpp` - Reference for playhead implementation

**Tests:**
- `src/tests/unit/sequencer/TestAccumulator.cpp` - Unit tests (passing but may not cover bug)
- Need integration tests that verify actual engine behavior

---

## Verification Status

**Unit Tests:** ✅ All passing (15 tests)
- Tests mock behavior, may not catch engine integration bugs
- Need end-to-end integration tests

**Simulator Testing:** ⚠️ Not tested (bug found on hardware)
- Should reproduce bug in simulator

**Hardware Testing:** ❌ Bugs confirmed on real hardware
- Both bugs reproducible
- STEP/GATE modes completely non-functional

---

## Priority

**Bug #1 (Trigger Modes):** 🔴 **P0 - CRITICAL**
- Blocks feature usage
- Must fix before any release
- Affects all users attempting to use STEP or GATE modes

**Bug #2 (Playhead):** 🟡 **P1 - HIGH**
- UX issue but not blocking
- Should fix for consistency
- Lower priority than Bug #1

---

## Next Steps

1. **Reproduce in simulator** - Verify bugs exist in sim environment
2. **Add integration tests** - Write tests that catch these bugs
3. **Fix Bug #1** - Correct trigger mode logic in NoteTrackEngine.cpp
4. **Fix Bug #2** - Add playhead rendering to AccumulatorStepsPage
5. **Regression test** - Verify all trigger modes work correctly
6. **Hardware verify** - Confirm fixes on real hardware

---

## Notes

These bugs suggest that the implementation was tested via unit tests but not via integration/end-to-end testing. The unit tests pass because they mock the behavior, but the actual engine integration is incorrect.

**Lesson learned:** Need better integration test coverage for engine layer.

---

**Document Version**: 1.0
**Last Updated**: November 2025
**Project**: PEW|FORMER Bug Tracking
