# Tuesday Algorithm Porting Analysis - REVISED
## Based on Performer Architecture Review

**Date**: 2024-12-04
**Revision**: 2.0 (Architecture-Grounded)

---

## Executive Summary

After examining the actual Performer architecture (NoteTrack, TuesdayTrack, clock division, scale quantization), this revised analysis provides grounded recommendations for porting backup algorithms. The key insight: **NoteTrack and TuesdayTrack serve fundamentally different purposes** - one is a step sequencer with per-step programming, the other is an algorithmic pattern generator.

**Key Finding**: Most backup algorithms were designed for the OLD buffer-based TuesdayTrack. The NEW TuesdayTrack uses `generateStep()` which is much simpler to port to, and shares the same clock/scale infrastructure as NoteTrack.

**Revised Recommendations**: Port **3 algorithms** that provide unique value vs NoteTrack:
1. **MINIMAL** (Pattern A) - Sparse mutation engine
2. **DRILL** (Pattern B) - Techno gesture machine
3. **FUNK** (Pattern B) - Syncopated groove generator

**RAGA removed from recommendations** - too similar to NoteTrack's Harmony + Accumulator capabilities.

---

## Part 1: Architecture Comparison

### 1.1 NoteTrack vs TuesdayTrack - Design Philosophy

| Aspect | NoteTrack | TuesdayTrack |
|--------|-----------|--------------|
| **Purpose** | Step Sequencer | Pattern Generator |
| **Control** | Per-step programming | Algorithmic parameters |
| **User Input** | Direct: edit each step | Indirect: tweak algorithm params |
| **Layers** | 19 per-step layers | 14 sequence-level params |
| **Predictability** | High (you set each step) | Medium (algorithms surprise) |
| **Complexity** | High (many params per step) | Low (few params control many steps) |
| **Best For** | Precise melodic sequences | Evolving generative patterns |

**Key Insight**: They're **complementary**, not competing. NoteTrack is for "I want this exact sequence." TuesdayTrack is for "surprise me with variations on this style."

### 1.2 Clock Division System (IDENTICAL)

Both systems use the same clock division formula:

```cpp
// NoteTrackEngine.cpp:226 and TuesdayTrackEngine.cpp:944
uint32_t divisor = sequence.divisor() * (CONFIG_PPQN / CONFIG_SEQUENCE_PPQN);
// = sequence.divisor() * (192 / 48) = sequence.divisor() * 4
```

**Implications for Porting**:
- ✅ Backup algorithms don't need timing conversion
- ✅ Step triggers align naturally
- ✅ Micro-gate queue timing is already correct
- ✅ No architectural changes needed for clock system

### 1.3 Scale Quantization (SIMILAR, KEY DIFFERENCES)

Both systems use `scale.noteToVolts()` for final quantization, but with different integration:

**NoteTrack** (`NoteTrackEngine.cpp:72-177`):
```cpp
static float evalStepNote(...) {
    int note = step.note() + evalTransposition(scale, octave, transpose);

    // 1. Harmony follower system (reads master track)
    if (harmonyRole != HarmonyOff) {
        note = harmonyEngine.harmonize(masterNote, scaleDegree);
    }

    // 2. Accumulator modulation
    if (sequence.accumulator().enabled()) {
        note += accumulatorValue;  // Track or Stage mode
    }

    // 3. Per-step variation (probability + range)
    if (rng.nextRange() <= probability) {
        note += rng.nextRange(variationRange);
    }

    return scale.noteToVolts(note) + rootNoteOffset;
}
```

**TuesdayTrack** (`TuesdayTrackEngine.cpp:295-341`):
```cpp
float scaleToVolts(int noteIndex, int octave) const {
    // 1. Resolve scale (track or project)
    const Scale &scale = useProjectScale ? project.scale : Scale::get(trackScale);

    // 2. Resolve root note
    int rootNote = (trackRoot < 0) ? project.rootNote() : trackRoot;

    // 3. Quantize if enabled
    if (quantize) {
        int totalDegree = noteIndex + (octave * scale.notesPerOctave());
        totalDegree += transpose;  // Apply transpose in degrees
        voltage = scale.noteToVolts(totalDegree) + rootNoteOffset;
    } else {
        // Chromatic mode: direct semitone → voltage
        voltage = (noteIndex + octave*12 + transpose) * (1.f / 12.f);
    }

    return voltage + octaveOffset;
}
```

