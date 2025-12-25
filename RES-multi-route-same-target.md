# Research: Multiple Routes to Same Target+Track Combination

## Current Limitation

**Problem**: You cannot have multiple routes targeting the same parameter on the same track(s).

**Example of what SHOULD work but doesn't**:
- Route 1: CV In 1 → Transpose on Tracks 1-2
- Route 2: CV In 1 → Transpose on Tracks 3-4
- Route 3: CV In 2 → Transpose on Tracks 1-2

Currently, the system prevents proper tracking when multiple routes target overlapping track+target combinations.

## Root Cause Analysis

### 1. The Routed Flag Tracking System

**File**: `src/apps/sequencer/model/Routing.cpp:303-329`

```cpp
static std::array<uint8_t, size_t(Routing::Target::Last)> routedSet;

bool Routing::isRouted(Target target, int trackIndex) {
    size_t targetIndex = size_t(target);
    if (isPerTrackTarget(target)) {
        if (trackIndex >= 0 && trackIndex < CONFIG_TRACK_COUNT) {
            return (routedSet[targetIndex] & (1 << trackIndex)) != 0;
        }
    } else {
        return routedSet[targetIndex] != 0;
    }
    return false;
}

void Routing::setRouted(Target target, uint8_t tracks, bool routed) {
    size_t targetIndex = size_t(target);
    if (isPerTrackTarget(target)) {
        if (routed) {
            routedSet[targetIndex] |= tracks;  // Set bits
        } else {
            routedSet[targetIndex] &= ~tracks; // Clear bits
        }
    } else {
        routedSet[targetIndex] = routed ? 1 : 0;
    }
}
```

**Purpose**:
- Track which parameters are currently under routing control
- Display "→" indicator in UI next to routed parameters

**The Bug**:
- Global array: one bitmask per target (not per route)
- When Route A changes configuration, it clears its old target flags
- But if Route B also targets the same tracks, Route B's flags get cleared too
- Result: "→" indicator disappears even though Route B is still active

### 2. How Routes Are Processed

**File**: `src/apps/sequencer/engine/RoutingEngine.cpp:287-405`

**Sequence when route configuration changes**:

1. **Detect change** (line 275):
   ```cpp
   bool routeChanged = route.target() != routeState.target ||
                       route.tracks() != routeState.tracks;
   ```

2. **Clear old routing** (line 289):
   ```cpp
   Routing::setRouted(routeState.target, routeState.tracks, false);
   ```
   **BUG**: This clears flags for ALL routes with this target+tracks!

3. **Apply new routing** (line 394):
   ```cpp
   Routing::setRouted(route.target(), route.tracks(), true);
   ```

4. **Write values** (line 375):
   ```cpp
   _routing.writeTarget(target, (1 << trackIndex), routed);
   ```
   **This actually works fine** - last writer wins, which is expected behavior

### 3. Impact on Different Scenarios

#### Scenario A: Same source, same target, different tracks
**Route 1**: CV In 1 → Transpose Tracks 1-2
**Route 2**: CV In 1 → Transpose Tracks 3-4

**What happens**:
- ✅ **Routing works**: Both routes apply their values
- ❌ **UI broken**: Routed indicator flickers when routes change

#### Scenario B: Different sources, same target, same tracks
**Route 1**: CV In 1 → Transpose Tracks 1-2
**Route 2**: CV In 2 → Transpose Tracks 1-2

**What happens**:
- ✅ **Routing works**: Route 2 overwrites Route 1 (last writer wins)
- ❌ **UI broken**: Routed indicator flickers
- ⚠️ **Confusing**: User doesn't see that Route 2 is winning

#### Scenario C: Same source, different targets, overlapping tracks
**Route 1**: CV In 1 → Transpose Tracks 1-4
**Route 2**: CV In 1 → Octave Tracks 1-2

**What happens**:
- ✅ **Routing works**: Different targets, no conflict
- ✅ **UI works**: Different `routedSet` entries

## Why Current Design Exists

The `routedSet` mechanism was designed with the assumption:
> **"Each target+track combination is controlled by at most one route"**

This assumption is **too restrictive** for creative use cases.

## Technical Feasibility Analysis

### Good News: Routing Engine Already Supports It!

