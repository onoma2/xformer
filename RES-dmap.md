# DiscreteMapTrack Implementation Plan

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

        uint8_t threshold;      // 0-255
        TriggerDir direction;
        uint8_t noteIndex;      // 0-127, maps through scale

        // Accessors with dirty flag
    };

    // === Clock ===
    enum class ClockSource : uint8_t { Synced, Free };
    enum class ResetMode : uint8_t { None, Stop, FirstStep };

    ClockSource clockSource() const;
    int divisor() const;            // 1,2,4,8,16,32,64
    bool loop() const;
    ResetMode resetMode() const;

    // === Threshold ===
    enum class ThresholdMode : uint8_t { Position, Length };

    ThresholdMode thresholdMode() const;
    int rangeMode() const;          // 0: ±5V, 1: 0-5V, 2: 0-2.5V

    // === Scale ===
    enum class ScaleSource : uint8_t { Project, Track };

    ScaleSource scaleSource() const;
    int rootNote() const;
    bool slewEnabled() const;

    // === Routing ===
    int cvInputTrack() const;       // -1: none, 0-7: track

    // === Stages ===
    Stage& stage(int index);
    const Stage& stage(int index) const;

    // === Serialization ===
    void write(VersionedSerializedWriter &writer) const;
    void read(VersionedSerializedReader &reader);
    void clear();

private:
    ClockSource _clockSource;
    uint8_t _divisor;
    bool _loop;
    ResetMode _resetMode;

    ThresholdMode _thresholdMode;
    uint8_t _rangeMode;

    ScaleSource _scaleSource;
    int8_t _rootNote;
    bool _slewEnabled;

    int8_t _cvInputTrack;

    Stage _stages[kStageCount];
};
```

**File:** `src/apps/sequencer/model/DiscreteMapSequence.cpp`
- Accessors/mutators
- Serialization read/write
- clear() defaults

---

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
    DiscreteMap,  // ADD
    Last
};

class Track {
    // ...
    DiscreteMapSequence &discreteMapSequence(int pattern);
    const DiscreteMapSequence &discreteMapSequence(int pattern) const;

private:
    // Storage - union or conditional
    std::array<DiscreteMapSequence, CONFIG_PATTERN_COUNT> _discreteMapSequences;
};
```

---

### 1.3 Serialization Version

**File:** `src/apps/sequencer/model/ProjectVersion.h`

```cpp
enum class ProjectVersion : uint32_t {
    // ...existing...
    Version_DiscreteMap = XX,
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
    DiscreteMapTrackEngine(Engine &engine, int trackIndex);

    void reset() override;
    void restart() override;
    void tick(uint32_t tick) override;
    void update(float dt) override;

    // Outputs
    float cvOutput() const { return _cvOutput; }
    bool gateOutput() const { return _activeStage >= 0; }
    int activeStage() const { return _activeStage; }
    float currentInput() const { return _currentInput; }

    // For UI
    float rampPhase() const { return _rampPhase; }

private:
    // References
    const DiscreteMapSequence *_sequence;

    // === Ramp State ===
    float _rampPhase;           // 0.0 - 1.0
    float _rampValue;           // Voltage
    uint32_t _periodTicks;
    bool _running;

    // === Input ===
    float _currentInput;
    float _prevInput;

    // === Threshold (Length mode cache) ===
    float _lengthThresholds[DiscreteMapSequence::kStageCount];
    bool _thresholdsDirty;

    // === Stage State ===
    int _activeStage;           // -1 = none

    // === Output ===
    float _cvOutput;
    float _slewedOutput;

    // === Methods ===
    void updateRamp(uint32_t tick);
    float getRoutedInput();
    float getThresholdVoltage(int stage);
    void recalculateLengthThresholds();
    int findActiveStage(float input, float prev);
    float noteIndexToVoltage(int noteIndex);
};
```

**File:** `src/apps/sequencer/engine/DiscreteMapTrackEngine.cpp`

