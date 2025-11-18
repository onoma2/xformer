# Accumulator RTRIG Hardware Testing Guide

**Date**: 2025-11-18
**Feature**: RTRIG Mode Accumulator Ticking (Burst vs Spread)
**Status**: Ready for Phase 5 Hardware Testing
**Target Hardware**: PEW|FORMER Eurorack Module

---

## Table of Contents
1. [Prerequisites](#prerequisites)
2. [Test Environment Setup](#test-environment-setup)
3. [Flag=0 Tests (Burst Mode - Stable)](#flag0-tests-burst-mode-stable)
4. [Flag=1 Tests (Spread Mode - Experimental)](#flag1-tests-spread-mode-experimental)
5. [Edge Case Testing](#edge-case-testing)
6. [Performance Testing](#performance-testing)
7. [UI Responsiveness Testing](#ui-responsiveness-testing)
8. [Results Documentation](#results-documentation)
9. [Troubleshooting](#troubleshooting)

---

## Prerequisites

### Hardware Requirements
- ✅ PEW|FORMER eurorack module
- ✅ Eurorack power supply
- ✅ CV/Gate monitoring (oscilloscope or multimeter)
- ✅ Audio output monitoring (speakers/headphones)
- ✅ MIDI interface (optional, for clock sync)

### Software Requirements
- ✅ Firmware built with both flag=0 and flag=1
- ✅ USB cable for firmware flashing
- ✅ OpenOCD or DFU flashing tools

### Knowledge Requirements
- ✅ Familiarity with PEW|FORMER UI (pages, buttons, encoder)
- ✅ Understanding of CV/Gate concepts
- ✅ Knowledge of accumulator feature (see CLAUDE.md)
- ✅ Understanding of retrigger (ratcheting) concept

---

## Test Environment Setup

### Build Firmware with Flag=0 (Stable - Burst Mode)

```bash
cd /path/to/performer-phazer

# Edit Config.h - set flag to 0
# Line 73: #define CONFIG_EXPERIMENTAL_SPREAD_RTRIG_TICKS 0

# Build
cd build/stm32/release
make clean
cmake ../../..
make -j sequencer

# Flash to hardware
make flash_sequencer

# Verify
# Boot module, check no crashes, sequencer starts normally
```

### Build Firmware with Flag=1 (Experimental - Spread Mode)

```bash
cd /path/to/performer-phazer

# Edit Config.h - set flag to 1
# Line 73: #define CONFIG_EXPERIMENTAL_SPREAD_RTRIG_TICKS 1

# Build
cd build/stm32/release
make clean
cmake ../../..
make -j sequencer

# Flash to hardware
make flash_sequencer

# Verify
# Boot module, check no crashes, sequencer starts normally
```

### Basic Sequence Setup (Common to All Tests)

1. **Create new project**: PROJECT → Initialize
2. **Select Track 1**: Press T1
3. **Navigate to NOTE page**: Press S2 (SEQ)
4. **Enter note sequence**:
   - Step 1: C3 (MIDI note 48)
   - Step 2: D3 (MIDI note 50)
   - Step 3: E3 (MIDI note 52)
   - Step 4: F3 (MIDI note 53)
5. **Enable gates**: Press F1 (GATE) → Enable steps 1-4
6. **Set tempo**: TEMPO/CLOCK → 120 BPM

---

## Flag=0 Tests (Burst Mode - Stable)

**Goal**: Verify existing stable behavior is NOT broken by code changes.

### Test 1.1: RTRIG Mode Basic Functionality

**Setup**:
1. Flash firmware with **flag=0**
2. Navigate to ACCUM page (SEQ → ACCUM)
3. Configure:
   - ENABLE: On
   - TRIG: RTRIG
   - DIRECTION: Up
   - ORDER: Wrap
   - MIN: 0
   - MAX: 12 (one octave)
   - STEP: 1

**Procedure**:
1. Navigate to ACCST page (SEQ → ACCST)
2. Enable accumulator trigger on steps 1, 2, 3, 4
3. Navigate to STEPS page
4. Press F2 (RETRIG) → Set retrig=3 on step 1
5. Press PLAY

**Expected Result (Burst Mode)**:
- Step 1 fires 3 retrigger gates (hear 3 ratchets)
- Accumulator increments by **3 immediately** at step start
- First retrigger plays at C3 + 3 semitones = D#3
- Second retrigger plays at D#3 (same pitch)
- Third retrigger plays at D#3 (same pitch)
- Step 2 plays at E3 (D3 + 3 accumulated semitones)

**Pass Criteria**: ✅ All 3 retriggers have SAME pitch (burst happened upfront)

**Why This Matters**: Confirms backward compatibility - no regressions in stable mode.

---

### Test 1.2: RTRIG with Different Retrigger Counts

**Setup**: Same as Test 1.1

**Procedure**:
1. Set retrig=1 on step 1 → PLAY → Stop
2. Set retrig=2 on step 1 → PLAY → Stop
3. Set retrig=4 on step 1 → PLAY → Stop
4. Set retrig=7 on step 1 → PLAY → Stop

**Expected Results**:
- retrig=1: Accumulator +1, single gate
- retrig=2: Accumulator +2, two ratchets (same pitch)
- retrig=4: Accumulator +4, four ratchets (same pitch)
- retrig=7: Accumulator +7, seven ratchets (same pitch)

**Pass Criteria**: ✅ All retriggers within a step have identical pitch

---

### Test 1.3: STEP Mode Unchanged

**Setup**: Same as Test 1.1

**Procedure**:
1. Navigate to ACCUM → Set TRIG: STEP
2. Set retrig=3 on step 1
3. PLAY

**Expected Result**:
- Step 1 fires 3 retrigger gates
- Accumulator increments by **1 only** (STEP mode = once per step)
- All 3 retriggers play at C3 + 1 = C#3

**Pass Criteria**: ✅ STEP mode ticks once regardless of retrigger count

---

### Test 1.4: GATE Mode Unchanged

**Setup**: Same as Test 1.1

**Procedure**:
1. Navigate to ACCUM → Set TRIG: GATE
2. Navigate to STEPS
3. Set retrig=3 on step 1
4. Press F1 → Press F1 → Press F1 → Press F1 → Press F1 (cycle to GATE MODE layer)
5. Set gate mode = A (ALL) on step 1
6. PLAY

**Expected Result**:
- Step 1 fires 3 retrigger gates
- Accumulator increments by **4** (1 initial + 3 retriggers = 4 gates)
- First retrigger at C3 + 4 = E3
- Subsequent retriggers at E3 (burst mode)

**Pass Criteria**: ✅ GATE mode counts gates correctly with flag=0

---

### Test 1.5: Pattern Changes (Stability)

**Setup**: Same as Test 1.1

**Procedure**:
1. PLAY sequence
2. While playing: Switch to Pattern 2 (PATT)
3. Edit Pattern 2 (different notes)
4. Switch back to Pattern 1
5. Repeat 10 times rapidly

**Expected Result**:
- No crashes
- No audio glitches
- Accumulator state resets correctly
- Sequence plays smoothly

**Pass Criteria**: ✅ Stable operation, no crashes or glitches

---

### Test 1.6: Fill Mode (Stability)

**Setup**: Same as Test 1.1

**Procedure**:
1. Configure main sequence with retrig=3
2. Configure fill sequence differently
3. PLAY
4. Hold FILL button
5. Release FILL
6. Repeat 20 times

**Expected Result**:
- Smooth transitions between main and fill
- No accumulator state corruption
- No crashes

**Pass Criteria**: ✅ Stable fill transitions

---

## Flag=1 Tests (Spread Mode - Experimental)

**Goal**: Verify experimental spread-ticks behavior works as designed.

**⚠️ IMPORTANT**: These tests verify the **known behavior** where all N ticks fire immediately at step start (not truly spread over time). See RTRIG-Timing-Research.md for architectural explanation.

### Test 2.1: RTRIG Spread Mode Basic Behavior

**Setup**:
1. Flash firmware with **flag=1**
2. Same sequence setup as Test 1.1
3. Configure ACCUM with TRIG: RTRIG

**Procedure**:
1. Set retrig=3 on step 1
2. Connect CV1 OUT to oscilloscope
3. Connect GATE1 OUT to oscilloscope (second channel)
4. PLAY

**Expected Behavior (Known Limitation)**:
- Step 1 fires 3 retrigger gates spread over time
- Accumulator increments by **3 immediately** at step start (same as burst mode)
- All 3 retriggers have the SAME pitch (C3 + 3 = D#3)
- Gates are spread, but ticks happen upfront

**Why This Happens**:
- Gate queue processes all events at step start
- All `shouldTickAccumulator=true` gates tick immediately
- This is an architectural limitation (see RTRIG-Timing-Research.md)

**Pass Criteria**: ✅ No crashes, gates fire correctly, accumulator increments (even if not spread)

**Visual Verification**:
- Oscilloscope shows 3 distinct gate pulses spread over step duration
- CV voltage jumps to D#3 immediately and stays there for all 3 gates

---

### Test 2.2: Sequence ID Validation

**Setup**: Same as Test 2.1

**Procedure**:
1. Configure main sequence: retrig=3, accumulator enabled
2. Configure fill sequence: retrig=5, accumulator disabled
3. PLAY main
4. Switch to FILL
5. Switch back to main
6. Repeat 10 times

**Expected Result**:
- Main sequence: Accumulator ticks (3 per step)
- Fill sequence: Accumulator does NOT tick (disabled)
- No crashes when switching
- Correct sequence ID validation prevents stale ticks

**Pass Criteria**: ✅ Sequence validation works, no crashes, correct behavior per sequence

---

### Test 2.3: High Retrigger Count (Queue Stress)

**Setup**: Same as Test 2.1

**Procedure**:
1. Set retrig=7 on step 1
2. Navigate to STEPS → F2 → F2 → F2 (PULSE COUNT layer)
3. Set pulse count=7 on step 1 (creates 8 pulses × 7 retriggers = 56 gate events!)
4. PLAY

**Expected Result**:
- Module does NOT crash
- Gates fire correctly (may be truncated if queue overflows)
- Accumulator state remains valid

**Critical Safety Check**:
- Gate queue capacity: 16 events max
- If overflow: Oldest events dropped (verify graceful handling)

**Pass Criteria**: ✅ No crashes, graceful degradation if queue overflows

---

### Test 2.4: Rapid Pattern Switching (Sequence ID Safety)

**Setup**: Same as Test 2.1

**Procedure**:
1. Configure Pattern 1: retrig=3, accumulator enabled
2. Configure Pattern 2: retrig=5, accumulator enabled
3. PLAY
4. While step 1 is playing with scheduled retriggers:
   - Switch to Pattern 2 (PATT button)
   - Wait 1 second
   - Switch back to Pattern 1
5. Repeat rapidly 20 times

**Expected Result**:
- No crashes (sequence validation prevents stale ticks)
- Stale gates from old pattern are safely ignored
- New pattern starts cleanly

**Pass Criteria**: ✅ No crashes, clean pattern transitions

---

### Test 2.5: Null Sequence Handling

**Setup**: Same as Test 2.1

**Procedure**:
1. PLAY sequence with retriggers active
2. While playing: Load new project (PROJECT → Load)
3. Select different project
4. Load confirms

**Expected Result**:
- Old gate queue with stale sequence IDs is cleared
- No crashes from null pointer dereference
- New project loads and plays correctly

**Pass Criteria**: ✅ No crashes during project load

---

### Test 2.6: Accumulator Disabled Mid-Playback

**Setup**: Same as Test 2.1

**Procedure**:
1. PLAY with accumulator enabled, retrig=3
2. While playing step 1:
   - Navigate to ACCUM page
   - Set ENABLE: Off
3. Continue playback

**Expected Result**:
- Scheduled gates still fire (already in queue)
- Accumulator tick() is NOT called (validation checks enabled state)
- No crashes

**Pass Criteria**: ✅ State validation prevents ticking disabled accumulator

---

### Test 2.7: Trigger Mode Change Mid-Playback

**Setup**: Same as Test 2.1

**Procedure**:
1. PLAY with TRIG: RTRIG, retrig=3
2. While playing step 1:
   - Navigate to ACCUM page
   - Change TRIG: STEP
3. Continue playback

**Expected Result**:
- Scheduled RTRIG gates still fire (already queued)
- Accumulator tick() is NOT called (validation checks trigger mode)
- No crashes

**Pass Criteria**: ✅ State validation prevents ticking wrong mode

---

## Edge Case Testing

### Test 3.1: Maximum Retrigger Count

**Setup**: flag=1 firmware

**Procedure**:
1. Set retrig=7 (maximum)
2. Set accumulator STEP: 10
3. Set MAX: 100
4. PLAY and let run for 20 steps

**Expected Result**:
- Accumulator increments by 7 per step (retrig count)
- Wraps correctly at MAX value
- No overflow errors

**Pass Criteria**: ✅ Handles maximum retrigger count

---

### Test 3.2: Minimum Values

**Setup**: flag=1 firmware

**Procedure**:
1. Set retrig=1 (minimum)
2. Set accumulator MIN: -100, MAX: -90
3. PLAY

**Expected Result**:
- Accumulator operates in negative range
- Single retrigger per step
- Correct pitch modulation

**Pass Criteria**: ✅ Handles minimum and negative values

---

### Test 3.3: Wrap Boundary

**Setup**: flag=1 firmware

**Procedure**:
1. Set accumulator: MIN: 0, MAX: 2, STEP: 1, ORDER: Wrap
2. Set retrig=3 on step 1
3. PLAY

**Expected Result**:
- Step 1: Value wraps 0 → 1 → 2 → 0
- Wrapping happens correctly even with multiple ticks

**Pass Criteria**: ✅ Wrap order mode works correctly

---

### Test 3.4: Pendulum Boundary

**Setup**: flag=1 firmware

**Procedure**:
1. Set accumulator: MIN: 0, MAX: 5, STEP: 2, ORDER: Pendulum
2. Set retrig=7
3. PLAY for 10 steps

**Expected Result**:
- Accumulator counts up: 0, 2, 4 (at MAX, reverses)
- Counts down: 2, 0 (at MIN, reverses)
- Reversal happens correctly with multiple ticks

**Pass Criteria**: ✅ Pendulum order mode direction changes work

---

### Test 3.5: Hold Order Mode

**Setup**: flag=1 firmware

**Procedure**:
1. Set accumulator: MIN: 0, MAX: 10, STEP: 5, ORDER: Hold
2. Set retrig=3
3. PLAY

**Expected Result**:
- Accumulator reaches MAX and stops (holds)
- Does not wrap or reverse
- Subsequent ticks have no effect

**Pass Criteria**: ✅ Hold order mode clamps at boundaries

---

### Test 3.6: Random Order Mode

**Setup**: flag=1 firmware

**Procedure**:
1. Set accumulator: MIN: 0, MAX: 12, ORDER: Random
2. Set retrig=5
3. PLAY and observe pitch changes

**Expected Result**:
- Each tick produces random value within range
- Retriggers may have different pitches (random)
- No crashes, values stay in bounds

**Pass Criteria**: ✅ Random order mode generates valid random values

---

## Performance Testing

### Test 4.1: CPU Load with Heavy Retriggers

**Setup**: flag=1 firmware

**Procedure**:
1. All 8 tracks active
2. Each track: retrig=7, pulse count=7
3. All tracks: accumulator enabled with TRIG: RTRIG
4. Tempo: 180 BPM (fast)
5. PLAY for 5 minutes

**Expected Result**:
- UI remains responsive (< 100ms button latency)
- Audio output clean (no dropouts)
- Display updates smoothly
- No thermal throttling

**Pass Criteria**: ✅ System stable under heavy load

---

### Test 4.2: Memory Stability

**Setup**: flag=1 firmware

**Procedure**:
1. Create complex project with all features active
2. PLAY for 30 minutes continuously
3. Monitor for memory corruption symptoms:
   - Display glitches
   - Unexpected crashes
   - Parameter value changes
   - Audio glitches

**Expected Result**:
- Stable operation for extended period
- No memory leaks
- No corruption

**Pass Criteria**: ✅ No memory-related issues

---

### Test 4.3: Gate Queue Overflow Handling

**Setup**: flag=1 firmware

**Procedure**:
1. Configure step to generate > 16 gate events:
   - retrig=7
   - pulse count=7
   - gate mode=ALL
2. PLAY

**Expected Result**:
- System does NOT crash
- Oldest gates dropped if queue full
- Warning indication (if implemented)
- Graceful degradation

**Pass Criteria**: ✅ Handles overflow gracefully

---

## UI Responsiveness Testing

### Test 5.1: Button Response During Heavy Load

**Setup**: flag=1 firmware with Test 4.1 configuration

**Procedure**:
1. While running heavy retrigger load:
2. Press buttons rapidly:
   - PLAY/STOP
   - Track selection (T1-T8)
   - Page navigation (S1-S8)
   - Function keys (F1-F5)
3. Turn encoder rapidly
4. Measure response time

**Expected Result**:
- Button press acknowledged within 100ms
- Encoder turns register accurately
- No missed inputs
- UI updates smoothly

**Pass Criteria**: ✅ UI responsive under load

---

### Test 5.2: Display Update Rate

**Setup**: flag=1 firmware

**Procedure**:
1. Navigate to ACCUM page
2. PLAY with retrig=7 (value changes rapidly)
3. Observe accumulator value display
4. Check for:
   - Flickering
   - Tearing
   - Dropped frames
   - Readability

**Expected Result**:
- Display updates smoothly
- Value readable at all times
- No visual artifacts
- Frame rate adequate (30+ fps)

**Pass Criteria**: ✅ Display updates cleanly

---

## Results Documentation

### Test Result Template

For each test, document:

```markdown
## Test X.Y: [Test Name]

**Date**: YYYY-MM-DD
**Tester**: [Name]
**Firmware**: flag=[0/1], commit=[hash]
**Hardware**: [Serial number if available]

**Result**: [PASS / FAIL / PARTIAL]

**Observations**:
- [Detailed observations]
- [Unexpected behavior]
- [Performance notes]

**Issues Found**:
- [Issue 1 description]
- [Issue 2 description]

**Audio Recording**: [filename or link]
**Video Recording**: [filename or link]
**Oscilloscope Capture**: [filename or link]

**Notes**:
- [Additional notes]
```

### Summary Report Template

```markdown
# RTRIG Accumulator Hardware Test Summary

**Date**: YYYY-MM-DD
**Firmware Versions Tested**:
- flag=0: commit [hash]
- flag=1: commit [hash]

## Test Results Overview

| Category | Tests Run | Passed | Failed | Partial |
|----------|-----------|--------|--------|---------|
| Flag=0 Stability | X | X | X | X |
| Flag=1 Experimental | X | X | X | X |
| Edge Cases | X | X | X | X |
| Performance | X | X | X | X |
| UI Responsiveness | X | X | X | X |
| **TOTAL** | **X** | **X** | **X** | **X** |

## Critical Issues Found
1. [Issue description]
2. [Issue description]

## Recommendations
- [ ] flag=1 ready for production: YES / NO / NEEDS_WORK
- [ ] Additional testing needed: [areas]
- [ ] Code changes recommended: [suggestions]

## Sign-off
- Tester: [Name]
- Reviewer: [Name]
- Approval: [Name]
```

---

## Troubleshooting

### Issue: Module Crashes on PLAY

**Possible Causes**:
- Gate queue overflow
- Null pointer dereference
- Stack overflow

**Debug Steps**:
1. Reduce retrigger count
2. Disable accumulator
3. Test with simple pattern
4. Check serial output (if available)

---

### Issue: Accumulator Not Ticking

**Possible Causes**:
- Trigger mode wrong
- Accumulator disabled
- No accumulator triggers on steps

**Debug Steps**:
1. Verify ACCUM → ENABLE: On
2. Verify ACCUM → TRIG: RTRIG
3. Navigate to ACCST → Verify triggers enabled
4. Check retrigger count > 0

---

### Issue: Wrong Pitch on Retriggers

**Expected Behavior**:
- **flag=0 (burst)**: All retriggers same pitch (ticks happened upfront)
- **flag=1 (spread)**: All retriggers same pitch (known limitation - ticks still happen upfront)

**If different**: Possible bug, document and report

---

### Issue: UI Freezes

**Possible Causes**:
- Infinite loop
- Deadlock
- Priority inversion

**Debug Steps**:
1. Reduce load
2. Test with flag=0
3. Check specific trigger combinations
4. Document sequence to reproduce

---

### Issue: Gate Queue Overflow

**Symptoms**:
- Missing gates
- Truncated retriggers
- Unexpected behavior

**Mitigation**:
- Reduce retrigger count
- Reduce pulse count
- Don't exceed 16 total gate events per track

---

## Test Completion Checklist

Before declaring testing complete:

- [ ] All Flag=0 tests passed (6 tests)
- [ ] All Flag=1 tests passed (7 tests)
- [ ] All edge case tests passed (6 tests)
- [ ] All performance tests passed (3 tests)
- [ ] All UI tests passed (2 tests)
- [ ] Audio recordings captured
- [ ] Video demonstrations recorded
- [ ] Oscilloscope captures saved
- [ ] Test results documented
- [ ] Summary report written
- [ ] Issues logged in GitHub
- [ ] Code review completed
- [ ] Documentation updated

**Total Tests**: 24 comprehensive test scenarios

---

## Appendix: Quick Test Setup Script

### Minimal Accumulator RTRIG Test

Quick sanity check (5 minutes):

1. **Flash**: flag=0 or flag=1
2. **Setup**:
   - New project
   - Track 1: C-D-E-F notes
   - Gates on all steps
   - ACCUM: Enable, RTRIG, Up, Wrap, 0-12, step=1
   - ACCST: Triggers on all steps
   - Retrig=3 on step 1
3. **Test**:
   - PLAY
   - Listen: 3 ratchets on step 1
   - Check: All ratchets same pitch (flag=0 or flag=1)
4. **Result**: PASS if no crash and behavior matches expectations

### Full Regression Test

Complete test suite (2-4 hours):
- Run all 24 test scenarios
- Document all results
- Create summary report
- File GitHub issues for failures
- Update documentation

---

## References

- **CLAUDE.md**: Accumulator feature documentation
- **RTRIG-Timing-Research.md**: Technical architecture explanation
- **RTRIG-SPREAD-TICKS-SUMMARY.md**: Implementation summary
- **TDD-METHOD.md**: Testing methodology
- **TODO.md**: Implementation status and future work

---

**END OF TESTING GUIDE**
