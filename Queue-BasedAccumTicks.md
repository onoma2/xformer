# Option C: Queue-Based Accumulator Ticks - Detailed Plan & Risk Analysis

**Related Documentation:**
- **RTRIG-Timing-Research.md** - Technical investigation into gate queue architecture and why ticks can't spread over time
- **QWEN.md** - Complete accumulator feature documentation with RTRIG timing summary

## Overview
Schedule accumulator ticks in a queue alongside gates, so ticks happen when each retrigger actually fires (spread over time) rather than all at once when the step starts.

**Why This Is Needed:**
- Currently, RTRIG mode fires all N ticks immediately at step start (burst mode)
- Retrigger gates fire spread over time (you hear 3 distinct ratchets)
- But accumulator increments happen upfront, all at once
- See RTRIG-Timing-Research.md for detailed architecture investigation

---

## 🎯 Goals
1. **RETRIGGER mode ticks spread over time** - one tick per retrigger as it fires
2. **Maintain STEP/GATE mode behavior** - no regression
3. **Timing accuracy** - ticks align exactly with retrigger gate firings
4. **Minimal performance impact** - queue processing must be efficient

---

## 📋 Implementation Plan

### **Phase 1: Design & Architecture (2-3 days)**

#### 1.1 Queue Structure Design
**Options:**
- **Option A**: Extend `_gateQueue` with accumulator tick info
  - Add optional `accumulatorTick` field to gate queue entries
  - Pros: Single queue, simpler timing coordination
  - Cons: Couples accumulator logic with gate logic

- **Option B**: Separate `_accumulatorTickQueue`
  - Independent queue parallel to `_gateQueue`
  - Pros: Clean separation of concerns
  - Cons: Two queues to manage, timing sync complexity

**Recommendation**: Option A (extend gate queue)

#### 1.2 Queue Entry Structure
```cpp
struct GateQueueEntry {
    uint32_t tick;           // Existing
    bool value;              // Existing
    bool triggerAccumulator; // NEW: Should this gate fire also tick accumulator?
    // Could add: which accumulator, tick count, etc.
};
```

#### 1.3 Processing Location
**Where to process the queue and call `tick()`:**
- Likely in `Engine::update()` or wherever `_gateQueue` is currently processed
- Need to identify exact location in codebase

**Risks:**
- ⚠️ **Unknown processing location** - need to find where gates are fired
- ⚠️ **Thread safety** - accumulator is mutable, multiple threads may access
- ⚠️ **Timing precision** - must fire at exact tick, not approximate

---

### **Phase 2: Code Investigation (1 day)**

#### 2.1 Find Gate Queue Processing
**Tasks:**
1. Search for `_gateQueue` usage in NoteTrackEngine
2. Find where gate queue entries are dequeued and processed
3. Understand timing mechanism (tick counter, clock)
4. Check thread context (which task processes gates?)

**Files to investigate:**
- `src/apps/sequencer/engine/NoteTrackEngine.h/cpp`
- `src/apps/sequencer/engine/Engine.h/cpp`
- Possibly `src/platform/*/drivers/` for hardware gate output

**Risks:**
- ⚠️ **Distributed processing** - gates might be processed in multiple places
- ⚠️ **Hardware coupling** - gate firing might be tightly coupled to hardware
- ⚠️ **Timing constraints** - might be in real-time critical section

---

### **Phase 3: Implementation (3-5 days)**

#### 3.1 Modify Queue Structure
```cpp
// In NoteTrackEngine.h
struct GateEvent {
    uint32_t tick;
    bool gateValue;
    bool shouldTickAccumulator;  // NEW
};
```

#### 3.2 Update RETRIGGER Mode to Queue Ticks
```cpp
// In NoteTrackEngine.cpp, triggerStep()
if (stepRetrigger > 1) {
    // REMOVE the immediate tick loop
    // ADD queue scheduling instead

    uint32_t retriggerLength = divisor / stepRetrigger;
    uint32_t retriggerOffset = 0;
    while (stepRetrigger-- > 0 && retriggerOffset <= stepLength) {
        bool shouldTick = (step.isAccumulatorTrigger() &&
                          accumulator.enabled() &&
                          accumulator.triggerMode() == Accumulator::Retrigger);

        _gateQueue.pushReplace({
            tick + gateOffset + retriggerOffset,
            true,
            shouldTick  // NEW: Mark this gate to tick accumulator
        });

        _gateQueue.pushReplace({
            tick + gateOffset + retriggerOffset + retriggerLength / 2,
            false,
            false  // Gate-off doesn't tick
        });

        retriggerOffset += retriggerLength;
    }
}
```

