# Kria-Style Polymetric Layers for NoteTrack

**Goal:** Add independent loop points (and eventually run modes) for Gate and Note layers in NoteTrack, enabling polymetric/polyrhythmic sequencing inspired by Monome Kria.

**Approach:** Two-phase implementation leveraging existing NoteSequence step-based architecture.

---

## Phase 1: Independent Loop Points (MVP)

### Overview
Add independent loop start/end points for Gate and Note layers. Both layers advance on the same clock tick but wrap at different boundaries, creating evolving polyrhythmic patterns.

**Example Pattern:**
- Gate loop: steps 0-3 (4 steps)
- Note loop: steps 0-6 (7 steps)
- Pattern fully repeats after 28 steps (LCM of 4 and 7)

### Data Model Changes

**File:** `src/apps/sequencer/model/NoteSequence.h`

Add per-layer loop configuration:

```cpp
class NoteSequence {
public:
    struct LayerLoop {
        uint8_t loopStart;   // 0-63 (default: 0)
        uint8_t loopEnd;     // 0-63 (default: 15)

        LayerLoop() : loopStart(0), loopEnd(15) {}

        void clear() {
            loopStart = 0;
            loopEnd = 15;
        }

        void write(VersionedSerializedWriter &writer) const {
            writer.write(loopStart);
            writer.write(loopEnd);
        }

        void read(VersionedSerializedReader &reader) {
            reader.read(loopStart);
            reader.read(loopEnd);
        }
    };

    enum class PolymetricLayer {
        Gate,
        Note,
        Last
    };

private:
    // Polymetric loop configuration (Phase 1: 2 layers)
    LayerLoop _gateLoop;
    LayerLoop _noteLoop;
    bool _polymetricMode = false;  // Enable/disable feature

public:
    // Accessors
    bool polymetricMode() const { return _polymetricMode; }
    void setPolymetricMode(bool enabled) { _polymetricMode = enabled; }

    const LayerLoop& gateLoop() const { return _gateLoop; }
    LayerLoop& gateLoop() { return _gateLoop; }

    const LayerLoop& noteLoop() const { return _noteLoop; }
    LayerLoop& noteLoop() { return _noteLoop; }

    // Serialization (add to existing write/read methods)
    void write(VersionedSerializedWriter &writer) const {
        // ... existing writes ...
        writer.write(_polymetricMode);
        if (_polymetricMode) {
            _gateLoop.write(writer);
            _noteLoop.write(writer);
        }
    }

    void read(VersionedSerializedReader &reader) {
        // ... existing reads ...
        reader.read(_polymetricMode);
        if (_polymetricMode) {
            _gateLoop.read(reader);
            _noteLoop.read(reader);
        }
    }
};
```

**Storage cost:** 5 bytes per sequence (1 bool + 2 LayerLoop * 2 bytes)

### Engine Changes

**File:** `src/apps/sequencer/engine/NoteTrackEngine.h`

Add per-layer playhead tracking:

```cpp
class NoteTrackEngine : public TrackEngine {
private:
    int _currentStep;  // Existing - keep for compatibility

    // Polymetric playheads (Phase 1)
    int _gatePlayhead;
    int _notePlayhead;

    // Helper to advance playhead with loop boundaries
    int advancePlayhead(int current, int loopStart, int loopEnd,
                        Types::RunMode runMode) const;
};
```

**File:** `src/apps/sequencer/engine/NoteTrackEngine.cpp`

Modify `triggerStep()` to use independent playheads when polymetric mode is enabled:

```cpp
void NoteTrackEngine::triggerStep(uint32_t tick, uint32_t divisor) {
    const auto &sequence = *_sequence;
    const auto &evalSequence = useFillSequence ? *_fillSequence : *_sequence;

    if (sequence.polymetricMode()) {
        // POLYMETRIC MODE: Use independent playheads

        // Advance gate playhead
        _gatePlayhead = advancePlayhead(
            _gatePlayhead,
            sequence.gateLoop().loopStart,
            sequence.gateLoop().loopEnd,
            sequence.runMode()  // Phase 1: shared runMode
        );

        // Advance note playhead
        _notePlayhead = advancePlayhead(
            _notePlayhead,
            sequence.noteLoop().loopStart,
            sequence.noteLoop().loopEnd,
            sequence.runMode()  // Phase 1: shared runMode
        );

        // Read gate from gate playhead
        const auto &gateStep = evalSequence.step(_gatePlayhead);
        bool stepGate = evalStepGate(gateStep, _noteTrack.gateProbabilityBias());

        // Read note from note playhead
        const auto &noteStep = evalSequence.step(_notePlayhead);
        float baseNote = evalStepNote(noteStep, _noteTrack.noteProbabilityBias(),
                                      scale, rootNote, octave, transpose, evalSequence);

        // Merge gate timing with note pitch
        if (stepGate) {
            uint32_t gateOffset = (divisor * gateStep.gateOffset()) / (NoteSequence::GateOffset::Max + 1);
            uint32_t stepLength = (divisor * evalStepLength(gateStep, _noteTrack.lengthBias())) / NoteSequence::Length::Range;

            _gateQueue.pushReplace({ tick + gateOffset, true });
            _gateQueue.pushReplace({ tick + gateOffset + stepLength, false });

            _cvQueue.push({ tick + gateOffset, finalNote, noteStep.slide() });
        }

        // Update _currentStep for UI display (show gate playhead)
        _currentStep = _gatePlayhead;

    } else {
        // NORMAL MODE: Existing behavior (unchanged)
        _currentStep = SequenceUtils::rotateStep(_sequenceState.step(),
                                                 sequence.firstStep(),
                                                 sequence.lastStep(),
                                                 rotate);
        // ... existing triggerStep code ...
    }
}

int NoteTrackEngine::advancePlayhead(int current, int loopStart, int loopEnd,
                                     Types::RunMode runMode) const {
    // For Phase 1: implement basic forward advancement
    // Phase 2 will use SequenceState for full runMode support
    int next = current + 1;
    if (next > loopEnd) {
        next = loopStart;
    }
    return next;
}
```

### Reset/Restart Changes

Ensure playheads are reset properly:

```cpp
void NoteTrackEngine::reset() {
    // ... existing resets ...
    _gatePlayhead = 0;
    _notePlayhead = 0;
}

void NoteTrackEngine::restart() {
    // ... existing restarts ...
    _gatePlayhead = 0;
    _notePlayhead = 0;
}
```

### UI Changes (Minimal for Phase 1)

**File:** `src/apps/sequencer/ui/pages/NoteSequenceEditPage.h/cpp`

Add UI controls to:
1. **Toggle polymetric mode** (enable/disable feature)
2. **Set loop points per layer** when editing Gate or Note layer:
   - Hold SHIFT + press step buttons to set loop start/end
   - Visual feedback: highlight active loop range
3. **Show both playheads** during playback:
   - Bright indicator for current layer's playhead
   - Dimmer indicator for other layer's playhead

### Phase 1 Limitations

- Both layers advance on **same clock** (every tick)
- Both layers use **same runMode** (forward/backward/pendulum/etc)
- Only **Gate and Note** have independent loops (other layers use shared playhead)
- No per-layer clock divisions

**Musical Result:** Still creates evolving polyrhythmic patterns through different loop lengths.

---

## Phase 2: Per-Layer Run Modes

### Overview
Add independent run modes (Forward, Backward, Pendulum, Random, etc.) for Gate and Note layers, allowing one layer to play forward while the other plays backward.

**Example Pattern:**
- Gate loop: steps 0-3, Forward → 0,1,2,3,0,1,2,3...
- Note loop: steps 0-6, Backward → 6,5,4,3,2,1,0,6,5,4...

### Data Model Changes

**Extend LayerLoop:**

```cpp
struct LayerLoop {
    uint8_t loopStart;
    uint8_t loopEnd;
    Types::RunMode runMode;  // NEW: per-layer run mode (1 byte)

    LayerLoop() : loopStart(0), loopEnd(15), runMode(Types::RunMode::Forward) {}

    void write(VersionedSerializedWriter &writer) const {
        writer.write(loopStart);
        writer.write(loopEnd);
        writer.write(runMode);  // NEW
    }

    void read(VersionedSerializedReader &reader) {
        reader.read(loopStart);
        reader.read(loopEnd);
        reader.read(runMode);  // NEW
    }
};
```

