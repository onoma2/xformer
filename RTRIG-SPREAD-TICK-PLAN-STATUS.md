# RTRIG Spread-Tick Plan Status

**Date**: 2025-11-17
**Location**: TODO.md § EXPERIMENTAL: RTRIG Mode - Spread Accumulator Ticks Over Time

## Current Status: PARTIALLY UPDATED

### ✅ Completed Updates (Phases 1-2)

**Phase 1: Model Layer - Extend Gate Struct**
- ✅ Updated to use single-queue approach (extend existing `Gate` struct)
- ✅ Added `shouldTickAccumulator` flag with TDD tests
- ✅ Added `sequenceId` field with TDD tests
- ✅ Memory analysis and static_assert checks
- ✅ Sequence ID constants (MainSequenceId, FillSequenceId)

**Phase 2: Engine Layer - Schedule Gates with Metadata**
- ✅ Updated triggerStep() to set flags on gates
- ✅ TDD tests for gate scheduling with metadata
- ✅ Proper sequenceId assignment (main vs fill)

### ⚠️ Needs Cleanup (Phases 3-6)

**Phase 3: Needs update to single-queue approach**
- ❌ Remove references to `_accumulatorTickQueue` (doesn't exist in single-queue)
- ❌ Update tick() processing to check `shouldTickAccumulator` flag
- ✅ Partially done: Helper method `tickAccumulatorForGateEvent(event)` concept
- ⚠️  Need to complete helper method implementation
- ⚠️  Remove leftover code from two-queue approach

**Phase 4: Edge Case Handling**
- ⚠️  Currently has `clearAccumulatorTickQueue()` - not needed for single-queue
- ✅ Sequence validation logic is good
- ⚠️  Need to clarify: Gates with stale metadata are safe (validation checks sequence)
- ⚠️  Pattern changes don't need queue clearing (gates auto-validate)

**Phase 5: Integration Testing**
- ✅ Test cases are good
- ⚠️  Need to add stress testing from user suggestions:
  - Queue capacity testing (retrig=7, pulseCount=8)
  - Rapid pattern switching
  - Fill mode transitions
  - Project loading
  - UI responsiveness

**Phase 6: Documentation**
- ⚠️  Needs updates for single-queue approach

## Key Implementation Points (Single-Queue)

### Helper Method Pattern
```cpp
// In NoteTrackEngine.cpp
void NoteTrackEngine::tickAccumulatorForGateEvent(const Gate& event) {
    // Lookup sequence by ID
    NoteSequence* targetSeq = nullptr;
    if (event.sequenceId == MainSequenceId && _sequence) {
        targetSeq = _sequence;
    } else if (event.sequenceId == FillSequenceId && _fillSequence) {
        targetSeq = _fillSequence;
    }

    // Validate and tick
    if (targetSeq &&
        (targetSeq == _sequence || targetSeq == _fillSequence) &&  // Extra safety
        targetSeq->accumulator().enabled() &&
        targetSeq->accumulator().triggerMode() == Accumulator::Retrigger) {
        const_cast<Accumulator&>(targetSeq->accumulator()).tick();
    }
}
```

### No Separate Queue Clearing Needed
- Gates with `shouldTickAccumulator = true` but invalid sequence → safely ignored
- Validation checks ensure no crashes on stale gates
- Pattern changes naturally invalidate sequence pointers
- Fill transitions handled by sequence ID lookup

### Memory Overhead
- Gate struct: 4 (tick) + 1 (gate) + 1 (shouldTickAccum) + 1 (seqId) = 7 bytes
- With padding: likely 8 bytes (check with sizeof)
- Queue: 16 entries × 8 bytes = 128 bytes (was 128 bytes originally)
- **No additional memory overhead if struct packs to 8 bytes**

## Stress Testing Scenarios (From User)

Must add these to Phase 5:

1. **Queue Capacity**
   - Test: retrig=7, pulseCount=8 (16 gate events = queue limit)
   - Verify: No overflow, graceful handling

2. **Rapid Pattern Switching**
   - Test: Schedule gates, switch pattern rapidly
   - Verify: No crashes, stale gates safely ignored

3. **Fill Mode Transitions**
   - Test: Main → Fill → Main while retriggers active
   - Verify: Sequence ID validation works correctly

4. **Project Loading**
   - Test: Load new project while gates scheduled
   - Verify: Old gates don't fire, no crashes

5. **UI Responsiveness**
   - Test: Heavy retrigger/accumulator load
   - Verify: UI remains responsive (< 5% performance hit)

## Next Steps

1. Clean up Phase 3 to remove two-queue references
2. Complete `tickAccumulatorForGateEvent()` implementation
3. Update Phase 4 to clarify no queue clearing needed
4. Add stress testing scenarios to Phase 5
5. Update Phase 6 documentation references
6. Test struct size with `sizeof(Gate)` to verify memory claims

## Files to Update

- `TODO.md` lines 1777-2100 (Phases 3-6)
- Clean up leftover `_accumulatorTickQueue` references
- Add user's stress testing scenarios
- Finalize helper method implementation

---

**Status**: Work in progress, 60% complete
**Commit**: e97d9eb (WIP: Update RTRIG TDD plan to single-queue approach Phase 1-2))
