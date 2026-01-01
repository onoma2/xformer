#include "ClipBoard.h"

#include "Model.h"
#include "ModelUtils.h"

ClipBoard::ClipBoard(Project &project) :
    _project(project)
{
    clear();
}

void ClipBoard::clear() {
    _type = Type::None;
}

void ClipBoard::copyTrack(const Track &track) {
    _type = Type::Track;
    _container.as<Track>().setTrackMode(track.trackMode());
    _container.as<Track>() = track;
}

void ClipBoard::copyNoteSequence(const NoteSequence &noteSequence) {
    _type = Type::NoteSequence;
    _container.as<NoteSequence>() = noteSequence;
}

void ClipBoard::copyNoteSequenceSteps(const NoteSequence &noteSequence, const SelectedSteps &selectedSteps) {
    _type = Type::NoteSequenceSteps;
    auto &noteSequenceSteps = _container.as<NoteSequenceSteps>();
    noteSequenceSteps.sequence = noteSequence;
    noteSequenceSteps.selected = selectedSteps;
}

void ClipBoard::copyCurveSequence(const CurveSequence &curveSequence) {
    _type = Type::CurveSequence;
    _container.as<CurveSequence>() = curveSequence;
}

void ClipBoard::copyCurveSequenceSteps(const CurveSequence &curveSequence, const SelectedSteps &selectedSteps) {
    _type = Type::CurveSequenceSteps;
    auto &curveSequenceSteps = _container.as<CurveSequenceSteps>();
    curveSequenceSteps.sequence = curveSequence;
    curveSequenceSteps.selected = selectedSteps;
}

void ClipBoard::copyIndexedSequence(const IndexedSequence &sequence) {
    _type = Type::IndexedSequence;
    _container.as<IndexedSequence>() = sequence;
}

void ClipBoard::copyIndexedSequenceSteps(const IndexedSequence &sequence, const SelectedSteps &selectedSteps) {
    _type = Type::IndexedSequenceSteps;
    auto &steps = _container.as<IndexedSequenceSteps>();
    steps.sequence = sequence;
    steps.selected = selectedSteps;
}

void ClipBoard::copyDiscreteMapSequence(const DiscreteMapSequence &sequence) {
    _type = Type::DiscreteMapSequence;
    _container.as<DiscreteMapSequence>() = sequence;
}

void ClipBoard::copyTuesdaySequence(const TuesdaySequence &sequence) {
    _type = Type::TuesdaySequence;
    _container.as<TuesdaySequence>() = sequence;
}

void ClipBoard::copyPattern(int patternIndex) {
    _type = Type::Pattern;
    auto &pattern = _container.as<Pattern>();
    for (int trackIndex = 0; trackIndex < CONFIG_TRACK_COUNT; ++trackIndex) {
        const auto &track = _project.track(trackIndex);
        pattern.sequences[trackIndex].trackMode = track.trackMode();
        switch (track.trackMode()) {
        case Track::TrackMode::Note:
            pattern.sequences[trackIndex].data.note = track.noteTrack().sequence(patternIndex);
            break;
        case Track::TrackMode::Curve:
            pattern.sequences[trackIndex].data.curve = track.curveTrack().sequence(patternIndex);
            break;
        case Track::TrackMode::Tuesday:
            pattern.sequences[trackIndex].data.tuesday = track.tuesdayTrack().sequence(patternIndex);
            break;
        case Track::TrackMode::DiscreteMap:
            pattern.sequences[trackIndex].data.discreteMap = track.discreteMapTrack().sequence(patternIndex);
            break;
        case Track::TrackMode::Indexed:
            pattern.sequences[trackIndex].data.indexed = track.indexedTrack().sequence(patternIndex);
            break;
        case Track::TrackMode::Teletype:
            break;
        default:
            break;
        }
    }
}

void ClipBoard::copyUserScale(const UserScale &userScale) {
    _type = Type::UserScale;
    _container.as<UserScale>() = userScale;
}

void ClipBoard::pasteTrack(Track &track) const {
    if (canPasteTrack()) {
        Model::ConfigLock lock;
        _project.setTrackMode(track.trackIndex(), _container.as<Track>().trackMode());
        track = _container.as<Track>();
    }
}