#### 3.3 Process Queue to Fire Ticks
```cpp
// In gate queue processing location (TBD)
void processGateQueue(uint32_t currentTick) {
    while (!_gateQueue.empty() && _gateQueue.peek().tick <= currentTick) {
        auto event = _gateQueue.pop();

        // Fire gate (existing logic)
        setGateOutput(event.gateValue);

        // NEW: Tick accumulator if scheduled
        if (event.shouldTickAccumulator) {
            // Need reference to accumulator here!
            // This is a problem - queue doesn't know which accumulator
            accumulator.tick();  // How do we get accumulator reference?
        }
    }
}
```

**Major Problem Identified:**
- ⚠️ **Queue doesn't know which accumulator to tick!**
- Need to pass accumulator reference or ID through queue
- Accumulator is part of NoteSequence, not globally accessible

#### 3.4 Pass Accumulator Reference Through Queue
**Options:**
1. Store `Accumulator*` pointer in queue entry
   - Risky: pointer might become invalid
2. Store sequence index/ID
   - Safer: lookup accumulator when processing
3. Store callback function
   - Most flexible: lambda that captures accumulator

**Recommendation**: Store sequence pointer + validation

```cpp
struct GateEvent {
    uint32_t tick;
    bool gateValue;
    const NoteSequence* sequence;  // NEW: Which sequence's accumulator to tick
    bool shouldTickAccumulator;
};
```

**Risks:**
- ⚠️ **Pointer invalidation** - sequence might be deleted/moved
- ⚠️ **Memory safety** - need validation before dereferencing
- ⚠️ **Thread safety** - sequence accessed from multiple threads

---

### **Phase 4: Testing (2-3 days)**

#### 4.1 Unit Tests
**New tests needed:**
- Queue scheduling correctness
- Tick timing accuracy
- Multiple retriggers per step
- Edge cases (retrigger=1, high retrigger counts)

#### 4.2 Integration Tests
- STEP mode still works
- GATE mode still works
- RETRIGGER mode ticks spread over time
- Fill modes work correctly
- Sequence switching mid-playback

#### 4.3 Manual Testing
- Simulator: visual verification
- Hardware: actual timing verification
- Tempo changes during playback
- Very slow tempo (15 BPM) - ticks should visibly spread out

**Risks:**
- ⚠️ **Timing-dependent tests are flaky** - hard to test exact timing
- ⚠️ **Simulator vs hardware differences** - timing might differ
- ⚠️ **Hard to observe** - ticks happen fast, hard to verify spread

---

## ⚠️ Comprehensive Risk Assessment

### **Critical Risks (Project-Blocking)**

1. **Architecture Unknown** 🔴
   - We don't know where gate queue is processed
   - Might be in real-time critical path
   - Might be hardware-coupled
   - **Impact**: Can't implement without finding this
   - **Mitigation**: Code investigation phase mandatory

2. **Thread Safety** 🔴
   - Accumulator is mutable (`tick()` modifies state)
   - Multiple threads might access same accumulator
   - Gate processing might be on audio thread
   - **Impact**: Crashes, data corruption
   - **Mitigation**: Add locks, make tick() thread-safe

3. **Pointer Invalidation** 🔴
   - Queue holds pointers to sequences
   - Sequences might be deleted (pattern change, project load)
   - Dangling pointer → crash
   - **Impact**: Crashes, data corruption
   - **Mitigation**: Smart pointers, validation, weak references

### **High Risks (Significant Issues)**

4. **Performance Degradation** 🟠
   - Extra queue processing overhead
   - Pointer dereferencing per gate event
   - Validation checks per tick
   - **Impact**: Audio glitches, timing jitter
   - **Mitigation**: Profile, optimize, consider object pooling

5. **Timing Precision** 🟠
   - Ticks must fire at EXACT tick timestamp
   - Clock jitter might cause early/late ticks
   - **Impact**: Incorrect accumulator values, musical timing errors
   - **Mitigation**: Careful tick counter handling, testing

6. **Regression Risk** 🟠
   - Changes affect core engine code
   - STEP/GATE modes might break
   - Fill modes might break
   - **Impact**: Existing features stop working
   - **Mitigation**: Comprehensive regression testing, feature flags

### **Medium Risks (Manageable)**

7. **Complexity Increase** 🟡
   - More complex code to understand
   - Harder to debug timing issues
   - Longer onboarding for new developers
   - **Impact**: Maintenance burden, slower future development
   - **Mitigation**: Good documentation, code comments

8. **Test Complexity** 🟡
   - Hard to test timing-dependent behavior
   - Flaky tests
   - Simulator vs hardware differences
   - **Impact**: Bugs slip through, false test failures
   - **Mitigation**: Mocking, dependency injection, hardware testing

9. **Serialization Challenges** 🟡
   - Queue state during save/load
   - Mid-playback save → incomplete ticks
   - **Impact**: Inconsistent state after load
   - **Mitigation**: Clear queue on save, or save queue state

