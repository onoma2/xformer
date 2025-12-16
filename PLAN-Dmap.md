
# DiscreteMap Track Implementation Plan (Updated)
  Todos
  ☒ Create DiscreteMapSequence model (header)
  ☒ Create DiscreteMapSequence model (implementation)
  ☒ Extend Track model for DiscreteMap mode
  ☒ Update ProjectVersion for DiscreteMap
  ☒ Create DiscreteMapTrackEngine (header)
  ☒ Create DiscreteMapTrackEngine (implementation)
  ☐ Register engine in TrackEngine factory
  ☐ Create DiscreteMapSequencePage (header)
  ☐ Create DiscreteMapSequencePage (implementation)
  ☐ Register page in PageManager
  ☐ Add DiscreteMap to track mode selection
  ☐ Write unit tests for DiscreteMapSequence
  ☐ Write unit tests for DiscreteMapTrackEngine
  ☐ Build and run tests

## Overview
Implementing a threshold-based CV mapper as a new track type. It monitors CV input (internal synced ramp or external routed CV) and triggers quantized note outputs when input crosses configurable voltage thresholds.

## Key Changes from Original Plan
1. **Removed range mode** - voltage ranges handled by routing system
2. **FN buttons as toggles** - switch between states, update footer text
3. **Separate edit page** - DiscreteMapEditPage for detailed parameter editing
4. **Bipolar values**:
   - Note index: -63 to +64 (128 values)
   - Threshold: -127 to +128 (256 values)
5. **UI follows TuesdaySequencePage conventions**

---

## Phase 1: Data Model

### 1.1 Create DiscreteMapSequence

**File:** `src/apps/sequencer/model/DiscreteMapSequence.h`

```cpp
class DiscreteMapSequence {
public:
    static constexpr int kStageCount = 8;

    // === Stage ===
    struct Stage {
        enum class TriggerDir : uint8_t { Rise, Fall, Off };

        // Bipolar values
        int8_t threshold() const { return _threshold; }      // -127 to +128
        void setThreshold(int8_t threshold) {
            _threshold = clamp(threshold, -127, 128);
            _dirty = true;
        }

        TriggerDir direction() const { return _direction; }
        void setDirection(TriggerDir dir) {
            _direction = dir;
            _dirty = true;
        }

        int8_t noteIndex() const { return _noteIndex; }     // -63 to +64
        void setNoteIndex(int8_t index) {
            _noteIndex = clamp(index, -63, 64);
            _dirty = true;
        }

        void write(VersionedSerializedWriter &writer) const;
        void read(VersionedSerializedReader &reader);
        void clear();

    private:
        int8_t _threshold = 0;        // -127 to +128
        TriggerDir _direction = TriggerDir::Off;
        int8_t _noteIndex = 0;        // -63 to +64
        bool _dirty = false;
    };

    // === Clock ===
    enum class ClockSource : uint8_t {
        Internal,  // Always synced to clock (no "Free" mode)
        External   // Uses routed CV input (when routing enabled)
    };

    ClockSource clockSource() const { return _clockSource; }
    void setClockSource(ClockSource source) { _clockSource = source; }
    void toggleClockSource() {
        _clockSource = (_clockSource == ClockSource::Internal)
            ? ClockSource::External : ClockSource::Internal;
    }

    int divisor() const { return _divisor; }        // 1,2,4,8,16,32,64
    void setDivisor(int div) { _divisor = clamp(div, 1, 64); }

    bool loop() const { return _loop; }
    void setLoop(bool loop) { _loop = loop; }
    void toggleLoop() { _loop = !_loop; }

    // === Threshold ===
    enum class ThresholdMode : uint8_t { Position, Length };

    ThresholdMode thresholdMode() const { return _thresholdMode; }
    void setThresholdMode(ThresholdMode mode) { _thresholdMode = mode; }
    void toggleThresholdMode() {
        _thresholdMode = (_thresholdMode == ThresholdMode::Position)
            ? ThresholdMode::Length : ThresholdMode::Position;
    }

    // === Scale ===
    enum class ScaleSource : uint8_t { Project, Track };

    ScaleSource scaleSource() const { return _scaleSource; }
    void setScaleSource(ScaleSource source) { _scaleSource = source; }

    int rootNote() const { return _rootNote; }       // 0-11
    void setRootNote(int root) { _rootNote = clamp(root, 0, 11); }

    bool slewEnabled() const { return _slewEnabled; }
    void setSlewEnabled(bool enabled) { _slewEnabled = enabled; }
    void toggleSlew() { _slewEnabled = !_slewEnabled; }

    // === Routing ===
    // Note: CV input routing is handled by routing system, not stored here
    // Track reads routed CV via _track.routedCvInput() or similar

    // === Stages ===
    Stage& stage(int index) { return _stages[index]; }
    const Stage& stage(int index) const { return _stages[index]; }

    // === Serialization ===
    void write(VersionedSerializedWriter &writer) const;
    void read(VersionedSerializedReader &reader);
    void clear();

    // === Routing Support ===
    void setRouting(Routing::Target target) { _routingTarget = target; }
    bool isRouted(Routing::Target target) const;

private:
    ClockSource _clockSource = ClockSource::Internal;
    uint8_t _divisor = 4;
    bool _loop = true;

    ThresholdMode _thresholdMode = ThresholdMode::Position;

    ScaleSource _scaleSource = ScaleSource::Project;
    int8_t _rootNote = 0;          // C
    bool _slewEnabled = false;

    Stage _stages[kStageCount];
    Routing::Target _routingTarget = Routing::Target::None;
};
```

