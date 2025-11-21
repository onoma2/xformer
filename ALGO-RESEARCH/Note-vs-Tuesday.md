# NoteTrack vs TuesdayTrack UI Feature Comparison

## Overview

This document compares the UI features available for NoteTrack vs TuesdayTrack in the PEW|FORMER sequencer, identifying what could be ported to enhance the Tuesday track type.

---

## Feature Comparison Summary

| Feature | NoteTrack | TuesdayTrack | Notes |
|---------|-----------|--------------|-------|
| **Dedicated Pages** | 6 pages | 1 page | Tuesday uses TrackPage only |
| **Layer Cycling (F1-F5)** | 20+ layers | Flat list | Could be added |
| **Visual Step Grid** | 16-step display | No visualization | Generated patterns |
| **Mute in Performer** | Yes | Yes | Works for all track types |
| **Quick Edit Keys** | Page+S9-S16 | Not implemented | Could be added |
| **Routing Targets** | 12 CV-routable params | 0 routable | Could be added |
| **Context Menu** | INIT/COPY/PASTE/DUPL | Track level only | Could add sequence-level |
| **Reseed Function** | N/A | Shift+F5 | Unique to Tuesday |

---

## 1. Mute on PerformancePage

**Status: ALREADY WORKS**

Mute is handled at the `PlayState` level and works for ALL track types including Tuesday.

### Implementation Details

**File:** `src/apps/sequencer/ui/pages/PerformerPage.cpp`

```cpp
// Lines 217-224: Toggle mute via T1-T8 keys
if (key.isTrackSelect()) {
    if (key.shiftModifier()) {
        playState.soloTrack(key.track(), executeType);     // Solo mode
    } else {
        playState.toggleMuteTrack(key.track(), executeType); // Toggle mute
    }
    event.consume();
}
```

**Visual Feedback (lines 71-78):**
- Muted: Inner rectangle filled
- Mute Requested: Wider border (pending state)
- Unmuted: No fill

**Execution Modes:**
- Immediate: Takes effect right away
- Latched: Commits when LATCH released
- Synced: Executes at next sync point

---

## 2. Page Structure Comparison

### NoteTrack Pages (6 total)

| Page | File | Purpose |
|------|------|---------|
| **NoteSequencePage** | NoteSequencePage.cpp | Sequence properties (7 params) |
| **NoteSequenceEditPage** | NoteSequenceEditPage.cpp | Step editor with 20+ layers |
| **AccumulatorPage** | AccumulatorPage.cpp | Real-time modulation config |
| **AccumulatorStepsPage** | AccumulatorStepsPage.cpp | Per-step trigger toggles |
| **HarmonyPage** | HarmonyPage.cpp | Master/follower harmony |
| **TrackPage** | TrackPage.cpp | Track-level parameters |

### TuesdayTrack Pages (1 total)

| Page | File | Purpose |
|------|------|---------|
| **TrackPage** | TrackPage.cpp | All 9 parameters in flat list |

---

## 3. Parameter Comparison

### NoteTrack Parameters (NoteTrackListModel.h)

12 parameters, most with routing targets:

1. PlayMode
2. FillMode
3. FillMuted
4. CvUpdateMode (NEW)
5. SlideTime (routable)
6. Octave (routable)
7. Transpose (routable)
8. Rotate (routable)
9. GateProbabilityBias (routable)
10. RetriggerProbabilityBias (routable)
11. LengthBias (routable)
12. NoteProbabilityBias (routable)

### TuesdayTrack Parameters (TuesdayTrackListModel.h)

9 parameters, none with routing targets:

1. Algorithm
2. Flow
3. Ornament
4. Power
5. LoopLength
6. Glide
7. Scale
8. Skew
9. CvUpdateMode (NEW)

---

## 4. NoteSequencePage Features

**File:** `src/apps/sequencer/ui/pages/NoteSequencePage.cpp`

### Sequence-Level Parameters (7 items)

1. First Step (0-15)
2. Last Step (0-15)
3. Run Mode (Forward, Backward, Pendulum, etc.)
4. Divisor (clock division 1-16)
5. Reset Measure
6. Scale (quantization)
7. Root Note (0-11 + Off)

### Context Menu Actions

- INIT - Initialize sequence
- COPY - Copy to clipboard
- PASTE - Paste from clipboard
- DUPL - Duplicate sequence
- ROUTE - Configure CV routing

---

## 5. NoteSequenceEditPage Features

**File:** `src/apps/sequencer/ui/pages/NoteSequenceEditPage.cpp` (1048 lines)

### Layer Cycling via Function Keys

| Key | Layer Cycle |
|-----|-------------|
| **F1 (Gate)** | Gate → GateProbability → GateOffset → Slide → Gate |
| **F2 (Retrigger)** | Retrigger → RetriggerProbability → PulseCount → GateMode → Retrigger |
| **F3 (Length)** | Length → LengthVariationRange → LengthVariationProbability → Length |
| **F4 (Note)** | Note → NoteVariationRange → NoteVariationProbability → AccumulatorTrigger → [Harmony overrides] → Note |
| **F5 (Condition)** | Condition (direct) |

