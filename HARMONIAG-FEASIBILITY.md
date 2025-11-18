# Harmonàig Integration - Feasibility Assessment & Options

**Date**: 2025-11-18
**Assessment**: Technical Feasibility Analysis
**Status**: Planning Phase

---

## Executive Summary

**Overall Feasibility**: ✅ **TECHNICALLY FEASIBLE** but **HIGH COMPLEXITY**

**Recommendation**: Start with **Option 2 (Lite Mode)** or **Option 3 (Minimal MVP)** to validate concept before committing to full implementation.

---

## Complexity Analysis

### Core Challenges

| Challenge | Severity | Mitigation |
|-----------|----------|------------|
| **Music Theory Complexity** | 🟡 MEDIUM | Well-documented lookup tables available |
| **Real-time Performance** | 🟡 MEDIUM | Calculations are deterministic and fast |
| **Master/Follower Coordination** | 🔴 HIGH | Complex state management across tracks |
| **UI Design Constraints** | 🟡 MEDIUM | OLED display limits, many parameters |
| **Memory Footprint** | 🟢 LOW | Lookup tables are small (~250 bytes) |
| **Testing Scope** | 🔴 HIGH | 14 modes × 8 qualities × 4 inversions × 4 voicings = 1,792 combinations |
| **Development Time** | 🔴 HIGH | 10-12 weeks estimated |
| **Integration Complexity** | 🔴 HIGH | Touches many existing systems |

---

## Technical Feasibility Score

### Music Theory Logic: ✅ **HIGHLY FEASIBLE**

**Evidence**:
- All scale intervals are well-documented
- Chord formulas are standardized
- Inversion logic is straightforward
- Voicing algorithms are deterministic

**Complexity**: 🟢 **LOW**
- Lookup tables: ~300 lines of code
- Calculation logic: ~500 lines of code
- No complex algorithms required

**Confidence**: **95%** - Music theory is deterministic and well-understood

---

### Data Model Extension: ✅ **FEASIBLE**

**Evidence**:
- Performer already has NoteSequence class to extend
- Serialization framework exists
- Project model is extensible

**Complexity**: 🟢 **LOW-MEDIUM**
- HarmonySequence extends NoteSequence
- HarmonySettings added to Project
- Serialization follows existing patterns

**Challenges**:
- Maintaining backward compatibility
- Version migration for old projects

**Confidence**: **85%** - Follows existing patterns

---

### Engine Integration: ⚠️ **COMPLEX BUT FEASIBLE**

**Evidence**:
- TrackEngine abstraction exists
- Engine tick loop is well-structured
- CV output routing available

**Complexity**: 🟡 **MEDIUM-HIGH**
- New HarmonyTrackEngine class: ~1000 lines
- Master/follower relationship management
- Real-time coordination between tracks
- Slew processing per follower

**Challenges**:
1. **Timing**: Master must update before followers
2. **State Management**: Followers need master reference
3. **Pattern Changes**: Must handle mid-playback changes
4. **Performance**: 8 tracks × harmony calculations

**Risks**:
- Race conditions between master and followers
- State corruption on pattern changes
- CPU overhead with many active tracks

**Confidence**: **70%** - Complex but manageable with careful design

---

### UI Implementation: ⚠️ **FEASIBLE WITH COMPROMISES**

**Evidence**:
- ListPage framework exists
- TopPage navigation extensible
- Similar pages already implemented (AccumulatorPage)

**Complexity**: 🟡 **MEDIUM**
- HarmonyGlobalPage: ~300 lines
- HarmonyTrackConfigPage: ~200 lines
- TopPage modifications: ~50 lines

**Challenges**:
1. **Parameter Count**: 8 parameters on HarmonyGlobalPage
2. **Visual Feedback**: Limited OLED space for chord display
3. **Navigation Depth**: Already 3-4 levels deep
4. **Performance Mode**: Button keyboard in limited space

**Compromises**:
- Simplified chord display ("C∆7" vs "C-E-G-B")
- Paginated parameters (scroll to see all)
- Performance mode on separate screen