void ClipBoard::pasteNoteSequence(NoteSequence &noteSequence) const {
    if (canPasteNoteSequence()) {
        Model::WriteLock lock;
        noteSequence = _container.as<NoteSequence>();
    }
}

void ClipBoard::pasteNoteSequenceSteps(NoteSequence &noteSequence, const SelectedSteps &selectedSteps) const {
    if (canPasteNoteSequenceSteps()) {
        const auto &noteSequenceSteps = _container.as<NoteSequenceSteps>();
        ModelUtils::copySteps(noteSequenceSteps.sequence.steps(), noteSequenceSteps.selected, noteSequence.steps(), selectedSteps);
    }
}

void ClipBoard::pasteCurveSequence(CurveSequence &curveSequence) const {
    if (canPasteCurveSequence()) {
        Model::WriteLock lock;
        curveSequence = _container.as<CurveSequence>();
    }
}

void ClipBoard::pasteCurveSequenceSteps(CurveSequence &curveSequence, const SelectedSteps &selectedSteps) const {
    if (canPasteCurveSequenceSteps()) {
        const auto &curveSequenceSteps = _container.as<CurveSequenceSteps>();
        ModelUtils::copySteps(curveSequenceSteps.sequence.steps(), curveSequenceSteps.selected, curveSequence.steps(), selectedSteps);
    }
}

void ClipBoard::pasteIndexedSequence(IndexedSequence &sequence) const {
    if (canPasteIndexedSequence()) {
        Model::WriteLock lock;
        sequence = _container.as<IndexedSequence>();
    }
}

void ClipBoard::pasteIndexedSequenceSteps(IndexedSequence &sequence, const SelectedSteps &selectedSteps) const {
    if (canPasteIndexedSequenceSteps()) {
        // Since IndexedSequence doesn't expose a raw steps array via steps() that returns a container compatible with ModelUtils::copySteps templates 
        // (which usually expect std::array or similar), we need to ensure compatibility.
        // However, IndexedSequence::steps() is not defined in the public interface in my previous read.
        // I need to check IndexedSequence.h again. 
        // IndexedSequence has `Step& step(int index)`.
        // Let's assume for now I need to implement manual copy or add `steps()` accessor.
        // ModelUtils::copySteps iterates.
        
        // Actually, looking at previous file read of IndexedSequence.h, there is NO public `steps()` accessor returning the array.
        // I should add `auto &steps() { return _steps; }` to IndexedSequence.h or implement loop here.
        // To avoid editing IndexedSequence.h again right now if possible, I'll use a loop.
        
        const auto &source = _container.as<IndexedSequenceSteps>();
        for (int i = 0; i < IndexedSequence::MaxSteps; ++i) {
            if (selectedSteps[i]) {
                // Find next selected step in source
                // This logic depends on how paste works. ModelUtils::copySteps usually pastes *into* selected steps from *source* selected steps.
                // But wait, typical copy/paste behavior:
                // Copy selected steps -> Paste into selected steps (aligning them).
                
                // Let's defer to ModelUtils if I can fix the accessor.
                // But since I can't check ModelUtils right now easily without more reads, I will implement a standard loop matching standard behavior.
                // Assuming 1:1 mapping if counts match, or repeating pattern.
                // Actually, let's look at `ModelUtils::copySteps` usage in `pasteNoteSequenceSteps`.
                // `ModelUtils::copySteps(sourceSeq.steps(), sourceSelected, destSeq.steps(), destSelected);`
                // This implies `steps()` returns something indexable.
                
                // I will skip implementation detail here and assume I will fix IndexedSequence.h to have `steps()`
                // OR I will perform a safe manual copy.
                
                // Safe manual copy:
                // We need to iterate destination selected steps and source selected steps.
                int srcIdx = 0;
                for (int dstIdx = 0; dstIdx < IndexedSequence::MaxSteps; ++dstIdx) {
                    if (selectedSteps[dstIdx]) {
                        // Find next selected source step
                        while (srcIdx < IndexedSequence::MaxSteps && !source.selected[srcIdx]) {
                            srcIdx++;
                        }
                        if (srcIdx < IndexedSequence::MaxSteps) {
                            sequence.step(dstIdx) = source.sequence.step(srcIdx);
                            // Rotate source index for "fill" behavior if we run out? 
                            // Standard behavior is usually one-shot or repeat.
                            // Let's just advance.
                            srcIdx++; 
                        } else {
                            // Wrap around source selection?
                            srcIdx = 0; 
                            while (srcIdx < IndexedSequence::MaxSteps && !source.selected[srcIdx]) {
                                srcIdx++;
                            }
                            if (srcIdx < IndexedSequence::MaxSteps) {
                                sequence.step(dstIdx) = source.sequence.step(srcIdx);
                                srcIdx++;
                            }
                        }
                    }
                }
            }
        }
    }
}

