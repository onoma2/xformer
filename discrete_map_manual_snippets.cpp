/*
 * DISCRETE MAP TRACK - Code Snippets for Manual
 *
 * This file contains key code snippets that explain the DiscreteMap track implementation
 * in the PEW|FORMER sequencer firmware.
 */

// 1. DISCRETE MAP SEQUENCE MODEL
// ==============================

// The DiscreteMapSequence class holds all the sequence data
class DiscreteMapSequence {
public:
    static constexpr int StageCount = 32;  // Maximum number of stages

    // Each stage has a threshold, direction, and note index
    class Stage {
    public:
        enum class TriggerDir : uint8_t {
            Rise,   // Trigger on rising edge
            Fall,   // Trigger on falling edge  
            Off,    // No triggering
            Both    // Trigger on both edges
        };

        // Threshold (-100 to +100) - determines voltage crossing point
        int8_t threshold() const { return _threshold; }
        void setThreshold(int threshold) {
            _threshold = clamp(threshold, -100, 100);
        }

        // Direction - determines trigger behavior
        TriggerDir direction() const { return _direction; }
        void setDirection(TriggerDir dir) {
            _direction = dir;
        }
        void cycleDirection() {
            _direction = advanceDirection(_direction, 1);
        }

        // Note index (-63 to +64) - determines output voltage
        int8_t noteIndex() const { return _noteIndex; }
        void setNoteIndex(int8_t index) {
            _noteIndex = clamp(index, int8_t(-63), int8_t(64));
        }

    private:
        int8_t _threshold = 0;           // -100 to +100
        TriggerDir _direction = TriggerDir::Off;  // Trigger direction
        int8_t _noteIndex = 0;           // -63 to +64
    };

    // Clock source determines input for threshold detection
    enum class ClockSource : uint8_t {
        Internal,           // Sawtooth ramp
        InternalTriangle,   // Triangle ramp
        External            // Routed CV input
    };

    // Threshold mode affects how threshold values are interpreted
    enum class ThresholdMode : uint8_t {
        Position,   // Absolute voltage positions
        Length      // Proportional distribution
    };

    // Range parameters define the voltage window for threshold mapping
    float rangeHigh() const { return _rangeHigh; }
    void setRangeHigh(float v) {
        _rangeHigh = clamp(v, -5.0f, 5.0f);
    }

    float rangeLow() const { return _rangeLow; }
    void setRangeLow(float v) {
        _rangeLow = clamp(v, -5.0f, 5.0f);
    }

    // Gate length determines duration of gate output
    int gateLength() const { return _gateLength; }
    void setGateLength(int length) {
        _gateLength = clamp(length, 0, 100);
    }

    // Divisor affects timing of internal clock
    int divisor() const { return _divisor; }
    void setDivisor(int div) {
        _divisor = clamp(div, 1, 768);
    }

    // Loop mode
    bool loop() const { return _loop; }
    void setLoop(bool loop) {
        _loop = loop;
    }

    // Scale and pitch parameters
    int scale() const { return _scale; }
    void setScale(int scale) {
        _scale = clamp(scale, -1, Scale::Count - 1);
    }

    int rootNote() const { return _rootNote; }
    void setRootNote(int root) {
        _rootNote = clamp(root, 0, 11);
    }

    int octave() const { return _octave; }
    void setOctave(int octave) {
        _octave = clamp(octave, -10, 10);
    }

    int transpose() const { return _transpose; }
    void setTranspose(int transpose) {
        _transpose = clamp(transpose, -60, 60);
    }

    int offset() const { return _offset; }
    void setOffset(int offset) {
        _offset = clamp(offset, -500, 500);  // -5.00V to +5.00V
    }

    // Slew time for smooth CV transitions
    int slewTime() const { return _slewTime.get(isRouted(Routing::Target::SlideTime)); }
    void setSlewTime(int time, bool routed = false) {
        _slewTime.set(clamp(time, 0, 100), routed);
    }