**Confidence**: **75%** - UI will work but may feel cramped

---

### Performance Impact: ✅ **ACCEPTABLE**

**Evidence**:
- Simple arithmetic operations
- Small lookup tables
- No complex DSP required

**CPU Load Estimate**:
```
Per track per tick:
- Scale degree lookup: ~10 instructions
- Chord interval lookup: ~10 instructions
- Inversion calculation: ~20 instructions
- Voicing calculation: ~30 instructions
- Note-to-CV conversion: ~10 instructions
Total: ~80 instructions (~0.1μs @ 1GHz ARM)

With 4 follower tracks @ 192 PPQN:
- 4 tracks × 0.1μs = 0.4μs per tick
- At 120 BPM: 192 ticks/beat × 2 beats/sec = 384 ticks/sec
- Total: 0.4μs × 384 = 0.15ms per second
- CPU load: 0.015% (negligible)
```

**Memory Footprint**:
- Static lookup tables: ~250 bytes
- Runtime state per track: ~50 bytes
- Total with 8 tracks: ~650 bytes (0.06% of 1MB RAM)

**Confidence**: **95%** - Performance impact minimal

---

### Testing Complexity: 🔴 **VERY HIGH**

**Test Matrix**:
- 14 modes
- × 8 chord qualities
- × 4 inversions
- × 4 voicings
- = **1,792 combinations**

**Manual Testing**: Impractical to test all combinations

**Strategy**:
- Automated unit tests for core logic
- Sample-based integration testing
- Focus on common use cases

**Confidence**: **60%** - Cannot test exhaustively

---

## Implementation Options

### Option 1: Full Harmonàig Feature (**NOT RECOMMENDED**)

**Scope**: Complete implementation as documented in HARMONIAG.md

**Features**:
- ✅ 14 modes (7 Ionian + 7 Harmonic Minor)
- ✅ 8 chord qualities
- ✅ 4 inversions
- ✅ 4 voicings
- ✅ Diatonic vs Manual mode
- ✅ Performance mode with button keyboard
- ✅ Unison mode
- ✅ Individual slew per voice
- ✅ CV control over all parameters
- ✅ Master/follower track architecture

**Effort**: **10-12 weeks** (full-time)

**Pros**:
- Complete feature parity with Harmonàig
- Maximum musical flexibility
- Future-proof architecture

**Cons**:
- 🔴 Very high complexity
- 🔴 Long development time
- 🔴 Extensive testing required
- 🔴 High risk of bugs
- 🔴 UI overcrowded

**Risk**: **HIGH** - Too much scope for first iteration

**Verdict**: ❌ **Not recommended** - Too ambitious

---

### Option 2: Harmonàig Lite (**RECOMMENDED**)

**Scope**: Core harmony features with simplified options

**Features**:
- ✅ 7 Ionian modes only (no Harmonic Minor)
- ✅ 4 most common chord qualities (∆7, -7, 7, ø)
- ✅ 2 inversions (Root, 1st)
- ✅ 2 voicings (Close, Drop2)
- ✅ Diatonic mode only (no manual quality selection)
- ⚠️ Simplified Performance mode (no button keyboard)
- ⚠️ No unison mode
- ✅ Individual slew per voice
- ✅ CV control (inversion, voicing, transpose only)
- ✅ Master/follower architecture (simplified)

**Effort**: **5-6 weeks**

**Pros**:
- ✅ Manageable scope
- ✅ Covers 80% of use cases
- ✅ Simpler UI (fewer parameters)
- ✅ Easier to test
- ✅ Lower risk
- ✅ Foundation for future expansion

**Cons**:
- ⚠️ Limited compared to full Harmonàig
- ⚠️ No Harmonic Minor modes
- ⚠️ Fewer chord qualities

**Risk**: **MEDIUM** - Still complex but achievable

**Verdict**: ✅ **RECOMMENDED** - Best balance of features vs effort