```cpp
void DiscreteMapTrackEngine::reset() {
    _rampPhase = 0.0f;
    _rampValue = rangeMin();
    _prevInput = rangeMin() - 0.001f;  // Ensure first RISE triggers
    _activeStage = -1;
    _cvOutput = 0.0f;
    _running = true;
    _thresholdsDirty = true;
}

void DiscreteMapTrackEngine::tick(uint32_t tick) {
    // 1. Update input source
    if (_sequence->clockSource() == ClockSource::Synced) {
        updateRamp(tick);
        _currentInput = _rampValue;
    } else {
        _currentInput = getRoutedInput();
    }

    // 2. Recalc length thresholds if needed
    if (_thresholdsDirty && _sequence->thresholdMode() == ThresholdMode::Length) {
        recalculateLengthThresholds();
        _thresholdsDirty = false;
    }

    // 3. Find stage from threshold crossings
    int newStage = findActiveStage(_currentInput, _prevInput);
    _activeStage = newStage;

    // 4. Update CV output
    if (_activeStage >= 0) {
        float target = noteIndexToVoltage(_sequence->stage(_activeStage).noteIndex);
        if (_sequence->slewEnabled()) {
            // Simple slew
            _cvOutput += (_target - _cvOutput) * _slewRate;
        } else {
            _cvOutput = target;
        }
    } else {
        _cvOutput = 0.0f;
    }

    _prevInput = _currentInput;
}

void DiscreteMapTrackEngine::updateRamp(uint32_t tick) {
    uint32_t periodTicks = _engine.clockTicksPerBeat() * _sequence->divisor();
    uint32_t posInPeriod = tick % periodTicks;

    _rampPhase = float(posInPeriod) / float(periodTicks);

    float min = rangeMin();
    float max = rangeMax();
    _rampValue = min + _rampPhase * (max - min);

    // Handle loop/once
    if (!_sequence->loop() && _rampPhase >= 1.0f) {
        _running = false;
        _rampValue = max;
    }
}

int DiscreteMapTrackEngine::findActiveStage(float input, float prev) {
    for (int i = 0; i < DiscreteMapSequence::kStageCount; i++) {
        const auto& stage = _sequence->stage(i);
        if (stage.direction == TriggerDir::Off) continue;

        float thresh = getThresholdVoltage(i);
        bool crossed = false;

        if (stage.direction == TriggerDir::Rise) {
            crossed = (prev < thresh && input >= thresh);
        } else {
            crossed = (prev > thresh && input <= thresh);
        }

        if (crossed) return i;
    }

    return _activeStage;  // No change
}

void DiscreteMapTrackEngine::recalculateLengthThresholds() {
    float totalWeight = 0;
    for (int i = 0; i < DiscreteMapSequence::kStageCount; i++) {
        totalWeight += _sequence->stage(i).threshold;
    }
    if (totalWeight == 0) totalWeight = 1;

    float range = rangeMax() - rangeMin();
    float cumulative = rangeMin();

    for (int i = 0; i < DiscreteMapSequence::kStageCount; i++) {
        float proportion = _sequence->stage(i).threshold / totalWeight;
        cumulative += proportion * range;
        _lengthThresholds[i] = cumulative;
    }
}

float DiscreteMapTrackEngine::noteIndexToVoltage(int noteIndex) {
    const Scale& scale = (_sequence->scaleSource() == ScaleSource::Project)
        ? _engine.project().scale()
        : _track.scale();

    int root = _sequence->rootNote();
    int note = scale.noteAt(noteIndex % scale.size());
    int octave = noteIndex / scale.size();

    return (root + note + octave * 12) / 12.0f;
}
```

---

### 2.2 Register Engine in Factory

**File:** `src/apps/sequencer/engine/TrackEngine.cpp`

```cpp
std::unique_ptr<TrackEngine> TrackEngine::create(Track &track, Engine &engine) {
    switch (track.trackMode()) {
        // ...existing...
        case TrackMode::DiscreteMap:
            return std::make_unique<DiscreteMapTrackEngine>(engine, track.trackIndex());
    }
}
```

---

### 2.3 CV/Gate Output Integration

**File:** `src/apps/sequencer/engine/Engine.cpp`

