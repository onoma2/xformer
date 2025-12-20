#include "IndexedStepsPage.h"

#include "Pages.h"

#include "ui/painters/WindowPainter.h"
#include "ui/LedPainter.h"
#include "model/Project.h"

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
                scale.noteName(str, step.noteIndex(), _sequence->rootNote(), Scale::Format::Short1);
            } else {
                str("%+d", int(step.noteIndex()));
            }
        } else if (isDurationRow(row)) {
            // Display duration in ticks
            str("%d", step.duration());
        } else if (isGateRow(row)) {
            // Display gate length as percentage
            str("%d%%", step.gateLength());
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
        // Edit duration: shift = fine (1 tick), normal = coarse (48 ticks = 16th note at 192 PPQN)
        int stepSize = shift ? 1 : 48;
        int newDuration = static_cast<int>(step.duration()) + value * stepSize;
        step.setDuration(clamp(newDuration, 0, 65535));
    } else if (isGateRow(row)) {
        // Edit gate length: shift = 1%, normal = 10%
        int stepSize = shift ? 1 : 10;
        int newGate = static_cast<int>(step.gateLength()) + value * stepSize;
        step.setGateLength(clamp(newGate, 0, 100));
    }
}