**File:** `src/apps/sequencer/model/DiscreteMapSequence.cpp`
- Implement accessors/mutators
- Serialization (bit-pack bipolar values efficiently)
- clear() sets defaults: all stages OFF, threshold=0, noteIndex=0

### 1.2 Extend Track Model

**File:** `src/apps/sequencer/model/Track.h`

```cpp
enum class TrackMode : uint8_t {
    Note,
    Curve,
    MidiCv,
    Stochastic,
    Logic,
    Arp,
    DiscreteMap,  // NEW
    Last
};

class Track {
    // ...
    DiscreteMapSequence &discreteMapSequence(int pattern);
    const DiscreteMapSequence &discreteMapSequence(int pattern) const;

private:
    std::array<DiscreteMapSequence, CONFIG_PATTERN_COUNT> _discreteMapSequences;
};
```

**File:** `src/apps/sequencer/model/Track.cpp`
- Add accessors for _discreteMapSequences
- Update serialization to handle new mode
- Update clear() to initialize discrete map sequences

### 1.3 Update ProjectVersion

**File:** `src/apps/sequencer/model/ProjectVersion.h`

```cpp
enum class ProjectVersion : uint32_t {
    // ...existing versions...
    Version_DiscreteMap = XX,  // Next available version number
    Last
};
```

---

## Phase 2: Engine

### 2.1 Create DiscreteMapTrackEngine

**File:** `src/apps/sequencer/engine/DiscreteMapTrackEngine.h`

