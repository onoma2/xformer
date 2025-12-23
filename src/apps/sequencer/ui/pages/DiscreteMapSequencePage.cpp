#include "DiscreteMapSequencePage.h"

#include "Pages.h"

#include "ui/MatrixMap.h"
#include "ui/LedPainter.h"
#include "ui/painters/WindowPainter.h"

#include "model/Routing.h"
#include "model/Types.h"
#include "model/Scale.h"

#include "core/math/Math.h"
#include "core/utils/StringBuilder.h"

#include <cmath>

static const ContextMenuModel::Item contextMenuItems[] = {
    { "INIT" },
    { "COPY" },
    { "PASTE" },
    { "GEN" },
    { "ROUTE" },
};

enum {
    QuickEditNone = -1,
    QuickEditEven = -2,
};

static const int quickEditItems[8] = {
    int(DiscreteMapSequenceListModel::Item::RangeHigh), // Step 9
    int(DiscreteMapSequenceListModel::Item::RangeLow),  // Step 10
    int(DiscreteMapSequenceListModel::Item::Divisor),   // Step 11
    QuickEditEven,                                      // Step 12
    QuickEditNone,
    QuickEditNone,
    QuickEditNone,
    QuickEditNone,
};

DiscreteMapSequencePage::DiscreteMapSequencePage(PageManager &manager, PageContext &context) :
    BasePage(manager, context)
{}

void DiscreteMapSequencePage::enter() {
    refreshPointers();
    // Reset generator and init modes when entering the page to ensure clean state
    _generatorStage = GeneratorStage::Inactive;
    _generatorKind = GeneratorKind::Random;
    _initStage = InitStage::Inactive;

    if (_sequence) {
        // Sync the macro indicator to current Above/Below values if they match a preset
        for (int i = 0; i < static_cast<int>(RangeMacro::Last); ++i) {
            float low, high;
            getRangeMacroValues(static_cast<RangeMacro>(i), low, high);
            if (std::abs(low - _sequence->rangeLow()) < 0.001f &&
                std::abs(high - _sequence->rangeHigh()) < 0.001f) {
                _currentRangeMacro = static_cast<RangeMacro>(i);
                break;
            }
        }
    }
}

void DiscreteMapSequencePage::exit() {
    // Ensure generator and init modes are reset when exiting the page
    _generatorStage = GeneratorStage::Inactive;
    _generatorKind = GeneratorKind::Random;
    _initStage = InitStage::Inactive;
}

void DiscreteMapSequencePage::refreshPointers() {
    _sequence = nullptr;
    _enginePtr = nullptr;

    if (_project.selectedTrack().trackMode() != Track::TrackMode::DiscreteMap) {
        return;
    }

    _sequence = &_project.selectedDiscreteMapSequence();

    auto &trackEngine = _engine.trackEngine(_project.selectedTrackIndex());
    if (trackEngine.trackMode() == Track::TrackMode::DiscreteMap) {
        _enginePtr = static_cast<DiscreteMapTrackEngine *>(&trackEngine);
    }
}

void DiscreteMapSequencePage::draw(Canvas &canvas) {
    refreshPointers();
    WindowPainter::clear(canvas);
    WindowPainter::drawHeader(canvas, _model, _engine, "DMAP");

    FixedStringBuilder<32> headerName;
    headerName(Track::trackModeName(_project.selectedTrack().trackMode()));
    headerName(_editMode == EditMode::NoteValue ? ": NOTE" : ": THR");
    headerName(" %d/4", _section + 1);
    WindowPainter::drawActiveFunction(canvas, headerName);

    if (!_sequence) {
        canvas.drawText(8, 24, "Select a DiscreteMap track");
        WindowPainter::drawFooter(canvas);
        return;
    }

    drawThresholdBar(canvas);
    drawStageInfo(canvas);
    drawFooter(canvas);
}

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

