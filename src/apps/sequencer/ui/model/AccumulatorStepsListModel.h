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
        return 16; // 0-15: 0=OFF, 1=S(global), 2-15=override
    }

    virtual int indexed(int row) const override {
        if (!_sequence || row >= 16) return 0;
        return _sequence->step(row).accumulatorStepValue();
    }

    virtual void setIndexed(int row, int index) override {
        if (!_sequence || row >= 16 || index < 0 || index > 15) return;

        const_cast<NoteSequence::Step&>(_sequence->step(row)).setAccumulatorStepValue(index);
    }

private:
    void formatName(int stepIndex, StringBuilder &str) const {
        str("STP%d", stepIndex + 1);
    }

    void formatValue(int stepIndex, StringBuilder &str) const {
        if (_sequence && stepIndex < 16) {
            int value = _sequence->step(stepIndex).accumulatorStepValue();
            if (value == 0) {
                str("OFF");
            } else if (value == 1) {
                str("S");  // S = use global sequence stepValue
            } else {
                str("%d", value);
            }
        } else {
            str("OFF");
        }
    }

    void editValue(int stepIndex, int value, bool shift) {
        if (!_sequence || stepIndex >= 16) return;

        int currentValue = _sequence->step(stepIndex).accumulatorStepValue();
        int step = shift ? 5 : 1;  // Shift = faster increment
        int newValue = currentValue + (value * step);

        // Wrap around: 0 → 15 → 0
        if (newValue < 0) newValue = 15;
        if (newValue > 15) newValue = 0;

        const_cast<NoteSequence::Step&>(_sequence->step(stepIndex)).setAccumulatorStepValue(newValue);
    }

    NoteSequence *_sequence;
};