Ensure `DiscreteMapTrackEngine` outputs are routed to CV/Gate outputs same as other track types.

---

## Phase 3: UI

### 3.1 Create DiscreteMapSequencePage

**File:** `src/apps/sequencer/ui/pages/DiscreteMapSequencePage.h`

```cpp
class DiscreteMapSequencePage : public BasePage {
public:
    DiscreteMapSequencePage(PageManager &pageManager, PageContext &context);

    void enter() override;
    void exit() override;
    void draw(Canvas &canvas) override;
    void updateLeds(Leds &leds) override;

    void onKeyDown(KeyEvent &event) override;
    void onKeyUp(KeyEvent &event) override;
    void onEncoder(EncoderEvent &event) override;

private:
    enum class EditMode { None, Threshold, NoteValue, DualThreshold };
    enum class Footer { None, Clock, Threshold, Scale };
    enum class FooterEncoder { None, Divisor, RootNote };

    // State
    EditMode _editMode;
    int8_t _selectedStage;
    int8_t _secondaryStage;
    Footer _activeFooter;
    FooterEncoder _footerEncoderMode;

    // References
    DiscreteMapSequence *_sequence;
    DiscreteMapTrackEngine *_engine;

    // Drawing
    void drawHeader(Canvas &canvas);
    void drawThresholdBar(Canvas &canvas);
    void drawStageInfo(Canvas &canvas);
    void drawFooter(Canvas &canvas);

    // Footer drawing
    void drawClockFooter(Canvas &canvas);
    void drawThresholdFooter(Canvas &canvas);
    void drawScaleFooter(Canvas &canvas);

    // Handlers
    void handleTopRowKey(int key, bool shift);
    void handleBottomRowKey(int key);
    void handleFooterButton(int btn);
    void handleEncoder(int delta, bool shift);
};
```

**File:** `src/apps/sequencer/ui/pages/DiscreteMapSequencePage.cpp`

Key implementations:

```cpp
void DiscreteMapSequencePage::draw(Canvas &canvas) {
    drawHeader(canvas);
    drawThresholdBar(canvas);
    drawStageInfo(canvas);

    if (_activeFooter != Footer::None) {
        drawFooter(canvas);
    }
}

void DiscreteMapSequencePage::drawThresholdBar(Canvas &canvas) {
    const int barX = 8;
    const int barY = 20;
    const int barW = 240;
    const int barH = 12;

    // Background
    canvas.setColor(Color::Medium);
    canvas.fillRect(barX, barY, barW, barH);

    // Thresholds
    for (int i = 0; i < 8; i++) {
        const auto& stage = _sequence->stage(i);
        if (stage.direction == Stage::TriggerDir::Off) continue;

        float norm = getThresholdNormalized(i);
        int x = barX + int(norm * barW);

        bool selected = (i == _selectedStage);
        bool active = (i == _engine->activeStage());

        canvas.setColor(active ? Color::Bright : (selected ? Color::Medium : Color::Low));
        canvas.drawVLine(x, barY, barH);

        // Direction indicator
        char dir = (stage.direction == Stage::TriggerDir::Rise) ? '^' : 'v';
        canvas.drawChar(x - 2, barY + barH + 2, dir);
    }

    // Input cursor
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

        // Stage number
        canvas.drawText(x, y, std::to_string(i + 1));

        // Note name (or -- if off)
        if (stage.direction != Stage::TriggerDir::Off) {
            canvas.drawText(x, y + 10, noteToName(stage.noteIndex));
        } else {
            canvas.drawText(x, y + 10, "--");
        }
    }
}

void DiscreteMapSequencePage::onKeyDown(KeyEvent &event) {
    int key = event.key();

    // FN buttons
    if (key == Key::Fn1) { _activeFooter = Footer::Clock; return; }
    if (key == Key::Fn2) { _activeFooter = Footer::Threshold; return; }
    if (key == Key::Fn3) { _activeFooter = Footer::Scale; return; }

    // Footer mode: top row = footer buttons
    if (_activeFooter != Footer::None && isTopRowKey(key)) {
        handleFooterButton(keyToIndex(key));
        return;
    }

    // Normal mode
    if (isBottomRowKey(key)) {
        handleBottomRowKey(keyToIndex(key));
        return;
    }

    if (isTopRowKey(key)) {
        handleTopRowKey(keyToIndex(key), _shiftHeld);
        return;
    }
}

void DiscreteMapSequencePage::handleBottomRowKey(int idx) {
    auto& stage = _sequence->stage(idx);

    // Cycle: Rise -> Fall -> Off -> Rise
    switch (stage.direction) {
        case TriggerDir::Rise: stage.setDirection(TriggerDir::Fall); break;
        case TriggerDir::Fall: stage.setDirection(TriggerDir::Off); break;
        case TriggerDir::Off:  stage.setDirection(TriggerDir::Rise); break;
    }
}

void DiscreteMapSequencePage::handleTopRowKey(int idx, bool shift) {
    if (shift) {
        _editMode = EditMode::NoteValue;
    } else if (_selectedStage >= 0 && _selectedStage != idx) {
        _secondaryStage = idx;
        _editMode = EditMode::DualThreshold;
    } else {
        _editMode = EditMode::Threshold;
    }
    _selectedStage = idx;
}

void DiscreteMapSequencePage::handleFooterButton(int btn) {
    switch (_activeFooter) {
        case Footer::Clock:
            switch (btn) {
                case 0: _sequence->toggleClockSource(); break;
                case 1: _footerEncoderMode = FooterEncoder::Divisor; break;
                case 2: _sequence->toggleLoop(); break;
                case 3: _sequence->cycleResetMode(); break;
            }
            break;

        case Footer::Threshold:
            switch (btn) {
                case 0: _sequence->setThresholdMode(ThresholdMode::Position); break;
                case 1: _sequence->setThresholdMode(ThresholdMode::Length); break;
                case 2: _sequence->setRangeMode(0); break;
                case 3: _sequence->setRangeMode(1); break;
                case 4: _sequence->setRangeMode(2); break;
            }
            break;

        case Footer::Scale:
            switch (btn) {
                case 0: _sequence->setScaleSource(ScaleSource::Project); break;
                case 1: _sequence->setScaleSource(ScaleSource::Track); break;
                case 2: _footerEncoderMode = FooterEncoder::RootNote; break;
                case 3: _sequence->toggleSlew(); break;
            }
            break;
    }
}

void DiscreteMapSequencePage::onEncoder(EncoderEvent &event) {
    int delta = event.value();

    // Footer encoder modes
    if (_footerEncoderMode == FooterEncoder::Divisor) {
        _sequence->setDivisor(cycleDivisor(_sequence->divisor(), delta));
        return;
    }
    if (_footerEncoderMode == FooterEncoder::RootNote) {
        _sequence->setRootNote(clamp(_sequence->rootNote() + delta, 0, 11));
        return;
    }

    // Stage editing
    switch (_editMode) {
        case EditMode::Threshold: {
            auto& s = _sequence->stage(_selectedStage);
            int step = _shiftHeld ? 1 : 8;
            s.setThreshold(clamp(s.threshold() + delta * step, 0, 255));
            break;
        }

        case EditMode::NoteValue: {
            auto& s = _sequence->stage(_selectedStage);
            int step = _shiftHeld ? 12 : 1;
            s.setNoteIndex(clamp(s.noteIndex() + delta * step, 0, 127));
            break;
        }

        case EditMode::DualThreshold: {
            auto& s1 = _sequence->stage(_selectedStage);
            auto& s2 = _sequence->stage(_secondaryStage);
            int step = _shiftHeld ? 1 : 8;
            s1.setThreshold(clamp(s1.threshold() + delta * step, 0, 255));
            s2.setThreshold(clamp(s2.threshold() + delta * step, 0, 255));
            break;
        }

        default:
            break;
    }
}

void DiscreteMapSequencePage::updateLeds(Leds &leds) {
    if (_activeFooter != Footer::None) {
        updateFooterLeds(leds);
        return;
    }

    // Bottom row: direction state
    for (int i = 0; i < 8; i++) {
        auto dir = _sequence->stage(i).direction;
        switch (dir) {
            case TriggerDir::Rise: leds.set(bottomLed(i), true); break;
            case TriggerDir::Fall: leds.set(bottomLed(i), 0.3f); break;
            case TriggerDir::Off:  leds.set(bottomLed(i), false); break;
        }
    }

    // Top row: selection + active
    for (int i = 0; i < 8; i++) {
        bool selected = (i == _selectedStage);
        bool active = (i == _engine->activeStage());

        if (selected) {
            leds.set(topLed(i), true);
        } else if (active) {
            leds.set(topLed(i), 0.5f);
        } else {
            leds.set(topLed(i), false);
        }
    }
}
```