**Parameter Count**: 6 (vs 8 in full version)
1. MODE (7 options)
2. INVERSION (2 options)
3. VOICING (2 options)
4. TRANSPOSE (-24 to +24)
5. TRACK ROLE (6 options)
6. SLEW (0-127)

---

### Option 3: Minimal Harmony MVP (**FASTEST PATH**)

**Scope**: Proof-of-concept with absolute minimum features

**Features**:
- ✅ 1 mode only (Ionian/Major)
- ✅ 1 chord quality (automatic diatonic)
- ✅ Root position only (no inversions)
- ✅ Close voicing only
- ⚠️ No transpose
- ⚠️ No CV control
- ⚠️ No performance mode
- ✅ Master/follower architecture (basic)
- ⚠️ Fixed slew (not configurable)

**Effort**: **2-3 weeks**

**Pros**:
- ✅ Extremely fast to implement
- ✅ Validates core concept
- ✅ Minimal risk
- ✅ Can iterate based on feedback
- ✅ Tests architecture feasibility

**Cons**:
- 🔴 Very limited musically
- 🔴 May not demonstrate full potential
- 🔴 Might need significant rework for expansion

**Risk**: **LOW** - Quick validation

**Verdict**: ✅ **Good starting point** - Validate before expanding

**Parameter Count**: 3
1. TRACK ROLE (6 options: Independent, Master, Root, 3rd, 5th, 7th)
2. MASTER TRACK (T1-T8)
3. (Mode and quality fixed, no user controls)

---

### Option 4: Hybrid Approach (**PHASED IMPLEMENTATION**)

**Scope**: Start with MVP, expand incrementally

**Phase 1** (2-3 weeks): MVP (Option 3)
- Ionian mode only
- Diatonic chords
- Root position, Close voicing
- Master/follower architecture

**Phase 2** (2-3 weeks): Add Modes & Inversions
- All 7 Ionian modes
- 2 inversions (Root, 1st)
- Transpose control

**Phase 3** (1-2 weeks): Add Voicings & Slew
- 2 voicings (Close, Drop2)
- Configurable slew per voice

**Phase 4** (2-3 weeks): Add Advanced Features
- More chord qualities
- CV control
- Performance mode

**Phase 5+** (Optional): Full Feature Set
- Harmonic Minor modes
- All 8 chord qualities
- All 4 inversions/voicings
- Unison mode

**Total**: **7-11 weeks** (to Phase 3 completion)

**Pros**:
- ✅ Incremental delivery
- ✅ Early feedback
- ✅ Can stop at any phase
- ✅ Lower risk per phase
- ✅ Validates each feature independently

**Cons**:
- ⚠️ Potential rework between phases
- ⚠️ UI changes may confuse users

**Risk**: **MEDIUM-LOW** - Controlled risk per phase

**Verdict**: ✅ **EXCELLENT APPROACH** - Recommended for production

---

## Comparison Matrix

| Feature | Option 1 (Full) | Option 2 (Lite) | Option 3 (MVP) | Option 4 (Phased) |
|---------|----------------|----------------|----------------|------------------|
| **Modes** | 14 | 7 | 1 | 1→7→14 |
| **Chord Qualities** | 8 | 4 | 1 (auto) | 1→4→8 |
| **Inversions** | 4 | 2 | 0 | 0→2→4 |
| **Voicings** | 4 | 2 | 1 | 1→2→4 |
| **Performance Mode** | ✅ | ⚠️ Simplified | ❌ | Phase 4 |
| **CV Control** | Full | Partial | ❌ | Phase 4 |
| **Effort (weeks)** | 10-12 | 5-6 | 2-3 | 7-11 (phased) |
| **Risk** | 🔴 HIGH | 🟡 MEDIUM | 🟢 LOW | 🟢 LOW per phase |
| **Recommendation** | ❌ | ✅ | ✅ (prototype) | ✅ (production) |

---

## Recommended Path Forward

### Best Option: **Option 4 (Phased Hybrid)**

