#include "IndexedSequenceEditPage.h"

#include "Pages.h"

#include "ui/LedPainter.h"
#include "ui/painters/WindowPainter.h"
#include "ui/painters/SequencePainter.h"

#include "model/Scale.h"
#include "model/Routing.h"

#include "core/utils/StringBuilder.h"

enum class ContextAction {
    Init,
    Copy,
    Paste,
    Route,
    Insert,
    Split,
    Delete,
    Last
};

static const ContextMenuModel::Item seqContextMenuItems[] = {
    { "INIT" },
    { "COPY" },
    { "PASTE" },
    { "ROUTE" },
};

static const ContextMenuModel::Item stepContextMenuItems[] = {
    { "INSERT" },
    { "SPLIT" },
    { "DELETE" },
    { "COPY" },
    { "PASTE" },
};

IndexedSequenceEditPage::IndexedSequenceEditPage(PageManager &manager, PageContext &context) :
    BasePage(manager, context)
{
}

void IndexedSequenceEditPage::enter() {
}

void IndexedSequenceEditPage::exit() {
}

void IndexedSequenceEditPage::draw(Canvas &canvas) {
    WindowPainter::clear(canvas);
    WindowPainter::drawHeader(canvas, _model, _engine, "INDEXED EDIT");

    const auto &sequence = _project.selectedIndexedSequence();
    const auto &trackEngine = _engine.selectedTrackEngine().as<IndexedTrackEngine>();

    // 1. Top Section: Timeline Bar
    int totalTicks = 0;
    int activeLength = sequence.activeLength();
    for (int i = 0; i < activeLength; ++i) {
        totalTicks += sequence.step(i).duration();
    }

    if (totalTicks > 0) {
        const int barX = 8;
        const int barY = 14;
        const int barW = 240;
        const int barH = 16;

        int currentX = barX;
        float pixelsPerTick = (float)barW / totalTicks;

        for (int i = 0; i < activeLength; ++i) {
            const auto &step = sequence.step(i);
            int stepW = (int)(step.duration() * pixelsPerTick);
            if (stepW < 1 && step.duration() > 0) stepW = 1;
            
            if (i == activeLength - 1) {
                stepW = (barX + barW) - currentX;
            }

            bool selected = _stepSelection[i];
            bool active = (trackEngine.currentStep() == i);

            canvas.setColor(selected ? Color::Bright : (active ? Color::MediumBright : Color::Medium));
            canvas.drawRect(currentX, barY, stepW, barH);

            int gateW = (int)(stepW * (step.gateLength() / 100.0f));
            if (gateW > 0) {
                canvas.setColor(selected ? Color::Bright : (active ? Color::MediumBright : Color::Low));
                canvas.fillRect(currentX + 1, barY + 1, gateW, barH - 2);
            }

            currentX += stepW;
        }
    }

    // 2. Bottom Section: Info & Edit (F1, F2, F3)
    if (_stepSelection.any()) {
        int stepIndex = _stepSelection.first();
        const auto &step = sequence.step(stepIndex);
        
        const int y = 40;
        
        canvas.setFont(Font::Small);
        canvas.setBlendMode(BlendMode::Set);
        canvas.setColor(Color::Bright);

        // F1: Note
        FixedStringBuilder<16> noteStr;
        const auto &scale = sequence.selectedScale(_project.selectedScale());
        scale.noteName(noteStr, step.noteIndex(), sequence.rootNote(), Scale::Format::Short1);
        canvas.drawTextCentered(0, y, 51, 16, noteStr);

        // F2: Duration
        FixedStringBuilder<16> durStr;
        durStr("%d", step.duration());
        canvas.drawTextCentered(51, y, 51, 16, durStr);

        // F3: Gate
        FixedStringBuilder<16> gateStr;
        gateStr("%d%%", step.gateLength());
        canvas.drawTextCentered(102, y, 51, 16, gateStr);
    } else {
        int currentStep = trackEngine.currentStep() + 1;
        int totalSteps = sequence.activeLength();
        const auto &timeSig = _project.timeSignature();
        uint32_t measureTicks = timeSig.measureDivisor();
        float bars = (float)totalTicks / measureTicks;

        canvas.setColor(Color::Medium);
        FixedStringBuilder<32> infoStr;
        infoStr("STEP %d/%d  BARS %.1f", currentStep, totalSteps, bars);
        canvas.drawTextCentered(0, 40, 256, 16, infoStr);
    }

    // Footer Labels
    // F4 toggles between SEQ and STEP context mode
    const char *footerLabels[] = { 
        "NOTE", "DUR", "GATE", 
        (_contextMode == ContextMode::Sequence) ? "SEQ" : "STEP", 
        "NEXT" 
    };
    WindowPainter::drawFooter(canvas, footerLabels, pageKeyState(), (int)_editMode);
}