    // Access to individual stages
    Stage& stage(int index) {
        return _stages[clamp(index, 0, StageCount - 1)];
    }
    const Stage& stage(int index) const {
        return _stages[clamp(index, 0, StageCount - 1)];
    }

private:
    ClockSource _clockSource = ClockSource::Internal;
    uint16_t _divisor = 192;
    uint8_t _gateLength = 0;     // 0 = 1T pulse
    bool _loop = true;

    ThresholdMode _thresholdMode = ThresholdMode::Position;

    int8_t _scale = -1;
    int8_t _rootNote = 0;       // C
    Routable<uint8_t> _slewTime;
    int8_t _octave = 0;
    int8_t _transpose = 0;
    int16_t _offset = 0;

    float _rangeHigh = 5.0f;    // Default +5V
    float _rangeLow = -5.0f;    // Default -5V

    std::array<Stage, StageCount> _stages;
};

// 2. DISCRETE MAP TRACK ENGINE LOGIC
// ==================================

// The DiscreteMapTrackEngine processes the sequence and generates outputs
class DiscreteMapTrackEngine : public TrackEngine {
public:
    virtual TickResult tick(uint32_t tick) override {
        _sequence = &_discreteMapTrack.sequence(pattern());

        // 1. Update input source
        if (_sequence->clockSource() != DiscreteMapSequence::ClockSource::External) {
            // Internal clock sources: sawtooth or triangle wave
            switch (_discreteMapTrack.playMode()) {
            case Types::PlayMode::Aligned: {
                // Align ramp phase to bar position while preserving divisor-based period
                uint32_t periodTicks = _sequence->divisor() * (CONFIG_PPQN / CONFIG_SEQUENCE_PPQN);
                if (periodTicks == 0) periodTicks = 1;

                uint32_t resetDivisor = _sequence->resetMeasure() * _engine.measureDivisor();
                uint32_t alignTick = (resetDivisor > 0) ? (relativeTick % resetDivisor) : relativeTick;

                if (_running || _sequence->loop()) {
                    uint32_t posInPeriod = alignTick % periodTicks;
                    _rampPhase = static_cast<float>(posInPeriod) / periodTicks;

                    // Apply triangle wave if InternalTriangle mode
                    if (_sequence->clockSource() == DiscreteMapSequence::ClockSource::InternalTriangle) {
                        float triPhase = (_rampPhase < 0.5f) ? (_rampPhase * 2.0f) : (1.0f - (_rampPhase - 0.5f) * 2.0f);
                        _rampValue = kInternalRampMin + triPhase * (kInternalRampMax - kInternalRampMin);
                    } else {
                        _rampValue = kInternalRampMin + _rampPhase * (kInternalRampMax - kInternalRampMin);
                    }
                }
                break;
            }
            case Types::PlayMode::Free:
                if (_running || _sequence->loop()) {
                    updateRamp(relativeTick);
                }
                break;
            case Types::PlayMode::Last:
                break;
            }
            _currentInput = _rampValue;
        } else {
            // External mode: use routed input
            _currentInput = getRoutedInput();
        }

        // 2. Recalculate thresholds if needed
        if (_thresholdsDirty) {
            if (_sequence->thresholdMode() == DiscreteMapSequence::ThresholdMode::Length) {
                recalculateLengthThresholds();
            } else {
                recalculatePositionThresholds();
            }
            _thresholdsDirty = false;
        }

        // 3. Find active stage from threshold crossings
        int newStage = findActiveStage(_currentInput, _prevInput);

        // 4. Update outputs based on active stage
        if (newStage != _activeStage && newStage >= 0) {
            // Trigger gate
            uint32_t stepTicks = _sequence->divisor() * (CONFIG_PPQN / CONFIG_SEQUENCE_PPQN);
            if (stepTicks == 0) stepTicks = 1;
            int gateLengthPercent = _sequence->gateLength();
            if (gateLengthPercent == 0) {
                _gateTimer = 3; // Explicit 1-tick pulse
            } else {
                _gateTimer = (stepTicks * gateLengthPercent) / 100;
            }

            // Update CV target
            _targetCv = noteIndexToVoltage(_sequence->stage(newStage).noteIndex());
            _targetCv += _sequence->offset() * 0.01f;
        }

        _activeStage = newStage;
        _prevInput = _currentInput;

        // Apply slew to CV output
        int slewTime = _sequence->slewTime();
        if (slewTime > 0) {
            _cvOutput = Slide::applySlide(_cvOutput, _targetCv, slewTime, dt);
        } else {
            _cvOutput = _targetCv;
        }

        TickResult result = TickResult::NoUpdate;
        bool currentGate = _gateTimer > 0 && _activeStage >= 0;
        if (currentGate != prevGate) {
            result |= TickResult::GateUpdate;
        }
        if (gateChanged || std::abs(_cvOutput - prevCv) > 1e-6f) {
            result |= TickResult::CvUpdate;
        }

        return result;
    }