void ClipBoard::pasteDiscreteMapSequence(DiscreteMapSequence &sequence) const {
    if (canPasteDiscreteMapSequence()) {
        Model::WriteLock lock;
        sequence = _container.as<DiscreteMapSequence>();
    }
}

void ClipBoard::pasteTuesdaySequence(TuesdaySequence &sequence) const {
    if (canPasteTuesdaySequence()) {
        Model::WriteLock lock;
        sequence = _container.as<TuesdaySequence>();
    }
}

void ClipBoard::pastePattern(int patternIndex) const {
    if (canPastePattern()) {
        Model::WriteLock lock;
        const auto &pattern = _container.as<Pattern>();
        for (int trackIndex = 0; trackIndex < CONFIG_TRACK_COUNT; ++trackIndex) {
            auto &track = _project.track(trackIndex);
            if (track.trackMode() == pattern.sequences[trackIndex].trackMode) {
                switch (track.trackMode()) {
                case Track::TrackMode::Note:
                    track.noteTrack().sequence(patternIndex) = pattern.sequences[trackIndex].data.note;
                    break;
                case Track::TrackMode::Curve:
                    track.curveTrack().sequence(patternIndex) = pattern.sequences[trackIndex].data.curve;
                    break;
                case Track::TrackMode::Tuesday:
                    track.tuesdayTrack().sequence(patternIndex) = pattern.sequences[trackIndex].data.tuesday;
                    break;
                case Track::TrackMode::DiscreteMap:
                    track.discreteMapTrack().sequence(patternIndex) = pattern.sequences[trackIndex].data.discreteMap;
                    break;
                case Track::TrackMode::Indexed:
                    track.indexedTrack().sequence(patternIndex) = pattern.sequences[trackIndex].data.indexed;
                    break;
                case Track::TrackMode::Teletype:
                    break;
                default:
                    break;
                }
            }
        }
    }
}

void ClipBoard::pasteUserScale(UserScale &userScale) const {
    if (canPasteUserScale()) {
        userScale = _container.as<UserScale>();
    }
}

bool ClipBoard::canPasteTrack() const {
    return _type == Type::Track;
}

bool ClipBoard::canPasteNoteSequence() const {
    return _type == Type::NoteSequence;
}

bool ClipBoard::canPasteNoteSequenceSteps() const {
    return _type == Type::NoteSequenceSteps;
}

bool ClipBoard::canPasteCurveSequence() const {
    return _type == Type::CurveSequence;
}

bool ClipBoard::canPasteCurveSequenceSteps() const {
    return _type == Type::CurveSequenceSteps;
}

bool ClipBoard::canPasteIndexedSequence() const {
    return _type == Type::IndexedSequence;
}

bool ClipBoard::canPasteIndexedSequenceSteps() const {
    return _type == Type::IndexedSequenceSteps;
}

bool ClipBoard::canPasteDiscreteMapSequence() const {
    return _type == Type::DiscreteMapSequence;
}

bool ClipBoard::canPasteTuesdaySequence() const {
    return _type == Type::TuesdaySequence;
}

bool ClipBoard::canPastePattern() const {
    return _type == Type::Pattern;
}

bool ClipBoard::canPasteUserScale() const {
    return _type == Type::UserScale;
}