```cpp
class DiscreteMapTrackEngine : public TrackEngine {
public:
    DiscreteMapTrackEngine(Engine &engine, Model &model, Track &track);

    void reset() override;
    void restart() override;
    void tick(uint32_t tick) override;
    void update(float dt) override;

    // Outputs
    float cvOutput() const { return _cvOutput; }
    bool gateOutput() const { return _activeStage >= 0; }

    // For UI
    int activeStage() const { return _activeStage; }
    float currentInput() const { return _currentInput; }
    float rampPhase() const { return _rampPhase; }

private:
    const DiscreteMapSequence *_sequence = nullptr;

    // === Ramp State (Internal clock source) ===
    float _rampPhase = 0.0f;        // 0.0 - 1.0
    float _rampValue = 0.0f;        // Current voltage
    bool _running = true;

    // === Input ===
    float _currentInput = 0.0f;
    float _prevInput = 0.0f;

    // === Threshold Cache (Length mode) ===
    float _lengthThresholds[DiscreteMapSequence::kStageCount];
    bool _thresholdsDirty = true;

    // === Stage State ===
    int _activeStage = -1;          // -1 = none

    // === Output ===
    float _cvOutput = 0.0f;
    float _targetCv = 0.0f;

    // === Methods ===
    void updateRamp(uint32_t tick);
    float getRoutedInput();
    float getThresholdVoltage(int stageIndex);
    void recalculateLengthThresholds();
    int findActiveStage(float input, float prevInput);
    float noteIndexToVoltage(int8_t noteIndex);

    // Voltage range helpers (always ±5V for internal ramp)
    float rangeMin() const { return -5.0f; }
    float rangeMax() const { return 5.0f; }
};
```

**File:** `src/apps/sequencer/engine/DiscreteMapTrackEngine.cpp`

Key implementations:

```cpp
void DiscreteMapTrackEngine::tick(uint32_t tick) {
    _sequence = &_track.discreteMapSequence(_project.selectedPatternIndex());

    // 1. Update input source
    if (_sequence->clockSource() == ClockSource::Internal) {
        updateRamp(tick);
        _currentInput = _rampValue;
    } else {
        _currentInput = getRoutedInput();  // Read from routing system
    }

    // 2. Recalc length thresholds if needed
    if (_thresholdsDirty && _sequence->thresholdMode() == ThresholdMode::Length) {
        recalculateLengthThresholds();
        _thresholdsDirty = false;
    }

    // 3. Find active stage from threshold crossings
    int newStage = findActiveStage(_currentInput, _prevInput);
    _activeStage = newStage;

    // 4. Update CV output
    if (_activeStage >= 0) {
        _targetCv = noteIndexToVoltage(_sequence->stage(_activeStage).noteIndex());

        if (_sequence->slewEnabled()) {
            // Simple slew rate (can be made configurable later)
            float slewRate = 0.1f;
            _cvOutput += (_targetCv - _cvOutput) * slewRate;
        } else {
            _cvOutput = _targetCv;
        }
    } else {
        _cvOutput = 0.0f;
    }

    _prevInput = _currentInput;
}

int DiscreteMapTrackEngine::findActiveStage(float input, float prev) {
    // Scan stages in order, first crossing wins
    for (int i = 0; i < DiscreteMapSequence::kStageCount; i++) {
        const auto& stage = _sequence->stage(i);
        if (stage.direction() == Stage::TriggerDir::Off) continue;

        float thresh = getThresholdVoltage(i);
        bool crossed = false;

        if (stage.direction() == Stage::TriggerDir::Rise) {
            crossed = (prev < thresh && input >= thresh);
        } else {  // Fall
            crossed = (prev > thresh && input <= thresh);
        }

        if (crossed) return i;
    }

    return _activeStage;  // No crossing, keep current
}

void DiscreteMapTrackEngine::recalculateLengthThresholds() {
    // Length mode: distribute thresholds proportionally
    float totalWeight = 0;
    for (int i = 0; i < DiscreteMapSequence::kStageCount; i++) {
        totalWeight += abs(_sequence->stage(i).threshold());
    }
    if (totalWeight == 0) totalWeight = 1;

    float range = rangeMax() - rangeMin();
    float cumulative = rangeMin();

    for (int i = 0; i < DiscreteMapSequence::kStageCount; i++) {
        float proportion = abs(_sequence->stage(i).threshold()) / totalWeight;
        cumulative += proportion * range;
        _lengthThresholds[i] = cumulative;
    }
}

float DiscreteMapTrackEngine::noteIndexToVoltage(int8_t noteIndex) {
    const Scale& scale = (_sequence->scaleSource() == ScaleSource::Project)
        ? _model.project().scale()
        : _track.scale();

    int root = _sequence->rootNote();

    // noteIndex is -63 to +64, map to scale
    int octave = noteIndex / scale.notesPerOctave();
    int idx = noteIndex % scale.notesPerOctave();
    if (idx < 0) {
        idx += scale.notesPerOctave();
        octave -= 1;
    }

    int note = scale.note(idx);
    return (root + note + octave * 12) / 12.0f;  // 1V/oct
}
```