void DiscreteMapSequencePage::drawStageInfo(Canvas &canvas) {
    const int y = 30;
    const int spacing = 30;

    // Draw row selection brackets
    // Row 2 (y+8): Threshold
    // Row 3 (y+16): Note
    int bracketY = (_editMode == EditMode::NoteValue) ? y + 16 : y + 8;
    int bracketH = 6;
    canvas.setColor(Color::Bright);
    canvas.vline(8, bracketY - 4, bracketH);
    canvas.vline(246, bracketY - 4, bracketH);

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
        }
        char dirStr[2] = { dirChar, 0 };
        canvas.drawText(x, y, dirStr); // Center aligned x

        // Row 2: Threshold
        canvas.setColor(active ? Color::Bright : (selected ? Color::MediumBright : Color::Medium));
        FixedStringBuilder<6> thresh("%+d", stage.threshold());
        // Adjust x for multi-digit numbers if needed?
        // FixedStringBuilder doesn't auto-center.
        // 30px width. "+127" is 4 chars * 7px = 28px.
        // "+11" offset centers 1 char.
        // If x is center of column?
        // My code: x = 8 + i*30 + 11.
        // This makes x the START of the text.
        // If I draw "+127" at x, it might spill.
        // The diff used: int x = barX + i * segmentWidth + segmentWidth / 2 - 4; (-4 to center text?)
        // If I use +11 (approx 30/2 - 4), it's close.
        // But for "+127", width is 24-28.
        // x should be center - width/2.
        // I'll stick to x as start position for now, assuming 2-3 chars average.
        canvas.drawText(x - 4, y + 8, thresh); // Shift left for numbers?

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
            canvas.drawText(x - 4, y + 16, name); // Shift left
        } else {
            canvas.setColor(Color::Medium);
            canvas.drawText(x - 4, y + 16, "--");
        }
    }
}

void DiscreteMapSequencePage::drawFooter(Canvas &canvas) {
    if (_initStage == InitStage::ChooseTarget) {
        const char *fnLabels[5] = { "ALL", "THR", "NOTE", "", "X" };
        WindowPainter::drawFooter(canvas, fnLabels, pageKeyState(), -1);
        return;
    }

    if (_generatorStage == GeneratorStage::ChooseKind) {
        const char *fnLabels[5] = { "RAND", "LIN", "LOG", "EXP", "X" };
        WindowPainter::drawFooter(canvas, fnLabels, pageKeyState(), -1);
        return;
    }

    if (_generatorStage == GeneratorStage::Execute) {
        // Show TOG on F4 only for RAND; otherwise use NOTE2
        const bool isRand = _generatorKind == GeneratorKind::Random;
        const char *fnLabels[5] = { "ALL", "THR", isRand ? "NOTE" : "NOTE5", isRand ? "TOG" : "NOTE2", "X" };
        WindowPainter::drawFooter(canvas, fnLabels, pageKeyState(), -1);
        return;
    }

    // Original footer display when not in generator mode
    const char *clockSource = "INT";
    switch (_sequence->clockSource()) {
    case DiscreteMapSequence::ClockSource::Internal: clockSource = "SAW"; break;
    case DiscreteMapSequence::ClockSource::InternalTriangle: clockSource = "TRI"; break;
    case DiscreteMapSequence::ClockSource::External: clockSource = "EXT"; break;
    }
    FixedStringBuilder<8> syncLabel;
    _sequence->printSyncModeShort(syncLabel);

    // Keep macro indicator aligned with current Above/Below values (handles routing edits)
    for (int i = 0; i < static_cast<int>(RangeMacro::Last); ++i) {
        float low, high;
        getRangeMacroValues(static_cast<RangeMacro>(i), low, high);
        if (std::abs(low - _sequence->rangeLow()) < 0.001f &&
            std::abs(high - _sequence->rangeHigh()) < 0.001f) {
            _currentRangeMacro = static_cast<RangeMacro>(i);
            break;
        }
    }

    const char *fnLabels[5] = {
        clockSource,
        getRangeMacroName(_currentRangeMacro),
        _sequence->thresholdMode() == DiscreteMapSequence::ThresholdMode::Position ? "POS" : "LEN",
        _sequence->loop() ? "LOOP" : "ONCE",
        syncLabel
    };

    WindowPainter::drawFooter(canvas, fnLabels, pageKeyState(), -1);
}

