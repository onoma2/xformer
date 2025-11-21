# Harmony Feature - Hardware Capabilities Analysis

**Date**: 2025-11-19
**Purpose**: Evaluate STM32F4 hardware capabilities for implementing remaining harmony features
**Hardware**: STM32F407VG (ARM Cortex-M4, 168MHz, 192KB RAM, 1MB Flash)

---

## Current Implementation Status

### ✅ Implemented and Working
- HarmonyEngine with 7 modal scales
- Master/follower track relationships
- 4-voice chord generation (root, 3rd, 5th, 7th)
- Per-track harmony configuration via UI
- Synchronized step playback
- **Performance**: <1µs per harmonization
- **Memory**: ~1KB per HarmonyEngine instance (stack-allocated)

### ❌ Not Implemented (Optional Features)
1. Inversion parameter (0-3) - UI exposure
2. Voicing parameter (Close/Drop2/Drop3/Spread) - UI exposure
3. Per-step inversion/voicing override
4. Manual chord quality selection
5. CV input for harmony parameters
6. Performance mode
7. Slew/portamento for harmony transitions
8. Additional scales (Harmonic Minor, Melodic Minor)
9. Extended voicings (Drop3, Spread)

---

## Hardware Capability Analysis

### STM32F407VG Specifications

**CPU:**
- ARM Cortex-M4 @ 168MHz
- DSP instructions
- Single-cycle multiply
- Hardware FPU (32-bit)

**Memory:**
- 192KB SRAM
- 1MB Flash
- 64KB CCM (Core Coupled Memory) - fast, no DMA

**Peripherals:**
- 8 channels CV/Gate output (DAC + GPIO)
- 4 channels CV input (ADC)
- OLED display (SPI)
- Encoder + button matrix
- SD card (SDIO)
- MIDI (UART)
- USB (Host/Device)

**Current Utilization** (estimated):
- Flash: ~600KB used (~60% of 1MB)
- RAM: ~120KB used (~62% of 192KB)
- CPU: ~40% average load (engine task)

---

## Feature-by-Feature Analysis

### 1. Inversion Parameter (0-3) - UI Exposure Only

**Hardware Requirements:** ✅ NONE (already supported in HarmonyEngine)

**Implementation:**
- Add 2 bytes to NoteSequence (harmonyInversion uint8_t)
- Pass to HarmonyEngine.harmonize() call
- Add 1 row to HarmonyListModel UI
- **Effort**: ~1.5 hours
- **Flash**: +~200 bytes
- **RAM**: +8 bytes (8 tracks × 1 byte)
- **CPU**: 0% increase (already computed)

**Verdict:** ✅ **TRIVIAL** - No hardware constraints whatsoever

---

### 2. Voicing Parameter (Close/Drop2/Drop3/Spread) - UI Exposure Only

**Hardware Requirements:** ✅ NONE (already supported in HarmonyEngine)

**Implementation:**
- Add 2 bytes to NoteSequence (harmonyVoicing uint8_t)
- Pass to HarmonyEngine.harmonize() call
- Add 1 row to HarmonyListModel UI
- **Effort**: ~1.5 hours
- **Flash**: +~200 bytes
- **RAM**: +8 bytes (8 tracks × 1 byte)
- **CPU**: 0% increase (already computed)

**Verdict:** ✅ **TRIVIAL** - No hardware constraints whatsoever

---

### 3. Per-Step Inversion/Voicing Override

**Hardware Requirements:**
- **RAM**: 16 steps × 2 bytes × 8 tracks = 256 bytes (per-step storage)
- **Flash**: ~1KB (UI layer, step editing)
- **CPU**: Negligible (read from step, pass to harmonize)

**Implementation:**
- Add bitfields to NoteSequence::Step (3 bits inversion + 2 bits voicing = 5 bits)
- Check if NoteSequence::Step._data1 has space
  - Currently: 22 bits used, **10 bits free** ✅
  - Need: 5 bits
  - **Result**: FITS perfectly!
- UI: Add to NoteSequenceEditPage layer cycling
- Similar to Gate/GateProb/GateOffset layers

**Analysis:**
- **Flash**: Current 600KB → ~601KB (+0.16% increase)
- **RAM**: Current 120KB → 120.25KB (+0.2% increase)
- **CPU**: Negligible (0.01% increase per step evaluation)

**Verdict:** ✅ **EASY** - Plenty of bitfield space, minimal overhead

---

### 4. Manual Chord Quality Selection