The actual routing logic (RoutingEngine.cpp:310-378) **already works correctly**:
- Each route independently:
  1. Reads its source value
  2. Applies shapers/bias/depth
  3. Writes to target

**Multiple routes targeting the same parameter**:
- ✅ All routes execute
- ✅ Values are written sequentially
- ✅ Last route in list wins (see "How Route Mixing Actually Works" section below)

### Only Problem: UI Indicator Tracking

The `routedSet` array needs to be **reference-counted** instead of boolean.

## How Route "Mixing" Actually Works

**TL;DR**: There is **NO mixing** - routes use simple overwrite. Last route wins.

### The Complete Data Flow

When multiple routes target the same parameter, here's the exact sequence:

#### 1. Sequential Route Processing

**File**: `src/apps/sequencer/engine/RoutingEngine.cpp:310-378`

```cpp
for (int routeIndex = 0; routeIndex < CONFIG_ROUTE_COUNT; ++routeIndex) {
    const auto &route = _routing.route(routeIndex);

    // Read source (CV, MIDI, etc.)
    float sourceValue = readSourceValue(route);

    // Apply shapers, bias, depth
    float shaped = applyShaping(sourceValue, route, trackIndex);

    // Write to target - THIS IS THE KEY PART
    _routing.writeTarget(target, (1 << trackIndex), shaped);  // Line 375
}
```

Routes execute **in order** (Route 0, then Route 1, then Route 2...). Each route writes independently.

#### 2. Target Writing

**File**: `src/apps/sequencer/model/Routing.cpp:211-241`

```cpp
void Routing::writeTarget(Target target, uint8_t tracks, float normalized) {
    float floatValue = denormalizeTargetValue(target, normalized);
    int intValue = std::round(floatValue);

    // For track targets (Transpose, Octave, etc.):
    for (int trackIndex = 0; trackIndex < CONFIG_TRACK_COUNT; ++trackIndex) {
        if (tracks & (1<<trackIndex)) {
            auto &track = _project.track(trackIndex);

            // Dispatch to track's writeRouted() method
            track.noteTrack().writeRouted(target, intValue, floatValue);
        }
    }
}
```

#### 3. Track Parameter Writing

**File**: `src/apps/sequencer/model/NoteTrack.cpp:4-13`

```cpp
void NoteTrack::writeRouted(Routing::Target target, int intValue, float floatValue) {
    switch (target) {
    case Routing::Target::Transpose:
        setTranspose(intValue, true);  // true = write to "routed" slot
        break;
    // ... other targets
    }
}
```

#### 4. Routable Parameter Storage

**File**: `src/apps/sequencer/model/NoteTrack.h:157-160, 305`

```cpp
// Member variable declaration:
Routable<int8_t> _transpose;  // Has TWO storage slots: base and routed

// Setter:
void setTranspose(int transpose, bool routed = false) {
    _transpose.set(clamp(transpose, -100, 100), routed);
    //                                          ^^^^^^
    //                                     true = write to routed slot
}
```

#### 5. The Routable Template (The Key!)

**File**: `src/apps/sequencer/model/Routing.h:782-797`

```cpp
template<typename T>
struct Routable {
    union {
        struct {
            T base;    // User-edited value (knobs, UI)
            T routed;  // Routing-controlled value
        };
        T values[2];   // Array: values[0]=base, values[1]=routed
    };

    // THIS IS WHERE THE "MIXING" HAPPENS (spoiler: it doesn't mix)
    inline void set(T value, bool selectRouted) {
        values[selectRouted] = value;  // ← SIMPLE OVERWRITE!
    }

    inline T get(bool selectRouted) const {
        return values[selectRouted];
    }

    inline void write(int value) {
        routed = value;  // Direct assignment to routed slot
    }
};
```

### The Formula

When 2 routes target the same parameter on the same track:

```
Route 5 executes: parameter.routed = route5Value  (overwrites previous)
Route 8 executes: parameter.routed = route8Value  (overwrites Route 5)
Final result:     parameter.routed = route8Value  (last writer wins)
```

**No averaging, no accumulation, no blending - just sequential overwrites.**

### Concrete Example

**Setup**:
- Route 5: CV In 1 → Transpose Track 1 (range: 0 to +12)
- Route 8: CV In 2 → Transpose Track 1 (range: -12 to 0)

**Runtime sequence** (every tick):