    // Find which stage is currently active based on threshold crossings
    int findActiveStage(float input, float prevInput) {
        // Scan stages in order, first crossing wins
        for (int i = 0; i < DiscreteMapSequence::StageCount; i++) {
            const auto &stage = _sequence->stage(i);

            float thresh = getThresholdVoltage(i);
            bool crossed = false;

            switch (stage.direction()) {
            case DiscreteMapSequence::Stage::TriggerDir::Rise:
                // Rising edge: previous was below, current is at or above
                crossed = (prevInput < thresh && input >= thresh);
                break;
            case DiscreteMapSequence::Stage::TriggerDir::Fall:
                // Falling edge: previous was above, current is at or below
                crossed = (prevInput > thresh && input <= thresh);
                break;
            case DiscreteMapSequence::Stage::TriggerDir::Both:
                crossed = (prevInput < thresh && input >= thresh) ||
                          (prevInput > thresh && input <= thresh);
                break;
            case DiscreteMapSequence::Stage::TriggerDir::Off:
                crossed = false;
                break;
            }

            if (crossed) {
                return i;
            }
        }

        // No crossing detected
        return _activeStage;
    }

    // Convert note index to voltage using scale
    float noteIndexToVoltage(int8_t noteIndex) {
        const Scale &scale = _sequence->selectedScale(_model.project().selectedScale());

        int octave = _sequence->octave();
        int transpose = _sequence->transpose();
        int rootNote = _sequence->rootNote();
        int shift = octave * scale.notesPerOctave() + transpose;

        // Convert note index to volts using scale. For chromatic scales add rootNote in semitones.
        float volts = scale.noteToVolts(noteIndex + shift);
        if (scale.isChromatic()) {
            volts += rootNote * (1.f / 12.f);
        }
        return volts;
    }

    // Recalculate thresholds in Position mode
    void recalculatePositionThresholds() {
        float minV = rangeMin();
        float spanV = rangeMax() - minV;

        for (int i = 0; i < DiscreteMapSequence::StageCount; i++) {
            const auto &stage = _sequence->stage(i);
            // Position mode: use threshold value directly as voltage
            // Map from -100..+100 to rangeMin..rangeMax
            float normalizedThreshold = (stage.threshold() + 100) / 200.0f;
            _positionThresholds[i] = minV + normalizedThreshold * spanV;
        }
    }

    // Recalculate thresholds in Length mode
    void recalculateLengthThresholds() {
        // Length mode: each threshold value determines the length of the interval
        // from the previous threshold to the current threshold.
        // Map bipolar threshold values [-100, +100] to positive range [0, 200]
        // for length calculation (time unit division)
        float totalWeight = 0;

        for (int i = 0; i < DiscreteMapSequence::StageCount; i++) {
            // Map from [-100, +100] to [0, 200]
            int mappedValue = _sequence->stage(i).threshold() + 100;  // -100 -> 0, 0 -> 100, +100 -> 200
            totalWeight += mappedValue;
        }

        // Handle the special case where all mapped values sum to 0 (all sliders down to -100)
        if (totalWeight == 0) {
            // Distribute evenly across the range
            for (int i = 0; i < DiscreteMapSequence::StageCount; i++) {
                _lengthThresholds[i] = rangeMin() + (float(i + 1) / float(DiscreteMapSequence::StageCount)) * (rangeMax() - rangeMin());
            }
            return;
        }

        // Calculate cumulative threshold positions
        float currentVoltage = rangeMin(); // Start from the bottom (-5V)

        for (int i = 0; i < DiscreteMapSequence::StageCount; i++) {
            // Map threshold value to determine proportional length
            int mappedValue = _sequence->stage(i).threshold() + 100;  // Convert to [0, 200] range
            float stageProportion = float(mappedValue) / totalWeight;
            float stageLength = stageProportion * (rangeMax() - rangeMin());

            // Move to the end of this stage's interval
            currentVoltage += stageLength;

            // Set this stage's threshold voltage (end of its interval)
            _lengthThresholds[i] = currentVoltage;
        }
    }

private:
    static constexpr float kInternalRampMin = -5.0f;
    static constexpr float kInternalRampMax = 5.0f;

