# Tuesday Track - TDD Plan & Phase 0 Setup

## Intent & Vision

### What Tuesday Track Is

The Tuesday track represents a **paradigm shift** from Performer's existing manual-programming model to **generative/algorithmic sequencing**. Instead of programming each step by hand, users set high-level parameters and let algorithms generate musical patterns in real-time.

**Existing Track Types (Manual Programming):**
- **NoteTrack**: User programs each of 64 steps with note, gate, length, etc.
- **CurveTrack**: User draws CV curves point by point
- **MidiCvTrack**: Converts external MIDI input to CV

**Tuesday Track (Generative):**
- User selects an **algorithm** and tunes **4 musical parameters**
- Algorithm generates CV/Gate patterns based on parameters
- Output evolves based on mathematical/musical rules
- Less predictable, more "alive" - the sequencer becomes a performer

### Design Philosophy

The Tuesday track is inspired by generative Eurorack modules like:
- **Mutable Instruments Marbles** - random/structured CV generation
- **Noise Engineering Mimetic Digitalis** - algorithmic sequencing
- **Intellijel Metropolix** - probability-based step modulation

The 5 parameters are intentionally **high-level musical controls** rather than technical settings:

| Parameter | Range | Musical Meaning |
|-----------|-------|-----------------|
| **Algorithm** | 0-31 | The "personality" - MARKOV (probabilistic), STOMPER (rhythmic), SCALEWALK (melodic), etc. |
| **Flow** | 0-16 | How the sequence moves - 0=static, 16=chaotic |
| **Ornament** | 0-16 | Embellishments and fills - 0=none, 16=heavily decorated |
| **Power** | 0-16 | Activity/density - 0=silent, 16=maximum gates |
| **LoopLength** | Inf, 1-16, 19, 21, 24, 32, 35, 42, 48, 56, 64 | Pattern length (Inf = evolving/non-repeating) |
| **Glide** | 0-100% | Slide probability - 0=no slides, 100=always slide |
| **Scale** | Free/Project | Note quantization - Free=chromatic, Project=use project scale |
| **Skew** | -8 to +8 | Density curve across loop - 0=flat, positive=build-up, negative=fade-out |

**Default = 0**: All parameters default to 0, producing silence. Increase values to add activity.

### Skew Parameter Behavior

The Skew parameter creates density variations across the loop length:

**Positive Skew (build-up):**
- **Skew 8**: First 50% uses Power setting, last 50% at power 16 (maximum density)
- **Skew 4**: First 75% uses Power setting, last 25% at power 16
- **Skew 2**: First 87.5% uses Power setting, last 12.5% at power 16

**Negative Skew (fade-out):**
- **Skew -8**: First 50% at power 16, last 50% uses Power setting
- **Skew -4**: First 25% at power 16, last 75% uses Power setting

**Formula:** `|skew|/16 = fraction at maximum density`

### Reseed Functionality

**Shift+F5**: Reseeds the loop with new random values while preserving current parameters.

**Context Menu**: TRACK page context menu includes RESEED option for Tuesday tracks.

The reseed operation:
1. Resets step index to 0
2. Generates new random seeds from current RNG state
3. Reinitializes algorithm with new seeds
4. Produces completely new pattern with same Flow/Ornament values

### Why F0-F4 + Encoder Control

Unlike NoteTrack which uses step buttons (S1-S16) to select steps for editing, Tuesday track uses **F0-F4 to select parameters** because:

1. **No steps to edit** - patterns are generated, not programmed
2. **Real-time tweaking** - parameters should be adjustable during playback like synthesizer knobs
3. **5 parameters map directly** to F0-F4 function buttons
4. **Performance-oriented** - encourages live manipulation

### Use Cases

1. **Generative bass lines** - Algorithm generates evolving bass patterns
2. **Random melodies** - Constrained randomness within musical scales
3. **Rhythmic variations** - Algorithmically varied drum triggers
4. **Ambient CV** - Slowly evolving modulation sources
5. **Live performance** - Tweak parameters to "play" the algorithm

### UI Mockup - What It Looks Like

**Main TuesdayPage (OLED 256x64):**