### 2.2 Register Engine in Factory

**File:** `src/apps/sequencer/engine/TrackEngine.cpp`

```cpp
std::unique_ptr<TrackEngine> TrackEngine::create(
    Engine &engine, Model &model, Track &track
) {
    switch (track.trackMode()) {
        // ...existing cases...
        case TrackMode::DiscreteMap:
            return std::make_unique<DiscreteMapTrackEngine>(engine, model, track);
    }
}
```

---

## Phase 3: UI - Main Sequence Page

### 3.1 Create DiscreteMapSequencePage (Visual/Interactive)

**File:** `src/apps/sequencer/ui/pages/DiscreteMapSequencePage.h`

Visual threshold bar interface with direct hardware button mapping:

```cpp
class DiscreteMapSequencePage : public BasePage {
public:
    DiscreteMapSequencePage(PageManager &manager, PageContext &context);

    void enter() override;
    void exit() override;
    void draw(Canvas &canvas) override;
    void updateLeds(Leds &leds) override;

    void keyDown(KeyEvent &event) override;
    void keyUp(KeyEvent &event) override;
    void keyPress(KeyPressEvent &event) override;
    void encoder(EncoderEvent &event) override;

private:
    enum class EditMode { None, Threshold, NoteValue, DualThreshold };

    // State
    EditMode _editMode = EditMode::None;
    int8_t _selectedStage = 0;      // 0-7
    int8_t _secondaryStage = -1;    // For dual threshold editing
    bool _shiftHeld = false;

    // References
    DiscreteMapSequence *_sequence = nullptr;
    DiscreteMapTrackEngine *_engine = nullptr;

    // Drawing
    void drawHeader(Canvas &canvas);
    void drawThresholdBar(Canvas &canvas);
    void drawStageInfo(Canvas &canvas);
    void drawFooter(Canvas &canvas);

    // Helpers
    float getThresholdNormalized(int stageIndex) const;
    float rangeMin() const { return -5.0f; }
    float rangeMax() const { return 5.0f; }

    // Handlers
    void handleTopRowKey(int key, bool shift);
    void handleBottomRowKey(int key);
    void handleFunctionKey(int fnIndex);

    // Context menu
    void contextShow();
    void contextAction(int index);
    bool contextActionEnabled(int index) const;

    void initSequence();
    void initRoute();
};
```

**File:** `src/apps/sequencer/ui/pages/DiscreteMapSequencePage.cpp`

Key implementations:

```cpp
void DiscreteMapSequencePage::draw(Canvas &canvas) {
    WindowPainter::clear(canvas);
    WindowPainter::drawHeader(canvas, _model, _engine, "DMAP");

    drawThresholdBar(canvas);
    drawStageInfo(canvas);
    drawFooter(canvas);
}

void DiscreteMapSequencePage::drawThresholdBar(Canvas &canvas) {
    const int barX = 8;
    const int barY = 20;
    const int barW = 240;
    const int barH = 12;

    // Background bar
    canvas.setColor(Color::Medium);
    canvas.fillRect(barX, barY, barW, barH);

    // Draw threshold markers for each stage
    for (int i = 0; i < 8; i++) {
        const auto& stage = _sequence->stage(i);
        if (stage.direction() == Stage::TriggerDir::Off) continue;

        float norm = getThresholdNormalized(i);
        int x = barX + int(norm * barW);

        bool selected = (i == _selectedStage);
        bool active = (i == _engine->activeStage());

        canvas.setColor(active ? Color::Bright : (selected ? Color::Medium : Color::Low));
        canvas.drawVLine(x, barY, barH);

        // Direction indicator (^ or v)
        char dir = (stage.direction() == Stage::TriggerDir::Rise) ? '^' : 'v';
        canvas.drawChar(x - 2, barY + barH + 2, dir);
    }

    // Live input cursor (bright vertical line)
    float inputNorm = (_engine->currentInput() - rangeMin()) / (rangeMax() - rangeMin());
    int cursorX = barX + int(clamp(inputNorm, 0.f, 1.f) * barW);
    canvas.setColor(Color::Bright);
    canvas.drawVLine(cursorX, barY - 2, barH + 4);
}

void DiscreteMapSequencePage::drawStageInfo(Canvas &canvas) {
    const int y = 44;
    const int spacing = 30;

    for (int i = 0; i < 8; i++) {
        const auto& stage = _sequence->stage(i);
        int x = 8 + i * spacing;

        // Stage number (1-8)
        canvas.drawText(x, y, FixedStringBuilder<2>("%d", i + 1));

        // Note name or "--" if off
        if (stage.direction() != Stage::TriggerDir::Off) {
            canvas.drawText(x, y + 10, Types::noteName(stage.noteIndex()));
        } else {
            canvas.drawText(x, y + 10, "--");
        }
    }
}

void DiscreteMapSequencePage::handleTopRowKey(int idx, bool shift) {
    if (shift) {
        // Shift + top button = edit note value
        _editMode = EditMode::NoteValue;
        _selectedStage = idx;
    } else if (_selectedStage >= 0 && _selectedStage != idx) {
        // Second selection = dual threshold edit mode
        _secondaryStage = idx;
        _editMode = EditMode::DualThreshold;
    } else {
        // Single selection = threshold edit
        _editMode = EditMode::Threshold;
        _selectedStage = idx;
        _secondaryStage = -1;
    }
}

void DiscreteMapSequencePage::handleBottomRowKey(int idx) {
    auto& stage = _sequence->stage(idx);

    // Cycle direction: Rise -> Fall -> Off -> Rise
    switch (stage.direction()) {
        case TriggerDir::Rise: stage.setDirection(TriggerDir::Fall); break;
        case TriggerDir::Fall: stage.setDirection(TriggerDir::Off); break;
        case TriggerDir::Off:  stage.setDirection(TriggerDir::Rise); break;
    }
}

void DiscreteMapSequencePage::encoder(EncoderEvent &event) {
    int delta = event.value();

    switch (_editMode) {
        case EditMode::Threshold: {
            auto& s = _sequence->stage(_selectedStage);
            int step = _shiftHeld ? 1 : 8;
            s.setThreshold(s.threshold() + delta * step);  // Clamped in setter
            break;
        }

        case EditMode::NoteValue: {
            auto& s = _sequence->stage(_selectedStage);
            int step = _shiftHeld ? 12 : 1;  // Octaves vs semitones
            s.setNoteIndex(s.noteIndex() + delta * step);
            break;
        }

        case EditMode::DualThreshold: {
            auto& s1 = _sequence->stage(_selectedStage);
            auto& s2 = _sequence->stage(_secondaryStage);
            int step = _shiftHeld ? 1 : 8;
            s1.setThreshold(s1.threshold() + delta * step);
            s2.setThreshold(s2.threshold() + delta * step);
            break;
        }

        default:
            break;
    }
}

void DiscreteMapSequencePage::updateLeds(Leds &leds) {
    // Bottom row: direction state
    for (int i = 0; i < 8; i++) {
        auto dir = _sequence->stage(i).direction();
        switch (dir) {
            case TriggerDir::Rise: leds.set(MatrixMap::fromStep(i), true); break;
            case TriggerDir::Fall: leds.set(MatrixMap::fromStep(i), 0.3f); break;  // Dim
            case TriggerDir::Off:  leds.set(MatrixMap::fromStep(i), false); break;
        }
    }

    // Top row: selection + active stage
    for (int i = 0; i < 8; i++) {
        bool selected = (i == _selectedStage || i == _secondaryStage);
        bool active = (i == _engine->activeStage());

        if (selected) {
            leds.set(MatrixMap::fromStep(i + 8), true);
        } else if (active) {
            leds.set(MatrixMap::fromStep(i + 8), 0.5f);
        } else {
            leds.set(MatrixMap::fromStep(i + 8), false);
        }
    }
}
```

