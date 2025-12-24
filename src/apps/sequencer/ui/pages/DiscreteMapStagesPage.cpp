#include "DiscreteMapStagesPage.h"

#include "Pages.h"

#include "ui/painters/WindowPainter.h"
#include "ui/LedPainter.h"
#include "model/Project.h"

DiscreteMapStagesPage::DiscreteMapStagesPage(PageManager &manager, PageContext &context) :
    ListPage(manager, context, _listModel)
{}

void DiscreteMapStagesPage::enter() {
    if (_project.selectedTrack().trackMode() == Track::TrackMode::DiscreteMap) {
        _sequence = &_project.selectedDiscreteMapSequence();
        _listModel.setSequence(_sequence);
        _listModel.setProject(&_project);
        setListModel(_listModel);
    } else {
        _sequence = nullptr;
        _listModel.setSequence(nullptr);
        _listModel.setProject(nullptr);
    }
}

void DiscreteMapStagesPage::exit() {
}

void DiscreteMapStagesPage::draw(Canvas &canvas) {
    WindowPainter::clear(canvas);
    WindowPainter::drawHeader(canvas, _model, _engine, "DMAP STAGES");
    WindowPainter::drawFooter(canvas);
    ListPage::draw(canvas);
}

void DiscreteMapStagesPage::updateLeds(Leds &leds) {
    ListPage::updateLeds(leds);
}

const char *DiscreteMapStagesPage::StageListModel::dirName(DiscreteMapSequence::Stage::TriggerDir dir) {
    switch (dir) {
    case DiscreteMapSequence::Stage::TriggerDir::Rise: return "Rise";
    case DiscreteMapSequence::Stage::TriggerDir::Fall: return "Fall";
    case DiscreteMapSequence::Stage::TriggerDir::Off:  return "Off";
    case DiscreteMapSequence::Stage::TriggerDir::Both: return "Both";
    }
    return "";
}

DiscreteMapSequence::Stage::TriggerDir DiscreteMapStagesPage::StageListModel::cycleDir(DiscreteMapSequence::Stage::TriggerDir dir, int delta) {
    return DiscreteMapSequence::Stage::advanceDirection(dir, delta);
}

void DiscreteMapStagesPage::StageListModel::cell(int row, int column, StringBuilder &str) const {
    if (!_sequence) {
        return;
    }
    int index = stageIndex(row);
    const auto &stage = _sequence->stage(index);

    if (column == 0) {
        str("St%d %s", index + 1,
            isDirRow(row) ? "Dir" :
            isThresholdRow(row) ? "Thresh" : "Note");
    } else {
        if (isDirRow(row)) {
            str(dirName(stage.direction()));
        } else if (isThresholdRow(row)) {
            str("%+d", int(stage.threshold()));
        } else if (isNoteRow(row)) {
            if (_project) {
                const Scale &scale = _sequence->selectedScale(_project->selectedScale());
                scale.noteName(str, stage.noteIndex(), _sequence->rootNote(), Scale::Format::Short1);
            } else {
                str("%+d", int(stage.noteIndex()));
            }
        }
    }
}

void DiscreteMapStagesPage::StageListModel::edit(int row, int column, int value, bool shift) {
    if (!_sequence || column != 1) {
        return;
    }
    int index = stageIndex(row);
    auto &stage = _sequence->stage(index);

    if (isDirRow(row)) {
        stage.setDirection(cycleDir(stage.direction(), value > 0 ? 1 : -1));
    } else if (isThresholdRow(row)) {
        int step = shift ? 1 : 8;
        stage.setThreshold(stage.threshold() + value * step);
    } else if (isNoteRow(row)) {
        int step = shift ? 12 : 1;
        stage.setNoteIndex(stage.noteIndex() + value * step);
    }
}
