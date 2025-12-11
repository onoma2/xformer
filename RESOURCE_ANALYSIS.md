# PEW|FORMER Sequencer Firmware Resource Analysis for IndexedTrackEngine Implementation

## Hardware Specifications
- **Microcontroller**: STM32F405RGT6
- **Flash Memory**: 1024 KB (1 MB)
- **RAM**: 128 KB (plus 64 KB CCMRAM)
- **CPU**: ARM Cortex-M4 @ 168 MHz
- **Real-time OS**: FreeRTOS

## Current Project Configuration (Config.h)
- **Tracks**: 8 tracks (CONFIG_TRACK_COUNT)
- **Patterns**: 16 patterns + 1 snapshot (CONFIG_PATTERN_COUNT + CONFIG_SNAPSHOT_COUNT)
- **Steps**: 64 steps per sequence (CONFIG_STEP_COUNT)
- **PPQN**: 192 (parts per quarter note)
- **Sequence PPQN**: 48 (sequence resolution)

## Memory Usage Analysis

### 1. Sequence Memory Footprint

#### NoteSequence Memory:
- Each `NoteSequence::Step` uses 2 x 32-bit unions = 8 bytes
- Each `NoteSequence` has 64 steps × 8 bytes = 512 bytes
- Each `NoteSequence` has additional metadata: ~128 bytes
- Total per `NoteSequence`: ~640 bytes
- Per track: 17 sequences (16 patterns + 1 snapshot) × 640 bytes = 10,880 bytes
- For 8 tracks: 8 × 10,880 = **87,040 bytes**

#### CurveSequence Memory:
- Each `CurveSequence::Step` uses 1 × 32-bit + 1 × 16-bit unions = 6 bytes
- Each `CurveSequence` has 64 steps × 6 bytes = 384 bytes
- Each `CurveSequence` has additional metadata: ~192 bytes
- Total per `CurveSequence`: ~576 bytes
- Per track: 17 sequences × 576 bytes = 9,792 bytes
- For 8 tracks: 8 × 9,792 = **78,336 bytes**

#### TuesdaySequence Memory:
- Each `TuesdaySequence` is metadata-only (no per-step data)
- Each `TuesdaySequence` has ~128 bytes of parameters
- Per track: 17 sequences × 128 bytes = 2,176 bytes
- For 8 tracks: 8 × 2,176 = **17,408 bytes**

#### Total Sequence Memory: ~182,784 bytes
Wait, this exceeds the available RAM. Let me recalculate more carefully.

### Corrected Memory Analysis

Actually, looking at the Track model, only one track type is active per track at a time:
- Each track can be Note, Curve, MidiCv, or Tuesday (not all simultaneously)
- Each track contains only its relevant sequence type

So the memory is:
- If all tracks were Note tracks: 8 × 10,880 = 87,040 bytes
- If all tracks were Curve tracks: 8 × 9,792 = 78,336 bytes
- If all tracks were Tuesday tracks: 8 × 2,176 = 17,408 bytes

The system supports mixed track types, so the maximum would be if all are Note tracks: **~87 KB**

### 2. Track Engine Memory Footprint

#### NoteTrackEngine Memory:
- Base `TrackEngine`: ~64 bytes
- Sorted queues: 2 × 16 × (4-8 bytes) = ~384 bytes
- Additional state variables: ~256 bytes
- Total per engine: ~600 bytes

#### CurveTrackEngine Memory:
- Base `TrackEngine`: ~64 bytes
- Sorted queues: 1 × 16 × (4-8 bytes) = ~192 bytes
- Chaos generators: ~128 bytes
- Additional state variables: ~192 bytes
- Total per engine: ~576 bytes

#### MidiCvTrackEngine Memory:
- Base `TrackEngine`: ~64 bytes
- 8 voice structures: 8 × 16 bytes = ~128 bytes
- Arpeggiator: ~256 bytes
- Additional state variables: ~128 bytes
- Total per engine: ~576 bytes

#### TuesdayTrackEngine Memory:
- Base `TrackEngine`: ~64 bytes
- Algorithm state: ~512 bytes
- Additional state variables: ~128 bytes
- Total per engine: ~704 bytes

### 3. System-Wide Memory Usage

#### FreeRTOS Task Memory:
- Driver Task: 1024 stack = 1 KB
- Engine Task: 4096 stack = 4 KB
- USB Host Task: 2048 stack = 2 KB
- UI Task: 4096 stack = 4 KB
- File Task: 2048 stack = 2 KB
- Profiler Task: 2048 stack = 2 KB
- **Total Task Stacks**: 15 KB