---

### 3.2 Register Page

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
    _pages[PageId::DiscreteMapSequence] = std::make_unique<DiscreteMapSequencePage>(*this, _context);
}

PageId PageManager::pageForTrackMode(TrackMode mode) {
    switch (mode) {
        // ...existing...
        case TrackMode::DiscreteMap: return PageId::DiscreteMapSequence;
    }
}
```

---

## Phase 4: Integration

### 4.1 CV Output Routing

Ensure `DiscreteMapTrackEngine::cvOutput()` feeds into existing CV output system.

**File:** `src/apps/sequencer/engine/CvOutput.cpp`

Should already work if `TrackEngine::cvOutput()` is virtual and overridden.

---

### 4.2 Gate Output

Gate = `_activeStage >= 0`. Wire to existing gate output system.

---

### 4.3 Track Mode Selection UI

Ensure DiscreteMap appears in track mode selection menu.

**File:** `src/apps/sequencer/ui/pages/TrackPage.cpp` (or equivalent)

---

## Phase 5: Testing

### 5.1 Simulator Tests

1. Create DiscreteMapSequence, set stages
2. Run engine ticks, verify stage transitions
3. Verify CV output voltages
4. Test Position vs Length mode
5. Test all range modes
6. Test scale integration

### 5.2 Hardware Tests

1. CV output accuracy
2. Gate timing
3. Sync to external clock
4. UI responsiveness
5. Save/load persistence

---

## File Summary

| Phase | File | Action |
|-------|------|--------|
| 1.1 | `model/DiscreteMapSequence.h` | CREATE |
| 1.1 | `model/DiscreteMapSequence.cpp` | CREATE |
| 1.2 | `model/Track.h` | MODIFY |
| 1.2 | `model/Track.cpp` | MODIFY |
| 1.3 | `model/ProjectVersion.h` | MODIFY |
| 2.1 | `engine/DiscreteMapTrackEngine.h` | CREATE |
| 2.1 | `engine/DiscreteMapTrackEngine.cpp` | CREATE |
| 2.2 | `engine/TrackEngine.cpp` | MODIFY |
| 2.3 | `engine/Engine.cpp` | MODIFY (if needed) |
| 3.1 | `ui/pages/DiscreteMapSequencePage.h` | CREATE |
| 3.1 | `ui/pages/DiscreteMapSequencePage.cpp` | CREATE |
| 3.2 | `ui/pages/Pages.h` | MODIFY |
| 3.2 | `ui/PageManager.cpp` | MODIFY |
| 4.3 | `ui/pages/TrackPage.cpp` | MODIFY |

---

## Estimated Timeline

| Phase | Duration | Dependencies |
|-------|----------|--------------|
| Phase 1: Data Model | 2-3 days | None |
| Phase 2: Engine | 3-4 days | Phase 1 |
| Phase 3: UI | 3-4 days | Phase 1, 2 |
| Phase 4: Integration | 1-2 days | Phase 2, 3 |
| Phase 5: Testing | 2-3 days | All |
| **Total** | **11-16 days** | |

---

Ready to start implementation?
## Revised UI Specification

### Header (stolen from NoteTrackPage)

```
┌──────────────────────────────────────────────────┐
│ TR1  DMAP  ●RUN   ▶1/4   Root:C   Scale:Minor   │
└──────────────────────────────────────────────────┘
```

Matches existing track header pattern:
- Track number
- Track type
- Play state
- Clock division indicator
- Scale info (from routing system)

---

### Footer (FN button pages)

**FN1: Clock**
```
┌──────────────────────────────────────────────────┐
│ Src:SYNC   Div:4   Loop:ON   Reset:STOP         │
└──────────────────────────────────────────────────┘
```

**FN2: Threshold**
```
┌──────────────────────────────────────────────────┐
│ Mode:POS   Range:±5V   Above:--   Below:--      │
└──────────────────────────────────────────────────┘
```

**FN3: Scale/Output**
```
┌──────────────────────────────────────────────────┐
│ Scale:PRJ   Root:C   OutRange:0-5V   Slew:OFF   │
└──────────────────────────────────────────────────┘
```

**FN4: Routing** (reuse existing)
```
┌──────────────────────────────────────────────────┐
│ CvIn:TR2   Gate:--   Fill:--   Mute:--          │
└──────────────────────────────────────────────────┘
```

---

### Main Display (no header/footer visible)

```
┌──────────────────────────────────────────────────┐
│ TR1  DMAP  ●RUN   ▶1/4   Root:C   Scale:Minor   │
├──────────────────────────────────────────────────┤
│                                                  │
│ Thr: ░░░▓▓▓░░░░░░▓▓░░░░▓▓▓▓░░░░░▓░░░░░▓▓░░░░▓▓▓ │
│      1   2   3   4   5   6   7   8   ▼          │
│                                    cursor        │
│ f(X) C4  D4  E4  F4  G4  --  B4  C5              │
│  Dir  ▲   ▲   ▼   ▲   ▲   -   ▼   ▲              │
│                                                  │
├──────────────────────────────────────────────────┤
│                 [FN pressed footer]              │
└──────────────────────────────────────────────────┘
```

---

### FN Button Handling

```cpp
void DiscreteMapSequencePage::onKeyDown(KeyEvent &event) {
    if (isFnKey(event.key())) {
        _activeFooter = fnKeyToFooter(event.key());
        _footerCursor = 0;
        return;
    }

    // ... rest of button handling
}