**Layout:**
```
┌────────────────────────────────────────────────┐
│ TR1  DMAP  ●RUN  ▶1/4  Root:C  Scale:Minor    │ Header (y=0-9)
├────────────────────────────────────────────────┤
│                                                │
│ Thr: ░░░▓▓▓░░░░░░▓▓░░░░▓▓▓▓░░░░░▓░░░░░▓▓░░░░▓ │ Threshold bar (y=20)
│      1   2   3   4   5   6   7   8   ▼        │ Stage numbers + cursor
│                                                │
│ f(X) C4  D4  E4  F4  G4  --  B4  C5            │ Note names (y=44)
│  Dir  ▲   ▲   ▼   ▲   ▲   -   ▼   ▲            │ Directions
│                                                │
├────────────────────────────────────────────────┤ (y=55)
│  [  INT  ] [      ] [  POS  ] [ LOOP  ]       │ Footer
└────────────────────────────────────────────────┘
   FN0         FN1      FN2        FN3
```

### 3.2 Footer Behavior (FN Toggles)

**FN0 - Clock Source:**
- Display: "INT" or "EXT"
- Toggles between Internal/External

**FN2 - Threshold Mode:**
- Display: "POS" or "LEN"
- Toggles between Position/Length mode

**FN3 - Loop Mode:**
- Display: "LOOP" or "ONCE"
- Toggles loop on/off

```cpp
void DiscreteMapSequencePage::handleFunctionKey(int fnIndex) {
    switch (fnIndex) {
    case 0:  // FN0 - Clock source
        _sequence->toggleClockSource();
        break;
    case 2:  // FN2 - Threshold mode
        _sequence->toggleThresholdMode();
        break;
    case 3:  // FN3 - Loop
        _sequence->toggleLoop();
        break;
    }
}

void DiscreteMapSequencePage::drawFooter(Canvas &canvas) {
    const char *fnLabels[5] = {
        _sequence->clockSource() == ClockSource::Internal ? "INT" : "EXT",
        nullptr,  // FN1 unused
        _sequence->thresholdMode() == ThresholdMode::Position ? "POS" : "LEN",
        _sequence->loop() ? "LOOP" : "ONCE",
        nullptr   // FN4 unused
    };

    WindowPainter::drawFooter(canvas, fnLabels, pageKeyState(), -1);
}
```

### 3.3 Button Interaction Summary

**Top Row (Steps 9-16):**
- Press: Select stage for threshold editing
- Press + Shift: Edit note value of that stage
- Press two stages: Dual threshold edit mode (adjust both together)

**Bottom Row (Steps 1-8):**
- Press: Cycle direction (Rise → Fall → Off → Rise)

**Encoder:**
- EditMode::Threshold: Adjust threshold position
- EditMode::NoteValue: Adjust note (shift=octaves, no shift=semitones)
- EditMode::DualThreshold: Adjust both thresholds together
- Shift modifier: Fine adjustment (step=1 vs step=8)

**Function Keys:**
- FN0: Toggle INT/EXT clock
- FN2: Toggle POS/LEN threshold mode
- FN3: Toggle LOOP/ONCE

