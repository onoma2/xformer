#pragma once

#include "ListPage.h"

#include "model/IndexedSequence.h"

class Project;

class IndexedStepsPage : public ListPage {
public:
    IndexedStepsPage(PageManager &manager, PageContext &context);

    virtual void enter() override;
    virtual void exit() override;

    virtual void draw(Canvas &canvas) override;
    virtual void updateLeds(Leds &leds) override;

private:
    class StepListModel : public ListModel {
    public:
        void setSequence(IndexedSequence *sequence) { _sequence = sequence; }
        void setProject(Project *project) { _project = project; }

        virtual int rows() const override { return _sequence ? IndexedSequence::MaxSteps * 3 : 0; }
        virtual int columns() const override { return 2; }

        virtual void cell(int row, int column, StringBuilder &str) const override;
        virtual void edit(int row, int column, int value, bool shift) override;

    private:
        IndexedSequence *_sequence = nullptr;
        Project *_project = nullptr;

        static bool isNoteRow(int row) { return (row % 3) == 0; }
        static bool isDurationRow(int row) { return (row % 3) == 1; }
        static bool isGateRow(int row) { return (row % 3) == 2; }
        static int stepIndex(int row) { return row / 3; }
    };

    StepListModel _listModel;
    IndexedSequence *_sequence = nullptr;
};