1. **Route 5 processes**:
   - Reads CV In 1 = 0.5V
   - Normalizes to 0.5
   - Maps to [0, +12] → value = +6
   - Calls `setTranspose(6, true)`
   - **Stores**: `_transpose.routed = 6`

2. **Route 8 processes**:
   - Reads CV In 2 = 1.0V
   - Normalizes to 1.0
   - Maps to [-12, 0] → value = 0
   - Calls `setTranspose(0, true)`
   - **Stores**: `_transpose.routed = 0` ← **Overwrites Route 5!**

3. **Track engine reads value**:
   - Calls `transpose()`
   - Returns `_transpose.get(isRouted)`
   - Returns `0` (Route 8's value)
   - **Route 8 wins, Route 5 is completely ignored**

### Why This Is Actually Correct

This "last writer wins" behavior is **intentional** and matches modular synthesis conventions:

- **Multiple CV sources to same input**: Last cable wins
- **Predictable**: Routes execute in order (0-15)
- **Deterministic**: Same configuration always gives same result
- **Simple**: No hidden mixing logic to debug

If you want **additive modulation**, you have options:
- Use different targets (Transpose + Octave)
- Use external summing (CV Out → hardware mixer → CV In)
- Route to intermediate parameters and combine in algorithm

### Impact on Multi-Route Scenarios

#### Scenario A: Different Tracks (This is the useful case!)
```
Route 1: CV In 1 → Transpose Tracks 1-2
Route 2: CV In 1 → Transpose Tracks 3-4
```

**What happens**:
- ✅ Route 1 writes to tracks 1-2 only
- ✅ Route 2 writes to tracks 3-4 only
- ✅ No overwriting - different track indices
- ✅ **This is what we want to enable!**

#### Scenario B: Same Track (Overwrite)
```
Route 1: CV In 1 → Transpose Track 1
Route 2: CV In 2 → Transpose Track 1
```

**What happens**:
- ⚠️ Route 1 writes to track 1
- ⚠️ Route 2 writes to track 1 (overwrites!)
- ⚠️ Only Route 2's value is used
- ℹ️ This is correct modular behavior, but UI should show both routes are active

### Why the UI Bug Matters

Even though Route 1 is being overwritten in Scenario B, **the UI should still show the "→" indicator** because:

1. Route 1 is actively configured and processing
2. If you delete Route 2, Route 1 immediately takes effect
3. User needs visual feedback that routing is active
4. Helps debugging: "why isn't my modulation working?" → "oh, Route 8 is overwriting it"

**Current bug**: When you modify Route 8, the UI incorrectly clears Route 1's "→" indicator, making it look like Route 1 isn't active at all.

### Verification

To confirm last-writer-wins behavior, check these files:

- `src/apps/sequencer/model/Routing.h:792` - `set()` is simple assignment
- `src/apps/sequencer/engine/RoutingEngine.cpp:310-378` - routes loop sequentially
- No summing, averaging, or accumulation logic anywhere in the routing path

## Alternative: Route Averaging Instead of Last-Wins

While last-writer-wins is correct for modular synthesis conventions, **averaging multiple routes** would enable powerful new use cases.

### Current vs Averaging Behavior

**Current (Last-Wins)**:
```
Route 5: CV In 1 (0.5V) → Transpose Track 1 [0, +12] → value = +6
Route 8: CV In 2 (1.0V) → Transpose Track 1 [-12, 0] → value = 0
Result: track.transpose = 0 (Route 8 wins)
```

**With Averaging**:
```
Route 5: CV In 1 (0.5V) → Transpose Track 1 [0, +12] → value = +6
Route 8: CV In 2 (1.0V) → Transpose Track 1 [-12, 0] → value = 0
Result: track.transpose = (6 + 0) / 2 = +3 (averaged!)
```

### Implementation: Accumulate-Then-Average

**Current flow** (immediate write):
```cpp
for (route 0..15) {
    float routed = computeRoutedValue(route);
    writeTarget(target, tracks, routed);  // ← Overwrites previous
}
```

**New flow** (accumulate, then average):
```cpp
// 1. Clear accumulators
for (each target+track) {
    accumulator[target][track] = {sum: 0, count: 0}
}

// 2. Accumulate all route values
for (route 0..15) {
    float routed = computeRoutedValue(route);
    accumulator[target][track].sum += routed;
    accumulator[target][track].count++;
}

// 3. Compute averages and write once
for (each target+track) {
    if (count > 0) {
        float average = sum / count;
        writeTarget(target, tracks, average);  // ← Write averaged value
    }
}
```

### Code Changes Required

#### 1. Add Accumulator Storage (RoutingEngine.h)

```cpp
class RoutingEngine : ... {
private:
    struct RouteAccumulator {
        float sum = 0.f;
        uint8_t count = 0;

        void clear() { sum = 0.f; count = 0; }
        void add(float value) { sum += value; count++; }
        float average() const { return count > 0 ? sum / float(count) : 0.f; }
    };

    // One accumulator per target per track
    // ~60 targets × 8 tracks × 5 bytes = ~2400 bytes
    std::array<std::array<RouteAccumulator, CONFIG_TRACK_COUNT>,
               size_t(Routing::Target::Last)> _routeAccumulators;
};
```

#### 2. Modify updateSinks() (RoutingEngine.cpp:270-407)

**Replace the current immediate-write loop with three-phase approach:**

```cpp
void RoutingEngine::updateSinks() {
    const auto &_routes = _routing.routes();

    // PHASE 1: CLEAR ACCUMULATORS
    for (auto &targetAccumulators : _routeAccumulators) {
        for (auto &acc : targetAccumulators) {
            acc.clear();
        }
    }

    // PHASE 2: ACCUMULATE ROUTE VALUES
    for (int routeIndex = 0; routeIndex < CONFIG_ROUTE_COUNT; ++routeIndex) {
        const auto &route = _routes[routeIndex];

        if (!route.active()) continue;

        auto target = route.target();
        auto tracks = route.tracks();

        // Handle edge-triggered targets (PlayToggle, RecordToggle, Reset)
        if (target == Routing::Target::PlayToggle) {
            // ... existing edge detection logic, no accumulation
            continue;
        }
        if (target == Routing::Target::RecordToggle) {
            // ... existing edge detection logic, no accumulation
            continue;
        }

        // ... existing route processing (source read, shapers, bias/depth)

        // Special handling for targets that should NOT average
        if (target == Routing::Target::Run) {
            // Run gate: last-wins makes sense (boolean-ish)
            // Could accumulate and threshold, but keep current behavior
            for (int trackIndex = 0; trackIndex < CONFIG_TRACK_COUNT; ++trackIndex) {
                if (tracks & (1 << trackIndex)) {
                    auto &track = _project.track(trackIndex);
                    track.setRunGate(routed > 0.55f, true);
                }
            }
            continue;
        }

        if (target == Routing::Target::Reset) {
            // Reset: edge-triggered, handle separately
            // ... existing edge detection logic
            continue;
        }

        // ACCUMULATE for all other targets
        if (Routing::isPerTrackTarget(target)) {
            size_t targetIndex = size_t(target);
            for (int trackIndex = 0; trackIndex < CONFIG_TRACK_COUNT; ++trackIndex) {
                if (tracks & (1 << trackIndex)) {
                    _routeAccumulators[targetIndex][trackIndex].add(routed);
                }
            }
        } else {
            // Global targets (Tempo, Play, etc.)
            size_t targetIndex = size_t(target);
            _routeAccumulators[targetIndex][0].add(routed);
        }
    }

    // PHASE 3: COMPUTE AVERAGES AND WRITE
    for (size_t targetIndex = 0; targetIndex < size_t(Routing::Target::Last); ++targetIndex) {
        Routing::Target target = static_cast<Routing::Target>(targetIndex);

        // Skip targets we handled specially above
        if (target == Routing::Target::PlayToggle ||
            target == Routing::Target::RecordToggle ||
            target == Routing::Target::Run ||
            target == Routing::Target::Reset) {
            continue;
        }

        if (Routing::isPerTrackTarget(target)) {
            for (int trackIndex = 0; trackIndex < CONFIG_TRACK_COUNT; ++trackIndex) {
                auto &acc = _routeAccumulators[targetIndex][trackIndex];

                if (acc.count > 0) {
                    float average = acc.average();
                    _routing.writeTarget(target, (1 << trackIndex), average);
                }
            }
        } else {
            // Global targets
            auto &acc = _routeAccumulators[targetIndex][0];
            if (acc.count > 0) {
                float average = acc.average();
                uint8_t allTracks = 0xFF; // Not used for global targets
                _routing.writeTarget(target, allTracks, average);
            }
        }
    }
}
```

### Memory Impact

```
RouteAccumulator: float (4 bytes) + uint8_t (1 byte) = 5 bytes
~60 targets × 8 tracks × 5 bytes = ~2400 bytes
```

**Acceptable on STM32F4** (192KB RAM total, current usage unknown but should have headroom).

### Creative Use Cases Enabled

#### 1. CV Crossfader / Blender
```
Route 1: CV In 1 → Transpose Tracks 1-8 (range: -12 to 0)
Route 2: CV In 2 → Transpose Tracks 1-8 (range: 0 to +12)

With averaging:
- CV In 1 at 5V, CV In 2 at 0V → average(-12, 0) = -6 semitones
- CV In 1 at 0V, CV In 2 at 5V → average(0, +12) = +6 semitones
- Both at 2.5V → average(-6, +6) = 0 semitones (center)
```

#### 2. Multi-Shaper Blending
```
Route 1: CV In 1 → Filter Cutoff Track 1 (Location shaper)
Route 2: CV In 1 → Filter Cutoff Track 1 (Envelope shaper)
Route 3: CV In 1 → Filter Cutoff Track 1 (Activity shaper)

Result: Blended modulation combining spatial + envelope + activity characteristics
```

#### 3. Overlapping Modulation Zones
```
Route 1: CV Out 8 → Octave Tracks 1-4
Route 2: CV Out 7 → Octave Tracks 3-6

Tracks 1-2: Only Route 1 (no averaging needed)
Tracks 3-4: Average of Route 1 + Route 2 (blend zone!)
Tracks 5-6: Only Route 2 (no averaging needed)
```

#### 4. Macro Control via Averaging
```
Route 1: CV In 1 → Algorithm Track 1 (range: 0-5)
Route 2: CV In 2 → Algorithm Track 1 (range: 5-10)
Route 3: CV In 3 → Algorithm Track 1 (range: 10-15)

Three-knob control blending across algorithm space
```

### Design Considerations

#### Option A: Always Average (Simplest)
**Pros**:
- Simple implementation
- No UI changes needed
- Predictable behavior

**Cons**:
- **Breaking change**: Existing patches sound different
- Can't get last-wins when you want it
- May confuse users expecting modular conventions

#### Option B: Global Project Setting
Add `Project::routeMixMode()` with values:
- `LastWins` (default, preserves current behavior)
- `Average` (new blending mode)

**Pros**:
- Non-breaking (default to last-wins)
- Simple UI (one global setting)
- Easy to understand

**Cons**:
- All-or-nothing (can't mix modes)
- Need to add UI for setting

#### Option C: Per-Target Mix Mode
Each target defines its mixing strategy:
- `Transpose`: Average
- `Octave`: Average
- `Run`: Last-wins
- `Reset`: Edge-triggered (no mixing)

**Pros**:
- Intelligent defaults per target type
- No user configuration needed

**Cons**:
- Hard-coded policy (less flexible)
- Some targets ambiguous (should Rotate average?)

#### Option D: Per-Route Mix Flag
Each route has a `mixMode` setting:
- `Overwrite` (last-wins, default)
- `Blend` (participates in averaging)

**Pros**:
- Maximum flexibility
- Can mix strategies in same patch

**Cons**:
- Most complex UI
- Cognitive load: "which routes are blending?"
- Hard to predict final behavior

### Recommended Approach

**Start with Option B (Global Project Setting)**:

1. **Add to Project.h**:
   ```cpp
   enum class RouteMixMode : uint8_t {
       LastWins,
       Average,
       Last
   };

   RouteMixMode routeMixMode() const { return _routeMixMode; }
   void setRouteMixMode(RouteMixMode mode) { _routeMixMode = mode; }

   private:
   RouteMixMode _routeMixMode = RouteMixMode::LastWins;
   ```

2. **Conditional logic in RoutingEngine::updateSinks()**:
   ```cpp
   if (_project.routeMixMode() == Project::RouteMixMode::Average) {
       // Three-phase accumulate-average-write
   } else {
       // Current last-writer-wins logic
   }
   ```

3. **Add UI in Project settings page**:
   ```
   Route Mix: Last Wins / Average
   ```

4. **Serialize in Project** (add to version, save/load)

### Special Cases with Averaging

**Edge-triggered targets** (PlayToggle, RecordToggle, Reset):
- **Don't average** - averaging 0.3 + 0.7 = 0.5 makes no sense for triggers
- Keep edge detection logic separate
- Only process first active route per frame

**Run target**:
- Could average then threshold: `(route1 + route2) / 2 > 0.55f`
- Or keep last-wins (more intuitive)
- Recommend: **last-wins** for Run

**Discrete targets** (Pattern, Algorithm, Scale):
- Averaging 3.5 + 7.2 = 5.35 → rounds to 5 or 6?
- Could work, but might feel unpredictable
- Recommend: **test both modes**, see what feels better

### Testing Strategy

1. **Preserve current behavior**: Default to LastWins mode
2. **Test averaging with continuous targets**: Transpose, Octave, Offset
3. **Test averaging with discrete targets**: Algorithm, Pattern
4. **Test edge cases**: Single route (should equal non-averaged), 16 routes
5. **Performance test**: Verify no audio dropouts with max routes active

### Performance Considerations

**Current**: Write 16 times (worst case, 16 routes to same target)
**With averaging**: Accumulate 16 times + write 1 time = 17 operations

**Negligible difference** - the bottleneck is source reading and shaping, not the final write.

### Summary

**Averaging is feasible and straightforward**:
- ~100 lines of code changes (mostly in RoutingEngine.cpp)
- ~2400 bytes of RAM (negligible)
- Enables powerful new modulation techniques
- Non-breaking if implemented as optional mode

**Recommendation**: Implement as global Project setting with "Last Wins" default.

## Solution Options

### Option 1: Reference Counting (Recommended)

**Change**: Track how many routes are active per target+track, not just boolean.

**Implementation**:

```cpp
// In Routing.cpp
static std::array<std::array<uint8_t, CONFIG_TRACK_COUNT>,
                  size_t(Routing::Target::Last)> routedRefCount;

void Routing::setRouted(Target target, uint8_t tracks, bool routed) {
    size_t targetIndex = size_t(target);
    if (isPerTrackTarget(target)) {
        for (int i = 0; i < CONFIG_TRACK_COUNT; ++i) {
            if (tracks & (1 << i)) {
                if (routed) {
                    routedRefCount[targetIndex][i]++;
                } else {
                    if (routedRefCount[targetIndex][i] > 0) {
                        routedRefCount[targetIndex][i]--;
                    }
                }
            }
        }
    } else {
        if (routed) {
            routedRefCount[targetIndex][0]++;
        } else {
            if (routedRefCount[targetIndex][0] > 0) {
                routedRefCount[targetIndex][0]--;
            }
        }
    }
}

bool Routing::isRouted(Target target, int trackIndex) {
    size_t targetIndex = size_t(target);
    if (isPerTrackTarget(target)) {
        if (trackIndex >= 0 && trackIndex < CONFIG_TRACK_COUNT) {
            return routedRefCount[targetIndex][trackIndex] > 0;
        }
    } else {
        return routedRefCount[targetIndex][0] > 0;
    }
    return false;
}
```

**Pros**:
- ✅ Fixes the UI indicator bug
- ✅ Minimal code changes (just Routing.cpp)
- ✅ No changes to routing engine logic
- ✅ Preserves all existing behavior

**Cons**:
- ⚠️ Slightly more memory (8 bytes → 64 bytes per target for per-track targets)
- ⚠️ Need careful initialization/reset logic

**Memory Impact**:
- Current: `uint8_t routedSet[Last]` = ~60 bytes
- New: `uint8_t routedRefCount[Last][8]` = ~480 bytes
- **Net increase: ~420 bytes** (negligible on STM32F4)

### Option 2: Full Route Audit (More Complex)

Instead of reference counting, scan all routes when checking if something is routed.

**Pros**:
- ✅ No state to maintain
- ✅ Always accurate

**Cons**:
- ❌ Performance cost (scan 16 routes × 8 tracks on every UI draw)
- ❌ More complex implementation
- ❌ Violates separation of concerns (Routing.cpp would need RoutingEngine access)

### Option 3: Remove setRouted Mechanism (Breaking Change)

Just remove the "→" indicator entirely and let users infer routing from route config.

**Pros**:
- ✅ Simple

**Cons**:
- ❌ Loss of useful UI feedback
- ❌ Harder to debug routing issues
- ❌ Not user-friendly

## Recommended Implementation

**Go with Option 1: Reference Counting**

### Implementation Steps

1. **Change routedSet to reference count array**
   - File: `src/apps/sequencer/model/Routing.cpp`
   - Lines: 303-329

2. **Update setRouted() to increment/decrement**
   - Handle edge cases (don't underflow)
   - Add reset function to clear all counts

3. **Update isRouted() to check count > 0**
   - No changes to callers needed

4. **Add initialization in Routing constructor**
   - Zero all reference counts

5. **Test scenarios**:
   - Multiple routes same target different tracks
   - Multiple routes same target same tracks
   - Route deletion/modification
   - Project load/clear

### Edge Cases to Handle

1. **Route deletion**: Decrement ref count
2. **Route target change**: Decrement old, increment new
3. **Route track change**: Update affected track counts
4. **Project clear**: Reset all counts to zero
5. **Overflow protection**: Cap at 255 (uint8_t max)

## Why This Matters for Creativity

### Current Use Cases That Don't Work

**1. CV Crossfader**:
- Route 1: CV In 1 → Transpose Tracks 1-8 (min=-12, max=0)
- Route 2: CV In 2 → Transpose Tracks 1-8 (min=0, max=12)
- **Goal**: Blend two transposition sources
- **Status**: ❌ Broken UI indicators

**2. Per-Track Modulation from Shared Source**:
- Route 1: CV In 1 → Filter Cutoff Track 1 (via Location shaper)
- Route 2: CV In 1 → Filter Cutoff Track 2 (via Envelope shaper)
- Route 3: CV In 1 → Filter Cutoff Track 3 (via Activity shaper)
- **Goal**: Same CV, different shaping per track
- **Status**: ❌ Broken UI indicators

**3. Layered Modulation**:
- Route 1: CV Out 8 → Octave Tracks 1-4
- Route 2: CV Out 7 → Octave Tracks 3-6
- **Goal**: Overlapping modulation zones
- **Status**: ❌ Broken UI indicators

### After Fix

All these scenarios will:
- ✅ Work correctly
- ✅ Show proper UI indicators
- ✅ Enable complex modulation matrices

## Special Cases: Run and Reset

**Current behavior**: Run and Reset are handled specially in RoutingEngine.cpp

**Run** (line 233-236 in Routing.cpp):
- Writes to `track.setRunGate()`
- Multiple routes to same track: **last wins**
- This is correct behavior

**Reset** (line 364-372 in RoutingEngine.cpp):
- Edge-triggered, not value-based
- Multiple routes to same track: **all trigger resets independently**
- This is correct behavior
- Reference counting will properly show "→" when any route targets Reset

## Conclusion

The routing engine **already supports** multiple routes to the same target+tracks.

### What Works

1. **Route processing**: All routes execute every frame (RoutingEngine.cpp:310-378)
2. **Value writing**: Each route writes to its target via `writeTarget()`
3. **Last-writer-wins**: Routes execute sequentially, later routes overwrite earlier ones
4. **Routable storage**: The `Routable<T>` template correctly stores both base and routed values

### What's Broken

**Only** the UI indicator tracking (`routedSet` in Routing.cpp:303-329):
- Uses boolean flags instead of reference counts
- When Route A changes, it clears flags that Route B might also be using
- Result: "→" indicator flickers/disappears even though routing still works

### The Fix

**Replace boolean `routedSet` with reference-counted array.**

**Impact**:
- ✅ Minimal code change (only Routing.cpp)
- ✅ Negligible memory increase (~420 bytes)
- ✅ No logic changes to routing engine
- ✅ Preserves all existing behavior
- ✅ Massive creative flexibility gain

**Recommendation**: Implement Option 1 (Reference Counting)

### Additional Notes

- See "How Route Mixing Actually Works" section for complete technical analysis
- Mixing behavior is **last-writer-wins** (simple overwrite, no blending)
- This matches modular synthesis conventions (last cable wins)
- The useful case is **different tracks** from same source (e.g., CV In 1 → Transpose Tracks 1-2 AND Tracks 3-4)
- The overwrite case (same track, multiple routes) is valid but Route B overwrites Route A