### 3.4 Context Menu

```cpp
enum class ContextAction {
    Init,
    Route,
    Last
};

static const ContextMenuModel::Item contextMenuItems[] = {
    { "INIT" },
    { "ROUTE" },
};
```

---

## Phase 4: Integration

### 4.1 Register Pages

**File:** `src/apps/sequencer/ui/pages/Pages.h`

```cpp
enum class PageId {
    // ...existing...
    DiscreteMapSequence,
};
```

**File:** `src/apps/sequencer/ui/PageManager.cpp`

```cpp
void PageManager::createPages() {
    // ...existing...
    _pages[PageId::DiscreteMapSequence] =
        std::make_unique<DiscreteMapSequencePage>(*this, _context);
}

PageId PageManager::pageForTrackMode(TrackMode mode) {
    switch (mode) {
        // ...existing...
        case TrackMode::DiscreteMap: return PageId::DiscreteMapSequence;
    }
}
```

### 4.2 CV/Gate Output Integration

Ensure `DiscreteMapTrackEngine::cvOutput()` and `::gateOutput()` feed into existing output system.

**File:** `src/apps/sequencer/engine/CvOutput.cpp`

Should already work if `TrackEngine` virtual methods are properly overridden.

### 4.3 Track Mode Selection

Add DiscreteMap to track mode selection menu.

**File:** `src/apps/sequencer/ui/pages/TrackPage.cpp` (or equivalent)

---

## Phase 5: Testing

### 5.1 Unit Tests

**File:** `src/tests/unit/sequencer/TestDiscreteMapSequence.cpp`

```cpp
UNIT_TEST("DiscreteMapSequence") {

CASE("default_values") {
    DiscreteMapSequence seq;
    expectEqual(static_cast<int>(seq.clockSource()),
                static_cast<int>(DiscreteMapSequence::ClockSource::Internal),
                "default clock source");
    expectEqual(seq.divisor(), 4, "default divisor");
    expectTrue(seq.loop(), "default loop enabled");
    expectEqual(static_cast<int>(seq.thresholdMode()),
                static_cast<int>(DiscreteMapSequence::ThresholdMode::Position),
                "default threshold mode");
}

CASE("stage_bipolar_threshold") {
    DiscreteMapSequence seq;
    auto &stage = seq.stage(0);

    stage.setThreshold(-100);
    expectEqual(stage.threshold(), -100, "negative threshold");

    stage.setThreshold(100);
    expectEqual(stage.threshold(), 100, "positive threshold");

    stage.setThreshold(200);  // Out of range
    expectEqual(stage.threshold(), 128, "clamped to max");

    stage.setThreshold(-200);
    expectEqual(stage.threshold(), -127, "clamped to min");
}

CASE("stage_bipolar_note_index") {
    DiscreteMapSequence seq;
    auto &stage = seq.stage(0);

    stage.setNoteIndex(-50);
    expectEqual(stage.noteIndex(), -50, "negative note index");

    stage.setNoteIndex(50);
    expectEqual(stage.noteIndex(), 50, "positive note index");

    stage.setNoteIndex(100);
    expectEqual(stage.noteIndex(), 64, "clamped to max");

    stage.setNoteIndex(-100);
    expectEqual(stage.noteIndex(), -63, "clamped to min");
}

CASE("toggle_methods") {
    DiscreteMapSequence seq;

    seq.toggleClockSource();
    expectEqual(static_cast<int>(seq.clockSource()),
                static_cast<int>(DiscreteMapSequence::ClockSource::External),
                "toggled to external");

    seq.toggleThresholdMode();
    expectEqual(static_cast<int>(seq.thresholdMode()),
                static_cast<int>(DiscreteMapSequence::ThresholdMode::Length),
                "toggled to length");

    seq.toggleLoop();
    expectFalse(seq.loop(), "toggled loop off");
}

} // UNIT_TEST
```