```
┌────────────────────────────────────────────────────────────┐
│ T1  TUESDAY                              120.0 BPM  ▶ P01  │
├────────────────────────────────────────────────────────────┤
│                                                            │
│  MARKOV       8          5         12         16           │
│             ████░░░░   ███░░░░░   ████████░               │
│                                                            │
│                                                            │
│                                                            │
├────────────────────────────────────────────────────────────┤
│  ALGO       FLOW       ORN        POWER      LOOP          │
└────────────────────────────────────────────────────────────┘
```

Algorithm shows name only. Flow/Ornament/Power show value + bar graph. Loop shows value only.

**Interaction Flow:**

1. **Press F1** → Select ALGO parameter
   - Turn encoder → Cycle through: MARKOV → STOMPER → SCALEWALK → WOBBLE → ...

2. **Press F2** → Select FLOW parameter
   - Turn encoder → Adjust 0-16
   - Bar graph updates in real-time

3. **Press F3** → Select ORN (Ornament) parameter
   - Turn encoder → Adjust 0-16
   - Bar graph updates in real-time

4. **Press F4** → Select POWER parameter
   - Turn encoder → Adjust 0-16
   - Bar graph updates in real-time

5. **Press F5** → Select LOOP parameter
   - Turn encoder → Cycle: Inf, 1-16, 19, 21, 24, 32, 35, 42, 48, 64
   - Sets pattern loop length

**Visual Feedback During Playback:**

```
┌────────────────────────────────────────────────────────────┐
│ T1  TUESDAY                              120.0 BPM  ▶ P01  │
├────────────────────────────────────────────────────────────┤
│                                              ┌──────────┐  │
│  STOMPER      16         3         14        │ ●  C4    │  │
│             ████████   ██░░░░░░   █████████  │ CV: 2.1V │  │
│                                        Inf   │ GT: HIGH │  │
│                                              └──────────┘  │
│                                                            │
├────────────────────────────────────────────────────────────┤
│ >ALGO       FLOW       ORN        POWER      LOOP          │
└────────────────────────────────────────────────────────────┤
```

The `>` indicator shows currently selected parameter. Activity box (top-right) shows:
- Current note (C4)
- CV output voltage
- Gate state (HIGH/LOW)

**Comparison with NoteSequenceEditPage:**

| Aspect | NoteTrack | Tuesday Track |
|--------|-----------|---------------|
| Main display | 16 step grid with notes | 5 parameter columns with values + bars |
| F1-F5 | Gate/Retrig/Length/Note/Cond layers | ALGO/FLOW/ORN/POWER/LOOP params |
| S1-S16 | Select/toggle steps | Not used (no steps) |
| Encoder | Edit selected step value | Edit selected parameter |
| Visual focus | Step data | Parameter values + output monitor |

**Track Selection View (OverviewPage):**

```
┌────────────────────────────────────────────────────────────┐
│ OVERVIEW                                                   │
├────────────────────────────────────────────────────────────┤
│ T1 ████████████████  Note    C maj                         │
│ T2 ████░░░░████░░░░  Note    A min                         │
│ T3 ▀▀▀▀▀▄▄▄▀▀▀▀▀▄▄▄  Curve   LFO                           │
│ T4 ░░██░░██░░██░░██  Tuesday MARKOV                        │  ← Tuesday shows algorithm
│ T5 ────────────────  --                                    │
│ T6 ────────────────  --                                    │
│ T7 ────────────────  --                                    │
│ T8 ────────────────  --                                    │
└────────────────────────────────────────────────────────────┘
```

Tuesday track in Overview shows algorithm name instead of scale/sequence info.

### Why "Tuesday"

The name references the **Tuesday** Eurorack module - a generative algorithmic sequencer that produces tightly coupled gate+pitch pairs using various musical algorithms.

---

## Architecture Overview

The Performer follows strict **Model → Engine → UI** layered architecture:

```
┌─────────────────────────────────────┐
│            UI Layer                 │  ← Step 3: TuesdayPage
│  (pages, controllers, painters)     │
├─────────────────────────────────────┤
│          Engine Layer               │  ← Step 2: TuesdayTrackEngine
│  (tick, gate/CV output, routing)    │
├─────────────────────────────────────┤
│          Model Layer                │  ← Step 1: TuesdayTrack
│  (data storage, serialization)      │
└─────────────────────────────────────┘
```

