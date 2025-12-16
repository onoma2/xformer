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

enum class ContextAction {
    Init,
    Route,
    Last
};

static const ContextMenuModel::Item contextMenuItems[] = {
    { "INIT" },
    { "ROUTE" },
};

DiscreteMapSequencePage::DiscreteMapSequencePage(PageManager &manager, PageContext &context) :
    BasePage(manager, context)
{}

void DiscreteMapSequencePage::enter() {
    refreshPointers();
}

void DiscreteMapSequencePage::exit() {
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
    
    FixedStringBuilder<16> headerName;
    headerName(Track::trackModeName(_project.selectedTrack().trackMode()));
    headerName(_editMode == EditMode::NoteValue ? ": NOTE" : ": THR");
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
    const int barH = 6;

    canvas.setColor(Color::Medium);
    canvas.fillRect(barX, barY, barW, barH);

    for (int i = 0; i < DiscreteMapSequence::StageCount; ++i) {
        const auto &stage = _sequence->stage(i);
        if (stage.direction() == DiscreteMapSequence::Stage::TriggerDir::Off) {
            continue;
        }

        float norm = clamp(getThresholdNormalized(i), 0.f, 1.f);
        int x = barX + int(norm * barW);

        bool selected = (_selectionMask & (1 << i)) != 0;
        bool active = _enginePtr && _enginePtr->activeStage() == i;

        canvas.setColor(active ? Color::Bright : (selected ? Color::Medium : Color::Low));
        canvas.vline(x, barY, barH);
        canvas.vline(x + 1, barY, barH); // 2px wide
    }

    if (_enginePtr) {
        float inputNorm = clamp((_enginePtr->currentInput() - rangeMin()) / (rangeMax() - rangeMin()), 0.f, 1.f);
        int cursorX = barX + int(inputNorm * barW);
        canvas.setColor(Color::Bright);
        canvas.vline(cursorX, barY - 1, barH + 2);
    }
}

void DiscreteMapSequencePage::drawStageInfo(Canvas &canvas) {
    const int y = 24;
    const int spacing = 30;

    // Draw row selection brackets
    int bracketY = (_editMode == EditMode::NoteValue) ? y + 10 : y;
    int bracketH = 8;
    canvas.setColor(Color::Bright);
    canvas.vline(4, bracketY, bracketH);
    canvas.vline(250, bracketY, bracketH);

    for (int i = 0; i < DiscreteMapSequence::StageCount; ++i) {
        const auto &stage = _sequence->stage(i);
        int x = 8 + i * spacing;

        bool selected = (_selectionMask & (1 << i)) != 0;
        bool active = _enginePtr && _enginePtr->activeStage() == i;

        // Row 1: Threshold
        canvas.setColor(active ? Color::Bright : (selected ? Color::Medium : Color::Low));
        FixedStringBuilder<4> thresh("%+d", stage.threshold());
        canvas.drawText(x, y, thresh);

        // Row 2: Note
        if (stage.direction() != DiscreteMapSequence::Stage::TriggerDir::Off || selected) {
            FixedStringBuilder<8> name;
            const Scale &scale = _sequence->selectedScale(_project.selectedScale());
            scale.noteName(name, stage.noteIndex(), _sequence->rootNote(), Scale::Format::Long);
            
            canvas.setColor(active ? Color::Bright : (selected ? Color::Medium : Color::Low));
            canvas.drawText(x, y + 10, name);
        } else {
            canvas.setColor(Color::Low);
            canvas.drawText(x, y + 10, "--");
        }

        // Row 3: Direction
        canvas.setColor(active ? Color::Bright : (selected ? Color::Medium : Color::Low));
        char dirChar = '-';
        switch (stage.direction()) {
        case DiscreteMapSequence::Stage::TriggerDir::Rise: dirChar = '^'; break;
        case DiscreteMapSequence::Stage::TriggerDir::Fall: dirChar = 'v'; break;
        case DiscreteMapSequence::Stage::TriggerDir::Off:  dirChar = '-'; break;
        }
        char dirStr[2] = { dirChar, 0 };
        canvas.drawText(x + 2, y + 20, dirStr);
    }
}

