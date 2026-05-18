#pragma once

#include "Config.h"
#include "ListModel.h"
#include "model/StochasticSequence.h"
#include "model/Project.h"

class StochasticSequenceListModel : public ListModel {
public:
    enum Item {
        Gate,
        GateProbability,
        Note,
        NoteVariationProbability,
        NoteOctaveProbability,
        Length,
        LengthVariationRange,
        LengthVariationProbability,
        Retrigger,
        RetriggerProbability,
        Condition,
        Offset,
        Slide,
        Accent,
        Legato,
        Last
    };

    StochasticSequenceListModel() {}

    void setSequence(StochasticSequence &sequence, Project &project) {
        _sequence = &sequence;
        _project = &project;
    }

    void setStepIndex(int index) {
        _stepIndex = clamp(index, 0, CONFIG_STEP_COUNT - 1);
    }

    virtual int rows() const override {
        return Last;
    }

    virtual int columns() const override {
        return 2;
    }

    virtual void cell(int row, int column, StringBuilder &str) const override {
        if (column == 0) {
            switch (Item(row)) {
            case Gate:                          str("Gate"); break;
            case GateProbability:               str("Gate Prob"); break;
            case Note:                          str("Note"); break;
            case NoteVariationProbability:      str("Note Prob"); break;
            case NoteOctaveProbability:         str("Octave Prob"); break;
            case Length:                        str("Length"); break;
            case LengthVariationRange:          str("Length Var"); break;
            case LengthVariationProbability:    str("Length Prob"); break;
            case Retrigger:                     str("Retrigger"); break;
            case RetriggerProbability:          str("Retrig Prob"); break;
            case Condition:                     str("Condition"); break;
            case Offset:                        str("Offset"); break;
            case Slide:                         str("Slide"); break;
            case Accent:                        str("Accent"); break;
            case Legato:                        str("Legato"); break;
            case Last:                          break;
            }
        } else if (column == 1) {
            const auto &step = _sequence->step(_stepIndex);
            switch (Item(row)) {
            case Gate:                          str(step.gate() ? "On" : "Off"); break;
            case GateProbability:               str("%d", step.gateProbability()); break;
            case Note:                          _sequence->printNote(str, step.note(), _project->rootNote(), _project->scale()); break;
            case NoteVariationProbability:      str("%d", step.noteVariationProbability()); break;
            case NoteOctaveProbability:         str("%d", step.noteOctaveProbability()); break;
            case Length:                        str("%d", step.length()); break;
            case LengthVariationRange:          str("%d", step.lengthVariationRange()); break;
            case LengthVariationProbability:    str("%d", step.lengthVariationProbability()); break;
            case Retrigger:                     str("%d", step.retrigger()); break;
            case RetriggerProbability:          str("%d", step.retriggerProbability()); break;
            case Condition:                     Types::printCondition(str, step.condition(), Types::ConditionFormat::Short1); break;
            case Offset:                        str("%d", step.gateOffset()); break;
            case Slide:                         str(step.slide() ? "On" : "Off"); break;
            case Accent:                        str(step.accent() ? "On" : "Off"); break;
            case Legato:                        str(step.legato() ? "On" : "Off"); break;
            case Last:                          break;
            }
        }
    }

    virtual void edit(int row, int column, int value, bool shift) override {
        if (column == 1) {
            auto step = _sequence->step(_stepIndex);
            switch (Item(row)) {
            case Gate:                          step.setGate(value > 0); break;
            case GateProbability:               step.setGateProbability(step.gateProbability() + value); break;
            case Note:                          step.setNote(step.note() + value); break;
            case NoteVariationProbability:      step.setNoteVariationProbability(step.noteVariationProbability() + value); break;
            case NoteOctaveProbability:         step.setNoteOctaveProbability(step.noteOctaveProbability() + value); break;
            case Length:                        step.setLength(step.length() + value); break;
            case LengthVariationRange:          step.setLengthVariationRange(step.lengthVariationRange() + value); break;
            case LengthVariationProbability:    step.setLengthVariationProbability(step.lengthVariationProbability() + value); break;
            case Retrigger:                     step.setRetrigger(step.retrigger() + value); break;
            case RetriggerProbability:          step.setRetriggerProbability(step.retriggerProbability() + value); break;
            case Condition:                     step.setCondition(ModelUtils::adjustedEnum(step.condition(), value)); break;
            case Offset:                        step.setGateOffset(step.gateOffset() + value); break;
            case Slide:                         step.setSlide(value > 0); break;
            case Accent:                        step.setAccent(value > 0); break;
            case Legato:                        step.setLegato(value > 0); break;
            case Last:                          break;
            }
        }
    }

private:
    StochasticSequence *_sequence = nullptr;
    Project *_project = nullptr;
    int _stepIndex = 0;
};