Each layer only depends on layers below it. Engine references Model. UI references both.

---

## FEASIBILITY EVALUATION

### Technical Assessment: ✅ FULLY SUPPORTED

The architecture is well-designed for new track types:

| Layer | Pattern | Tuesday Integration |
|-------|---------|---------------------|
| **Model** | `Container<NoteTrack, CurveTrack, MidiCvTrack>` | Add `TuesdayTrack` to template |
| **Engine** | `Container<NoteTrackEngine, CurveTrackEngine, MidiCvTrackEngine>` | Add `TuesdayTrackEngine` |
| **UI** | `Pages` struct with track-specific pages | Add `TuesdayPage` |

**Key Advantages:**
- Tuesday track is simpler than existing tracks (5 parameters vs 17 sequences)
- No sequence array needed - follows MidiCvTrack pattern (simplest track type)
- Memory footprint ~20 bytes vs ~50KB for NoteTrack
- Well-documented existing patterns to follow

### Effort Estimation

| Category | Count | Complexity |
|----------|-------|------------|
| Files to create | 7 | Low |
| Files to modify | 13 | Medium (mechanical switch statements) |
| Total switch statements to update | 25+ | Medium |

**Overall Complexity: MEDIUM**

The complexity is medium because:
- The patterns are well-established and documented
- Tuesday track is simpler than existing tracks (no sequences)
- Many files need modification but changes are mechanical (adding switch cases)
- Algorithm implementation (the actual musical logic) adds complexity but is isolated

### Risk Assessment

| Risk | Level | Mitigation |
|------|-------|------------|
| Missing switch statements | Medium | Grep for `TrackMode::` before completion |
| Serialization compatibility | Low | Version35 follows established pattern |
| Memory constraints | Low | TuesdayTrack is smallest track type |
| Algorithm real-time performance | Medium | Profile early, must be <50μs/tick |
| LaunchpadController integration | Medium | Has 12+ switch statements - easy to miss one |

### Dependencies

**Must Exist First:**
- Build environment (simulator minimum)
- Understanding of algorithm logic (MARKOV, STOMPER, etc.)
- Familiarity with UnitTest.h framework (NOT Catch2/gtest)

**External Dependencies:**
- None - all required libraries already integrated

---

## PHASE 0: TDD FOUNDATION

This phase creates the scaffolding needed before actual TDD implementation begins. All files compile but contain minimal/stub functionality.

Following **Model → Engine → UI** architecture order.

---

### Model Layer Setup (Steps 0.1-0.5)

#### Step 0.1: Create TuesdayTrack Model Stub

**Purpose:** Establish TuesdayTrack model class with empty implementation

**Files to create:**
- `src/apps/sequencer/model/TuesdayTrack.h`
- `src/apps/sequencer/model/TuesdayTrack.cpp`

**TuesdayTrack.h:**
```cpp
#pragma once

#include "Config.h"
#include "Serialize.h"

class TuesdayTrack {
public:
    // Constructor
    TuesdayTrack() { clear(); }

    // Placeholder accessors
    uint8_t algorithm() const { return _algorithm; }
    void setAlgorithm(uint8_t) {}

    uint8_t flow() const { return _flow; }
    void setFlow(uint8_t) {}

    uint8_t ornament() const { return _ornament; }
    void setOrnament(uint8_t) {}

    uint8_t power() const { return _power; }
    void setPower(uint8_t) {}

    uint8_t loopLength() const { return _loopLength; }
    void setLoopLength(uint8_t) {}

    void clear();
    void write(VersionedSerializedWriter &writer) const;
    void read(VersionedSerializedReader &reader);

private:
    void setTrackIndex(int trackIndex) { _trackIndex = trackIndex; }

    int8_t _trackIndex = -1;
    uint8_t _algorithm = 0;
    uint8_t _flow = 0;        // 0-16, default silent
    uint8_t _ornament = 0;    // 0-16, default none
    uint8_t _power = 0;       // 0-16, default silent
    uint8_t _loopLength = 16; // Inf, 1-16, 19, 21, 24, 32, 35, 42, 48, 64

    friend class Track;
};
```

