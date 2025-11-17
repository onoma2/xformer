#pragma once

#include "ListModel.h"

#include "model/NoteSequence.h"

class AccumulatorStepsListModel : public ListModel {
public:
    enum Item {
        Step1, Step2, Step3, Step4, 
        Step5, Step6, Step7, Step8,
        Step9, Step10, Step11, Step12,
        Step13, Step14, Step15, Step16,
        Last
    };

    AccumulatorStepsListModel() : _sequence(nullptr) {}

    void setSequence(NoteSequence *sequence) {
        _sequence = sequence;
    }

    virtual int rows() const override {
        return _sequence ? 16 : 0; // 16 steps
    }

    virtual int columns() const override {
        return 2;
    }

    virtual void cell(int row, int column, StringBuilder &str) const override {
        if (!_sequence || row >= 16) return;
        
        if (column == 0) {
            formatName(row, str);
        } else if (column == 1) {
            formatValue(row, str);
        }
    }

    virtual void edit(int row, int column, int value, bool shift) override {
        if (!_sequence || column != 1 || row >= 16) return;
        
        editValue(row, value, shift);
    }

    virtual int indexedCount(int row) const override {
        return 2; // true/false for each step
    }

    virtual int indexed(int row) const override {
        if (!_sequence || row >= 16) return 0;
        return _sequence->step(row).isAccumulatorTrigger() ? 1 : 0;
    }

    virtual void setIndexed(int row, int index) override {
        if (!_sequence || row >= 16 || index < 0 || index > 1) return;
        
        const_cast<NoteSequence::Step&>(_sequence->step(row)).setAccumulatorTrigger(index != 0);
    }

private:
    void formatName(int stepIndex, StringBuilder &str) const {
        str("STP%d", stepIndex + 1);
    }

    void formatValue(int stepIndex, StringBuilder &str) const {
        if (_sequence && stepIndex < 16) {
            str(_sequence->step(stepIndex).isAccumulatorTrigger() ? "ON" : "OFF");
        } else {
            str("OFF");
        }
    }

    void editValue(int stepIndex, int value, bool shift) {
        if (!_sequence || stepIndex >= 16) return;
        
        bool newValue = !_sequence->step(stepIndex).isAccumulatorTrigger();
        const_cast<NoteSequence::Step&>(_sequence->step(stepIndex)).setAccumulatorTrigger(newValue);
    }

    NoteSequence *_sequence;
};