void DiscreteMapSequencePage::updateLeds(Leds &leds) {
    if (!_sequence) {
        return;
    }

    int stepOffset = _section * 8;

    // Direction (Top Row: 8-15)
    for (int i = 0; i < 8; ++i) {
        int stageIndex = stepOffset + i;
        if (stageIndex >= DiscreteMapSequence::StageCount) break;

        const auto &stage = _sequence->stage(stageIndex);
        auto ledIndex = MatrixMap::fromStep(i + 8);
        switch (stage.direction()) {
        case DiscreteMapSequence::Stage::TriggerDir::Rise:
            leds.set(ledIndex, false, true);
            break;
        case DiscreteMapSequence::Stage::TriggerDir::Fall:
            leds.set(ledIndex, true, false);
            break;
        case DiscreteMapSequence::Stage::TriggerDir::Off:
            leds.set(ledIndex, false, false);
            break;
        }
    }

    // Selection/Active (Bottom Row: 0-7)
    for (int i = 0; i < 8; ++i) {
        int stageIndex = stepOffset + i;
        if (stageIndex >= DiscreteMapSequence::StageCount) break;

        bool selected = (_selectionMask & (1U << stageIndex)) != 0;
        bool active = _enginePtr && _enginePtr->activeStage() == stageIndex;
        auto ledIndex = MatrixMap::fromStep(i);
        if (selected) {
            leds.set(ledIndex, true, true);
        } else if (active) {
            leds.set(ledIndex, false, true);
        } else {
            leds.set(ledIndex, false, false);
        }
    }

    if (globalKeyState()[Key::Page] && !globalKeyState()[Key::Shift]) {
        for (int i = 0; i < 8; ++i) {
            int index = MatrixMap::fromStep(i + 8);
            leds.unmask(index);
            leds.set(index, false, quickEditItems[i] != QuickEditNone);
            leds.mask(index);
        }
    }
}

void DiscreteMapSequencePage::keyDown(KeyEvent &event) {
    const auto &key = event.key();
    _shiftHeld = key.shiftModifier();
    refreshPointers();

    if (key.isContextMenu()) {
        contextShow();
        event.consume();
        return;
    }

    if (key.pageModifier() || !_sequence) {
        return;
    }

    if (key.isStep()) {
        int stepOffset = _section * 8;
        int idx = key.step();
        if (idx < 8) {
            _stepKeysHeld |= (1 << idx);
            handleTopRowKey(stepOffset + idx);
        } else {
            handleBottomRowKey(stepOffset + idx - 8);
        }
        event.consume();
        return;
    }

    if (key.isFunction()) {
        handleFunctionKey(key.function());
        event.consume();
    }
}

void DiscreteMapSequencePage::keyUp(KeyEvent &event) {
    if (event.key().isStep()) {
        int idx = event.key().step();
        // If it was a Selection Button (Bottom Row: 0-7)
        if (idx < 8) {
            _stepKeysHeld &= ~(1 << idx);
        }
    }
    _shiftHeld = event.key().shiftModifier();
}

void DiscreteMapSequencePage::keyPress(KeyPressEvent &event) {
    const auto &key = event.key();
    if (key.isContextMenu()) {
        contextShow();
        event.consume();
        return;
    }

    if (key.isQuickEdit() && !key.shiftModifier()) {
        quickEdit(key.quickEdit());
        event.consume();
        return;
    }

    if (key.isEncoder()) {
        // In normal mode, toggle between threshold and note value edit modes
        _editMode = (_editMode == EditMode::NoteValue) ? EditMode::Threshold : EditMode::NoteValue;
        event.consume();
        return;
    }

    if (key.isLeft()) {
        _section = std::max(0, _section - 1);
        event.consume();
    }
    if (key.isRight()) {
        _section = std::min(3, _section + 1);
        event.consume();
    }
}