void DiscreteMapSequencePage::onKeyUp(KeyEvent &event) {
    if (isFnKey(event.key())) {
        _activeFooter = Footer::None;
        return;
    }

    // ... rest of button handling
}

void DiscreteMapSequencePage::onEncoder(EncoderEvent &event) {
    if (_activeFooter != Footer::None) {
        // Footer param editing
        if (_shiftHeld) {
            adjustFooterParam(_activeFooter, _footerCursor, event.value());
        } else {
            _footerCursor = clamp(_footerCursor + event.value(), 0, footerParamCount(_activeFooter) - 1);
        }
        return;
    }

    // ... stage editing as before
}
```

---

### Footer Parameter Mapping

```cpp
enum class Footer : uint8_t {
    None,
    Clock,      // FN1
    Threshold,  // FN2
    Scale,      // FN3
    Routing     // FN4
};

struct FooterParams {
    static constexpr int ClockParams = 4;      // Src, Div, Loop, Reset
    static constexpr int ThresholdParams = 4;  // Mode, Range, Above, Below
    static constexpr int ScaleParams = 4;      // Scale, Root, OutRange, Slew
    static constexpr int RoutingParams = 4;    // CvIn, Gate, Fill, Mute
};
```

---

### Integration with Existing Page Structure

Steal from `NoteSequencePage`:

```cpp
class DiscreteMapSequencePage : public BasePage {
    // Header drawing - reuse existing
    void drawHeader(Canvas &canvas) override {
        // Same as NoteSequencePage::drawHeader()
    }