**Rationale**:
1. **Low risk start**: MVP validates core concept in 2-3 weeks
2. **Early value**: Users get functional harmony immediately
3. **Feedback-driven**: Each phase informed by user feedback
4. **Flexible scope**: Can stop at any phase if needed
5. **Manageable chunks**: Easier to test and debug incrementally

### Implementation Roadmap

**Week 1-3: Phase 1 (MVP)**
- Basic HarmonyEngine (Ionian mode, diatonic chords)
- HarmonySequence model
- HarmonyTrackEngine (master/follower)
- Minimal UI (track role selection only)
- Unit tests
- **Deliverable**: Working harmony feature with C Major scale

**Week 4-6: Phase 2 (Modes & Inversions)**
- Add 6 additional Ionian modes
- Implement 2 inversions (Root, 1st)
- Add transpose control
- Enhance UI (mode, inversion, transpose parameters)
- **Deliverable**: All major/minor modes with basic inversions

**Week 7-8: Phase 3 (Voicings & Polish)**
- Add Drop2 voicing
- Implement configurable slew
- UI polish and visual feedback
- Integration tests
- **Deliverable**: Production-ready lite version

**Week 9-11: Phase 4 (Optional Advanced)**
- More chord qualities
- CV control integration
- Performance mode
- **Deliverable**: Full-featured version

**Week 12+: Phase 5 (Optional Expansion)**
- Harmonic Minor modes
- All inversions/voicings
- Advanced features

---

## Risk Mitigation Strategies

### Technical Risks

**Risk 1: Master/Follower Timing Issues**
- **Mitigation**: Process master track first in tick() loop
- **Validation**: Integration tests with rapid pattern changes

**Risk 2: Performance Degradation**
- **Mitigation**: Profile early and often
- **Validation**: Benchmark with 8 active tracks

**Risk 3: State Corruption**
- **Mitigation**: Immutable chord calculations, atomic updates
- **Validation**: Stress testing with rapid mode changes

### Project Risks

**Risk 1: Scope Creep**
- **Mitigation**: Strict phase boundaries, no mid-phase additions
- **Validation**: Phase completion checklists

**Risk 2: Music Theory Errors**
- **Mitigation**: Automated unit tests for all combinations
- **Validation**: Compare against known-good Harmonàig module

**Risk 3: User Confusion**
- **Mitigation**: Clear documentation, example projects
- **Validation**: User testing before each phase

---

## Success Criteria

### Phase 1 (MVP) Success:
- ✅ Master track sequence triggers follower tracks
- ✅ Followers output correct diatonic chord tones
- ✅ No audio glitches or crashes
- ✅ All unit tests pass
- ✅ User can create simple chord progressions in C Major

### Phase 2 Success:
- ✅ All 7 modes produce correct chord qualities
- ✅ Inversions work correctly
- ✅ Transpose works across full ±2 octave range
- ✅ Smooth transitions between modes
- ✅ User can create progressions in any key/mode

### Phase 3 Success:
- ✅ Voicings produce audibly different results
- ✅ Slew smooths pitch transitions
- ✅ UI is intuitive and responsive
- ✅ Performance acceptable with 8 active tracks
- ✅ Production-ready quality

---

## Effort Estimate Breakdown

### Phase 1 (MVP) - 2-3 weeks

| Task | Hours | Confidence |
|------|-------|------------|
| HarmonyEngine (basic) | 16 | High |
| HarmonySequence | 8 | High |
| HarmonyTrackEngine | 24 | Medium |
| UI (minimal) | 8 | High |
| Unit tests | 12 | High |
| Integration testing | 8 | Medium |
| **Total** | **76 hours** (~2 weeks) | **Medium-High** |

### Phase 2 - 2-3 weeks

| Task | Hours | Confidence |
|------|-------|------------|
| Add 6 modes | 8 | High |
| Implement inversions | 12 | Medium |
| Add transpose | 4 | High |
| UI enhancements | 12 | Medium |
| Additional tests | 12 | High |
| **Total** | **48 hours** (~1.5 weeks) | **High** |