void DiscreteMapSequencePage::quickEdit(int index) {
    if (!_sequence || index < 0 || index >= 8) {
        return;
    }
    int item = quickEditItems[index];
    if (item == QuickEditNone) {
        return;
    }
    if (item == QuickEditEven) {
        distributeActiveStagesEvenly();
        return;
    }
    _listModel.setSequence(_sequence);
    _listModel.setTrack(&_project.selectedTrack().discreteMapTrack());
    _manager.pages().quickEdit.show(_listModel, item);
}

void DiscreteMapSequencePage::distributeActiveStagesEvenly() {
    if (!_sequence) {
        return;
    }

    int activeIndices[DiscreteMapSequence::StageCount];
    int activeCount = 0;
    for (int i = 0; i < DiscreteMapSequence::StageCount; ++i) {
        if (_sequence->stage(i).direction() != DiscreteMapSequence::Stage::TriggerDir::Off) {
            activeIndices[activeCount++] = i;
        }
    }

    if (activeCount <= 1) {
        showMessage("NO SPREAD");
        return;
    }

    if (_sequence->thresholdMode() == DiscreteMapSequence::ThresholdMode::Length) {
        for (int i = 0; i < DiscreteMapSequence::StageCount; ++i) {
            auto &stage = _sequence->stage(i);
            if (stage.direction() == DiscreteMapSequence::Stage::TriggerDir::Off) {
                stage.setThreshold(-99);
            } else {
                stage.setThreshold(0);
            }
        }
    } else {
        for (int i = 0; i < activeCount; ++i) {
            float t = float(i) / float(activeCount - 1);
            int value = int(std::round(-99.f + t * 198.f));
            _sequence->stage(activeIndices[i]).setThreshold(value);
        }
    }

    if (_enginePtr) {
        _enginePtr->invalidateThresholds();
    }

    showMessage("EVEN THR");
}

void DiscreteMapSequencePage::encoder(EncoderEvent &event) {
    if (!_sequence) {
        return;
    }

    int delta = event.value();

    for (int i = 0; i < DiscreteMapSequence::StageCount; ++i) {
        if (!(_selectionMask & (1U << i))) {
            continue;
        }

        switch (_editMode) {
        case EditMode::Threshold: {
            auto &s = _sequence->stage(i);
            int step = _shiftHeld ? 1 : 8;
            s.setThreshold(clamp(s.threshold() + delta * step, -99, 99));
            if (_enginePtr) {
                _enginePtr->invalidateThresholds();
            }
            break;
        }
        case EditMode::NoteValue: {
            auto &s = _sequence->stage(i);
            int step = _shiftHeld ? 12 : 1;
            s.setNoteIndex(s.noteIndex() + delta * step);
            break;
        }
        default:
            break;
        }
    }
}