**Additional storage:** +2 bytes per sequence (1 byte per layer)

### Engine Changes

**Add per-layer SequenceState tracking:**

```cpp
class NoteTrackEngine : public TrackEngine {
private:
    SequenceState _sequenceState;  // Existing - keep for compatibility

    // Phase 2: Per-layer sequence state
    SequenceState _gateSequenceState;
    SequenceState _noteSequenceState;
};
```

**Update advancePlayhead to use SequenceState:**

```cpp
void NoteTrackEngine::triggerStep(uint32_t tick, uint32_t divisor) {
    if (sequence.polymetricMode()) {
        // Phase 2: Advance using per-layer SequenceState

        _gateSequenceState.advanceAligned(
            relativeTick / divisor,
            sequence.gateLoop().runMode,  // Per-layer runMode
            sequence.gateLoop().loopStart,
            sequence.gateLoop().loopEnd,
            rng
        );

        _noteSequenceState.advanceAligned(
            relativeTick / divisor,
            sequence.noteLoop().runMode,  // Per-layer runMode
            sequence.noteLoop().loopStart,
            sequence.noteLoop().loopEnd,
            rng
        );

        _gatePlayhead = _gateSequenceState.step();
        _notePlayhead = _noteSequenceState.step();

        // ... rest of triggerStep logic ...
    }
}
```

### UI Changes (Phase 2)

Add per-layer run mode selection:
- When editing Gate layer: show/edit gate runMode
- When editing Note layer: show/edit note runMode
- Visual indicators for direction (arrows, etc.)

---

## Testing Strategy

### Unit Tests

**File:** `src/tests/unit/sequencer/TestNoteSequencePolymetric.cpp`

```cpp
#include "UnitTest.h"
#include "apps/sequencer/model/NoteSequence.h"

UNIT_TEST("NoteSequencePolymetric") {

CASE("default_values") {
    NoteSequence seq;
    expectFalse(seq.polymetricMode(), "polymetric disabled by default");
    expectEqual(seq.gateLoop().loopStart, 0, "gate loop start default");
    expectEqual(seq.gateLoop().loopEnd, 15, "gate loop end default");
    expectEqual(seq.noteLoop().loopStart, 0, "note loop start default");
    expectEqual(seq.noteLoop().loopEnd, 15, "note loop end default");
}

CASE("loop_point_setters") {
    NoteSequence seq;
    seq.setPolymetricMode(true);
    seq.gateLoop().loopStart = 2;
    seq.gateLoop().loopEnd = 5;
    seq.noteLoop().loopStart = 0;
    seq.noteLoop().loopEnd = 7;

    expectEqual(seq.gateLoop().loopStart, 2, "gate loop start");
    expectEqual(seq.gateLoop().loopEnd, 5, "gate loop end");
    expectEqual(seq.noteLoop().loopStart, 0, "note loop start");
    expectEqual(seq.noteLoop().loopEnd, 7, "note loop end");
}

CASE("serialization") {
    NoteSequence seq1;
    seq1.setPolymetricMode(true);
    seq1.gateLoop().loopStart = 3;
    seq1.gateLoop().loopEnd = 7;
    seq1.noteLoop().loopStart = 1;
    seq1.noteLoop().loopEnd = 9;

    // Serialize
    VersionedSerializedWriter writer;
    seq1.write(writer);

    // Deserialize
    VersionedSerializedReader reader(writer.data(), writer.size());
    NoteSequence seq2;
    seq2.read(reader);

    expectTrue(seq2.polymetricMode(), "polymetric mode preserved");
    expectEqual(seq2.gateLoop().loopStart, 3, "gate loop start preserved");
    expectEqual(seq2.gateLoop().loopEnd, 7, "gate loop end preserved");
    expectEqual(seq2.noteLoop().loopStart, 1, "note loop start preserved");
    expectEqual(seq2.noteLoop().loopEnd, 9, "note loop end preserved");
}

} // UNIT_TEST
```

**File:** `src/tests/unit/sequencer/TestNoteTrackEnginePolymetric.cpp`

