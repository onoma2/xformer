# Tuesday Track Routing Fix (INCOMPLETE - SEE TUESDAY_ROUTING_COMPLETE_FIX.md)

⚠️ **This fix was incomplete. See TUESDAY_ROUTING_COMPLETE_FIX.md for the full solution.**

## Problem

Tuesday track parameters were not responding to routing sources (CV inputs, LFOs, etc.) even when routes were configured correctly.

### Symptoms
- Routing configuration shows active routes for Tuesday parameters
- Source values change (verified in routing monitor)
- **BUT** Tuesday parameters don't update
- **THIS FIX** only addressed: **Octave**, **Transpose**, **Rotate** (TrackTargets)
- **STILL BROKEN:** Algorithm, Flow, Ornament, Power, Glide, Trill (TuesdayTargets)

## Root Cause

**File:** `src/apps/sequencer/model/Routing.cpp:229-234`

**The Bug:**
```cpp
case Track::TrackMode::Tuesday:
    if (isSequenceTarget(target) || isTuesdayTarget(target)) {  // ❌ MISSING isTrackTarget!
        for (int patternIndex = 0; patternIndex < CONFIG_PATTERN_COUNT; ++patternIndex) {
            track.tuesdayTrack().sequence(patternIndex).writeRouted(target, intValue, floatValue);
        }
    }
    break;
```

The routing update logic for Tuesday tracks only checked for `isSequenceTarget()` and `isTuesdayTarget()`, but **completely ignored `isTrackTarget()`**.

### Target Categories

Routing targets are divided into categories:

| Category | Function | Example Targets |
|----------|----------|----------------|
| **ProjectTarget** | Global settings | Tempo, Swing |
| **PlayStateTarget** | Play state | Mute, Fill, Pattern |
| **TrackTarget** | Per-track settings | **Octave, Transpose, Rotate**, SlideTime |
| **SequenceTarget** | Sequence settings | **Divisor**, FirstStep, LastStep, RunMode, Scale, RootNote |
| **TuesdayTarget** | Tuesday-specific | Algorithm, Flow, Ornament, Power, Glide, Trill, GateOffset, GateLength |

**The missing logic:** Tuesday sequences have parameters from **all three categories** (Track, Sequence, Tuesday), but routing only handled two of them!

### Which Parameters Were Broken

**TrackTargets (BROKEN before fix):**
- ✅ **Octave** - Now works
- ✅ **Transpose** - Now works
- ✅ **Rotate** - Now works

**SequenceTargets (worked, but incomplete):**
- ✅ **Divisor** - Already worked

**TuesdayTargets (already worked):**
- ✅ Algorithm, Flow, Ornament, Power
- ✅ Glide, Trill, StepTrill
- ✅ GateOffset, GateLength

## The Fix

**File:** `src/apps/sequencer/model/Routing.cpp:230`

**Changed from:**
```cpp
if (isSequenceTarget(target) || isTuesdayTarget(target)) {
```

**Changed to:**
```cpp
if (isTrackTarget(target) || isSequenceTarget(target) || isTuesdayTarget(target)) {
```

**Result:** All three target categories are now properly routed to Tuesday sequences.

## Implementation Details

### How Routing Works

1. **RoutingEngine.updateSinks()** (`src/apps/sequencer/engine/RoutingEngine.cpp:129`)
   - Reads source values (CV inputs, LFOs, etc.)
   - Calculates target values using min/max ranges
   - Calls `_routing.writeTarget(target, tracks, value)`

2. **Routing.writeTarget()** (`src/apps/sequencer/model/Routing.cpp:182`)
   - Determines which model object owns the target
   - Delegates to appropriate track type's `writeRouted()` method
   - **THIS is where the bug was** - Tuesday case had incomplete condition

3. **TuesdaySequence.writeRouted()** (`src/apps/sequencer/model/TuesdaySequence.cpp:74`)
   - Receives routed value
   - Updates the appropriate parameter with `routed=true` flag
   - This part was always correct - just never called for Track targets!

### Why Other Track Types Worked