void DiscreteMapSequencePage::handleTopRowKey(int idx) {
    // Shift+Click: Latching multi-select (toggle selection without changing edit mode)
    if (_shiftHeld) {
        _selectionMask ^= (1U << idx); // Toggle selection
        if (_selectionMask == 0) _selectionMask = (1U << idx); // Prevent empty selection
        _selectedStage = idx;
        return;
    }

    // Check if any OTHER selection key (0-7) is held
    // idx is logical index. Physical index is idx % 8.
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

void DiscreteMapSequencePage::handleBottomRowKey(int idx) {
    // Select the stage exclusively (no multi-select)
    _selectionMask = (1U << idx);
    _selectedStage = idx;

    // Existing Direction Toggle Logic
    auto &stage = _sequence->stage(idx);

    switch (stage.direction()) {
    case DiscreteMapSequence::Stage::TriggerDir::Rise:
        stage.setDirection(DiscreteMapSequence::Stage::TriggerDir::Fall);
        break;
    case DiscreteMapSequence::Stage::TriggerDir::Fall:
        stage.setDirection(DiscreteMapSequence::Stage::TriggerDir::Off);
        break;
    case DiscreteMapSequence::Stage::TriggerDir::Off:
        stage.setDirection(DiscreteMapSequence::Stage::TriggerDir::Rise);
        break;
    }

    if (_enginePtr) {
        _enginePtr->invalidateThresholds();
    }
}

void DiscreteMapSequencePage::handleFunctionKey(int fnIndex) {
    // INIT submenu handling
    if (_initStage == InitStage::ChooseTarget) {
        switch (fnIndex) {
        case 0: // ALL
            if (_sequence) {
                _sequence->clear();
                if (_enginePtr) {
                    _enginePtr->invalidateThresholds();
                }
                showMessage("INIT ALL");
            }
            _initStage = InitStage::Inactive;
            break;
        case 1: // THR (Thresholds)
            if (_sequence) {
                _sequence->clearThresholds();
                if (_enginePtr) {
                    _enginePtr->invalidateThresholds();
                }
                showMessage("INIT THR");
            }
            _initStage = InitStage::Inactive;
            break;
        case 2: // NOTE (Notes)
            if (_sequence) {
                _sequence->clearNotes();
                showMessage("INIT NOTE");
            }
            _initStage = InitStage::Inactive;
            break;
        case 4: // X (Exit)
            _initStage = InitStage::Inactive;
            break;
        default:
            break;
        }
        return;
    }

    // Generator flow: first choose generator kind, then execute on targets
    if (_generatorStage == GeneratorStage::ChooseKind) {
        switch (fnIndex) {
        case 0: _generatorKind = GeneratorKind::Random; _generatorStage = GeneratorStage::Execute; break;
        case 1: _generatorKind = GeneratorKind::Linear; _generatorStage = GeneratorStage::Execute; break;
        case 2: _generatorKind = GeneratorKind::Logarithmic; _generatorStage = GeneratorStage::Execute; break;
        case 3: _generatorKind = GeneratorKind::Exponential; _generatorStage = GeneratorStage::Execute; break;
        case 4: _generatorStage = GeneratorStage::Inactive; break;
        default: break;
        }
        return;
    }

    if (_generatorStage == GeneratorStage::Execute) {
        const bool isRand = _generatorKind == GeneratorKind::Random;
        if (isRand) {
            switch (fnIndex) {
            case 0: applyGenerator(true, true, true); _generatorStage = GeneratorStage::Inactive; break;   // ALL
            case 1: applyGenerator(true, false, false); _generatorStage = GeneratorStage::Inactive; break; // THR
            case 2: applyGenerator(false, true, false); _generatorStage = GeneratorStage::Inactive; break; // NOTE
            case 3: applyGenerator(false, false, true); _generatorStage = GeneratorStage::Inactive; break; // TOG
            case 4: _generatorStage = GeneratorStage::Inactive; break;                                    // EXIT
            default: break;
            }
        } else {
            switch (fnIndex) {
            case 0: applyGenerator(true, true, false, NoteSpread::Wide); _generatorStage = GeneratorStage::Inactive; break;   // ALL (Note5)
            case 1: applyGenerator(true, false, false, NoteSpread::Wide); _generatorStage = GeneratorStage::Inactive; break; // THR
            case 2: applyGenerator(false, true, false, NoteSpread::Wide); _generatorStage = GeneratorStage::Inactive; break; // NOTE5
            case 3: applyGenerator(false, true, false, NoteSpread::Narrow); _generatorStage = GeneratorStage::Inactive; break; // NOTE2
            case 4: _generatorStage = GeneratorStage::Inactive; break;                                                        // EXIT
            default: break;
            }
        }
        return;
    }

    // Handle normal mode function key presses
    switch (fnIndex) {
    case 0:
        _sequence->toggleClockSource();
        break;
    case 1: {
        int next = static_cast<int>(_currentRangeMacro) + 1;
        if (next >= static_cast<int>(RangeMacro::Last)) next = 0;
        applyRangeMacro(static_cast<RangeMacro>(next));
        break;
    }
    case 2:
        _sequence->toggleThresholdMode();
        if (_enginePtr) {
            _enginePtr->invalidateThresholds();
        }
        break;
    case 3:
        _sequence->toggleLoop();
        break;
    case 4:
        _sequence->cycleSyncMode();
        break;
    default:
        break;
    }
}

float DiscreteMapSequencePage::getThresholdNormalized(int stageIndex) const {
    const auto &stage = _sequence->stage(stageIndex);
    return (stage.threshold() + 100) / 200.f;
}

void DiscreteMapSequencePage::applyRangeMacro(RangeMacro macro) {
    if (!_sequence) {
        return;
    }

    float low = -5.f;
    float high = 5.f;
    getRangeMacroValues(macro, low, high);

    _sequence->setRangeLow(low);
    _sequence->setRangeHigh(high);
    _currentRangeMacro = macro;

    if (_enginePtr) {
        _enginePtr->invalidateThresholds();
    }

    // No message spam; footer already shows current macro
}

void DiscreteMapSequencePage::getRangeMacroValues(RangeMacro macro, float &low, float &high) const {
    switch (macro) {
    case RangeMacro::Full:         low = -5.f;  high = 5.f;    break;
    case RangeMacro::Inv:          low = 5.f;   high = -5.f;   break;
    case RangeMacro::Pos:          low = 0.f;   high = 5.f;     break;
    case RangeMacro::Neg:          low = -5.f;  high = 0.f;    break;
    case RangeMacro::Top:          low = 4.f;   high = 5.f;    break;
    case RangeMacro::Btm:          low = -5.f;  high = -4.f;   break;
    case RangeMacro::Ass:          low = -1.f;  high = 4.f;    break;
    case RangeMacro::BAss:         low = 3.f;   high = -2.f;   break;
    case RangeMacro::Last:         low = -5.f;  high = 5.f;    break;
    }
}

const char *DiscreteMapSequencePage::getRangeMacroName(RangeMacro macro) const {
    switch (macro) {
    case RangeMacro::Full:        return "-5/+5";
    case RangeMacro::Inv:         return "+5/-5";
    case RangeMacro::Pos:         return "0/+5";
    case RangeMacro::Neg:         return "-5/0";
    case RangeMacro::Top:         return "+4/+5";
    case RangeMacro::Btm:         return "-5/-4";
    case RangeMacro::Ass:         return "-1/+4";
    case RangeMacro::BAss:        return "+3/-2";
    case RangeMacro::Last:        break;
    }
    return "RANGE";
}

void DiscreteMapSequencePage::contextShow() {
    showContextMenu(ContextMenu(
        contextMenuItems,
        int(ContextAction::Last),
        [&] (int index) { contextAction(index); },
        [&] (int index) { return contextActionEnabled(index); }
    ));
}

void DiscreteMapSequencePage::contextAction(int index) {
    switch (ContextAction(index)) {
    case ContextAction::Init:
        // Enter INIT sublevel menu
        _initStage = InitStage::ChooseTarget;
        break;
    case ContextAction::Copy:
        _model.clipBoard().copyDiscreteMapSequence(*_sequence);
        showMessage("COPIED");
        break;
    case ContextAction::Paste:
        _model.clipBoard().pasteDiscreteMapSequence(*_sequence);
        if (_enginePtr) {
            _enginePtr->invalidateThresholds();
        }
        showMessage("PASTED");
        break;
    case ContextAction::Random:
        // Enter generator selection mode
        _generatorStage = GeneratorStage::ChooseKind;
        _generatorKind = GeneratorKind::Random;
        break;
    case ContextAction::Route:
        _manager.pages().top.editRoute(Routing::Target::Divisor, _project.selectedTrackIndex());
        break;
    case ContextAction::Last:
        break;
    }
}

bool DiscreteMapSequencePage::contextActionEnabled(int index) const {
    switch (ContextAction(index)) {
    case ContextAction::Paste:
        return _model.clipBoard().canPasteDiscreteMapSequence();
    case ContextAction::Random:
        return _sequence != nullptr;
    case ContextAction::Route:
        return true;
    default:
        return true;
    }
}

void DiscreteMapSequencePage::applyGenerator(bool applyThresholds, bool applyNotes, bool applyToggles, NoteSpread noteSpread) {
    if (!_sequence) {
        return;
    }

    auto generatorName = [] (GeneratorKind kind) -> const char * {
        switch (kind) {
        case GeneratorKind::Random:       return "RAND";
        case GeneratorKind::Linear:       return "LIN";
        case GeneratorKind::Logarithmic:  return "LOG";
        case GeneratorKind::Exponential:  return "EXP";
        }
        return "";
    };

    if (_generatorKind == GeneratorKind::Random) {
        if (applyThresholds && applyNotes && applyToggles) {
            _sequence->randomize(); // Includes directions
            showMessage("RAND ALL");
        } else {
            if (applyThresholds) {
                _sequence->randomizeThresholds();
            }
            if (applyNotes) {
                _sequence->randomizeNotes();
            }
            if (applyToggles) {
                _sequence->randomizeDirections();
            }

            if (applyThresholds && applyNotes && applyToggles) {
                showMessage("RAND ALL");
            } else if (applyThresholds) {
                showMessage("RAND THR");
            } else if (applyNotes) {
                showMessage("RAND NOTE");
            } else if (applyToggles) {
                showMessage("RAND TOG");
            }
        }
    } else {
        // LIN/LOG/EXP: ignore toggle requests, force directions to Rise
        if (applyThresholds) {
            generateThresholds(_generatorKind);
        }
        if (applyNotes) {
            generateNotes(_generatorKind, noteSpread);
        }

        FixedStringBuilder<16> msg;
        if (applyThresholds && applyNotes) {
            msg("%s ALL", generatorName(_generatorKind));
        } else if (applyThresholds) {
            msg("%s THR", generatorName(_generatorKind));
        } else if (applyNotes) {
            msg("%s %s", generatorName(_generatorKind), noteSpread == NoteSpread::Wide ? "NOTE5" : "NOTE2");
        }
        const char *text = msg;
        if (text[0] != 0) {
            showMessage(text);
        }
    }

    if (applyThresholds && _enginePtr) {
        _enginePtr->invalidateThresholds();
    }
}

void DiscreteMapSequencePage::generateThresholds(GeneratorKind kind) {
    const int count = DiscreteMapSequence::StageCount;
    const float minVal = -99.f;
    const float maxVal = 99.f;

    for (int i = 0; i < count; ++i) {
        float t = (count > 1) ? float(i) / float(count - 1) : 0.f;
        float shaped = shapeValue(t, kind);
        int val = int(std::round(minVal + shaped * (maxVal - minVal)));
        _sequence->stage(i).setThreshold(val);
    }
}

void DiscreteMapSequencePage::generateNotes(GeneratorKind kind, NoteSpread spread) {
    const int count = DiscreteMapSequence::StageCount;
    float minVal = -63.f;
    float maxVal = 64.f;

    if (spread == NoteSpread::Narrow) {
        minVal = 0.f;
        maxVal = 32.f;
    }

    for (int i = 0; i < count; ++i) {
        float t = (count > 1) ? float(i) / float(count - 1) : 0.f;
        float shaped = shapeValue(t, kind);
        int val = int(std::round(minVal + shaped * (maxVal - minVal)));
        _sequence->stage(i).setNoteIndex(val);
    }
}

float DiscreteMapSequencePage::shapeValue(float t, GeneratorKind kind) const {
    t = clamp(t, 0.f, 1.f);
    switch (kind) {
    case GeneratorKind::Random:
        return t; // Should not be used here
    case GeneratorKind::Linear:
        return t;
    case GeneratorKind::Logarithmic:
        return std::sqrt(t);       // Faster rise near start
    case GeneratorKind::Exponential:
        return pow(t, 1.3);              // Slower start, steeper end
    }
    return t;
}