```cpp
UNIT_TEST("NoteTrackEnginePolymetric") {

CASE("playhead_advancement_4_vs_7") {
    // Setup: Gate loop 0-3, Note loop 0-6
    NoteSequence seq;
    seq.setPolymetricMode(true);
    seq.gateLoop().loopStart = 0;
    seq.gateLoop().loopEnd = 3;
    seq.noteLoop().loopStart = 0;
    seq.noteLoop().loopEnd = 6;

    // Set gates and notes
    for (int i = 0; i <= 3; i++) seq.step(i).setGate(true);
    for (int i = 0; i <= 6; i++) seq.step(i).setNote(i);

    // Simulate playback - verify pattern repeats after 28 steps (LCM of 4 and 7)
    // Expected gate sequence: 0,1,2,3, 0,1,2,3, 0,1,2,3...
    // Expected note sequence: 0,1,2,3,4,5,6, 0,1,2,3,4,5,6, 0,1,2,3,4,5,6, 0,1,2,3,4,5,6

    // Test first 28 steps for expected gate/note combinations
    // (Full test implementation would verify engine state)
}

CASE("backward_vs_forward_runmode") {
    // Phase 2 test: Gate forward, Note backward
    NoteSequence seq;
    seq.setPolymetricMode(true);
    seq.gateLoop().runMode = Types::RunMode::Forward;
    seq.noteLoop().runMode = Types::RunMode::Backward;

    // Verify playheads move in opposite directions
}

} // UNIT_TEST
```

### Integration Testing

Manual testing checklist:
1. Enable polymetric mode, set different loop ranges → verify playheads advance independently
2. Set Gate 0-3, Note 0-6 → pattern should repeat every 28 steps
3. Disable polymetric mode → should revert to normal shared playhead
4. Test with different run modes (Pendulum, Random, etc.)
5. Save/load project → verify loop points persist
6. Phase 2: Test Gate forward + Note backward → verify opposite directions

---

## Implementation Roadmap

### Phase 1 Tasks
1. ✅ Plan document (this file)
2. ⬜ Add LayerLoop struct to NoteSequence.h
3. ⬜ Add polymetricMode flag and accessors
4. ⬜ Add serialization (write/read methods)
5. ⬜ Add _gatePlayhead and _notePlayhead to NoteTrackEngine
6. ⬜ Implement advancePlayhead() helper (basic forward mode)
7. ⬜ Modify triggerStep() to check polymetricMode
8. ⬜ Update reset()/restart() to clear playheads
9. ⬜ Write unit tests (TestNoteSequencePolymetric.cpp)
10. ⬜ Add minimal UI controls (enable/disable, loop point setting)
11. ⬜ Integration testing on hardware/simulator

### Phase 2 Tasks (Future)
1. ⬜ Add runMode to LayerLoop struct
2. ⬜ Add _gateSequenceState and _noteSequenceState
3. ⬜ Update advancePlayhead() to use SequenceState
4. ⬜ Add per-layer runMode UI controls
5. ⬜ Write Phase 2 tests
6. ⬜ Integration testing

---

## Open Questions / Future Enhancements

1. **More layers?** Extend to Length, Velocity, Retrigger layers?
   - Storage cost: +3 bytes per additional layer
   - UI complexity increases significantly

2. **Clock divisions?** Add per-layer clock div (Kria-style)?
   - Gate advances every 1 tick, Note every 2 ticks
   - Requires major refactoring of tick/trigger coupling
   - Consider for Phase 3

3. **UI representation?** How to visualize 2+ independent playheads?
   - Current step display shows one playhead
   - Could show multiple with different brightness/colors

4. **Compatibility?** What happens to old projects?
   - polymetricMode defaults to false
   - Existing sequences play normally
   - Loop points default to 0-15 (full range)

5. **Fill mode interaction?** How does fill work with polymetric mode?
   - Phase 1: disable polymetric during fill?
   - Or apply to fill sequence as well?

---

## References

- Original Kria documentation: [Monome Kria](https://monome.org/docs/ansible/kria/)
- Meadowkria port plan: `meadkria-full.md`
- SequenceState implementation: `src/apps/sequencer/engine/SequenceState.h`
- Test framework conventions: `CLAUDE.md` (Testing Conventions section)
