#include "IndexedStepsPage.h"

#include "Pages.h"

#include "ui/painters/WindowPainter.h"
#include "ui/LedPainter.h"
#include "model/Project.h"
#include "core/utils/StringBuilder.h"

enum class ContextAction {
    Insert,
    Split,
    Delete,
    Copy,
    Paste,
    Last
};

static const ContextMenuModel::Item contextMenuItems[] = {
    { "INSERT" },
    { "SPLIT" },
    { "DELETE" },
    { "COPY" },
    { "PASTE" },
};

IndexedStepsPage::IndexedStepsPage(PageManager &manager, PageContext &context) :
    ListPage(manager, context, _listModel)
{}

void IndexedStepsPage::enter() {
    if (_project.selectedTrack().trackMode() == Track::TrackMode::Indexed) {
        _sequence = &_project.selectedIndexedSequence();
        _listModel.setSequence(_sequence);
        _listModel.setProject(&_project);
        setListModel(_listModel);
    } else {
        _sequence = nullptr;
        _listModel.setSequence(nullptr);
        _listModel.setProject(nullptr);
    }
}

void IndexedStepsPage::exit() {
}

void IndexedStepsPage::draw(Canvas &canvas) {
    WindowPainter::clear(canvas);
    WindowPainter::drawHeader(canvas, _model, _engine, "INDEXED STEPS");
    WindowPainter::drawFooter(canvas);
    ListPage::draw(canvas);
}

void IndexedStepsPage::updateLeds(Leds &leds) {
    ListPage::updateLeds(leds);
}

void IndexedStepsPage::keyPress(KeyPressEvent &event) {
    const auto &key = event.key();

    if (key.isContextMenu()) {
        contextShow();
        event.consume();
        return;
    }

    if (key.pageModifier()) {
        return;
    }

    if (!event.consumed()) {
        ListPage::keyPress(event);
    }
}

void IndexedStepsPage::contextShow() {
    showContextMenu(ContextMenu(
        contextMenuItems,
        int(ContextAction::Last),
        [&] (int index) { contextAction(index); },
        [&] (int index) { return contextActionEnabled(index); }
    ));
}

void IndexedStepsPage::contextAction(int index) {
    switch (ContextAction(index)) {
    case ContextAction::Insert:
        insertStep();
        break;
    case ContextAction::Split:
        splitStep();
        break;
    case ContextAction::Delete:
        deleteStep();
        break;
    case ContextAction::Copy:
        copyStep();
        break;
    case ContextAction::Paste:
        pasteStep();
        break;
    case ContextAction::Last:
        break;
    }
}

bool IndexedStepsPage::contextActionEnabled(int index) const {
    if (!_sequence) return false;

    switch (ContextAction(index)) {
    case ContextAction::Insert:
        return _sequence->canInsert();
    case ContextAction::Split:
        return _sequence->canInsert();
    case ContextAction::Delete:
        return _sequence->canDelete();
    case ContextAction::Copy:
        return true;
    case ContextAction::Paste:
        return _model.clipBoard().canPasteIndexedSequenceSteps();
    default:
        return true;
    }
}

void IndexedStepsPage::insertStep() {
    if (!_sequence) return;

    // Get current step index from selected row
    int currentRow = selectedRow();
    int stepIndex = currentRow / 3;  // 3 rows per step (note/duration/gate)

    // Insert at current step
    _sequence->insertStep(stepIndex);

    // If clipboard has content, paste it into the new step
    if (_model.clipBoard().canPasteIndexedSequenceSteps()) {
        ClipBoard::SelectedSteps selectedSteps;
        selectedSteps.set(stepIndex);
        _model.clipBoard().pasteIndexedSequenceSteps(*_sequence, selectedSteps);
        showMessage("STEP INSERTED (PASTE)");
    } else {
        showMessage("STEP INSERTED");
    }
}

void IndexedStepsPage::splitStep() {
    if (!_sequence) return;

    // Get current step index from selected row
    int currentRow = selectedRow();
    int stepIndex = currentRow / 3;

    _sequence->splitStep(stepIndex);

    showMessage("STEP SPLIT");
}

