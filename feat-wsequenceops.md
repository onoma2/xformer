# Feature: Performer Sequence Operations for Teletype

## Overview

This feature adds direct sequence manipulation capabilities to the Teletype implementation in Performer firmware. Currently, Teletype can only indirectly influence sequences through CV/Gate modulation and routing. This enhancement allows direct access and modification of sequence data from Teletype scripts.

## Motivation

While Teletype has powerful pattern manipulation capabilities, there's no direct way to modify the underlying sequence data of Note, Curve, or other track types. Users currently rely on indirect methods like routing Teletype CV outputs to sequence parameters, which limits the possibilities for dynamic sequence generation and modification.

## Proposed Operations

### 1. Note Sequence Operations

#### `SEQ.NOTE` - Get/Set Note Value
```
// Get note value: track pattern step -> note_value
X SEQ.NOTE 0 0 5    // X gets note value at track 0, pattern 0, step 5

// Set note value: note_value -> track pattern step
60 SEQ.NOTE 0 0 5   // Set note value 60 at track 0, pattern 0, step 5
```

#### `SEQ.GATE` - Get/Set Gate State
```
// Get gate state: track pattern step -> gate_state
X SEQ.GATE 1 0 3    // X gets gate state at track 1, pattern 0, step 3

// Set gate state: gate_state -> track pattern step
1 SEQ.GATE 1 0 3    // Set gate ON at track 1, pattern 0, step 3
```

#### `SEQ.LENGTH` - Get/Set Sequence Length
```
// Get length: track pattern -> length
X SEQ.LENGTH 0 0    // X gets length of track 0, pattern 0

// Set length: length -> track pattern
16 SEQ.LENGTH 0 0   // Set length to 16 steps for track 0, pattern 0
```

#### `SEQ.START` - Get/Set Sequence Start Point
```
// Get start: track pattern -> start_point
X SEQ.START 0 0     // X gets start point of track 0, pattern 0

// Set start: start_point -> track pattern
4 SEQ.START 0 0     // Set start point to step 4 for track 0, pattern 0
```

#### `SEQ.END` - Get/Set Sequence End Point
```
// Get end: track pattern -> end_point
X SEQ.END 0 0       // X gets end point of track 0, pattern 0

// Set end: end_point -> track pattern
12 SEQ.END 0 0      // Set end point to step 12 for track 0, pattern 0
```

### 2. Curve Sequence Operations

#### `SEQ.SHAPE` - Get/Set Curve Shape
```
// Get shape: track pattern step -> shape_value
X SEQ.SHAPE 2 0 5   // X gets shape at track 2 (curve), pattern 0, step 5

// Set shape: shape_value -> track pattern step
3 SEQ.SHAPE 2 0 5   // Set shape to value 3 at track 2, pattern 0, step 5
```

#### `SEQ.MIN` - Get/Set Curve Minimum Value
```
// Get min: track pattern step -> min_value
X SEQ.MIN 2 0 5     // X gets min value at track 2, pattern 0, step 5

// Set min: min_value -> track pattern step
0 SEQ.MIN 2 0 5     // Set min value to 0 at track 2, pattern 0, step 5
```

#### `SEQ.MAX` - Get/Set Curve Maximum Value
```
// Get max: track pattern step -> max_value
X SEQ.MAX 2 0 5     // X gets max value at track 2, pattern 0, step 5

// Set max: max_value -> track pattern step
255 SEQ.MAX 2 0 5   // Set max value to 255 at track 2, pattern 0, step 5
```

### 3. General Sequence Operations

#### `SEQ.CLEAR` - Clear Sequence Data
```
SEQ.CLEAR 0 0       // Clear all data in track 0, pattern 0
```

#### `SEQ.COPY` - Copy Between Patterns
```
SEQ.COPY 0 0 1      // Copy from track 0, pattern 0 to track 0, pattern 1
```

## Implementation Architecture

### 1. Teletype Operation Definition
```c
// In ops.c or similar teletype file
static void op_SEQ_SET_NOTE_get(const void *NOTUSED(data), scene_state_t *state) {
    // Get parameters from stack
    tele_word_t step = acc_pop(state);
    tele_word_t pattern = acc_pop(state);
    tele_word_t track = acc_pop(state);
    
    // Call the performer function to get note value
    tele_word_t note_value = performer_seq_get_note(track, pattern, step);
    acc_push(state, note_value);
}

static void op_SEQ_SET_NOTE_set(const void *NOTUSED(data), scene_state_t *state) {
    // Get parameters from stack
    tele_word_t note_value = acc_pop(state);
    tele_word_t step = acc_pop(state);
    tele_word_t pattern = acc_pop(state);
    tele_word_t track = acc_pop(state);
    
    // Call the performer function to set note value
    performer_seq_set_note(track, pattern, step, note_value);
}
```