void IndexedSequenceEditPage::updateLeds(Leds &leds) {
    const auto &sequence = _project.selectedIndexedSequence();
    const auto &trackEngine = _engine.selectedTrackEngine().as<IndexedTrackEngine>();
    int currentStep = trackEngine.currentStep();

    int stepOffset = this->stepOffset();
    
    for (int i = 0; i < 16; ++i) {
        int stepIndex = stepOffset + i;
        if (stepIndex >= sequence.activeLength()) break;

        bool selected = _stepSelection[stepIndex];
        bool playing = (stepIndex == currentStep);

        bool green = playing;
        bool red = selected;

        leds.set(MatrixMap::fromStep(i), red, green);
    }

    LedPainter::drawSelectedSequenceSection(leds, _section);
}

void IndexedSequenceEditPage::keyPress(KeyPressEvent &event) {
    const auto &key = event.key();

    if (key.isContextMenu()) {
        contextShow();
        event.consume();
        return;
    }

    if (key.pageModifier()) {
        return;
    }

    if (key.isStep()) {
        int stepIndex = stepOffset() + key.step();
        if (stepIndex < _project.selectedIndexedSequence().activeLength()) {
            _stepSelection.keyPress(event, stepOffset());
        }
        event.consume();
        return;
    }

    if (key.isFunction()) {
        int fn = key.function();
        if (fn == 3) {
            // F4: Toggle Context Mode
            _contextMode = (_contextMode == ContextMode::Sequence) ? ContextMode::Step : ContextMode::Sequence;
        }
        // ... F5 page toggling placeholder ...
        event.consume();
        return;
    }
    
    if (key.isLeft()) {
        _section = std::max(0, _section - 1);
        event.consume();
    }
    if (key.isRight()) {
        _section = std::min(1, _section + 1);
        event.consume();
    }
}

void IndexedSequenceEditPage::encoder(EncoderEvent &event) {
    auto &sequence = _project.selectedIndexedSequence();
    
    if (!_stepSelection.any()) return;

    for (int i = 0; i < IndexedSequence::MaxSteps; ++i) {
        if (_stepSelection[i]) {
            auto &step = sequence.step(i);
            bool shift = globalKeyState()[Key::Shift];
            
            switch (_editMode) {
            case EditMode::Note:
                step.setNoteIndex(step.noteIndex() + event.value() * (shift ? 12 : 1));
                break;
            case EditMode::Duration: {
                int div = sequence.divisor();
                int stepVal = shift ? 1 : div;
                int newDur = step.duration() + event.value() * stepVal;
                step.setDuration(clamp(newDur, 0, 65535));
                break;
            }
            case EditMode::Gate:
                step.setGateLength(clamp(step.gateLength() + event.value() * (shift ? 1 : 5), 0, 100));
                break;
            }
        }
    }
    
    event.consume();
}

void IndexedSequenceEditPage::keyDown(KeyEvent &event) {
    const auto &key = event.key();
    if (key.isStep()) {
        _stepSelection.keyDown(event, stepOffset());
    }
    
    if (key.isFunction()) {
        int fn = key.function();
        if (fn == 0) _editMode = EditMode::Note;
        if (fn == 1) _editMode = EditMode::Duration;
        if (fn == 2) _editMode = EditMode::Gate;
    }
}

void IndexedSequenceEditPage::keyUp(KeyEvent &event) {
    const auto &key = event.key();
    if (key.isStep()) {
        _stepSelection.keyUp(event, stepOffset());
    }
}

void IndexedSequenceEditPage::contextShow() {
    if (_contextMode == ContextMode::Sequence) {
        showContextMenu(ContextMenu(
            seqContextMenuItems,
            4, // count
            [&] (int index) { contextAction(index); },
            [&] (int index) { return true; }
        ));
    } else {
        showContextMenu(ContextMenu(
            stepContextMenuItems,
            5, // count
            [&] (int index) { contextAction(index + 4); }, // Offset by 4 to map to ContextAction enum (Insert starts after Route)
            [&] (int index) { return contextActionEnabled(index + 4); }
        ));
    }
}