### All Available Layers (20+)

1. Gate (toggle on/off)
2. GateProbability (0-100%)
3. GateOffset (timing)
4. Slide (portamento)
5. Retrigger (subdivisions 1-8)
6. RetriggerProbability
7. PulseCount (1-8 pulses)
8. GateMode (A/1/H/1L)
9. Length (duration)
10. LengthVariationRange
11. LengthVariationProbability
12. Note (pitch)
13. NoteVariationRange
14. NoteVariationProbability
15. AccumulatorTrigger
16. HarmonyRoleOverride (master only)
17. InversionOverride (master only)
18. VoicingOverride (master only)
19. Condition

### Quick Edit Keys

Press Page+S9-S16 for quick parameter access:
- S9: FirstStep
- S10: LastStep
- S11: RunMode
- S12: Divisor
- S13: ResetMeasure
- S14: Scale
- S15: RootNote

---

## 6. Page Navigation

**File:** `src/apps/sequencer/ui/pages/TopPage.cpp`

### Sequence Key (S2) Navigation

**NoteTrack (lines 278-286):**
- First press → NoteSequencePage
- Second press → AccumulatorPage
- Third press → Cycles back

**TuesdayTrack (lines 295-298):**
```cpp
case Track::TrackMode::Tuesday:
    // Tuesday tracks use TrackPage for parameter editing (no step sequences)
    setMainPage(pages.track);
    break;
```

### Track Key (T1-T8) Navigation

**NoteTrack (lines 332-340):**
- First press → TrackPage
- Second press → HarmonyPage
- Cycles between them

**TuesdayTrack (lines 342-346):**
- Always → TrackPage (no additional pages)

---

## 7. Potential Enhancements for TuesdayTrack

### High Priority

#### 1. Routing Targets for CV Modulation

Add routing targets to TuesdayTrackListModel:

```cpp
virtual Routing::Target routingTarget(int row) const override {
    switch (Item(row)) {
    case Flow:      return Routing::Target::TuesdayFlow;
    case Power:     return Routing::Target::TuesdayPower;
    case Skew:      return Routing::Target::TuesdaySkew;
    default:        return Routing::Target::None;
    }
}
```

#### 2. Layer Grouping via F1-F5

Group parameters for faster navigation:
- F1: Algorithm selection
- F2: Density (Power, Skew)
- F3: Timing (LoopLength, Glide)
- F4: Generation (Flow, Ornament)
- F5: Output (CV Mode, Scale)

### Medium Priority

#### 3. TuesdaySequencePage

Add sequence-level parameters:
- Divisor (clock division)
- Run Mode (forward/backward/pendulum)
- Reset Measure
- First/Last step (for partial loop)

#### 4. Quick Edit Keys

Map Page+S9-S16:
- S9: Algorithm
- S10: Power
- S11: LoopLength
- S12: Flow
- S13: Ornament
- S14: Scale
- S15: Glide
- S16: Skew

#### 5. Visual Pattern Display

Show generated pattern as read-only grid:
- Display current step during playback
- Show gate/pitch patterns
- Would require real-time pattern generation

### Lower Priority

#### 6. Context Menu Expansion

Add sequence-level operations:
- INIT - Reset algorithm state
- COPY/PASTE - Copy algorithm settings
- RAND - Randomize Flow/Ornament

#### 7. Accumulator Integration

Allow accumulator to modulate Tuesday parameters:
- Flow modulation for evolving patterns
- Power modulation for dynamic density
- Skew modulation for shifting density curves

---

## 8. Key File References

| Feature | File | Lines |
|---------|------|-------|
| Mute Toggle | PerformerPage.cpp | 217-224 |
| Mute Visual | PerformerPage.cpp | 71-78 |
| Mute State | PlayState.h | 52-54 |
| Tuesday Navigation | TopPage.cpp | 295-298, 366-369 |
| Layer Cycling | NoteSequenceEditPage.cpp | 569-697 |
| Note Track Params | NoteTrackListModel.h | 61-94 |
| Tuesday Params | TuesdayTrackListModel.h | 44-109 |
| Sequence Params | NoteSequenceListModel.h | 12-20 |
| Reseed Function | TrackPage.cpp | 64-76, 190-199 |

---

## 9. Implementation Notes

### Why Some Features Don't Apply

1. **Per-Step Editing** - Tuesday generates patterns algorithmically, no manual step data
2. **Accumulator Pages** - Tuesday doesn't use accumulator (could add parameter modulation)
3. **Harmony Pages** - Tuesday generates melodies, not harmonies (could add harmonic quantization)

### What Makes Sense to Port

1. **Routing Targets** - Essential for live CV modulation
2. **Divisor/Run Mode** - Useful sequence-level control
3. **Quick Edit Keys** - Faster parameter access
4. **Visual Feedback** - Pattern visualization during playback

### Design Philosophy

NoteTrack: Manual programming with extensive per-step control
TuesdayTrack: Generative/algorithmic with high-level parameter control

The goal is to enhance TuesdayTrack's UI without forcing it into a step-sequencer paradigm.