**TuesdayTrack.cpp:**
```cpp
#include "TuesdayTrack.h"

void TuesdayTrack::clear() {
    _algorithm = 0;
    _flow = 0;
    _ornament = 0;
    _power = 0;
    _loopLength = 16;
}

void TuesdayTrack::write(VersionedSerializedWriter &writer) const {
    writer.write(_algorithm);
    writer.write(_flow);
    writer.write(_ornament);
    writer.write(_power);
    writer.write(_loopLength);
}

void TuesdayTrack::read(VersionedSerializedReader &reader) {
    reader.read(_algorithm);
    reader.read(_flow);
    reader.read(_ornament);
    reader.read(_power);
    reader.read(_loopLength);
}
```

**Verification:** Files created but not yet integrated

---

#### Step 0.2: Integrate into Track.h

**Purpose:** Add Tuesday to TrackMode enum and Container

**File:** `src/apps/sequencer/model/Track.h`

**Changes:**

1. Add include at top (after MidiCvTrack.h):
```cpp
#include "TuesdayTrack.h"
```

2. Modify TrackMode enum:
```cpp
enum class TrackMode : uint8_t {
    Note,
    Curve,
    MidiCv,
    Tuesday,  // ADD THIS
    Last,
    Default = Note
};
```

3. Update trackModeName():
```cpp
static const char *trackModeName(TrackMode trackMode) {
    switch (trackMode) {
    case TrackMode::Note:    return "Note";
    case TrackMode::Curve:   return "Curve";
    case TrackMode::MidiCv:  return "MIDI/CV";
    case TrackMode::Tuesday: return "Tuesday";  // ADD THIS
    case TrackMode::Last:    break;
    }
    return nullptr;
}
```

4. Update trackModeSerialize():
```cpp
static uint8_t trackModeSerialize(TrackMode trackMode) {
    switch (trackMode) {
    case TrackMode::Note:    return 0;
    case TrackMode::Curve:   return 1;
    case TrackMode::MidiCv:  return 2;
    case TrackMode::Tuesday: return 3;  // ADD THIS
    case TrackMode::Last:    break;
    }
    return 0;
}
```

5. Add accessor methods (after midiCvTrack accessors):
```cpp
// tuesdayTrack
const TuesdayTrack &tuesdayTrack() const { SANITIZE_TRACK_MODE(_trackMode, TrackMode::Tuesday); return _container.as<TuesdayTrack>(); }
      TuesdayTrack &tuesdayTrack()       { SANITIZE_TRACK_MODE(_trackMode, TrackMode::Tuesday); return _container.as<TuesdayTrack>(); }
```

6. Update Container template:
```cpp
Container<NoteTrack, CurveTrack, MidiCvTrack, TuesdayTrack> _container;
```

7. Update union:
```cpp
union {
    NoteTrack *note;
    CurveTrack *curve;
    MidiCvTrack *midiCv;
    TuesdayTrack *tuesday;  // ADD THIS
} _track;
```

8. Update setTrackIndex() switch:
```cpp
case TrackMode::Tuesday:
    _container.as<TuesdayTrack>().setTrackIndex(trackIndex);
    break;
```

9. Update initContainer() switch:
```cpp
case TrackMode::Tuesday:
    _track.tuesday = _container.create<TuesdayTrack>();
    _track.tuesday->setTrackIndex(_trackIndex);
    break;
```

**Verification:** Header compiles without errors

---

#### Step 0.3: Integrate into Track.cpp

**Purpose:** Add Tuesday cases to all switch statements in Track.cpp

**File:** `src/apps/sequencer/model/Track.cpp`

Add `case TrackMode::Tuesday:` to all 8 switch statements:

1. `clear()` - call tuesdayTrack().clear()
2. `clearPattern()` - stub empty (no patterns)
3. `copyPattern()` - stub empty (no patterns)
4. `duplicatePattern()` - return false (no patterns)
5. `gateOutputName()` - return "G" (basic gate)
6. `cvOutputName()` - return "CV" (basic CV)
7. `write()` - call tuesdayTrack().write(writer)
8. `read()` - call tuesdayTrack().read(reader)

