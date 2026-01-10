#include "NoteSequenceListModel.h"

const NoteSequenceListModel::Item NoteSequenceListModel::linearItems[] = {
    Mode,
    FirstStep,
    LastStep,
    RunMode,
    DivisorX,
    ClockMult,
    ResetMeasure,
    Scale,
    RootNote,
    Last
};

const NoteSequenceListModel::Item NoteSequenceListModel::reneItems[] = {
    Mode,
    FirstStep,
    LastStep,
    RunMode,
    DivisorX,
    DivisorY,
    ClockMult,
    ResetMeasure,
    Scale,
    RootNote,
    Last
};

const NoteSequenceListModel::Item NoteSequenceListModel::ikraItems[] = {
    Mode,
    FirstStep,
    LastStep,
    NoteFirstStep,
    NoteLastStep,
    RunMode,
    DivisorX,
    DivisorY,
    ClockMult,
    ResetMeasure,
    Scale,
    RootNote,
    Last
};
