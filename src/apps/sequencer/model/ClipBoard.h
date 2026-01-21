#pragma once

#include "Config.h"

#include "Track.h"
#include "NoteSequence.h"
#include "CurveSequence.h"
#include "TuesdaySequence.h"
#include "DiscreteMapSequence.h"
#include "IndexedSequence.h"
#include "Project.h"
#include "UserScale.h"

#include "core/utils/Container.h"

#include <bitset>

class ClipBoard {
public:
    using SelectedSteps = std::bitset<CONFIG_STEP_COUNT>;

    ClipBoard(Project &project);

    void clear();

    void copyTrack(const Track &track);
    void copyNoteSequence(const NoteSequence &noteSequence);
    void copyNoteSequenceSteps(const NoteSequence &noteSequence, const SelectedSteps &selectedSteps);
    void copyCurveSequence(const CurveSequence &curveSequence);
    void copyCurveSequenceSteps(const CurveSequence &curveSequence, const SelectedSteps &selectedSteps);
    void copyIndexedSequence(const IndexedSequence &sequence);
    void copyIndexedSequenceSteps(const IndexedSequence &sequence, const SelectedSteps &selectedSteps);
    void copyDiscreteMapSequence(const DiscreteMapSequence &sequence);
    void copyTuesdaySequence(const TuesdaySequence &sequence);
    void copyPattern(int patternIndex);
    void copyUserScale(const UserScale &userScale);

    void pasteTrack(Track &track) const;
    void pasteNoteSequence(NoteSequence &noteSequence) const;
    void pasteNoteSequenceSteps(NoteSequence &noteSequence, const SelectedSteps &selectedSteps) const;
    void pasteCurveSequence(CurveSequence &curveSequence) const;
    void pasteCurveSequenceSteps(CurveSequence &curveSequence, const SelectedSteps &selectedSteps) const;
    void pasteIndexedSequence(IndexedSequence &sequence) const;
    void pasteIndexedSequenceSteps(IndexedSequence &sequence, const SelectedSteps &selectedSteps) const;
    void pasteDiscreteMapSequence(DiscreteMapSequence &sequence) const;
    void pasteTuesdaySequence(TuesdaySequence &sequence) const;
    void pastePattern(int patternIndex) const;
    void pasteUserScale(UserScale &userScale) const;

    bool canPasteTrack() const;
    bool canPasteNoteSequence() const;
    bool canPasteNoteSequenceSteps() const;
    bool canPasteCurveSequence() const;
    bool canPasteCurveSequenceSteps() const;
    bool canPasteIndexedSequence() const;
    bool canPasteIndexedSequenceSteps() const;
    bool canPasteDiscreteMapSequence() const;
    bool canPasteTuesdaySequence() const;
    bool canPastePattern() const;
    bool canPasteUserScale() const;

private:
    enum class Type : uint8_t {
        None,
        Track,
        NoteSequence,
        NoteSequenceSteps,
        CurveSequence,
        CurveSequenceSteps,
        IndexedSequence,
        IndexedSequenceSteps,
        DiscreteMapSequence,
        TuesdaySequence,
        Pattern,
        UserScale,
    };

    struct NoteSequenceSteps {
        NoteSequence sequence;
        SelectedSteps selected;
    };

    struct CurveSequenceSteps {
        CurveSequence sequence;
        SelectedSteps selected;
    };

    struct IndexedSequenceSteps {
        IndexedSequence sequence;
        SelectedSteps selected;
    };

    struct Pattern {
        struct {
            Track::TrackMode trackMode;
            union {
                NoteSequence note;
                CurveSequence curve;
                TuesdaySequence tuesday;
                DiscreteMapSequence discreteMap;
                IndexedSequence indexed;
                TeletypeTrack::PatternSlot teletype;
            } data;
        } sequences[CONFIG_TRACK_COUNT];
    };

    Project &_project;
    Type _type = Type::None;
    Container<Track, NoteSequence, NoteSequenceSteps, CurveSequence, CurveSequenceSteps, IndexedSequence, IndexedSequenceSteps, DiscreteMapSequence, TuesdaySequence, Pattern, UserScale> _container;
};