**Hardware Requirements:**
- **RAM**: 1 byte per track = 8 bytes (chordQuality parameter)
- **Flash**: ~500 bytes (UI + logic to override diatonic detection)
- **CPU**: Negligible (skip diatonic detection, use manual setting)

**Implementation:**
```cpp
enum ChordQuality {
    Auto = 0,      // Use diatonic (current behavior)
    Minor7 = 1,    // Force -7
    Dominant7 = 2, // Force 7
    Major7 = 3,    // Force ∆7
    HalfDim7 = 4   // Force ø
};
```

**Changes:**
- Add `chordQuality` to NoteSequence (1 byte)
- Modify HarmonyEngine.harmonize() to accept optional quality override
- Add parameter to HarmonyListModel

**Analysis:**
- **Flash**: +500 bytes (+0.08% increase)
- **RAM**: +8 bytes (+0.007% increase)
- **CPU**: -0.01% (skip diatonic detection when manual)

**Verdict:** ✅ **TRIVIAL** - No constraints, actually slightly faster

---

### 5. CV Input for Harmony Parameters

**Hardware Requirements:**
- **ADC Channels**: 4 available (CV1-CV4 IN)
- **Sampling Rate**: Current ADC runs ~1kHz per channel
- **CPU**: ADC is DMA-driven (minimal CPU impact)
- **Flash**: ~2KB (routing matrix integration)
- **RAM**: ~100 bytes (routing configuration)

**Implementation:**
- Integrate with existing Routing system
- Map CV inputs to harmony parameters:
  - CV → harmonyScale (quantize 0-6)
  - CV → masterTrackIndex (quantize 0-7)
  - CV → inversion (quantize 0-3)
  - CV → voicing (quantize 0-3)
- Same architecture as existing CV routing

**Analysis:**
- Uses **existing ADC infrastructure** (already running)
- Minimal CPU overhead (routing already exists)
- **Flash**: +2KB (+0.3% increase)
- **RAM**: +100 bytes (+0.08% increase)
- **CPU**: +0.1% (routing evaluation per tick)

**Concerns:**
- **Noise**: CV inputs might be noisy → use hysteresis/schmitt trigger
- **Latency**: 1ms ADC sampling → acceptable for modulation
- **Quantization**: Need robust quantize-to-discrete-value logic

**Verdict:** ✅ **FEASIBLE** - Existing infrastructure supports it

---

### 6. Performance Mode

**Description**: Real-time harmony control via front panel (encoder/buttons)

**Hardware Requirements:**
- **Flash**: ~1-2KB (new performance page, button handlers)
- **RAM**: ~50 bytes (performance state)
- **CPU**: Negligible (button press handlers)
- **OLED**: Refresh rate impact (more pixels → more noise)

**Implementation:**
- New PerformanceHarmonyPage showing all 8 tracks
- Buttons to quickly set harmony roles
- Encoder to adjust scales/master assignments
- Visual feedback on OLED

**Analysis:**
- **Flash**: +1-2KB (+0.2% increase)
- **RAM**: +50 bytes (+0.04% increase)
- **CPU**: +0.05% (UI refresh)
- **Display Noise**: May increase (more pixels active)

**Concerns:**
- **Noise Reduction**: More OLED activity = more audio noise
  - Mitigation: Use bold text instead of highlight (existing pattern)
  - Respect brightness/screensaver settings
- **Ergonomics**: 8 tracks on small OLED = crowded
  - Could use paging or focus on selected track group

**Verdict:** ✅ **FEASIBLE** - But consider noise impact

---

### 7. Slew/Portamento for Harmony Transitions

**Hardware Requirements:**
- **RAM**: ~64 bytes (8 tracks × 8 bytes for slew state)
- **Flash**: ~1KB (slew calculation logic)
- **CPU**: +2-5% (per-sample slew interpolation in update loop)

**Implementation:**
- Track previous harmonized note per track
- Interpolate from previous → current over time
- Similar to existing Slide feature but for harmony
- Runs in NoteTrackEngine::update() (50Hz)

**Current Slide Implementation:**
```cpp
void NoteTrackEngine::update(float dt) {
    if (_slideActive) {
        _cvOutput += (_cvOutputTarget - _cvOutput) * slide_coefficient;
    }
}
```

**Analysis:**
- **Flash**: +1KB (+0.16% increase)
- **RAM**: +64 bytes (+0.05% increase)
- **CPU**: +2-5% per active track
  - 8 tracks × 50Hz × slew calc = 400 calcs/sec
  - Each calc: ~100 cycles → 40K cycles/sec
  - 168MHz CPU → 0.02% increase (negligible!)

