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
    WindowPainter::drawActiveFunction(canvas, Track::trackModeName(_project.selectedTrack().trackMode()));

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

        bool selected = (i == _selectedStage) || (i == _secondaryStage);
        bool active = _enginePtr && _enginePtr->activeStage() == i;

        canvas.setColor(active ? Color::Bright : (selected ? Color::Medium : Color::Low));
        canvas.vline(x, barY, barH);

        char dir = (stage.direction() == DiscreteMapSequence::Stage::TriggerDir::Rise) ? '^' : 'v';
        char dirStr[2] = { dir, 0 };
        canvas.drawText(x - 2, barY + barH + 2, dirStr);
    }

    if (_enginePtr) {
        float inputNorm = clamp((_enginePtr->currentInput() - rangeMin()) / (rangeMax() - rangeMin()), 0.f, 1.f);
        int cursorX = barX + int(inputNorm * barW);
        canvas.setColor(Color::Bright);
        canvas.vline(cursorX, barY - 1, barH + 2);
    }
}

void DiscreteMapSequencePage::drawStageInfo(Canvas &canvas) {
    const int y = 28;
    const int spacing = 30;

    for (int i = 0; i < DiscreteMapSequence::StageCount; ++i) {
        const auto &stage = _sequence->stage(i);
        int x = 8 + i * spacing;

        bool selected = (i == _selectedStage) || (i == _secondaryStage);
        bool active = _enginePtr && _enginePtr->activeStage() == i;

        // Draw Threshold Value (replaces stage number)
        canvas.setColor(active ? Color::Bright : (selected ? Color::Medium : Color::Low));
        FixedStringBuilder<4> thresh("%+d", stage.threshold());
        canvas.drawText(x, y, thresh);

        if (stage.direction() != DiscreteMapSequence::Stage::TriggerDir::Off) {
            FixedStringBuilder<8> name;
            const Scale &scale = _sequence->selectedScale(_project.selectedScale());
            scale.noteName(name, stage.noteIndex(), _sequence->rootNote(), Scale::Format::Short1);
            
            canvas.setColor(active ? Color::Bright : (selected ? Color::Medium : Color::Low));
            canvas.drawText(x, y + 10, name);
        } else {
            canvas.setColor(Color::Low);
            canvas.drawText(x, y + 10, "--");
        }
    }
}

void DiscreteMapSequencePage::drawFooter(Canvas &canvas) {
    const char *fnLabels[5] = {
        _sequence->clockSource() == DiscreteMapSequence::ClockSource::Internal ? "INT" : "EXT",
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

    for (int i = 0; i < DiscreteMapSequence::StageCount; ++i) {
        const auto &stage = _sequence->stage(i);
        auto ledIndex = MatrixMap::fromStep(i);
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

    for (int i = 0; i < DiscreteMapSequence::StageCount; ++i) {
        bool selected = (i == _selectedStage) || (i == _secondaryStage);
        bool active = _enginePtr && _enginePtr->activeStage() == i;
        auto ledIndex = MatrixMap::fromStep(i + 8);
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
            handleBottomRowKey(idx);
        } else {
            handleTopRowKey(idx - 8, key.shiftModifier());
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
        _editMode = EditMode::None;
        _secondaryStage = -1;
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
}

void DiscreteMapSequencePage::encoder(EncoderEvent &event) {
    if (!_sequence) {
        return;
    }

    int delta = event.value();

    switch (_editMode) {
    case EditMode::Threshold: {
        auto &s = _sequence->stage(_selectedStage);
        int step = _shiftHeld ? 1 : 8;
        s.setThreshold(s.threshold() + delta * step);
        if (_enginePtr) {
            _enginePtr->invalidateThresholds();
        }
        break;
    }
    case EditMode::NoteValue: {
        auto &s = _sequence->stage(_selectedStage);
        int step = _shiftHeld ? 12 : 1;
        s.setNoteIndex(s.noteIndex() + delta * step);
        break;
    }
    case EditMode::DualThreshold: {
        if (_secondaryStage >= 0) {
            auto &s1 = _sequence->stage(_selectedStage);
            auto &s2 = _sequence->stage(_secondaryStage);
            int step = _shiftHeld ? 1 : 8;
            s1.setThreshold(s1.threshold() + delta * step);
            s2.setThreshold(s2.threshold() + delta * step);
            if (_enginePtr) {
                _enginePtr->invalidateThresholds();
            }
        }
        break;
    }
    case EditMode::None:
        break;
    }
}

void DiscreteMapSequencePage::handleTopRowKey(int idx, bool shift) {
    if (shift) {
        _editMode = EditMode::NoteValue;
        _selectedStage = idx;
        _secondaryStage = -1;
    } else if (_selectedStage >= 0 && _selectedStage != idx) {
        _secondaryStage = idx;
        _editMode = EditMode::DualThreshold;
    } else {
        _selectedStage = idx;
        _secondaryStage = -1;
        _editMode = EditMode::Threshold;
    }
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