#### Core System Memory:
- Display buffer (256×64 pixels): ~2 KB (compressed)
- MIDI buffers: ~1-2 KB
- File system buffers: ~2-4 KB
- Other system overhead: ~4-8 KB
- **Total System Overhead**: ~10-16 KB

#### Project/Model Memory:
- Project metadata: ~2-4 KB
- PlayState: ~1-2 KB
- Routing tables: ~1-2 KB
- Settings: ~1 KB
- **Total Project Overhead**: ~5-9 KB

### 4. IndexedTrackEngine Memory Requirements

Based on RES-indexed.md research document:

#### IndexedSequence Memory:
- Each `IndexedStep`: ~8-12 bytes (index, duration, gateLength, flags)
- 64 steps × 12 bytes = 768 bytes per sequence
- With metadata: ~900 bytes per sequence
- 17 sequences per track: ~15,300 bytes per track
- For 8 tracks: ~122,400 bytes (if all Indexed tracks)

#### IndexedTrackEngine Memory:
- Base `TrackEngine`: ~64 bytes
- Timing accumulator: ~8 bytes
- Additional state variables: ~128 bytes
- Sorted queues (if needed): ~192 bytes
- Total per engine: ~392 bytes

#### VoltageTable Memory:
- 100 entries × 2 bytes (uint16_t) = 200 bytes per track
- For 8 tracks: ~1,600 bytes

#### Total IndexedTrack Memory:
- Sequences: 122,400 bytes
- Tables: 1,600 bytes
- Engines: 8 × 392 = 3,136 bytes
- **Total**: ~127,136 bytes

### 5. Available Memory Assessment

#### RAM Allocation:
- **Total RAM**: 128 KB
- **Maximum sequence storage** (all Note tracks): 87 KB
- **Task stacks**: 15 KB
- **System overhead**: 10-16 KB
- **Used**: ~112-118 KB
- **Available**: ~10-16 KB

#### Memory Constraints:
- The IndexedTrack implementation would require ~127 KB
- This exceeds available RAM if combined with existing track types
- Need to consider implementation strategy

### 6. Implementation Strategy Recommendations

#### Option 1: Replace Existing Track Type
- Replace Tuesday track with Indexed track (saves ~17 KB)
- Still need to fit within ~30-36 KB available

#### Option 2: Reduced Capacity Implementation
- Reduce sequence steps from 64 to 32 (saves ~50%)
- Reduce patterns from 16 to 8 (saves ~50%)
- Reduce voltage table size from 100 to 64 entries

#### Option 3: Memory-Optimized Data Structures
- Pack IndexedStep data more efficiently
- Use 8-bit values where possible
- Share voltage tables between tracks

#### Option 4: Hybrid Approach
- Implement subset of ER-101 features
- Simplified timing (no pulse-based, use existing tick system)
- Reduced transform operations

### 7. CPU Utilization Assessment

#### Current Engine Load:
- Engine task runs at 4KB stack (adequate for current load)
- Tick processing is optimized with sorted queues
- Complex algorithms (Tuesday) show good performance

#### IndexedTrackEngine CPU Impact:
- Accumulator-based timing: Low overhead (simple addition/comparison)
- Voltage table lookup: Very low overhead
- Math transforms: Moderate (but typically one-time operations)
- **Assessment**: CPU usage should be manageable

### 8. Realistic Headroom for IndexedTrackEngine

#### Memory Headroom:
- **Conservative**: Implement only core functionality with reduced capacity
  - 32 steps instead of 64
  - 8 patterns instead of 16
  - 64-entry voltage table
  - Estimated memory: ~64 KB (feasible)

- **Moderate**: Implement most features with careful optimization
  - 48 steps
  - 12 patterns
  - 80-entry voltage table
  - Estimated memory: ~96 KB (challenging but possible)

- **Full**: Attempt full ER-101 compatibility
  - 64 steps
  - 16 patterns
  - 100-entry voltage table
  - Estimated memory: ~127 KB (not feasible with current architecture)

#### Recommended Approach:
- Start with **Conservative** implementation
- Prove concept with reduced capacity
- Optimize and expand as needed
- Consider memory-saving techniques like CCMRAM for frequently accessed data

### 9. Implementation Priority

1. **Data Structures**: Implement optimized IndexedStep and VoltageTable
2. **Engine Logic**: Create basic timing and output logic
3. **Integration**: Add to TrackMode enum and engine container
4. **UI**: Basic sequence editing
5. **Advanced Features**: Math transforms, table editor

### 10. Conclusion

The current PEW|FORMER firmware has **limited headroom** for a full-featured IndexedTrackEngine implementation due to memory constraints. The most realistic approach is to implement a **reduced-capacity version** with approximately half the original ER-101 feature set. This would allow the core indexed sequencing concept to be implemented while staying within the 128KB RAM constraint of the STM32F405.