**Concerns:**
- **Interaction with existing Slide**: Need clear behavior definition
  - Option A: Harmony slew separate from step slide
  - Option B: Use existing slide for harmony transitions
  - **Recommendation**: Option B (reuse existing slide)

**Verdict:** ✅ **TRIVIAL** - Reuse existing slide infrastructure

---

### 8. Additional Scales (Harmonic Minor, Melodic Minor, etc.)

**Hardware Requirements:**
- **Flash**: ~500 bytes per scale (lookup tables)
  - Harmonic Minor: +500 bytes
  - Melodic Minor: +500 bytes
  - Harmonic Major: +500 bytes
  - Diminished: +500 bytes
  - Whole Tone: +500 bytes
  - **Total**: +2.5KB for 5 new scales
- **RAM**: 0 bytes (lookup tables in Flash/ROM)
- **CPU**: 0% increase (same lookup logic)

**Implementation:**
```cpp
// Add to HarmonyEngine.h
enum Mode {
    // Existing (Ionian modes)
    Ionian = 0, Dorian, Phrygian, Lydian, Mixolydian, Aeolian, Locrian,

    // New (Harmonic/Melodic Minor modes)
    HarmonicMinor = 7,
    HarmonicMinor2 = 8,   // 2nd mode of Harmonic Minor
    // ... 7 modes of Harmonic Minor

    MelodicMinor = 14,
    MelodicMinor2 = 15,
    // ... 7 modes of Melodic Minor

    // Symmetric scales
    Diminished = 21,      // Whole-Half Diminished
    DiminishedHW = 22,    // Half-Whole Diminished
    WholeTone = 23,

    Last
};

// Expand lookup tables
static const uint8_t ScaleIntervals[24][7] = {
    // ... existing Ionian modes ...
    { 0, 2, 3, 5, 7, 8, 11 },  // Harmonic Minor
    { 0, 1, 3, 5, 6, 9, 10 },  // Locrian #6 (2nd mode)
    // ...
};
```

**Analysis:**
- **Flash**: +2.5KB (+0.4% increase for 5 new scale families)
- **RAM**: 0 bytes (const lookup tables)
- **CPU**: 0% increase
- **UI**: Need scrolling or paging for 24+ modes

**Verdict:** ✅ **EASY** - Flash has plenty of space

---

### 9. Extended Voicings (Drop3, Spread)

**Hardware Requirements:** ✅ ALREADY IMPLEMENTED in HarmonyEngine!

**Current HarmonyEngine.cpp:**
```cpp
enum Voicing {
    Close = 0,   // ✅ Implemented
    Drop2 = 1,   // ✅ Implemented
    Drop3 = 2,   // ✅ Implemented
    Spread = 3,  // ✅ Implemented
    Last
};
```

