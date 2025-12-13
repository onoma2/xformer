# Tuesday Routing Investigation

## User Report

**Symptoms:**
- Tuesday routing was NEVER working (existing bug, not regression)
- When route is enabled, parameter jumps to minimum value
- Parameter stays at minimum and doesn't change
- UI controls stop working (can't manually change parameter)

**Affected Parameters:**
- Tuesday-specific targets: Algorithm, Flow, Ornament, Power, Glide, Trill, StepTrill, GateOffset, GateLength

**Working Parameters (after fix):**
- Track targets: Octave, Transpose, Rotate
- Sequence targets: Divisor

## Code Analysis

### Routing Flow

**1. Enable Route (`RoutingEngine.cpp:158-160`):**
```cpp
Routing::setRouted(route.target(), route.tracks(), true);
```
- Marks target as "routed" in global `routedSet` array
- This prevents UI from changing the value

**2. Update Value (`RoutingEngine.cpp:148-156`):**
```cpp
if (route.active()) {
    float value = route.min() + _sourceValues[routeIndex] * (route.max() - route.min());
    _routing.writeTarget(target, route.tracks(), value);
}
```
- Calculates value based on source (CV input, LFO, etc.)
- If source = 0, value = min
- If source = 1, value = max

**3. Write to Tuesday Sequences (`Routing.cpp:229-234`):**
```cpp
case Track::TrackMode::Tuesday:
    if (isTrackTarget(target) || isSequenceTarget(target) || isTuesdayTarget(target)) {
        for (int patternIndex = 0; patternIndex < CONFIG_PATTERN_COUNT; ++patternIndex) {
            track.tuesdayTrack().sequence(patternIndex).writeRouted(target, intValue, floatValue);
        }
    }
    break;
```
- Writes to ALL 16 patterns (0-15)
- Calls `TuesdaySequence.writeRouted()` for each pattern

**4. Update Parameter (`TuesdaySequence.cpp:74-118`):**
```cpp
case Routing::Target::Algorithm:
    setAlgorithm(intValue, true);  // true = routed flag
    break;
```
- Calls setter with `routed=true` flag
- Sets `_algorithm.routed = intValue`

**5. Read Parameter (`TuesdaySequence.h:30`):**
```cpp
int algorithm() const { return _algorithm.get(isRouted(Routing::Target::Algorithm)); }
```
- If routed: returns `_algorithm.routed`
- If not routed: returns `_algorithm.base`

### Parameter Storage

```cpp
template<typename T>
struct Routable {
    T base;    // User-set value (from UI)
    T routed;  // Routed value (from routing system)

    inline void set(T value, bool selectRouted) { values[selectRouted] = value; }
    inline T get(bool selectRouted) const { return values[selectRouted]; }
    inline void write(int value) { routed = value; }
};
```

### Routing Configuration

Tuesday targets have proper min/max ranges defined (`Routing.cpp:331-336`):
```cpp
[int(Routing::Target::Algorithm)]  = { 0, 20,  0, 20,  1 },
[int(Routing::Target::Flow)]       = { 0, 16,  0, 16,  1 },
[int(Routing::Target::Ornament)]   = { 0, 16,  0, 16,  1 },
[int(Routing::Target::Power)]      = { 0, 16,  0, 16,  1 },
[int(Routing::Target::Glide)]      = { 0, 100, 0, 100, 10 },
[int(Routing::Target::Trill)]      = { 0, 100, 0, 100, 10 },
```

## Possible Root Causes

### 1. Source Value Not Updating ⚠️ MOST LIKELY

**Hypothesis:** The routing source isn't actually generating a signal, so `_sourceValues[routeIndex]` stays at 0.

**Evidence:**
- Parameter jumps to min value (min + 0 * range = min)
- Parameter doesn't change (source stays at 0)

**Verification Needed:**
- Check if CV input is actually connected
- Check if source is configured correctly in route
- Monitor source value in routing display

### 2. Pattern Mismatch ⚠️ POSSIBLE

**Hypothesis:** User is on a different pattern than they expect.

**Evidence:**
- Routing writes to ALL 16 patterns
- User might be looking at pattern 1, but route configured for pattern 0's track

**Verification Needed:**
- Confirm which pattern is active
- Check if all patterns show same routed behavior

### 3. Tuesday vs Snapshot Sequences ⚠️ POSSIBLE

**Hypothesis:** Routing only writes to patterns 0-15, not snapshot sequences 16-23.

**Code:**
```cpp
for (int patternIndex = 0; patternIndex < CONFIG_PATTERN_COUNT; ++patternIndex) {
    // CONFIG_PATTERN_COUNT = 16, doesn't include snapshots
}
```

**If user is viewing a snapshot sequence (16-23), routing won't affect it.**

### 4. Track Index Mismatch ❌ UNLIKELY

**Hypothesis:** Track index isn't set correctly.

**Evidence Against:**
- Octave/Transpose/Rotate work (same track index)
- Track index is set in `TuesdayTrack::setTrackIndex()`

## Comparison with Working Track Types

### NoteTrack (WORKS)

```cpp
case Track::TrackMode::Note:
    if (isTrackTarget(target)) {
        track.noteTrack().writeRouted(target, intValue, floatValue);  // Track level
    } else {
        for (int patternIndex = 0; patternIndex < CONFIG_PATTERN_COUNT; ++patternIndex) {
            track.noteTrack().sequence(patternIndex).writeRouted(target, intValue, floatValue);
        }
    }
    break;
```

- Separates Track-level vs Sequence-level targets
- Track targets go to track object
- Sequence targets go to all sequences

### TuesdayTrack (BROKEN?)

```cpp
case Track::TrackMode::Tuesday:
    if (isTrackTarget(target) || isSequenceTarget(target) || isTuesdayTarget(target)) {
        for (int patternIndex = 0; patternIndex < CONFIG_PATTERN_COUNT; ++patternIndex) {
            track.tuesdayTrack().sequence(patternIndex).writeRouted(target, intValue, floatValue);
        }
    }
    break;
```

- ALL targets go to all sequences (no track-level storage)
- This is correct for Tuesday architecture

## Test Plan

### Manual Test

1. **Setup:**
   - Select Tuesday Track 1
   - Select Pattern 0
   - Set Algorithm to 10 manually (via UI)

2. **Configure Route:**
   - Route: CV In 1 → Tuesday Track 1 Algorithm
   - Min: 0
   - Max: 20
   - Enable route

3. **Test Source:**
   - Connect CV cable to CV In 1
   - Vary voltage from 0V to 5V

4. **Expected Behavior:**
   - At 0V: Algorithm = 0
   - At 2.5V: Algorithm = 10
   - At 5V: Algorithm = 20
   - UI controls should be disabled (can't manually change)

5. **Actual Behavior (reported by user):**
   - Algorithm jumps to 0
   - Algorithm stays at 0 (doesn't respond to CV changes)
   - UI controls disabled

### Debug Steps

**If algorithm stays at 0:**

1. **Check source value:**
   - Is CV cable actually connected?
   - Is voltage present at input?
   - Check routing monitor/display for source value

2. **Check route configuration:**
   - Is source set to "CV In 1" (not "None")?
   - Is target set to "Algorithm"?
   - Are min/max set correctly?
   - Is route enabled (active)?

3. **Check pattern:**
   - Which pattern is active? (should be 0)
   - Try switching to different patterns - do they all show 0?

4. **Check track:**
   - Is routing configured for Track 1?
   - Is Track 1 set to Tuesday mode?

5. **Check global state:**
   - Is `Routing::isRouted(Target::Algorithm, trackIndex=0)` returning true?
   - Is `_algorithm.routed` being set?
   - Is `_algorithm.base` still 10 (original value)?

## Recommended Fix (if source issue)

If the problem is that users don't understand why parameters stay at min value:

**UI Improvement:**
- Show routing source value in parameter display
- Indicate when parameter is routed (e.g., "Algo: 0 [R]" for routed)
- Show warning if route is enabled but source is "None" or not connected

**Code Improvement:**
- None needed - routing is working as designed
- Issue is user configuration/understanding

## Recommended Fix (if code issue)

**If routing is actually broken:**

Need to identify specific bug through testing. Possibilities:
- Source value not being read correctly
- Value calculation wrong
- writeRouted() not being called
- Routable storage not working

## Next Steps

1. **User provides more details:**
   - Exact routing configuration (source, min, max)
   - Is source actually connected/active?
   - Screenshots of routing setup

2. **Test on hardware:**
   - Reproduce issue with known-good setup
   - Monitor source values
   - Check if NoteTrack routing works with same source

3. **Code review:**
   - Double-check Tuesday routing path
   - Compare with working NoteTrack routing
   - Look for subtle differences

## Conclusion

Based on code analysis, the routing infrastructure appears **correct**. The most likely explanation for the reported behavior is that the **routing source is not generating a signal** (e.g., no CV cable connected, source set to "None", etc.).

However, without more details from the user or hardware testing, cannot definitively rule out a code bug.

**Recommended Action:** Request more specific information about routing configuration and verify source is actually connected/active.