    DiscreteMapTrack &_discreteMapTrack;
    DiscreteMapSequence *_sequence = nullptr;

    // Input state
    float _currentInput = 0.0f;
    float _prevInput = 0.0f;
    float _rampPhase = 0.0f;        // 0.0 - 1.0
    float _rampValue = 0.0f;        // Current voltage

    // Threshold cache
    float _lengthThresholds[DiscreteMapSequence::StageCount];
    float _positionThresholds[DiscreteMapSequence::StageCount];
    bool _thresholdsDirty = true;

    // Stage state
    int _activeStage = -1;          // -1 = none

    // Output state
    float _cvOutput = 0.0f;
    float _targetCv = 0.0f;
    uint32_t _gateTimer = 0;
};

// 3. DISCRETE MAP TRACK MODEL
// ===========================

// The DiscreteMapTrack class holds track-level parameters
class DiscreteMapTrack {
public:
    // CvUpdateMode determines when CV output updates
    enum class CvUpdateMode : uint8_t {
        Gate,    // Update CV only when a stage triggers (current behavior)
        Always,  // Update CV continuously regardless of stages (new)
        Last
    };

    CvUpdateMode cvUpdateMode() const { return _cvUpdateMode; }
    void setCvUpdateMode(CvUpdateMode cvUpdateMode) {
        _cvUpdateMode = ModelUtils::clampedEnum(cvUpdateMode);
    }

    // PlayMode affects timing behavior
    Types::PlayMode playMode() const { return _playMode; }
    void setPlayMode(Types::PlayMode playMode) {
        _playMode = ModelUtils::clampedEnum(playMode);
    }

    // Access to sequences
    const DiscreteMapSequenceArray &sequences() const { return _sequences; }
    DiscreteMapSequence &sequence(int index) { return _sequences[index]; }

    // Routed inputs
    float routedInput() const { return _routedInput; }
    float routedScanner() const { return _routedScanner; }
    float routedSync() const { return _routedSync; }

private:
    DiscreteMapSequenceArray _sequences;
    CvUpdateMode _cvUpdateMode = CvUpdateMode::Gate;
    Types::PlayMode _playMode = Types::PlayMode::Aligned;

    // Routed state
    float _routedInput = 0.f;
    float _routedScanner = 0.f;
    float _routedSync = 0.f;
};

// 4. UI IMPLEMENTATION SNIPPETS
// =============================