### Phase 3 - 1-2 weeks

| Task | Hours | Confidence |
|------|-------|------------|
| Implement voicings | 12 | Medium |
| Add slew control | 8 | Medium |
| UI polish | 12 | High |
| Integration tests | 8 | Medium |
| **Total** | **40 hours** (~1 week) | **High** |

**Cumulative Total: ~165 hours (4-6 weeks)**

---

## Alternatives to Full Implementation

### Alternative 1: Pre-computed Chord Tables

**Concept**: Instead of real-time calculation, use pre-computed chord progressions

**Pros**:
- Much simpler implementation
- Zero CPU overhead
- No music theory complexity

**Cons**:
- Less flexible
- Cannot modulate chords dynamically
- Limited to predefined progressions

**Verdict**: ❌ Not recommended - Loses core value proposition

---

### Alternative 2: External MIDI Control

**Concept**: Use external MIDI controller to send chord notes

**Pros**:
- Zero Performer development needed
- Full flexibility via external tool
- Can use DAW/computer for harmony

**Cons**:
- Requires external hardware
- Not self-contained
- Defeats purpose of integrated sequencer

**Verdict**: ❌ Not a real solution - Defeats purpose

---

### Alternative 3: Collaborate with Instruo

**Concept**: License Harmonàig algorithm or partner on integration

**Pros**:
- Access to proven implementation
- Marketing benefit (official collaboration)
- Potentially faster development

**Cons**:
- Licensing costs
- Legal complexity
- May not be feasible

**Verdict**: ⚠️ Worth exploring but unlikely

---

## Final Recommendation

### **Recommended Approach**: Option 4 (Phased Hybrid)

**Start with**: Phase 1 MVP (2-3 weeks)
- Prove the concept
- Validate architecture
- Get early user feedback

**Then decide**:
- If successful → Proceed to Phase 2
- If issues found → Fix before expanding
- If not valuable → Stop here (minimal sunk cost)

**Rationale**:
- ✅ Low risk entry point
- ✅ Fast time to value
- ✅ Incremental investment
- ✅ Flexibility to pivot
- ✅ Foundation for future

### **Alternative Quickstart**: Option 3 (Minimal MVP)

If time is critical, start with Option 3 (2-3 weeks) to validate feasibility before committing to full architecture.

---

## Next Steps

### Immediate Actions (Before Development)

1. **[ ] Decision**: Choose implementation option (Recommended: Option 4)
2. **[ ] Approval**: Get stakeholder buy-in for phased approach
3. **[ ] Research**: Validate music theory lookup tables
4. **[ ] Prototype**: Create standalone proof-of-concept (1-2 days)
5. **[ ] Design Review**: Review architecture with team

### Phase 1 Kickoff (if approved)

1. **[ ] Create feature branch**: `feature/harmony-mvp`
2. **[ ] Write HarmonyEngine stub**: Basic interface
3. **[ ] Implement Ionian scale lookup**: Test with unit tests
4. **[ ] Implement chord calculation**: Test all 7 degrees
5. **[ ] Create HarmonyTrackEngine**: Basic master/follower
6. **[ ] Add minimal UI**: Track role selection
7. **[ ] Integration test**: Full playback test
8. **[ ] User validation**: Get feedback from early testers

---

## Conclusion

The Harmonàig integration is **technically feasible** but should be approached incrementally through a **phased implementation strategy**.

**Best Path**:
1. Start with MVP (Phase 1) - 2-3 weeks
2. Validate with users
3. Expand based on feedback (Phases 2-3) - 3-4 weeks
4. Consider advanced features (Phase 4+) - Optional

**Total Commitment**: 5-7 weeks to production-ready lite version

**Risk Level**: LOW (with phased approach)

**Expected Value**: HIGH (unlocks polyphonic sequencing and harmonic intelligence)

**Recommendation**: ✅ **Proceed with Option 4** - Start Phase 1 MVP to validate concept.

---

**END OF FEASIBILITY ASSESSMENT**