### 2. Performer Bridge Implementation
```cpp
// In TeletypeTrackEngine.h
class TeletypeTrackEngine {
    // ... existing members
    tele_word_t getSequenceNote(int track, int pattern, int step);
    void setSequenceNote(int track, int pattern, int step, tele_word_t noteValue);
    
    // Helper functions for other sequence operations
    void setSequenceGate(int track, int pattern, int step, bool gateState);
    void setSequenceLength(int track, int pattern, int length);
    void setSequenceStart(int track, int pattern, int start);
    void setSequenceEnd(int track, int pattern, int end);
    void setCurveShape(int track, int pattern, int step, int shape);
    void setCurveMin(int track, int pattern, int step, int min);
    void setCurveMax(int track, int pattern, int step, int max);
};
```

### 3. Thread Safety
```cpp
// In TeletypeTrackEngine.cpp
tele_word_t TeletypeTrackEngine::getSequenceNote(int track, int pattern, int step) {
    if (track < 0 || track >= CONFIG_TRACK_COUNT) return 0;
    if (pattern < 0 || pattern >= CONFIG_PATTERN_COUNT) return 0;
    
    auto &project = _model.project();
    auto &trackObj = project.track(track);
    
    if (trackObj.trackMode() == Track::TrackMode::Note) {
        auto &sequence = project.noteSequence(track, pattern);
        if (step >= 0 && step <= sequence.lastStep()) {
            return sequence.step(step).note();
        }
    }
    return 0;
}

void TeletypeTrackEngine::setSequenceNote(int track, int pattern, int step, tele_word_t noteValue) {
    if (track < 0 || track >= CONFIG_TRACK_COUNT) return;
    if (pattern < 0 || pattern >= CONFIG_PATTERN_COUNT) return;
    
    // Use write lock to safely modify the sequence
    auto lock = _model.lockWrite();
    auto &project = _model.project();
    auto &trackObj = project.track(track);
    
    if (trackObj.trackMode() == Track::TrackMode::Note) {
        auto &sequence = project.noteSequence(track, pattern);
        if (step >= 0 && step <= sequence.lastStep()) {
            sequence.step(step).setNote(noteValue);
        }
    }
}
```

## Usage Examples

### 1. Dynamic Sequence Generation
```
#M (Metro script)
// Generate a random melody in track 0, pattern 0
STEP RAND 15
NOTE RAND 127
NOTE SEQ.NOTE 0 0 STEP
```

### 2. Pattern Evolution
```
#S0 (Init script)
LENGTH 8
LENGTH SEQ.LENGTH 1 0

#M (Metro script)
// Gradually increase sequence length
LENGTH SEQ.LENGTH 1 0
NEW_LENGTH ADD LENGTH 1
IF LT NEW_LENGTH 16: NEW_LENGTH SEQ.LENGTH 1 0
```

### 3. Conditional Sequence Modification
```
// When track 2's pattern is 8, modify track 0's sequence
IF EQ WP 2 8: 60 SEQ.NOTE 0 0 0
IF EQ WP 2 8: 1 SEQ.GATE 0 0 0
```

### 4. Algorithmic Composition
```
#M (Metro script)
// Fibonacci sequence in notes
A SEQ.NOTE 0 0 0
B SEQ.NOTE 0 0 1
NEXT ADD A B
NEXT SEQ.NOTE 0 0 2
A B
B NEXT
```

## Benefits

1. **Direct Sequence Manipulation**: Allows Teletype to directly modify sequence data rather than just modulating parameters
2. **Algorithmic Composition**: Enables complex algorithmic composition techniques directly in Teletype
3. **Dynamic Sequences**: Sequences can evolve and change in real-time based on Teletype logic
4. **Enhanced Interactivity**: Greater interactivity between Teletype scripts and sequence data
5. **Performance**: More efficient than routing-based approaches for complex sequence modifications

## Potential Issues and Mitigations

1. **Performance Impact**: Direct sequence modification could impact performance if done excessively
   - Mitigation: Limit the rate of sequence modifications or batch operations

2. **Thread Safety**: Accessing sequence data from Teletype thread requires proper synchronization
   - Mitigation: Use appropriate locks when modifying sequence data

3. **Complexity**: Adds complexity to the Teletype operation set
   - Mitigation: Well-documented examples and gradual rollout

4. **Memory Usage**: Additional operations consume memory in the Teletype VM
   - Mitigation: Efficient implementation and optional compilation

## Testing Strategy

1. **Unit Tests**: Test each sequence operation individually
2. **Integration Tests**: Test operations with actual sequence data
3. **Performance Tests**: Ensure operations don't impact audio performance
4. **Stress Tests**: Test rapid sequence modifications
5. **Safety Tests**: Verify thread safety and proper locking

## Implementation Priority

1. **Phase 1**: Basic note/gate operations (`SEQ.NOTE`, `SEQ.GATE`, `SEQ.LENGTH`)
2. **Phase 2**: Curve sequence operations (`SEQ.SHAPE`, `SEQ.MIN`, `SEQ.MAX`)
3. **Phase 3**: Advanced operations (`SEQ.COPY`, `SEQ.CLEAR`, start/end points)
4. **Phase 4**: Performance optimizations and safety checks

## Compatibility

- Maintains backward compatibility with existing Teletype scripts
- New operations will have no effect if used in older firmware versions
- Existing sequence functionality remains unchanged