// UI drawing of threshold bar visualization
void DiscreteMapSequencePage::drawThresholdBar(Canvas &canvas) {
    const int barX = 8;
    const int barY = 12;
    const int barW = 240;
    const int barLineY = barY + 8; // Baseline position

    // Draw thin 2px horizontal baseline (Color::Low)
    canvas.setColor(Color::Low);
    canvas.hline(barX, barLineY, barW);
    canvas.hline(barX, barLineY + 1, barW); // 2px thick

    // Draw threshold markers growing upward from baseline
    for (int i = 0; i < DiscreteMapSequence::StageCount; ++i) {
        const auto &stage = _sequence->stage(i);
        if (stage.direction() == DiscreteMapSequence::Stage::TriggerDir::Off) {
            continue;
        }

        float norm = clamp(getThresholdNormalized(i), 0.f, 1.f);
        int x = barX + int(norm * barW);

        bool selected = (_selectionMask & (1U << i)) != 0;
        bool active = _enginePtr && _enginePtr->activeStage() == i;

        // Determine marker height based on state
        int markerHeight = active ? 8 : (selected ? 6 : 4);

        canvas.setColor(active ? Color::Bright : (selected ? Color::Medium : Color::Low));
        canvas.vline(x, barLineY - markerHeight, markerHeight); // Grow upward
        canvas.vline(x, barLineY - markerHeight, markerHeight); // 2px wide
    }

    // Draw input cursor growing upward from baseline
    if (_enginePtr) {
        float inputNorm = clamp((_enginePtr->currentInput() - rangeMin()) / (rangeMax() - rangeMin()), 0.f, 1.f);
        int cursorX = barX + int(inputNorm * barW);
        const int cursorHeight = 8;

        canvas.setColor(Color::Bright);
        canvas.vline(cursorX, barLineY - cursorHeight, cursorHeight); // Grow upward
    }
}

// Stage information display
void DiscreteMapSequencePage::drawStageInfo(Canvas &canvas) {
    const int y = 30;
    const int spacing = 30;

    int stepOffset = _section * 8;
    for (int i = 0; i < 8; ++i) {
        int stageIndex = stepOffset + i;
        if (stageIndex >= DiscreteMapSequence::StageCount) break;

        const auto &stage = _sequence->stage(stageIndex);
        int x = 8 + i * spacing + 11; // Centered (+11)

        bool selected = (_selectionMask & (1U << stageIndex)) != 0;
        bool active = _enginePtr && _enginePtr->activeStage() == stageIndex;

        // Row 1: Direction (Top)
        canvas.setColor(active ? Color::Bright : (selected ? Color::MediumBright : Color::Medium));
        char dirChar = '-';
        switch (stage.direction()) {
        case DiscreteMapSequence::Stage::TriggerDir::Rise: dirChar = '^'; break;
        case DiscreteMapSequence::Stage::TriggerDir::Fall: dirChar = 'v'; break;
        case DiscreteMapSequence::Stage::TriggerDir::Off:  dirChar = '-'; break;
        case DiscreteMapSequence::Stage::TriggerDir::Both: dirChar = 'x'; break;
        }
        char dirStr[2] = { dirChar, 0 };
        canvas.drawText(x, y, dirStr);

        // Row 2: Threshold
        canvas.setColor(active ? Color::Bright : (selected ? Color::MediumBright : Color::Medium));
        FixedStringBuilder<6> thresh("%+d", stage.threshold());
        canvas.drawText(x - 4, y + 8, thresh);

        // Row 3: Note
        if (stage.direction() != DiscreteMapSequence::Stage::TriggerDir::Off || selected) {
            FixedStringBuilder<8> name;
            const Scale &scale = _sequence->selectedScale(_project.selectedScale());

            float volts = scale.noteToVolts(stage.noteIndex());
            if (scale.isChromatic()) {
                volts += _sequence->rootNote() * (1.f / 12.f);
            }
            int midiNote = int(volts * 12.f) + 12;

            // Handle negative midiNote values by using safe modulo for pitch class and floor-divide for octave.
            int pitchClass = modulo(midiNote, 12);
            int octave = roundDownDivide(midiNote, 12) - 1;
            Types::printNote(name, pitchClass);
            name("%d", octave);

            canvas.setColor(active ? Color::Bright : (selected ? Color::MediumBright : Color::Medium));
            canvas.drawText(x - 4, y + 16, name);
        } else {
            canvas.setColor(Color::Medium);
            canvas.drawText(x - 4, y + 16, "--");
        }
    }
}