    // Footer drawing - custom per FN
    void drawFooter(Canvas &canvas) override {
        switch (_activeFooter) {
            case Footer::None: break;
            case Footer::Clock: drawClockFooter(canvas); break;
            case Footer::Threshold: drawThresholdFooter(canvas); break;
            case Footer::Scale: drawScaleFooter(canvas); break;
            case Footer::Routing: drawRoutingFooter(canvas); break;
        }
    }
};
```

---

Updated. Proceed to serialization or code?
## Revised Footer

Footer shows when FN held. Each footer param mapped to a step button (not encoder navigation).

---

### Footer Layout

**FN1: Clock**
```
┌──────────────────────────────────────────────────┐
│  [SYNC]   [DIV]   [LOOP]   [RST]                │
│    1        2       3        4                   │
└──────────────────────────────────────────────────┘
```

**FN2: Threshold**
```
┌──────────────────────────────────────────────────┐
│  [POS]   [LEN]   [±5V]   [0-5]   [0-2.5]        │
│    1       2       3       4        5            │
└──────────────────────────────────────────────────┘
```

**FN3: Scale**
```
┌──────────────────────────────────────────────────┐
│  [PRJ]   [TRK]   [ROOT]   [SLEW]                │
│    1       2        3        4                   │
└──────────────────────────────────────────────────┘
```

---

### Button Behavior

| Button | FN1 (Clock) | FN2 (Threshold) | FN3 (Scale) |
|--------|-------------|-----------------|-------------|
| 1 | Toggle SYNC/FREE | Select POS mode | Use project scale |
| 2 | Cycle divisor | Select LEN mode | Use track scale |
| 3 | Toggle LOOP | Range ±5V | Encoder = root note |
| 4 | Cycle reset mode | Range 0-5V | Toggle slew |
| 5 | -- | Range 0-2.5V | -- |

---

### Implementation

```cpp
void DiscreteMapSequencePage::onKeyDown(KeyEvent &event) {
    int key = event.key();

    if (_activeFooter != Footer::None && isTopRowKey(key)) {
        int btnIdx = keyToIndex(key);
        handleFooterButton(_activeFooter, btnIdx);
        return;
    }

    // ... normal button handling
}

void DiscreteMapSequencePage::handleFooterButton(Footer footer, int btn) {
    switch (footer) {
        case Footer::Clock:
            switch (btn) {
                case 0: _sequence.toggleClockSource(); break;
                case 1: _sequence.cycleDivisor(); break;
                case 2: _sequence.toggleLoop(); break;
                case 3: _sequence.cycleResetMode(); break;
            }
            break;

        case Footer::Threshold:
            switch (btn) {
                case 0: _sequence.setThresholdMode(ThresholdMode::Position); break;
                case 1: _sequence.setThresholdMode(ThresholdMode::Length); break;
                case 2: _sequence.setRangeMode(0); break;  // ±5V
                case 3: _sequence.setRangeMode(1); break;  // 0-5V
                case 4: _sequence.setRangeMode(2); break;  // 0-2.5V
            }
            break;

        case Footer::Scale:
            switch (btn) {
                case 0: _sequence.setScaleSource(ScaleSource::Project); break;
                case 1: _sequence.setScaleSource(ScaleSource::Track); break;
                case 2: _footerEncoderMode = FooterEncoder::RootNote; break;
                case 3: _sequence.toggleSlew(); break;
            }
            break;
    }
}
```

---

### LED Feedback (FN held)

```cpp
void DiscreteMapSequencePage::updateFooterLeds() {
    clearTopRowLeds();

    switch (_activeFooter) {
        case Footer::Clock:
            setLed(0, _sequence.clockSource() == ClockSource::Synced);
            setLed(1, LedDim);  // Always available
            setLed(2, _sequence.loop());
            setLed(3, LedDim);
            break;

        case Footer::Threshold:
            setLed(0, _sequence.thresholdMode() == ThresholdMode::Position);
            setLed(1, _sequence.thresholdMode() == ThresholdMode::Length);
            setLed(2, _sequence.rangeMode() == 0);
            setLed(3, _sequence.rangeMode() == 1);
            setLed(4, _sequence.rangeMode() == 2);
            break;

        case Footer::Scale:
            setLed(0, _sequence.scaleSource() == ScaleSource::Project);
            setLed(1, _sequence.scaleSource() == ScaleSource::Track);
            setLed(2, _footerEncoderMode == FooterEncoder::RootNote);
            setLed(3, _sequence.slewEnabled());
            break;
    }
}
```

---

Simpler. Ready for serialization or code?