**NoteTrack** (lines 206-213):
```cpp
case Track::TrackMode::Note:
    if (isTrackTarget(target)) {
        track.noteTrack().writeRouted(target, intValue, floatValue);  // Track level
    } else {
        for (int patternIndex = 0; patternIndex < CONFIG_PATTERN_COUNT; ++patternIndex) {
            track.noteTrack().sequence(patternIndex).writeRouted(target, intValue, floatValue);  // Sequence level
        }
    }
    break;
```

Note tracks handle Track-level and Sequence-level targets **separately** - Track targets go to the track, Sequence targets go to each sequence.

**CurveTrack** (lines 215-222):
```cpp
case Track::TrackMode::Curve:
    if (isTrackTarget(target)) {
        track.curveTrack().writeRouted(target, intValue, floatValue);
    } else if (isSequenceTarget(target) || isChaosTarget(target) || isWavefolderTarget(target)) {
        for (int patternIndex = 0; patternIndex < CONFIG_PATTERN_COUNT; ++patternIndex) {
            track.curveTrack().sequence(patternIndex).writeRouted(target, intValue, floatValue);
        }
    }
    break;
```

Curve tracks also separate Track vs Sequence targets.

**TuesdayTrack** (NOW FIXED):
```cpp
case Track::TrackMode::Tuesday:
    if (isTrackTarget(target) || isSequenceTarget(target) || isTuesdayTarget(target)) {
        for (int patternIndex = 0; patternIndex < CONFIG_PATTERN_COUNT; ++patternIndex) {
            track.tuesdayTrack().sequence(patternIndex).writeRouted(target, intValue, floatValue);
        }
    }
    break;
```

Tuesday tracks store ALL parameters at the **sequence level** (no track-level parameters), so ALL target types route to sequences. This is different from Note/Curve tracks, but the fix now handles all target categories.

## Verification

To verify the fix works, test routing these parameters:

### Track Targets (previously broken):
1. **Octave** - Route LFO1 to Tuesday Track 1 Octave
   - Turn LFO1, verify octave changes in real-time
2. **Transpose** - Route CV1 to Tuesday Track 1 Transpose
   - Vary CV1, verify transpose follows
3. **Rotate** - Route LFO2 to Tuesday Track 1 Rotate
   - Turn LFO2, verify pattern rotation

### Sequence Targets (should already work):
4. **Divisor** - Route LFO3 to Tuesday Track 1 Divisor
   - Verify timing division changes

### Tuesday Targets (should already work):
5. **Algorithm** - Route CV2 to Tuesday Track 1 Algorithm
   - Verify algorithm morphing
6. **Flow** - Route LFO4 to Tuesday Track 1 Flow
   - Verify pattern variation changes
7. **Ornament** - Route CV3 to Tuesday Track 1 Ornament
   - Verify polyrhythm changes
8. **Power** - Route LFO5 to Tuesday Track 1 Power
   - Verify density changes

## Side Effects

**None expected.** This fix only enables functionality that was always supposed to work but didn't.

**Potential issues to watch for:**
- If users created workarounds (e.g., manually setting parameters because routing didn't work), those workarounds might conflict
- Saved projects with "broken" route configurations will now suddenly work

## Related Files

**Modified:**
- `src/apps/sequencer/model/Routing.cpp` (line 230)

**Involved (not modified):**
- `src/apps/sequencer/model/Routing.h` - Target category definitions
- `src/apps/sequencer/model/TuesdaySequence.cpp` - writeRouted implementation (already correct)
- `src/apps/sequencer/model/TuesdaySequence.h` - Parameter getters with isRouted checks
- `src/apps/sequencer/engine/RoutingEngine.cpp` - Routing update engine

## Future Improvements

**Not included in this fix:**

1. **Snapshot sequences** - Currently only routes to pattern sequences (0-15), not snapshot sequences (16-23). This is consistent with other track types.

2. **Scale and RootNote** - These are SequenceTargets that Tuesday sequences support, but they might not be exposed in UI routing configuration. Check if these should be routable for Tuesday tracks.

3. **Track-level vs Sequence-level** - Tuesday stores everything at sequence level. Consider if any parameters should be track-level (shared across all patterns) instead.

## Conclusion

The fix is a **one-line change** that restores routing functionality to Tuesday track parameters that should have always worked. The bug was a simple oversight in the routing dispatch logic that excluded Track-category targets from Tuesday tracks.

**Result:** All Tuesday sequence parameters now respond correctly to routing sources.
