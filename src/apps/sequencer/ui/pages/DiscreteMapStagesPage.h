#pragma once

#include "ListPage.h"

#include "model/DiscreteMapSequence.h"

class Project;

class DiscreteMapStagesPage : public ListPage {
public:
    DiscreteMapStagesPage(PageManager &manager, PageContext &context);

    virtual void enter() override;
    virtual void exit() override;

    virtual void draw(Canvas &canvas) override;
    virtual void updateLeds(Leds &leds) override;

private:
    class StageListModel : public ListModel {
    public:
        void setSequence(DiscreteMapSequence *sequence) { _sequence = sequence; }
        void setProject(Project *project) { _project = project; }

        virtual int rows() const override { return _sequence ? DiscreteMapSequence::StageCount * 3 : 0; }
        virtual int columns() const override { return 2; }

        virtual void cell(int row, int column, StringBuilder &str) const override;
        virtual void edit(int row, int column, int value, bool shift) override;

    private:
        DiscreteMapSequence *_sequence = nullptr;
        Project *_project = nullptr;
        static const char *dirName(DiscreteMapSequence::Stage::TriggerDir dir);
        static DiscreteMapSequence::Stage::TriggerDir cycleDir(DiscreteMapSequence::Stage::TriggerDir dir, int delta);
        static bool isDirRow(int row) { return (row % 3) == 0; }
        static bool isThresholdRow(int row) { return (row % 3) == 1; }
        static bool isNoteRow(int row) { return (row % 3) == 2; }
        static int stageIndex(int row) { return row / 3; }
    };

    StageListModel _listModel;
    DiscreteMapSequence *_sequence = nullptr;
};