**Verification:** Track.cpp compiles without errors

---

#### Step 0.4: Update Project Version

**Purpose:** Add serialization version for Tuesday track support

**File:** `src/apps/sequencer/model/ProjectVersion.h`

Add after Version34:
```cpp
// added Tuesday track type
Version35 = 35,

// automatically derive latest version
Last,
Latest = Last - 1
```

**Verification:** ProjectVersion.h compiles

---

#### Step 0.5: Create Test Infrastructure

**Purpose:** Set up test file and register with CMake

**File:** `src/tests/unit/sequencer/TestTuesdayTrack.cpp`

```cpp
#include "UnitTest.h"
#include "apps/sequencer/model/TuesdayTrack.h"

UNIT_TEST("TuesdayTrack") {

CASE("default_values") {
    TuesdayTrack track;
    expectEqual(track.algorithm(), 0, "default algorithm should be 0");
    expectEqual(track.flow(), 0, "default flow should be 0");
    expectEqual(track.ornament(), 0, "default ornament should be 0");
    expectEqual(track.power(), 0, "default power should be 0");
    expectEqual(track.loopLength(), 16, "default loopLength should be 16");
}

} // UNIT_TEST("TuesdayTrack")
```

**Verification:** Test compiles and runs, showing 1 passing test

---

### Engine Layer Setup (Steps 0.6-0.8)

#### Step 0.6: Create TuesdayTrackEngine Stub

**Purpose:** Establish TuesdayTrackEngine class with minimal implementation

**Files to create:**
- `src/apps/sequencer/engine/TuesdayTrackEngine.h`
- `src/apps/sequencer/engine/TuesdayTrackEngine.cpp`

**TuesdayTrackEngine.h:**
```cpp
#pragma once

#include "TrackEngine.h"
#include "model/Track.h"

class TuesdayTrackEngine : public TrackEngine {
public:
    TuesdayTrackEngine(Engine &engine, const Model &model, Track &track, const TrackEngine *linkedTrackEngine) :
        TrackEngine(engine, model, track, linkedTrackEngine),
        _tuesdayTrack(track.tuesdayTrack())
    {
        reset();
    }

    virtual Track::TrackMode trackMode() const override { return Track::TrackMode::Tuesday; }

    virtual void reset() override;
    virtual void restart() override;
    virtual TickResult tick(uint32_t tick) override;
    virtual void update(float dt) override;

    virtual bool activity() const override { return _activity; }
    virtual bool gateOutput(int index) const override { return _gateOutput; }
    virtual float cvOutput(int index) const override { return _cvOutput; }

private:
    const TuesdayTrack &_tuesdayTrack;

    bool _activity = false;
    bool _gateOutput = false;
    float _cvOutput = 0.f;
};
```

**TuesdayTrackEngine.cpp:**
```cpp
#include "TuesdayTrackEngine.h"

void TuesdayTrackEngine::reset() {
    _activity = false;
    _gateOutput = false;
    _cvOutput = 0.f;
}

void TuesdayTrackEngine::restart() {
    reset();
}

TrackEngine::TickResult TuesdayTrackEngine::tick(uint32_t tick) {
    // Stub - returns no update
    return TickResult::NoUpdate;
}

void TuesdayTrackEngine::update(float dt) {
    // Stub - no sliding needed yet
}
```

**Verification:** Files created but not yet integrated

---

#### Step 0.7: Integrate into Engine.h

**Purpose:** Add TuesdayTrackEngine to engine container

**File:** `src/apps/sequencer/engine/Engine.h`

**Changes:**

1. Add include:
```cpp
#include "TuesdayTrackEngine.h"
```

2. Update TrackEngineContainer typedef:
```cpp
using TrackEngineContainer = Container<NoteTrackEngine, CurveTrackEngine, MidiCvTrackEngine, TuesdayTrackEngine>;
```

**Verification:** Engine.h compiles

---

#### Step 0.8: Integrate into Engine.cpp

**Purpose:** Add TuesdayTrackEngine creation to updateTrackSetups()