### **Low Risks (Minor Issues)**

10. **Fill Mode Interactions** 🟢
    - Fill sequences have own accumulators
    - Queue needs to track which sequence
    - **Impact**: Wrong accumulator ticked during fill
    - **Mitigation**: Test fill modes thoroughly

11. **Delayed First Tick** 🟢
    - Still needs to work with queued ticks
    - First tick after PLAY should still be delayed
    - **Impact**: Unexpected behavior on first step
    - **Mitigation**: Test with delayed tick logic

---

## 📊 Effort Estimation

| Phase | Optimistic | Realistic | Pessimistic |
|-------|-----------|-----------|-------------|
| Phase 1: Design | 1 day | 2 days | 4 days |
| Phase 2: Investigation | 0.5 days | 1 day | 3 days |
| Phase 3: Implementation | 2 days | 4 days | 7 days |
| Phase 4: Testing | 1 day | 3 days | 5 days |
| **Total** | **4.5 days** | **10 days** | **19 days** |

**Assumptions:**
- Familiar with codebase
- No major roadblocks
- Gate queue processing is accessible

---

## 🚧 Blockers & Unknowns

### **Must Resolve Before Starting:**
1. Where is `_gateQueue` processed?
2. What thread processes it?
3. Can we safely access accumulator from that context?
4. How does timing work (tick counter, clock)?

### **Research Questions:**
1. Is there existing queue infrastructure we can reuse?
2. How do other features handle delayed/queued actions?
3. Are there thread safety mechanisms already in place?
4. How does serialization handle in-flight events?

---

## 🎯 Success Criteria

### **Must Have:**
1. ✅ RETRIGGER mode ticks spread over time
2. ✅ retrig=3 → 3 ticks at 3 different timestamps
3. ✅ STEP mode unchanged (1 tick per step)
4. ✅ GATE mode unchanged (N ticks per step, spread)
5. ✅ No crashes, no data corruption
6. ✅ All existing tests pass
7. ✅ Performance acceptable (<5% overhead)

### **Nice to Have:**
1. 👍 Queue visualizable in debug mode
2. 👍 Timing verifiable in simulator
3. 👍 Graceful handling of edge cases

---

## 🔄 Alternative Approaches

### **Alternative 1: Accept Current Behavior**
- **Effort**: 0 days
- **Pros**: No work, no risk, simple
- **Cons**: RETRIGGER ticks all at once (not ideal)
- **User Impact**: Immediate ticks, but predictable

### **Alternative 2: Change RETRIGGER Semantic**
- **Effort**: 0.5 days
- **Idea**: RETRIGGER mode = tick once per step when retrigger > 0
- **Pros**: Simple, no queue needed
- **Cons**: Not very useful, loses retrigger count info

### **Alternative 3: Hybrid - Use Existing Retrigger Logic**
- **Effort**: 1-2 days
- **Idea**: Call `tick()` from inside existing retrigger while loop, but with delays
- **Pros**: Simpler than full queue
- **Cons**: Still need timing mechanism, might not be possible

---

## 📝 Recommendation

**Status**: ⚠️ **High Risk, Uncertain Feasibility**

### **Before Proceeding:**
1. **Must complete Phase 2 (Investigation)** to understand gate queue processing
2. **Must assess thread safety implications**
3. **Must validate pointer/reference management approach**

### **Decision Points:**
- **If gate queue is easily accessible**: Proceed with Option C
- **If gate queue is hardware-coupled or real-time critical**: Consider Alternative 1
- **If thread safety is too complex**: Consider Alternative 1

### **My Recommendation:**
**Accept current behavior (Alternative 1)** unless RETRIGGER timing is critical to your use case. The risks are high, the effort is significant, and the benefit is marginal (ticks spread over time vs immediate).

**If you really need spread-out ticks**, let's first do Phase 2 investigation to assess feasibility.

---

## 📅 Current Status (As of 2025-11-17)

**Current Implementation:**
- RETRIGGER mode ticks N times immediately when step starts (all ticks at once)
- Retriggers fire correctly (gates spread over time), but accumulator ticks happen upfront
- This is a known architectural limitation, not a bug

**Investigation Complete:**
- Gate queue architecture investigated (see RTRIG-Timing-Research.md)
- Root cause identified: Gate struct is minimal (tick + bool) with no accumulator context
- Four potential workarounds evaluated with risk assessment
- All queue-based approaches have pointer invalidation risks

**Decision:**
- **Recommendation**: Accept current behavior (burst mode)
- Documented in this file for future consideration
- Current behavior is acceptable for MVP
- Queue-based approach requires significant investigation and refactoring
- See RTRIG-Timing-Research.md for complete technical analysis