void IndexedSequenceEditPage::contextAction(int index) {
    switch (ContextAction(index)) {
    // SEQ Actions
    case ContextAction::Init:
        initSequence();
        break;
    case ContextAction::Copy:
        copyStep(); // Wait, SEQ copy behavior? Currently sticking to step copy as per previous code
        break;
    case ContextAction::Paste:
        pasteStep();
        break;
    case ContextAction::Route:
        routeSequence();
        break;
    // STEP Actions
    case ContextAction::Insert:
        insertStep();
        break;
    case ContextAction::Split:
        splitStep();
        break;
    case ContextAction::Delete:
        deleteStep();
        break;
    default:
        break;
    }
}

bool IndexedSequenceEditPage::contextActionEnabled(int index) const {
    auto &sequence = _project.selectedIndexedSequence();
    switch (ContextAction(index)) {
    case ContextAction::Paste:
        return _model.clipBoard().canPasteIndexedSequenceSteps();
    case ContextAction::Insert:
    case ContextAction::Split:
        return sequence.canInsert();
    case ContextAction::Delete:
        return sequence.canDelete();
    default:
        return true;
    }
}

void IndexedSequenceEditPage::initSequence() {
    _project.selectedIndexedSequence().clear();
    showMessage("SEQUENCE CLEARED");
}

void IndexedSequenceEditPage::routeSequence() {
    _manager.pages().top.editRoute(Routing::Target::Divisor, _project.selectedTrackIndex());
}

void IndexedSequenceEditPage::insertStep() {
    auto &sequence = _project.selectedIndexedSequence();
    if (_stepSelection.any()) {
        sequence.insertStep(_stepSelection.first());
        // Auto-paste logic if clipboard valid
        if (_model.clipBoard().canPasteIndexedSequenceSteps()) {
            ClipBoard::SelectedSteps steps;
            steps.set(_stepSelection.first());
            _model.clipBoard().pasteIndexedSequenceSteps(sequence, steps);
            showMessage("STEP INSERTED (PASTE)");
        } else {
            showMessage("STEP INSERTED");
        }
    }
}

void IndexedSequenceEditPage::splitStep() {
    auto &sequence = _project.selectedIndexedSequence();
    if (!_stepSelection.any()) return;

    int selectedCount = _stepSelection.count();
    if (sequence.activeLength() + selectedCount > IndexedSequence::MaxSteps) {
        showMessage("CANNOT SPLIT: FULL");
        return;
    }

    // Iterate backwards to avoid index shifting issues
    bool splitAny = false;
    for (int i = sequence.activeLength() - 1; i >= 0; --i) {
        if (_stepSelection[i]) {
            sequence.splitStep(i);
            splitAny = true;
        }
    }

    if (splitAny) {
        // Clear selection because indices have shifted
        _stepSelection.clear(); 
        showMessage("STEPS SPLIT");
    }
}

void IndexedSequenceEditPage::deleteStep() {
    auto &sequence = _project.selectedIndexedSequence();
    if (!_stepSelection.any()) return;

    bool deletedAny = false;
    for (int i = sequence.activeLength() - 1; i >= 0; --i) {
        if (_stepSelection[i]) {
            sequence.deleteStep(i);
            deletedAny = true;
        }
    }

    if (deletedAny) {
        _stepSelection.clear();
        showMessage("STEPS DELETED");
    }
}

void IndexedSequenceEditPage::copyStep() {
    auto &sequence = _project.selectedIndexedSequence();
    if (_stepSelection.any()) {
        ClipBoard::SelectedSteps steps = _stepSelection.selected().to_ullong();
        _model.clipBoard().copyIndexedSequenceSteps(sequence, steps);
        showMessage("STEPS COPIED");
    }
}

void IndexedSequenceEditPage::pasteStep() {
    auto &sequence = _project.selectedIndexedSequence();
    if (_stepSelection.any()) {
        ClipBoard::SelectedSteps steps = _stepSelection.selected().to_ullong();
        _model.clipBoard().pasteIndexedSequenceSteps(sequence, steps);
        showMessage("STEPS PASTED");
    }
}

IndexedSequence::Step& IndexedSequenceEditPage::step(int index) {
    return _project.selectedIndexedSequence().step(index);
}

const IndexedSequence::Step& IndexedSequenceEditPage::step(int index) const {
    return _project.selectedIndexedSequence().step(index);
}