**File:** `src/apps/sequencer/engine/Engine.cpp`

In `updateTrackSetups()` switch, add:
```cpp
case Track::TrackMode::Tuesday:
    trackEngine = trackContainer.create<TuesdayTrackEngine>(*this, _model, track, linkedTrackEngine);
    break;
```

**Verification:** Engine compiles without errors

---

### UI Layer Setup (Steps 0.9-0.12)

#### Step 0.9: Create TuesdayPage Stub

**Purpose:** Create TuesdayPage with minimal list-based interface

**Files to create:**
- `src/apps/sequencer/ui/pages/TuesdayPage.h`
- `src/apps/sequencer/ui/pages/TuesdayPage.cpp`

**TuesdayPage.h:**
```cpp
#pragma once

#include "ListPage.h"

class TuesdayPage : public ListPage {
public:
    TuesdayPage(PageManager &manager, PageContext &context);

    void enter() override;
    void exit() override;
    void draw(Canvas &canvas) override;
    void keyPress(KeyPressEvent &event) override;
    void encoder(EncoderEvent &event) override;
};
```

**TuesdayPage.cpp:**
```cpp
#include "TuesdayPage.h"

TuesdayPage::TuesdayPage(PageManager &manager, PageContext &context) :
    ListPage(manager, context, 5)  // 5 parameters
{}

void TuesdayPage::enter() {}
void TuesdayPage::exit() {}

void TuesdayPage::draw(Canvas &canvas) {
    // Stub - just draw title
    canvas.setColor(Color::Bright);
    canvas.drawText(8, 8, "TUESDAY");
}

void TuesdayPage::keyPress(KeyPressEvent &event) {
    // Stub
}

void TuesdayPage::encoder(EncoderEvent &event) {
    // Stub
}
```

**Verification:** Page files compile

---

#### Step 0.10: Integrate Page into Pages.h

**Purpose:** Register TuesdayPage with page system

**File:** `src/apps/sequencer/ui/pages/Pages.h`

**Changes:**

1. Add include (after existing page includes):
```cpp
#include "TuesdayPage.h"
```

2. Add to struct (after existing pages):
```cpp
TuesdayPage tuesday;
```

3. Add to constructor (after existing initializations):
```cpp
tuesday(manager, context),
```

**Verification:** Pages.h compiles

---

#### Step 0.11: Update TopPage Navigation

**Purpose:** Add Tuesday track type routing to pages

**File:** `src/apps/sequencer/ui/pages/TopPage.cpp`

Add `case Track::TrackMode::Tuesday:` to 3 switches:

1. In `setSequenceView()`:
```cpp
case Track::TrackMode::Tuesday:
    setMainPage(pages.tuesday);
    break;
```

2. In `setTrackView()`:
```cpp
case Track::TrackMode::Tuesday:
    setMainPage(pages.track);
    break;
```

3. In `setSequenceEditPage()`:
```cpp
case Track::TrackMode::Tuesday:
    setMainPage(pages.tuesday);
    break;
```

**Verification:** TopPage compiles and navigates to Tuesday pages

---

#### Step 0.12: Update Other UI Files

**Purpose:** Add Tuesday cases to remaining switch statements

**Files to modify:**

1. `src/apps/sequencer/ui/pages/OverviewPage.cpp`
   - Add case in track display switch

2. `src/apps/sequencer/ui/pages/TrackPage.cpp`
   - Add case in track config switch

3. `src/apps/sequencer/ui/controllers/launchpad/LaunchpadController.cpp`
   - Add cases to all 12+ switch statements (can use default fallthrough initially)

**Verification:** All UI files compile

---

### Build System Integration (Step 0.13)

#### Step 0.13: Update CMakeLists.txt

**Purpose:** Register new source files with CMake

**File:** `src/apps/sequencer/CMakeLists.txt`

Add to appropriate sections:
```cmake
# Model section
model/TuesdayTrack.cpp

# Engine section
engine/TuesdayTrackEngine.cpp

# UI section
ui/pages/TuesdayPage.cpp
```

**File:** `src/tests/unit/sequencer/CMakeLists.txt`

Add:
```cmake
register_sequencer_test(TestTuesdayTrack TestTuesdayTrack.cpp)
```