// Footer display showing current settings
void DiscreteMapSequencePage::drawFooter(Canvas &canvas) {
    const char *clockSource = "INT";
    switch (_sequence->clockSource()) {
    case DiscreteMapSequence::ClockSource::Internal: clockSource = "SAW"; break;
    case DiscreteMapSequence::ClockSource::InternalTriangle: clockSource = "TRI"; break;
    case DiscreteMapSequence::ClockSource::External: clockSource = "EXT"; break;
    }
    FixedStringBuilder<8> syncLabel;
    _sequence->printSyncModeShort(syncLabel);

    const char *fnLabels[5] = {
        clockSource,
        getRangeMacroName(_currentRangeMacro),
        _sequence->thresholdMode() == DiscreteMapSequence::ThresholdMode::Position ? "POS" : "LEN",
        _sequence->loop() ? "LOOP" : "ONCE",
        syncLabel
    };

    WindowPainter::drawFooter(canvas, fnLabels, pageKeyState(), -1);
}

// 5. KEY INTERACTION HANDLING
// ===========================

// Handle top row key presses (for stage selection)
void DiscreteMapSequencePage::handleTopRowKey(int idx) {
    // Shift+Click: Latching multi-select (toggle selection without changing edit mode)
    if (_shiftHeld) {
        _selectionMask ^= (1U << idx); // Toggle selection
        if (_selectionMask == 0) _selectionMask = (1U << idx); // Prevent empty selection
        _selectedStage = idx;
        return;
    }

    // Check if any OTHER selection key (0-7) is held
    int physicalIdx = idx % 8;
    bool multiSelect = (_stepKeysHeld & 0xFF & ~(1 << physicalIdx)) != 0;

    // Ensure we are in a valid edit mode when selecting
    if (_editMode == EditMode::None) {
        _editMode = EditMode::Threshold;
    }

    if (multiSelect) {
        _selectionMask ^= (1U << idx); // Toggle
        if (_selectionMask == 0) _selectionMask = (1U << idx);
    } else {
        _selectionMask = (1U << idx); // Switch
    }

    _selectedStage = idx; // Update focus
}

// Handle bottom row key presses (for direction toggling)
void DiscreteMapSequencePage::handleBottomRowKey(int idx) {
    // Select the stage exclusively (no multi-select)
    _selectionMask = (1U << idx);
    _selectedStage = idx;

    // Toggle direction
    auto &stage = _sequence->stage(idx);
    stage.cycleDirection();

    if (_enginePtr) {
        _enginePtr->invalidateThresholds();
    }
}

// Function key handling
void DiscreteMapSequencePage::handleFunctionKey(int fnIndex) {
    switch (fnIndex) {
    case 0: // Clock source
        _sequence->toggleClockSource();
        break;
    case 1: // Range macro
        int next = static_cast<int>(_currentRangeMacro) + 1;
        if (next >= static_cast<int>(RangeMacro::Last)) next = 0;
        applyRangeMacro(static_cast<RangeMacro>(next));
        break;
    case 2: // Threshold mode
        _sequence->toggleThresholdMode();
        if (_enginePtr) {
            _enginePtr->invalidateThresholds();
        }
        break;
    case 3: // Loop mode
        _sequence->toggleLoop();
        break;
    case 4: // Sync mode
        _sequence->cycleSyncMode();
        break;
    default:
        break;
    }
}

// Encoder handling for value editing
void DiscreteMapSequencePage::encoder(EncoderEvent &event) {
    if (!_sequence) {
        return;
    }

    int delta = event.value();

    // Individual stage editing (when specific stages are selected)
    for (int i = 0; i < DiscreteMapSequence::StageCount; ++i) {
        if (!(_selectionMask & (1U << i))) {
            continue;
        }

        switch (_editMode) {
        case EditMode::Threshold: {
            auto &s = _sequence->stage(i);
            int step = _shiftHeld ? 10 : 1;
            s.setThreshold(clamp(s.threshold() + delta * step, -99, 99));
            if (_enginePtr) {
                _enginePtr->invalidateThresholds();
            }
            break;
        }
        case EditMode::NoteValue: {
            auto &s = _sequence->stage(i);
            const Scale &scale = _sequence->selectedScale(_project.selectedScale());
            int step = (_shiftHeld && scale.isChromatic()) ? scale.notesPerOctave() : 1;
            s.setNoteIndex(s.noteIndex() + delta * step);
            break;
        }
        default:
            break;
        }
    }
}