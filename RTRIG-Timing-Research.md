# RTRIG Timing Research - Why Accumulator Ticks Can't Spread Over Time

**Date**: 2025-11-17
**Author**: Claude Code Investigation
**Status**: Complete - Recommendation to accept current behavior

---

## The Timing Paradox

**Observed Behavior:**
- Retrigger gates fire spread over time (you hear 3 distinct ratchets for retrig=3)
- But accumulator increments happen all at once at step start
- This seems contradictory - if gates are separate events, why can't we hook into them?

---

## Architecture Investigation

### Gate Queue Structure

**Location**: `src/apps/sequencer/engine/NoteTrackEngine.h` lines 82-93

```cpp
struct Gate {
    uint32_t tick;    // Timestamp when gate fires
    bool gate;        // Gate value (true=ON, false=OFF)
};

SortedQueue<Gate, 16, GateCompare> _gateQueue;
```

**Key Findings:**
1. Gate struct is minimal - just tick timestamp + boolean value
2. No metadata about which step, which retrigger number, or which accumulator
3. Queue capacity: 16 entries total
4. Gates are scheduled with future timestamps, processed later

### Gate Scheduling (triggerStep)

**Location**: `src/apps/sequencer/engine/NoteTrackEngine.cpp` lines 408-429

```cpp
// Current implementation - accumulator ticks BEFORE gate scheduling
int stepRetrigger = evalStepRetrigger(...);
if (stepRetrigger > 1) {
    // All N ticks happen HERE (immediate, upfront)
    for (int i = 0; i < tickCount; ++i) {
        accumulator.tick();  // Tick #1, #2, #3 all at once
    }

    // THEN gates are scheduled for future firing
    while (stepRetrigger-- > 0 && retriggerOffset <= stepLength) {
        _gateQueue.pushReplace({ tick + offset, true });   // Gate ON (future)
        _gateQueue.pushReplace({ tick + offset + half, false }); // Gate OFF (future)
        retriggerOffset += retriggerLength;
    }
}
```

**Example with retrig=3, divisor=48:**
- Tick 0: All 3 accumulator ticks fire immediately
- Tick 0: Schedule gate ON
- Tick 8: Schedule gate OFF
- Tick 16: Schedule gate ON
- Tick 24: Schedule gate OFF
- Tick 32: Schedule gate ON
- Tick 40: Schedule gate OFF

### Gate Processing (tick)

**Location**: `src/apps/sequencer/engine/NoteTrackEngine.cpp` lines 210-219

```cpp
// Gates fire later when clock advances
while (!_gateQueue.empty() && tick >= _gateQueue.front().tick) {
    if (!_monitorOverrideActive) {
        result |= TickResult::GateUpdate;
        _activity = _gateQueue.front().gate;
        _gateOutput = _activity;
        midiOutputEngine.sendGate(_track.trackIndex(), _gateOutput);
    }
    _gateQueue.pop();  // Remove processed gate
}
```

**The Problem:**
- Gates are processed in a generic loop
- No context about which step they came from
- No way to know "this is retrigger #2 of step 5"
- No reference to which accumulator to tick

---

## Potential Workarounds

### Option 1: Extend Gate Structure ⚠️ HIGH RISK

**Approach**: Add metadata to Gate struct

```cpp
struct Gate {
    uint32_t tick;
    bool gate;
    bool tickAccumulator;        // NEW: Should tick accumulator?
    NoteSequence* sequence;      // NEW: Which accumulator to tick?
    int retriggerIndex;          // NEW: Which retrigger (0, 1, 2...)
};
```

**Implementation:**
```cpp
// In triggerStep() - schedule accumulator ticks WITH gates
while (stepRetrigger-- > 0 && retriggerOffset <= stepLength) {
    bool shouldTick = (step.isAccumulatorTrigger() &&
                      accumulator.enabled() &&
                      accumulator.triggerMode() == Accumulator::Retrigger);

    _gateQueue.pushReplace({
        tick + offset,
        true,
        shouldTick,           // Mark to tick accumulator
        &targetSequence,      // Which accumulator
        retriggerCounter      // Which retrigger number
    });
    // ... rest of gate scheduling
}

// In tick() gate processing - tick when gate fires
while (!_gateQueue.empty() && tick >= _gateQueue.front().tick) {
    auto event = _gateQueue.pop();

    // Fire gate (existing logic)
    _gateOutput = event.gate;

    // NEW: Tick accumulator if scheduled
    if (event.tickAccumulator && event.sequence) {
        // Validate pointer is still valid
        if (event.sequence == _sequence || event.sequence == _fillSequence) {
            const_cast<Accumulator&>(event.sequence->accumulator()).tick();
        }
    }
}
```

**Pros:**
- Direct approach - ticks fire when gates fire
- Uses existing queue infrastructure
- Timing synchronized perfectly with gates

**Cons:**
- **Pointer invalidation risk** 🔴
  - Sequence might be deleted (pattern change, project load)
  - Dangling pointer → crash
- **Memory overhead**:
  - Current Gate: 8 bytes (4 + 4 with padding)
  - New Gate: 20 bytes (4 + 4 + 1 + 8 pointer + 4 index = 21, padded to 24)
  - Queue size: 16 entries → 384 bytes (was 128 bytes)
- **Queue capacity issues**:
  - Each retrigger creates 2 gate events (ON + OFF)
  - retrig=7 with pulse count = 8 × 2 = 16 gate events
  - Might exceed 16-entry queue limit