**Key Differences**:

| Feature | NoteTrack | TuesdayTrack |
|---------|-----------|--------------|
| Harmony Engine | ✅ Full integration | ❌ None |
| Accumulator | ✅ Track/Stage modes | ❌ None |
| Per-step Variation | ✅ Probability + Range | ❌ None |
| Chromatic Mode | ✅ Via scale selection | ✅ Via `useScale` flag |
| Algorithm Control | ❌ None | ✅ Full integration |

**Implications for Porting**:
- ✅ Scale quantization is already identical at the pipeline level
- ✅ No changes needed to `scaleToVolts()`
- ❌ Backup algorithms can't use Harmony (but they don't need it - they generate their own harmony via algorithm logic)
- ❌ Backup algorithms can't use Accumulator (but they have internal state machines instead)

### 1.4 Gate/CV Queue Systems (DIFFERENT ARCHITECTURES)

**NoteTrack** - Separate queues:
```cpp
// NoteTrackEngine.h:99-106
struct Gate { uint32_t tick; bool gate; bool shouldTickAccumulator; uint8_t sequenceId; };
struct Cv { uint32_t tick; float cv; bool slide; };

SortedQueue<Gate, 16, GateCompare> _gateQueue;
SortedQueue<Cv, 16, CvCompare> _cvQueue;
```

**TuesdayTrack** - Unified micro-gate queue:
```cpp
// TuesdayTrackEngine.h:78-82
struct MicroGate { uint32_t tick; bool gate; float cvTarget; };

SortedQueue<MicroGate, 32, MicroGateCompare> _microGateQueue;
```

**Why the difference?**
- **NoteTrack**: Needs separate CV updates for slide/portamento (CV changes while gate stays high)
- **TuesdayTrack**: CV always updates with gate (simpler, gate-locked CV)

**Implications for Porting**:
- ✅ Unified queue is simpler for algorithm porting
- ✅ Polyrhythm/Trill modes work naturally with micro-gate queue
- ⚠️ If backup algorithm needs CV slide during held gates, may need refactoring

### 1.5 Parameter Philosophy (FUNDAMENTALLY DIFFERENT)

**NoteTrack - 19 Per-Step Layers**:
```cpp
// NoteSequence::Step has 19 editable layers:
Gate, GateProbability, GateOffset, GateMode,
Retrigger, RetriggerProbability,
Length, LengthVariationRange, LengthVariationProbability,
Note, NoteVariationRange, NoteVariationProbability,
Slide, PulseCount, Condition,
HarmonyRoleOverride, InversionOverride, VoicingOverride,
AccumulatorTrigger, AccumulatorStepValue
```

User controls: **Every. Single. Step.** (up to 64 steps × 19 layers = 1,216 editable values per pattern!)

**TuesdayTrack - 14 Sequence-Level Parameters**:
```cpp
// TuesdaySequence has 14 parameters that control ALL steps:
algorithm, flow, ornament, power,
loopLength, glide, trill,
octave, transpose, divisor, resetMeasure,
scale, rootNote, scan, rotate,
gateLength, gateOffset, cvUpdateMode
```

User controls: **The algorithm's character** (14 values control entire evolving pattern)

**This is the core difference**: NoteTrack is for **explicit control**, TuesdayTrack is for **emergent complexity**.

---

## Part 2: Backup Algorithm Classification

### 2.1 Pattern Types in Backup Algorithms

From analyzing `./algo-backup/`, I identified 4 pattern types:

**Pattern A: Sparse Mutation Engines** (5-15% state change per step)
- MINIMAL, AMBIENT, ACID, KRAFT
- Best for: Slowly evolving textures, generative ambience
- Unique value: **Emergent long-form composition** (NoteTrack can't do this)

**Pattern B: Gesture State Machines** (phrase-based, mode switching)
- DRILL, FUNK, POLYRHYTHM
- Best for: Dance music, rhythmic variation, syncopation
- Unique value: **Musical phrases and transitions** (NoteTrack does this via manual programming)

**Pattern C: Range-Biased Random** (stateless entropy)
- RAGA
- Best for: Controlled chaos within musical constraints
- Unique value: **OVERLAPS with NoteTrack's Note Variation + Probability system**

**Pattern D: Linear Walkers** (deterministic traversal)
- Already implemented: TEST (OCTSWEEPS, SCALEWALKER modes)
- Unique value: Minimal (NoteTrack can program these patterns manually)

### 2.2 Feature Overlap Analysis

**What backup algorithms can do that NoteTrack CANNOT**:
1. ✅ **Sparse mutation** (MINIMAL, AMBIENT) - NoteTrack plays fixed sequences, can't do 5% random note changes
2. ✅ **Gesture state machines** (DRILL, FUNK) - NoteTrack doesn't have "modes" that switch behaviors
3. ✅ **Polyrhythmic micro-sequencing** (POLYRHYTHM) - NoteTrack's PulseCount is simpler (no spatial distribution)
4. ✅ **Chaos probability cascades** (ACID's note→gate→slide triggering) - NoteTrack has independent probabilities

**What NoteTrack can do that backup algorithms CANNOT**:
1. ✅ **Harmony follower** - Read another track and harmonize in real-time
2. ✅ **Accumulator modulation** - Ramp CV over time with per-step triggers
3. ✅ **Complex conditions** (Fill, Loop, Pre, Not, etc.) - 14 condition types
4. ✅ **Per-step overrides** (inversion, voicing, harmony role per step)
5. ✅ **Precise manual control** - User sets exact notes/gates/lengths

**What they SHARE**:
1. ✅ **Scale quantization** - Both use `scale.noteToVolts()`
2. ✅ **Clock division** - Both use `divisor * 4`
3. ✅ **Retrigger/Ratcheting** - NoteTrack has Retrigger layer, Tuesday has micro-gates
4. ✅ **Slide/Portamento** - Both support glide
5. ✅ **Gate offset** - NoteTrack has GateOffset layer, Tuesday has gateOffset parameter

### 2.3 RAGA Re-Evaluation

**Original Recommendation**: RAGA for "controlled melodic chaos"

**After Architecture Review**: **REJECT RAGA**

**Why?** NoteTrack can already do everything RAGA does:

```cpp
// RAGA algorithm (from backup):
int note = rootNote + scale[rng.nextRange(scaleLength)];
int octave = baseOctave + rng.nextRange(octaveRange);
```

**NoteTrack equivalent** (user programs this in UI):
- Set step.note() = root note (e.g., 0 for C)
- Set step.noteVariationRange() = 6 (for 7-note scale)
- Set step.noteVariationProbability() = 100% (always vary)
- Set sequence.octave() = baseOctave
- Use Accumulator in Track mode to shift octaves over time

**Result**: Same output, but with MORE control (per-step probability, per-step range, accumulator ramps).

**Verdict**: RAGA is **redundant**. NoteTrack's per-step variation system is strictly more powerful.

---

## Part 3: Revised Porting Recommendations

### 3.1 Top 3 Algorithms for Porting

Based on **unique value vs NoteTrack** and **implementation complexity**:

#### **TIER 1: MUST-PORT** (Unique features, medium complexity)

**1. MINIMAL** (Pattern A - Sparse Mutation Engine)
- **Why**: Emergent long-form composition that NoteTrack can't replicate
- **Unique Feature**: 5-15% mutation rate creates slowly evolving soundscapes
- **Implementation**: ~150 lines, no gesture state, simple mutation logic
- **Porting Complexity**: ⭐⭐☆☆☆ (Easy - mostly RNG calls)
- **Testing Strategy**: Verify mutation rate stays within 5-15%, loop stability
- **Estimated Effort**: 4-6 hours

**2. DRILL** (Pattern B - Techno Gesture Machine)
- **Why**: Multi-mode state machine that NoteTrack can't replicate
- **Unique Feature**: GROOVE/FILL/BREAKDOWN modes with smooth transitions
- **Implementation**: ~200 lines, 3 gesture modes, transition logic
- **Porting Complexity**: ⭐⭐⭐☆☆ (Medium - state machine logic)
- **Testing Strategy**: Verify mode transitions, gesture phrase structure
- **Estimated Effort**: 8-12 hours

**3. FUNK** (Pattern B - Syncopated Groove Generator)
- **Why**: Syncopation and accent patterns that NoteTrack requires manual programming
- **Unique Feature**: Automatic offbeat emphasis and swing feel
- **Implementation**: ~180 lines, syncopation logic, accent system
- **Porting Complexity**: ⭐⭐⭐☆☆ (Medium - timing offset calculations)
- **Testing Strategy**: Verify syncopation timing, accent distribution
- **Estimated Effort**: 8-12 hours

#### **TIER 2: OPTIONAL** (Interesting but lower priority)

**4. AMBIENT** (Pattern A - Sparse Mutation Engine)
- **Why**: Similar to MINIMAL but with longer hold times
- **Unique Feature**: Slow drone evolution (1-2 minute timescales)
- **Implementation**: ~140 lines (very similar to MINIMAL)
- **Porting Complexity**: ⭐⭐☆☆☆ (Easy if MINIMAL done first)
- **Estimated Effort**: 4-6 hours

**5. POLYRHYTHM** (Pattern B - Spatial Polyrhythm)
- **Why**: NoteTrack's PulseCount is temporal (burst), this is spatial (spread)
- **Unique Feature**: Even distribution across beat window (3:4, 5:4, 7:4)
- **Implementation**: ~250 lines, complex micro-gate scheduling
- **Porting Complexity**: ⭐⭐⭐⭐☆ (Hard - queue management)
- **Estimated Effort**: 12-16 hours

#### **TIER 3: SKIP** (Redundant or low value)

**❌ RAGA** - NoteTrack's Note Variation system does this better
**❌ OCTSWEEPS/SCALEWALKER** - Already implemented in TEST algorithm
**❌ ACID** - Need to review if Slide probability + Retrigger covers this
**❌ KRAFT** - Very similar to MINIMAL, lower priority

### 3.2 Implementation Order

**Phase 1: Core Infrastructure** (Week 1)
- ✅ Already complete: generateStep() contract, RNG system, scale quantization
- ✅ Already complete: Micro-gate queue, cooldown system

**Phase 2: MINIMAL Port** (Week 2)
1. Implement MINIMAL algorithm (4-6 hours)
2. Write unit tests for mutation rate (2 hours)
3. Test with all scales/settings (2 hours)
4. Document algorithm behavior (1 hour)

**Phase 3: DRILL Port** (Week 3)
1. Implement gesture state machine (6-8 hours)
2. Write unit tests for mode transitions (3 hours)
3. Test with Flow parameter (flow = mode transition speed) (2 hours)
4. Document gesture modes (1 hour)

**Phase 4: FUNK Port** (Week 4)
1. Implement syncopation engine (6-8 hours)
2. Write unit tests for accent distribution (3 hours)
3. Test with Power parameter (power = accent strength) (2 hours)
4. Document syncopation logic (1 hour)

**Phase 5: Polish & Integration** (Week 5)
1. Cross-algorithm testing (all 14 algorithms + 3 new)
2. Performance profiling (ensure <10% CPU per track)
3. Documentation updates (Tuesday-Track-Flow.md)
4. User testing and feedback

**Total Effort**: ~40-50 hours for 3 algorithms (MINIMAL, DRILL, FUNK)

### 3.3 Architecture Considerations

**No Major Changes Needed**:
- ✅ Clock division: Already correct
- ✅ Scale quantization: Already correct
- ✅ Micro-gate queue: Already supports polyrhythm/trill
- ✅ RNG system: Already supports dual streams
- ✅ Parameter caching: Already implemented

**Minor Changes for Each Algorithm**:

**MINIMAL**:
```cpp
// Add to TuesdayTrackEngine.h (private section):
uint8_t _minimal_mode;          // 0=hold, 1=mutate
uint8_t _minimal_countdown;     // Steps until next mutation
int8_t _minimal_last_note;      // Last played note
uint8_t _minimal_last_octave;   // Last played octave
```

**DRILL**:
```cpp
// Add to TuesdayTrackEngine.h (private section):
enum DrillMode { GROOVE = 0, FILL = 1, BREAKDOWN = 2 };
DrillMode _drill_mode;          // Current gesture mode
uint8_t _drill_mode_countdown;  // Steps until mode switch
uint8_t _drill_accent_pattern;  // 8-bit accent mask
```

**FUNK**:
```cpp
// Add to TuesdayTrackEngine.h (private section):
uint8_t _funk_beat_phase;       // Position in 4-beat cycle
bool _funk_offbeat_active;      // Currently on offbeat
uint8_t _funk_accent_strength;  // 0-255
```

**No changes needed to**:
- TuesdaySequence.h (parameters already sufficient)
- Model layer (no new data types)
- UI layer (algorithms auto-discovered)
- Queue systems (micro-gate queue handles everything)

---

## Part 4: Testing Strategy

### 4.1 Unit Test Requirements

Each algorithm needs:

**1. Contract Compliance Tests** (verify TuesdayTickResult fields)
```cpp
UNIT_TEST("AlgorithmName") {

CASE("populates_all_contract_fields") {
    TuesdayTickResult result = generateStep(0);
    expectTrue(result.note >= 0 && result.note <= 6, "note in range");
    expectTrue(result.octave >= -2 && result.octave <= 4, "octave in range");
    expectTrue(result.velocity >= 0, "velocity non-negative");
    expectTrue(result.gateRatio > 0, "gateRatio positive");
    // ... etc for all fields
}

} // UNIT_TEST
```

**2. Parameter Response Tests** (verify Flow/Ornament/Power effects)
```cpp
CASE("flow_affects_behavior") {
    sequence.setFlow(0);
    auto result0 = generateStepMultiple(100);

    sequence.setFlow(16);
    auto result16 = generateStepMultiple(100);

    expectTrue(result0 != result16, "flow changes output");
}
```

**3. RNG Seed Tests** (verify deterministic behavior)
```cpp
CASE("same_seed_same_output") {
    reseed();  // Reset to seed1=flow, seed2=ornament
    auto result1 = generateStepMultiple(100);

    reseed();  // Same seeds
    auto result2 = generateStepMultiple(100);

    expectEqual(result1, result2, "deterministic output");
}
```

**4. Pattern Integrity Tests** (verify algorithm-specific behavior)
```cpp
// MINIMAL-specific:
CASE("mutation_rate_within_bounds") {
    int mutations = 0;
    int lastNote = -1;
    for (int i = 0; i < 100; i++) {
        auto result = generateStep(i * divisor);
        if (result.note != lastNote) mutations++;
        lastNote = result.note;
    }
    expectTrue(mutations >= 5 && mutations <= 15, "5-15% mutation rate");
}

// DRILL-specific:
CASE("mode_transitions_smooth") {
    // Verify mode changes happen at phrase boundaries
    // Verify no abrupt parameter jumps
}

// FUNK-specific:
CASE("syncopation_timing_correct") {
    // Verify offbeat gates have gateOffset != 0
    // Verify accent pattern follows 4-beat cycle
}
```

### 4.2 Integration Test Requirements

**1. Clock Division Test** (verify timing alignment)
```cpp
TEST_INTEGRATION("AlgorithmClockSync") {
    // Run algorithm for 4 bars (768 ticks at divisor=4)
    // Verify step triggers align with NoteTrack step triggers
}
```

**2. Scale Quantization Test** (verify all scales work)
```cpp
TEST_INTEGRATION("AlgorithmScaleSupport") {
    for (int scaleIdx = 0; scaleIdx < Scale::Count; scaleIdx++) {
        sequence.setScale(scaleIdx);
        // Verify output voltages map to scale degrees
    }
}
```

**3. Loop Stability Test** (verify finite loops repeat)
```cpp
TEST_INTEGRATION("AlgorithmLoopStability") {
    sequence.setLoopLength(16);  // 16-step loop

    // Capture first loop
    std::vector<TuesdayTickResult> loop1;
    for (int i = 0; i < 16; i++) {
        loop1.push_back(generateStep(i * divisor));
    }

    // Capture second loop
    reseed();  // Should reset to same state
    std::vector<TuesdayTickResult> loop2;
    for (int i = 0; i < 16; i++) {
        loop2.push_back(generateStep(i * divisor));
    }

    // Verify loops match
    expectEqual(loop1, loop2, "loop repeats");
}
```

**4. Parameter Range Test** (verify all param combos work)
```cpp
TEST_INTEGRATION("AlgorithmParameterCoverage") {
    // Test all combinations of:
    // - flow: 0, 8, 16
    // - ornament: 0, 8, 16
    // - power: 0, 8, 16
    // - loopLength: 0 (infinite), 16, 64
    // Total: 3^3 * 3 = 81 combinations

    for (auto [flow, ornament, power, loopLength] : testCombos) {
        sequence.setFlow(flow);
        sequence.setOrnament(ornament);
        sequence.setPower(power);
        sequence.setLoopLength(loopLength);

        // Run for 100 steps, verify no crashes
        for (int i = 0; i < 100; i++) {
            generateStep(i * divisor);
        }
    }
}
```

### 4.3 Hardware Test Requirements

**STM32 Performance** (from CLAUDE.md):
- ✅ Verify `Engine::update()` execution time stays under budget
- ✅ Test with 8 tracks running simultaneously
- ✅ Profile memory usage (stack + heap)
- ✅ Test OLED noise impact (algorithms shouldn't affect UI noise)

**Real-Time Behavior**:
- ✅ Verify micro-gate queue doesn't overflow (max 32 gates)
- ✅ Test at extreme tempos (40 BPM to 300 BPM)
- ✅ Verify no timing jitter (gates fire on exact ticks)

---

## Part 5: Architectural Insights

### 5.1 Why TuesdayTrack Exists (Despite NoteTrack's Power)

**NoteTrack Problem**: **"The Blank Canvas Problem"**
- 64 steps × 19 layers = 1,216 parameters to set
- Requires **explicit decision-making** for every note/gate/length
- Best for: **Intentional composition** (you know what you want)
- Worst for: **Exploration and happy accidents** (too much control)

**TuesdayTrack Solution**: **"The Constraint-Based Exploration"**
- 14 parameters control **emergent patterns**
- Requires **taste and curation** (tweak until it sounds good)
- Best for: **Discovery and variation** (surprise me!)
- Worst for: **Precise sequences** (you can't force exact notes)

**Analogy**:
- NoteTrack = **Photoshop** (pixel-perfect control)
- TuesdayTrack = **Instagram filters** (quick aesthetic transformations)

### 5.2 When to Use Which Track Mode

**Use NoteTrack when**:
- ✅ You have a specific melody/bassline/drum pattern in mind
- ✅ You need exact timing (gate offset, length per step)
- ✅ You want harmony follower (backing tracks that follow lead)
- ✅ You want accumulator ramps (rising/falling CV over bars)
- ✅ You need complex conditions (play on fill, skip on loop 2, etc.)

**Use TuesdayTrack when**:
- ✅ You want evolving textures (set-and-forget background patterns)
- ✅ You want happy accidents (algorithmic surprise)
- ✅ You want to explore musical styles (techno, ambient, acid, etc.)
- ✅ You want polyrhythmic patterns (3:4, 5:4, 7:4 without manual programming)
- ✅ You want to jam (tweak Flow/Ornament while sequence plays)

**Use Both Together**:
- ✅ NoteTrack on Track 1: Precise kick drum pattern
- ✅ TuesdayTrack on Track 2: Evolving bassline (DRILL algorithm)
- ✅ NoteTrack on Track 3: Lead melody (with Harmony following Track 2)
- ✅ TuesdayTrack on Track 4: Ambient pad (MINIMAL algorithm)

### 5.3 Architectural Lessons for Backup Porting

**1. Don't Try to Make TuesdayTrack Do NoteTrack's Job**
- ❌ Don't port RAGA (NoteTrack does it better)
- ❌ Don't port simple walkers (NoteTrack can program these)
- ✅ Port algorithms that create **emergent complexity** (MINIMAL, DRILL, FUNK)

**2. Leverage Existing Infrastructure**
- ✅ Clock division: Already identical
- ✅ Scale quantization: Already identical
- ✅ Micro-gate queue: Already supports polyrhythm
- ✅ RNG system: Already supports dual streams
- ❌ Don't create parallel systems (use what's there)

**3. Keep generateStep() Simple**
- ✅ Return TuesdayTickResult (11 fields)
- ✅ Use cached parameters (avoid re-reading sequence)
- ✅ Update state sparingly (5-15% mutation for Pattern A)
- ❌ Don't do direct gate scheduling (use micro-gate queue)
- ❌ Don't do CV smoothing (slide system handles it)

**4. Test Against NoteTrack Capabilities**
- ✅ Ask: "Can NoteTrack already do this?"
- ✅ If yes: Skip or simplify
- ✅ If no: Port it!

---

## Part 6: Final Recommendations

### 6.1 Priority Algorithms (Revised)

**TIER 1: Must-Port** (Unique + Achievable)
1. **MINIMAL** - Sparse mutation engine (emergent long-form)
2. **DRILL** - Techno gesture machine (mode-based variation)
3. **FUNK** - Syncopated groove generator (automatic offbeat)

**Total Effort**: ~40-50 hours for all three

**TIER 2: Nice-to-Have** (Unique but optional)
4. **AMBIENT** - Slow drone evolution (similar to MINIMAL)
5. **POLYRHYTHM** - Spatial polyrhythm (harder to implement)

**TIER 3: Skip** (Redundant or low value)
- ❌ **RAGA** - NoteTrack's variation system covers this
- ❌ **ACID** - Review if Slide + Retrigger covers this
- ❌ **KRAFT** - Too similar to MINIMAL
- ❌ Linear walkers - Already in TEST algorithm

### 6.2 Success Criteria

**Each ported algorithm must**:
1. ✅ Pass all unit tests (contract, parameters, RNG, pattern)
2. ✅ Pass all integration tests (clock, scale, loop, params)
3. ✅ Run on STM32 without performance issues
4. ✅ Provide **unique value vs NoteTrack** (verified in user testing)
5. ✅ Be documented in Tuesday-Track-Flow.md

**Project success**:
- ✅ 3 new algorithms ported and stable
- ✅ Total algorithm count: 14 + 3 = 17
- ✅ All algorithms share common infrastructure
- ✅ Clear documentation of when to use TuesdayTrack vs NoteTrack

### 6.3 Deployment Plan

**Phase 1: MINIMAL** (Week 2)
- Simplest algorithm, good for validating porting workflow
- Tests infrastructure changes
- Low risk

**Phase 2: DRILL** (Week 3)
- More complex (state machine)
- Tests gesture mode system
- Medium risk

**Phase 3: FUNK** (Week 4)
- Complex timing (syncopation)
- Tests micro-gate queue edge cases
- Medium risk

**Phase 4: Polish** (Week 5)
- Integration testing with all 17 algorithms
- Documentation updates
- User testing

### 6.4 Open Questions for User

1. **ACID Algorithm**: Should we review if NoteTrack's Slide probability + Retrigger system already covers ACID's features? Or is the cascade triggering (note → gate → slide) unique enough to port?

2. **AMBIENT Algorithm**: It's very similar to MINIMAL (just slower mutation). Port both, or just MINIMAL?

3. **POLYRHYTHM Algorithm**: High implementation cost (~12-16 hours). Worth it for spatial polyrhythm feature?

4. **Algorithm Metadata System**: The original analysis proposed 7 system improvements (metadata, categories, tags, etc.). Should we implement these alongside the porting work, or defer to later?

---

## Appendix A: Architecture Reference

### A.1 Key Files for Porting

**Model Layer**:
- `src/apps/sequencer/model/TuesdaySequence.h` - Sequence parameters (14 params)
- `src/apps/sequencer/model/TuesdaySequence.cpp` - Parameter methods

**Engine Layer**:
- `src/apps/sequencer/engine/TuesdayTrackEngine.h` - Engine state, TuesdayTickResult contract
- `src/apps/sequencer/engine/TuesdayTrackEngine.cpp` - generateStep(), scaleToVolts(), tick()

**Testing**:
- `src/tests/unit/sequencer/TestTuesdayXXX.cpp` - Unit tests
- `src/tests/integration/` - Integration tests (less common)

### A.2 Key Constants

```cpp
// From src/apps/sequencer/Config.h
CONFIG_PPQN = 192                    // Parts per quarter note
CONFIG_SEQUENCE_PPQN = 48            // Sequence resolution
divisor_multiplier = 192 / 48 = 4    // Clock division factor

// From TuesdayTrackEngine.h
MICRO_GATE_QUEUE_SIZE = 32           // Max scheduled gates
MAX_POLY_COUNT = 8                   // Max micro-gates per step

// From TuesdaySequence.h
ALGORITHM_COUNT = 11 (implemented)   // 0=Test, 1=Tri, 2=Stomper, 6=Markov...
ALGORITHM_AVAILABLE = 10 (backup)    // MINIMAL, RAGA, FUNK, etc.
```

### A.3 Key Functions

**Clock Division** (identical for both track types):
```cpp
uint32_t divisor = sequence.divisor() * (CONFIG_PPQN / CONFIG_SEQUENCE_PPQN);
bool stepTrigger = (relativeTick % divisor == 0);
```

**Scale Quantization** (both use same pipeline):
```cpp
// TuesdayTrack
float voltage = scale.noteToVolts(scaleDegree + octave*7) + rootOffset + octaveOffset;

// NoteTrack
float voltage = scale.noteToVolts(note + transpose + octave*7 + accumulator) + rootOffset;
```

**Micro-Gate Scheduling**:
```cpp
_microGateQueue.push({ tick + gateOffset, true, cvTarget });   // Gate ON
_microGateQueue.push({ tick + gateOffset + length, false, cvTarget });  // Gate OFF
```

---

## Appendix B: Comparison Tables

### B.1 Feature Matrix

| Feature | NoteTrack | TuesdayTrack |
|---------|-----------|--------------|
| **Control Style** | Explicit (per-step) | Implicit (algorithm) |
| **Parameter Count** | 1,216 (64×19) | 14 (sequence-level) |
| **Predictability** | High | Medium |
| **Harmony System** | ✅ Master/Follower | ❌ None |
| **Accumulator** | ✅ Track/Stage | ❌ None |
| **Per-Step Variation** | ✅ Probability+Range | ❌ None |
| **Algorithmic Generation** | ❌ None | ✅ 11 algorithms |
| **Polyrhythm** | ⚠️ Burst only | ✅ Spatial+Temporal |
| **Gate Queue** | Separate (size 16) | Unified (size 32) |
| **CV Queue** | Separate (size 16) | Unified with gate |
| **Condition System** | ✅ 14 types | ❌ None |
| **Fill Patterns** | ✅ Yes | ❌ None |
| **Slide/Portamento** | ✅ Per-step | ✅ Probability |
| **Clock Division** | ✅ divisor×4 | ✅ divisor×4 |
| **Scale Quantization** | ✅ noteToVolts() | ✅ noteToVolts() |

### B.2 Algorithm Unique Value Matrix

| Algorithm | Pattern Type | NoteTrack Can Do? | Unique Value | Port Priority |
|-----------|--------------|-------------------|--------------|---------------|
| MINIMAL | A (Sparse Mutation) | ❌ No | Emergent composition | **HIGH** |
| DRILL | B (Gesture Machine) | ⚠️ Manual | Mode-based variation | **HIGH** |
| FUNK | B (Gesture Machine) | ⚠️ Manual | Auto-syncopation | **HIGH** |
| AMBIENT | A (Sparse Mutation) | ❌ No | Slow drone evolution | MEDIUM |
| POLYRHYTHM | B (Spatial Poly) | ⚠️ Burst only | Spatial distribution | MEDIUM |
| RAGA | C (Range Random) | ✅ Yes | None (redundant) | **SKIP** |
| ACID | B (Cascade) | ⚠️ Maybe | Cascade triggering | REVIEW |
| KRAFT | A (Sparse Mutation) | ❌ No | Similar to MINIMAL | LOW |
| OCTSWEEPS | D (Linear Walker) | ✅ Yes | Already in TEST | **SKIP** |
| SCALEWALKER | D (Linear Walker) | ✅ Yes | Already in TEST | **SKIP** |

---

## Conclusion

After deep analysis of Performer architecture, the key insight is: **TuesdayTrack and NoteTrack are complementary, not competing**. Port algorithms that provide **emergent complexity** and **algorithmic surprise**, not algorithms that duplicate NoteTrack's explicit control capabilities.

**Final Recommendation**: Port **MINIMAL, DRILL, and FUNK** (~40-50 hours total). These three algorithms provide unique value that NoteTrack cannot replicate, while leveraging the existing Performer infrastructure without major architectural changes.

**Next Steps**:
1. User decision on open questions (ACID, AMBIENT, POLYRHYTHM, metadata system)
2. Begin Phase 2 implementation (MINIMAL algorithm)
3. Establish testing workflow (unit + integration + hardware)
4. Update Tuesday-Track-Flow.md with new algorithms

---

**End of Revised Analysis**