**File:** `src/tests/unit/sequencer/TestDiscreteMapTrackEngine.cpp`

Test engine tick logic, threshold detection, CV output calculation.

### 5.2 Integration Tests

1. Create sequence with stages
2. Run engine ticks, verify stage transitions
3. Verify CV voltages match scale mapping
4. Test Position vs Length threshold modes
5. Test bipolar threshold values
6. Test scale integration (Project vs Track scale)

### 5.3 Simulator Tests

Test in simulator before hardware:
1. UI responsiveness
2. FN button toggle behavior
3. Stage editing workflow
4. Routing integration

### 5.4 Hardware Tests

1. CV output accuracy (±5V range)
2. Gate timing
3. External clock sync
4. Save/load persistence
5. Display refresh rate (noise considerations)

---

## File Summary

| Phase | File | Action | Description |
|-------|------|--------|-------------|
| 1.1 | `model/DiscreteMapSequence.h` | CREATE | Main data model |
| 1.1 | `model/DiscreteMapSequence.cpp` | CREATE | Implementation |
| 1.2 | `model/Track.h` | MODIFY | Add DiscreteMap mode |
| 1.2 | `model/Track.cpp` | MODIFY | Add accessors |
| 1.3 | `model/ProjectVersion.h` | MODIFY | Version bump |
| 2.1 | `engine/DiscreteMapTrackEngine.h` | CREATE | Engine header |
| 2.1 | `engine/DiscreteMapTrackEngine.cpp` | CREATE | Engine logic |
| 2.2 | `engine/TrackEngine.cpp` | MODIFY | Factory registration |
| 3.1 | `ui/pages/DiscreteMapSequencePage.h` | CREATE | Visual page header |
| 3.1 | `ui/pages/DiscreteMapSequencePage.cpp` | CREATE | Visual page impl |
| 4.1 | `ui/pages/Pages.h` | MODIFY | Register page ID |
| 4.1 | `ui/PageManager.cpp` | MODIFY | Create page |
| 4.3 | `ui/pages/TrackPage.cpp` | MODIFY | Track mode menu |
| 5.1 | `tests/unit/sequencer/TestDiscreteMapSequence.cpp` | CREATE | Unit tests |
| 5.1 | `tests/unit/sequencer/TestDiscreteMapTrackEngine.cpp` | CREATE | Engine tests |
| 5.1 | `tests/unit/CMakeLists.txt` | MODIFY | Register tests |

---

## Key Design Decisions

1. **No range mode** - Voltage interpretation handled by routing system's per-route VoltageRange configuration

2. **Bipolar values** - More intuitive for musical parameters:
   - Threshold -127 to +128 allows symmetric positioning
   - Note index -63 to +64 gives ±5 octave range

3. **Single-page visual UI**:
   - DiscreteMapSequencePage with threshold bar visualization
   - Direct button mapping: 8 bottom buttons = stage directions, 8 top buttons = stage selection
   - All editing happens on main page (no separate edit page needed)

4. **FN button toggles** - Simple 2-state toggles:
   - Press once to toggle state
   - Footer text updates to show current state
   - No multi-state cycling (only 2-state toggles)

5. **Internal vs External clock** - Simplified from original:
   - Internal = always synced to clock divisor
   - External = reads routed CV input (configured in routing page)
   - No "free running" mode that's also synced

6. **Threshold modes**:
   - Position: absolute voltage thresholds
   - Length: proportional distribution (weights based on threshold magnitude)

7. **Scale integration** - Reuses existing scale system for note quantization

---

## Implementation Order

1. **Model layer** (Phase 1) - Can test serialization independently
2. **Engine layer** (Phase 2) - Test in unit tests with mock data
3. **UI page** (Phase 3) - Visual threshold bar page with all editing
4. **Integration** (Phase 4) - Wire everything together
5. **Testing** (Phase 5) - Comprehensive validation

---