- **Thread safety**:
  - Gate processing might be on different thread
  - Accumulator access needs synchronization

**Risk Level**: 🔴 HIGH (pointer invalidation, crashes possible)

---

### Option 2: Separate Accumulator Tick Queue ⚠️ MEDIUM RISK

**Approach**: Create parallel queue just for accumulator ticks

```cpp
// In NoteTrackEngine.h
struct AccumulatorTick {
    uint32_t tick;
    NoteSequence* sequence;
    int retriggerIndex;  // Optional: for debugging
};

SortedQueue<AccumulatorTick, 16, AccumulatorTickCompare> _accumulatorTickQueue;
```

**Implementation:**
```cpp
// In triggerStep() - schedule accumulator ticks separately
if (step.isAccumulatorTrigger() && accumulator.enabled() &&
    accumulator.triggerMode() == Accumulator::Retrigger) {

    for (int i = 0; i < stepRetrigger; ++i) {
        _accumulatorTickQueue.push({
            tick + gateOffset + (i * retriggerLength),
            &targetSequence,
            i
        });
    }
}

// In tick() - process accumulator queue alongside gate queue
while (!_accumulatorTickQueue.empty() &&
       tick >= _accumulatorTickQueue.front().tick) {
    auto event = _accumulatorTickQueue.pop();

    // Validate sequence pointer
    if (event.sequence == _sequence || event.sequence == _fillSequence) {
        const_cast<Accumulator&>(event.sequence->accumulator()).tick();
    }
}
```

**Pros:**
- Clean separation of concerns
- Doesn't pollute Gate struct
- Can use different queue size if needed
- Easier to debug (separate queue for accum ticks)

**Cons:**
- **Still has pointer invalidation risk** 🟠
- Two queues to manage (complexity)
- Timing sync between queues (must process in correct order)
- Additional memory overhead (separate queue)
- Queue might fill up with high retrigger counts

**Risk Level**: 🟠 MEDIUM (still has pointer safety issues)

---

### Option 3: Weak Reference with Validation ⚠️ MEDIUM RISK

**Approach**: Store sequence ID instead of pointer, lookup on tick

```cpp
// In NoteTrackEngine.h
struct AccumulatorTick {
    uint32_t tick;
    uint8_t sequenceId;  // 0 = main, 1 = fill, etc.
    int retriggerIndex;
};

SortedQueue<AccumulatorTick, 16, AccumulatorTickCompare> _accumulatorTickQueue;
```

**Implementation:**
```cpp
// In triggerStep() - schedule with sequence ID not pointer
_accumulatorTickQueue.push({
    tick + offset,
    useFillSequence ? 1 : 0,  // Sequence ID
    i
});

// In tick() - lookup sequence by ID
while (!_accumulatorTickQueue.empty() && tick >= _accumulatorTickQueue.front().tick) {
    auto event = _accumulatorTickQueue.pop();

    NoteSequence* targetSeq = (event.sequenceId == 0) ? _sequence : _fillSequence;

    if (targetSeq && targetSeq->accumulator().enabled()) {
        const_cast<Accumulator&>(targetSeq->accumulator()).tick();
    }
}
```

**Pros:**
- No dangling pointers (uses ID, not pointer)
- Safer than raw pointers
- Small memory footprint (1 byte for ID)
- Sequence lookup is O(1)

**Cons:**
- **Sequence might still be invalid** 🟠
  - Sequence might be deleted/changed between schedule and fire
  - Need additional validation
- Assumes sequence structure is stable
- Limited to known sequence IDs (main, fill)

**Risk Level**: 🟠 MEDIUM (safer but still has edge cases)

---

### Option 4: Accept Current Behavior ✅ SAFE

**Approach**: Document the limitation and move on

**Rationale:**
- Current implementation is simple, safe, and predictable
- All ticks at step start is musically useful (burst mode)
- No crashes, no undefined behavior
- Easy to understand and maintain

**Pros:**
- ✅ Zero risk
- ✅ Zero effort
- ✅ Simple codebase
- ✅ Predictable behavior

**Cons:**
- Ticks don't spread over time like gates do
- Less musically interesting for some use cases

**Risk Level**: ✅ NONE (current stable implementation)

---

## Recommendation

**Accept current behavior (Option 4)** for the following reasons:

1. **Pointer Safety**: Options 1-3 all have pointer invalidation risks that could cause crashes
2. **Complexity**: Queue-based approaches add significant code complexity
3. **Testing**: Timing-dependent behavior is hard to test reliably
4. **Musical Value**: Burst mode (all ticks at once) is still musically useful
5. **Effort**: High effort (10-19 days) for marginal benefit

**If absolutely needed in future:**
- Start with **Option 3** (Weak Reference) as lowest risk
- Comprehensive testing on hardware
- Feature flag to disable if issues arise
- Consider as separate feature branch, not main

**Current Status:**
- RTRIG mode works correctly (N ticks for N retriggers)
- Timing limitation documented in all docs
- Queue-based approach fully documented in `Queue-BasedAccumTicks.md`
- Can be revisited if user feedback demands it

---

## Related Documentation

- **Queue-BasedAccumTicks.md** - Full implementation plan with 4-phase approach
- **QWEN.md** - Complete accumulator feature documentation
- **CLAUDE.md** - Project overview and development guidelines
- **src/apps/sequencer/engine/NoteTrackEngine.cpp** - Engine implementation

---

**Document Version**: 1.0
**Last Updated**: November 2025
**Project**: PEW|FORMER Accumulator Feature Implementation