**Verification:** Project builds successfully

---

### Final Verification (Step 0.14)

#### Step 0.14: Full Build Verification

**Purpose:** Verify complete integration compiles and runs

**Actions:**

1. Build simulator:
```bash
cd /home/user/performer-phazer/build/sim/debug
make -j
```

2. Run simulator and verify:
   - Application starts without crash
   - Can switch track to Tuesday mode
   - Basic navigation works

3. Run unit tests:
```bash
./src/tests/unit/sequencer/TestTuesdayTrack
```

**Success Criteria:**
- ✅ Simulator builds without errors
- ✅ Can switch track to Tuesday mode
- ✅ Navigation to TuesdayPage works
- ✅ TestTuesdayTrack passes (1 test)
- ✅ No crashes when selecting Tuesday track

---

## Summary Tables

### Files to Create (7)

| File | Layer | Purpose |
|------|-------|---------|
| `src/apps/sequencer/model/TuesdayTrack.h` | Model | Data class header |
| `src/apps/sequencer/model/TuesdayTrack.cpp` | Model | Data class impl |
| `src/apps/sequencer/engine/TuesdayTrackEngine.h` | Engine | Processing header |
| `src/apps/sequencer/engine/TuesdayTrackEngine.cpp` | Engine | Processing impl |
| `src/apps/sequencer/ui/pages/TuesdayPage.h` | UI | Page header |
| `src/apps/sequencer/ui/pages/TuesdayPage.cpp` | UI | Page impl |
| `src/tests/unit/sequencer/TestTuesdayTrack.cpp` | Test | Unit tests |

### Files to Modify (13)

| File | Layer | Changes |
|------|-------|---------|
| `src/apps/sequencer/model/Track.h` | Model | Enum, accessors, Container, union, 5 switches |
| `src/apps/sequencer/model/Track.cpp` | Model | 8 switch statements |
| `src/apps/sequencer/model/ProjectVersion.h` | Model | Add Version35 |
| `src/apps/sequencer/engine/Engine.h` | Engine | Include, TrackEngineContainer typedef |
| `src/apps/sequencer/engine/Engine.cpp` | Engine | 1 switch statement |
| `src/apps/sequencer/ui/pages/Pages.h` | UI | Include, struct member, constructor |
| `src/apps/sequencer/ui/pages/TopPage.cpp` | UI | 3 switch statements |
| `src/apps/sequencer/ui/pages/OverviewPage.cpp` | UI | 1 switch statement |
| `src/apps/sequencer/ui/pages/TrackPage.cpp` | UI | 1 switch statement |
| `src/apps/sequencer/ui/controllers/launchpad/LaunchpadController.cpp` | UI | 12+ switch statements |
| `src/apps/sequencer/CMakeLists.txt` | Build | 3 source file additions |
| `src/tests/unit/sequencer/CMakeLists.txt` | Build | 1 test registration |

---

## Next Steps After Phase 0

Once Phase 0 is complete (all stubs compile and basic navigation works):

| Phase | Layer | Focus | Description |
|-------|-------|-------|-------------|
| **Phase 1** | Model | TDD | Parameter validation (setters with clamping, serialization) |
| **Phase 2** | Engine | TDD | tick() logic (gate/CV generation, algorithm dispatch) |
| **Phase 3** | UI | TDD | F0-F4 parameter control, encoder handling, display |
| **Phase 4** | Integration | Tests | Full system verification, multi-track interaction |
| **Phase 5** | Algorithms | Impl | MARKOV, STOMPER, SCALEWALK, etc. |

---

## Test Framework Reference

**CRITICAL**: This project uses custom **UnitTest.h** framework, NOT Catch2 or Google Test.

**Correct Pattern:**
```cpp
#include "UnitTest.h"

UNIT_TEST("TestName") {

CASE("test_case_name") {
    expectEqual(actual, expected, "message");
    expectTrue(condition, "message");
    expectFalse(condition, "message");
}

} // UNIT_TEST("TestName")
```

**Enum Comparison:**
```cpp
expectEqual(static_cast<int>(actual), static_cast<int>(expected), "message");
```