void IndexedStepsPage::deleteStep() {
    if (!_sequence) return;

    // Get current step index from selected row
    int currentRow = selectedRow();
    int stepIndex = currentRow / 3;  // 3 rows per step

    _sequence->deleteStep(stepIndex);

    showMessage("STEP DELETED");
}

void IndexedStepsPage::copyStep() {
    if (!_sequence) return;

    int currentRow = selectedRow();
    int stepIndex = currentRow / 3;

    ClipBoard::SelectedSteps selectedSteps;
    selectedSteps.set(stepIndex);

    _model.clipBoard().copyIndexedSequenceSteps(*_sequence, selectedSteps);
    showMessage("STEP COPIED");
}

void IndexedStepsPage::pasteStep() {
    if (!_sequence) return;

    int currentRow = selectedRow();
    int stepIndex = currentRow / 3;

    ClipBoard::SelectedSteps selectedSteps;
    selectedSteps.set(stepIndex);

    _model.clipBoard().pasteIndexedSequenceSteps(*_sequence, selectedSteps);
    showMessage("STEP PASTED");
}

void IndexedStepsPage::StepListModel::cell(int row, int column, StringBuilder &str) const {
    if (!_sequence) {
        return;
    }
    int index = stepIndex(row);
    const auto &step = _sequence->step(index);

    if (column == 0) {
        str("St%d %s", index + 1,
            isNoteRow(row) ? "Note" :
            isDurationRow(row) ? "Dur" : "Gate");
    } else {
        if (isNoteRow(row)) {
            // Display note index with scale note name if available
            if (_project) {
                const Scale &scale = _sequence->selectedScale(_project->selectedScale());
                FixedStringBuilder<8> noteName;
                int rootNote = _sequence->rootNote() < 0 ? _project->rootNote() : _sequence->rootNote();
                const auto &track = _project->selectedTrack().indexedTrack();
                int shift = track.octave() * scale.notesPerOctave() + track.transpose();
                int noteIndex = step.noteIndex() + shift;
                float volts = scale.noteToVolts(noteIndex);
                if (scale.isChromatic()) {
                    volts += rootNote * (1.f / 12.f);
                }
                scale.noteName(noteName, noteIndex, rootNote, Scale::Format::Short1);
                str("%.2f %s", volts, static_cast<const char *>(noteName));
            } else {
                str("%+d", int(step.noteIndex()));
            }
        } else if (isDurationRow(row)) {
            // Display duration in ticks
            str("%d", step.duration());
        } else if (isGateRow(row)) {
            // Display gate length as percentage or trigger
            if (step.gateLength() == IndexedSequence::GateLengthTrigger) {
                str("T");
            } else {
                str("%d%%", step.gateLength());
            }
        }
    }
}

void IndexedStepsPage::StepListModel::edit(int row, int column, int value, bool shift) {
    if (!_sequence || column != 1) {
        return;
    }
    int index = stepIndex(row);
    auto &step = _sequence->step(index);

    if (isNoteRow(row)) {
        // Edit note index: shift = octave (12 steps), normal = semitone
        int stepSize = shift ? 12 : 1;
        step.setNoteIndex(step.noteIndex() + value * stepSize);
    } else if (isDurationRow(row)) {
        // Edit duration: shift = divisor, normal = 1 tick
        int stepSize = shift ? _sequence->divisor() : 1;
        int newDuration = static_cast<int>(step.duration()) + value * stepSize;
        step.setDuration(clamp(newDuration, 0, int(IndexedSequence::MaxDuration)));
    } else if (isGateRow(row)) {
        // Edit gate length: shift = 1%, normal = 10%
        int stepSize = shift ? 1 : 10;
        int currentGate = step.gateLength();
        int newGate = currentGate + value * stepSize;
        if (currentGate == IndexedSequence::GateLengthTrigger && value < 0) {
            newGate = 100;
        } else if (currentGate <= 100 && newGate > 100) {
            newGate = IndexedSequence::GateLengthTrigger;
        }
        step.setGateLength(clamp(newGate, 0, int(IndexedSequence::GateLengthTrigger)));
    }
}