**Implementation:**
- Drop3 and Spread logic already in `applyVoicing()` method
- Just needs UI exposure (see Feature #2 above)
- **Effort**: 0 hours (already done)
- **Flash**: 0 bytes (already compiled in)
- **RAM**: 0 bytes (already allocated)
- **CPU**: 0% increase (already computed)

**Verdict:** ✅ **ALREADY DONE** - Just expose in UI!

---

## Summary: Hardware Capability Matrix

| Feature | Flash | RAM | CPU | Status | Difficulty | Est. Time |
|---------|-------|-----|-----|--------|------------|-----------|
| **1. Inversion UI** | +200B | +8B | 0% | ✅ Ready | Trivial | 1.5 hrs |
| **2. Voicing UI** | +200B | +8B | 0% | ✅ Ready | Trivial | 1.5 hrs |
| **3. Per-step Inv/Voice** | +1KB | +256B | <0.01% | ✅ Ready | Easy | 3-4 hrs |
| **4. Manual Quality** | +500B | +8B | -0.01% | ✅ Ready | Trivial | 2 hrs |
| **5. CV Input** | +2KB | +100B | +0.1% | ✅ Feasible | Medium | 4-6 hrs |
| **6. Performance Mode** | +2KB | +50B | +0.05% | ⚠️ Noise | Medium | 6-8 hrs |
| **7. Harmony Slew** | +1KB | +64B | +0.02% | ✅ Reuse | Easy | 2-3 hrs |
| **8. New Scales** | +2.5KB | 0B | 0% | ✅ Ready | Easy | 4-5 hrs |
| **9. Drop3/Spread** | 0B | 0B | 0% | ✅ Done! | None | 0 hrs |
| **TOTAL (all features)** | **+9.4KB** | **+494B** | **+0.2%** | ✅ **ALL FEASIBLE** | Mixed | **24-32 hrs** |

---

## Resource Availability

### Flash Memory
- **Current**: ~600KB / 1024KB (58.6% used)
- **After all features**: ~610KB / 1024KB (59.5% used)
- **Remaining**: ~414KB (40.5% free)
- **Verdict**: ✅ **PLENTY OF SPACE**

### RAM
- **Current**: ~120KB / 192KB (62.5% used)
- **After all features**: ~120.5KB / 192KB (62.8% used)
- **Remaining**: ~71.5KB (37.2% free)
- **Verdict**: ✅ **COMFORTABLE MARGIN**

### CPU Load
- **Current**: ~40% average (engine task @ 50Hz)
- **After all features**: ~40.2% average
- **Remaining**: ~59.8% headroom
- **Verdict**: ✅ **NO PERFORMANCE ISSUES**

---

## Recommendations

### Immediate (No Hardware Concerns)
These features have **zero hardware constraints** and should be implemented immediately if desired:

1. **Inversion UI** (1.5 hrs)
2. **Voicing UI** (1.5 hrs)
3. **Manual Quality** (2 hrs)
4. **Harmony Slew** (2-3 hrs) - Reuse existing slide

**Total**: 7-9 hours for major functionality boost

### Easy (Minimal Hardware Impact)
These features have **minimal impact** and are straightforward:

5. **Per-step Inversion/Voicing** (3-4 hrs)
6. **New Scales** (4-5 hrs) - Harmonic/Melodic Minor families

**Total**: 7-9 hours for advanced harmonic control

### Medium (Requires Testing)
These features need **careful testing** for noise/ergonomics:

7. **Performance Mode** (6-8 hrs) - Test OLED noise impact
8. **CV Input** (4-6 hrs) - Test noise on CV inputs, implement hysteresis

**Total**: 10-14 hours with thorough testing

### Prioritized Roadmap

**Phase A: Quick Wins** (7-9 hours total)
- Inversion UI
- Voicing UI
- Manual Quality
- Harmony Slew

**Phase B: Advanced Harmony** (7-9 hours total)
- Per-step Inversion/Voicing
- New Scales (Harmonic/Melodic Minor)

**Phase C: Performance Features** (10-14 hours total)
- CV Input for harmony parameters
- Performance Mode

**Grand Total**: 24-32 hours for COMPLETE feature set

---

## Hardware Limitations: NONE FOUND

### Analysis Conclusion

The STM32F407VG hardware has **MORE THAN SUFFICIENT** capabilities for implementing ALL remaining harmony features from the original plan:

✅ **Flash**: 40% free (414KB available, need 9.4KB)
✅ **RAM**: 37% free (71.5KB available, need 494B)
✅ **CPU**: 60% free (plenty of headroom for 0.2% increase)
✅ **Peripherals**: ADC/DAC/Display all have capacity
✅ **Real-time Performance**: <1µs harmonization, plenty fast

### No Architectural Changes Needed

All features can be implemented using:
- ✅ Existing memory layout (bitfields, lookup tables)
- ✅ Existing ADC/routing infrastructure
- ✅ Existing UI framework (ListPage, indexed values)
- ✅ Existing slide/portamento logic
- ✅ Existing HarmonyEngine implementation

### Risk Assessment: LOW

**Technical Risks**: MINIMAL
- All features are incremental additions
- No refactoring required
- Plenty of headroom in all resources
- Well-understood implementation patterns

**Performance Risks**: NEGLIGIBLE
- CPU increase: 0.2% (from 40% → 40.2%)
- RAM increase: 0.3% (from 62.5% → 62.8%)
- No timing-critical changes

**User Experience Risks**: LOW
- Performance mode may increase OLED noise (testable)
- CV input may be noisy (use hysteresis)
- UI pagination may be needed for 24+ scales (manageable)

---

## Conclusion

**The STM32F4 hardware can EASILY support ALL remaining harmony features.**

There are **NO hardware limitations** blocking any feature from the original WORKING-TDD plan. Implementation is purely a matter of development time (~24-32 hours total for everything).

**Recommendation**: Prioritize Phase A (Quick Wins) for immediate user value with minimal effort. Phase B and C can follow based on user demand.