void DiscreteMapSequencePage::drawFooter(Canvas &canvas) {
    const char *clockSource = "INT";
    switch (_sequence->clockSource()) {
    case DiscreteMapSequence::ClockSource::Internal: clockSource = "SAW"; break;
    case DiscreteMapSequence::ClockSource::InternalTriangle: clockSource = "TRI"; break;
    case DiscreteMapSequence::ClockSource::External: clockSource = "EXT"; break;
    }

    const char *fnLabels[5] = {
        clockSource,
        nullptr,
        _sequence->thresholdMode() == DiscreteMapSequence::ThresholdMode::Position ? "POS" : "LEN",
        _sequence->loop() ? "LOOP" : "ONCE",
        nullptr
    };

    WindowPainter::drawFooter(canvas, fnLabels, pageKeyState(), -1);
}

void DiscreteMapSequencePage::updateLeds(Leds &leds) {
    if (!_sequence) {
        return;
    }

    // Direction (Top Row: 8-15)
    for (int i = 0; i < DiscreteMapSequence::StageCount; ++i) {
        const auto &stage = _sequence->stage(i);
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
    for (int i = 0; i < DiscreteMapSequence::StageCount; ++i) {
        bool selected = (_selectionMask & (1 << i)) != 0;
        bool active = _enginePtr && _enginePtr->activeStage() == i;
        auto ledIndex = MatrixMap::fromStep(i);
        if (selected) {
            leds.set(ledIndex, true, true);
        } else if (active) {
            leds.set(ledIndex, false, true);
        } else {
            leds.set(ledIndex, false, false);
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
        int idx = key.step();
        if (idx < 8) {
            _stepKeysHeld |= (1 << idx);
            handleTopRowKey(idx, key.shiftModifier());
        } else {
            handleBottomRowKey(idx - 8);
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

    if (key.isEncoder()) {
        _editMode = (_editMode == EditMode::NoteValue) ? EditMode::Threshold : EditMode::NoteValue;
        event.consume();
        return;
    }
}

void DiscreteMapSequencePage::encoder(EncoderEvent &event) {
    if (!_sequence) {
        return;
    }

    int delta = event.value();

    for (int i = 0; i < DiscreteMapSequence::StageCount; ++i) {
        if (!(_selectionMask & (1 << i))) {
            continue;
        }

        switch (_editMode) {
        case EditMode::Threshold: {
            auto &s = _sequence->stage(i);
            int step = _shiftHeld ? 1 : 8;
            s.setThreshold(s.threshold() + delta * step);
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

void DiscreteMapSequencePage::handleTopRowKey(int idx, bool shift) {
    if (shift) {
        _editMode = EditMode::NoteValue;
    } else {
        _editMode = EditMode::Threshold;
    }

    // Check if any OTHER key is held (excluding current one)
    bool multiSelect = (_stepKeysHeld & ~(1 << idx)) != 0;

    if (multiSelect) {
        _selectionMask ^= (1 << idx); // Toggle
        // Ensure at least one stage is selected if we just toggled off the last one?
        // Actually, allowing toggle off is tricky if it leaves 0.
        if (_selectionMask == 0) _selectionMask = (1 << idx);
    } else {
        _selectionMask = (1 << idx); // Switch
    }

    _selectedStage = idx; // Update focus
}

void DiscreteMapSequencePage::handleBottomRowKey(int idx) {
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
    switch (fnIndex) {
    case 0:
        _sequence->toggleClockSource();
        break;
    case 2:
        _sequence->toggleThresholdMode();
        if (_enginePtr) {
            _enginePtr->invalidateThresholds();
        }
        break;
    case 3:
        _sequence->toggleLoop();
        break;
    default:
        break;
    }
}

float DiscreteMapSequencePage::getThresholdNormalized(int stageIndex) const {
    const auto &stage = _sequence->stage(stageIndex);
    return (stage.threshold() + 127) / 254.f;
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
        if (_sequence) {
            _sequence->clear();
            if (_enginePtr) {
                _enginePtr->invalidateThresholds();
            }
            showMessage("SEQUENCE INITIALIZED");
        }
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
    case ContextAction::Route:
        return true;
    default:
        return true;
    }
}
