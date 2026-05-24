#pragma once

#include "Config.h"
#include "RoutableListModel.h"
#include "model/StochasticTrack.h"
#include "model/Project.h"
#include "engine/Engine.h"
#include "engine/StochasticTrackEngine.h"

class StochasticPerformanceListModel : public RoutableListModel {
public:
    enum Item {
        Level,
        Mode,
        GateLength,
        Character,
        Rhythm,
        Melody,
        Complexity,
        Contour,
        RepeatProb,
        Repeat,
        Shape,
        Spread,
        Bias,
        Steps,
        NoteDuration,
        Variation,
        Rest,
        SlideProb,
        LegatoProb,
        Burst,
        BurstCount,
        BurstRate,
        BurstPitch,
        Mask,
        Tilt,
        Sleep,
        Patience,
        PatienceMelody,
        Mutate,
        Jump,
        Size,
        First,
        // Last removed 2026-05-24 — collapsed into Size.
        Rotate,
        Range,
        MinDegree,
        MaxDegree,
        Refresh,
        LastItem
    };

    StochasticPerformanceListModel() {}

    void setTrack(StochasticTrack &track, Project &project, Engine *engine = nullptr) {
        _track = &track;
        _project = &project;
        _engine = engine;
    }

    virtual int rows() const override {
        return _track ? itemCount() : 0;
    }

    virtual int columns() const override {
        return 2;
    }

    virtual void cell(int row, int column, StringBuilder &str) const override {
        auto &sequence = _track->sequence(_project->selectedPatternIndex());
        auto item = itemForRow(row);
        if (column == 0) {
            formatName(item, str);
        } else if (column == 1) {
            formatValue(item, sequence, str);
        }
    }

    virtual void edit(int row, int column, int value, bool shift) override {
        if (column == 1) {
            auto &sequence = _track->sequence(_project->selectedPatternIndex());
            editValue(itemForRow(row), sequence, value, shift);
        }
    }

    virtual Routing::Target routingTarget(int row) const override {
        switch (itemForRow(row)) {
        case Mask:          return Routing::Target::StochasticMask;
        case GateLength:    return Routing::Target::StochasticGateLength;
        case Tilt:          return Routing::Target::StochasticTilt;
        case Burst:         return Routing::Target::StochasticBurst;
        case Contour:       return Routing::Target::StochasticContour;
        case NoteDuration:  return Routing::Target::StochasticNoteDuration;
        case Variation:     return Routing::Target::StochasticVariation;
        case Rest:          return Routing::Target::StochasticRest;
        case SlideProb:     return Routing::Target::StochasticSlide;
        case Sleep:         return Routing::Target::StochasticSleep;
        case Patience:      return Routing::Target::StochasticPatienceRhythm;
        case Mutate:        return Routing::Target::StochasticMutate;
        case Jump:          return Routing::Target::StochasticJump;
        case Rotate:        return Routing::Target::Rotate;
        case Complexity:    return Routing::Target::StochasticComplexity;
        default:            return Routing::Target::None;
        }
    }

private:
    // Phase 11: single flat list. Level enum kept in model for serialization but
    // no longer gates visibility — all sequence-level controls are visible.
    static const Item items[];

    const Item *activeItems() const { return items; }

    int itemCount() const {
        int count = 0;
        const Item *items = activeItems();
        while (items[count] != LastItem) ++count;
        return count;
    }

    Item itemForRow(int row) const {
        const Item *items = activeItems();
        int count = itemCount();
        if (row < 0 || row >= count) return items[0];
        return items[row];
    }

    const char *itemName(Item item) const {
        switch (item) {
        case Level:         return "Level";
        case Mode:          return "Mode";
        case GateLength:    return "Gate Length";
        case Character:     return "Complexity";
        case Rhythm:        return "Rhythm";
        case Melody:        return "Melody";
        case Complexity:    return "Complexity";
        case Contour:       return "Contour";
        case RepeatProb:     return "RepeatProb";
        case Repeat:        return "Repeat";
        case Shape:         return "Shape";
        case Spread:        return "Spread";
        case Bias:          return "Bias";
        case Steps:         return "Steps";
        case NoteDuration:  return "Note Duration";
        case Variation:     return "Variation";
        case Rest:          return "Rest";
        case SlideProb:     return "Slide Prob";
        case LegatoProb:    return "Legato";
        case Burst:         return "Burst";
        case BurstCount:    return "Burst Count";
        case BurstRate:     return "Burst Rate";
        case BurstPitch:    return "Burst Pitch";
        case Mask:          return "Mask";
        case Tilt:          return "Tilt";
        case Sleep:         return "Sleep";
        case Patience:      return "Patience R";
        case PatienceMelody:return "Patience M";
        case Mutate:        return "Mutate";
        case Jump:          return "Jump";
        case Size:          return "Size";
        case First:         return "First";
        case Rotate:        return "Rotate";
        case Range:         return "Range";
        case MinDegree:     return "Low Degree";
        case MaxDegree:     return "High Degree";
        case Refresh:       return "Refresh";
        case LastItem:      break;
        }
        return nullptr;
    }

    void formatName(Item item, StringBuilder &str) const {
        str(itemName(item));
    }

    void formatValue(Item item, const StochasticSequence &sequence, StringBuilder &str) const {
        switch (item) {
        case Level:         sequence.printLevel(str); break;
        case Mode:          sequence.printCoupledMode(str); break;
        case GateLength:    sequence.printGateLength(str); break;
        case Character:     sequence.printComplexity(str); break;
        case Rhythm:        str(sequence.rhythmMode() == StochasticSourceMode::Loop ? "Loop" : "Live"); break;
        case Melody:        str(sequence.melodyMode() == StochasticSourceMode::Loop ? "Loop" : "Live"); break;
        case Complexity:    sequence.printComplexity(str); break;
        case Contour:       sequence.printContour(str); break;
        case RepeatProb:     sequence.printRepeatProb(str); break;
        case Repeat:        sequence.printRepeatProb(str); break;
        case Shape:         sequence.printMarblesMode(str); break;
        case Spread:        sequence.printMarblesSpread(str); break;
        case Bias:          sequence.printMarblesBias(str); break;
        case Steps:         sequence.printStepsSieve(str); break;
        case NoteDuration:  sequence.printNoteDuration(str); break;
        case Variation:     sequence.printVariation(str); break;
        case Rest:          sequence.printRest(str); break;
        case SlideProb:     sequence.printSlide(str); break;
        case LegatoProb:    sequence.printLegatoProb(str); break;
        case Burst:         sequence.printBurst(str); break;
        case BurstCount:    str("%d%%", sequence.burstCount()); break;
        case BurstRate:     str("%d%%", sequence.burstRate()); break;
        case BurstPitch:    sequence.printBurstPitch(str); break;
        case Mask:          sequence.printMask(str); break;
        case Tilt:          sequence.printTilt(str); break;
        case Sleep:         sequence.printSleep(str); break;
        case Patience:      sequence.printPatienceRhythm(str); break;
        case PatienceMelody:sequence.printPatienceMelody(str); break;
        case Mutate:        sequence.printMutate(str); break;
        case Jump:          sequence.printJump(str); break;
        case Size:          str("%d", sequence.size()); break;
        case First:         str("%d", sequence.first() + 1); break;
        case Rotate:        sequence.printRotate(str); break;
        case Range:         str("%d Oct", sequence.range()); break;
        case MinDegree:     sequence.printMinDegree(str); break;
        case MaxDegree:     sequence.printMaxDegree(str); break;
        case Refresh:       str("Exec"); break;
        case LastItem:      break;
        }
    }

    void editValue(Item item, StochasticSequence &sequence, int value, bool shift) {
        switch (item) {
        case Level:         sequence.editLevel(value, shift); break;
        case Mode:          sequence.editCoupledMode(value, shift); break;
        case GateLength:    sequence.editGateLength(value, shift); break;
        case Character:     sequence.editComplexityMacro(value, shift); break;
        case Rhythm:        sequence.setRhythmMode(ModelUtils::adjustedEnum(sequence.rhythmMode(), value)); break;
        case Melody:        sequence.setMelodyMode(ModelUtils::adjustedEnum(sequence.melodyMode(), value)); break;
        case Complexity:    sequence.editComplexity(value, shift); break;
        case Contour:       sequence.editContour(value, shift); break;
        case RepeatProb:     sequence.editRepeatProb(value, shift); break;
        case Repeat:        sequence.editRepeatProb(value, shift); break;
        case Shape:         sequence.editMarblesMode(value, shift); break;
        case Spread:        sequence.editMarblesSpread(value, shift); break;
        case Bias:          sequence.editMarblesBias(value, shift); break;
        case Steps:         sequence.editStepsSieve(value, shift); break;
        case NoteDuration:  sequence.editNoteDuration(value, shift); break;
        case Variation:     sequence.editVariation(value, shift); break;
        case Rest:          sequence.editRest(value, shift); break;
        case SlideProb:     sequence.editSlide(value, shift); break;
        case LegatoProb:    sequence.editLegatoProb(value, shift); break;
        case Burst:         sequence.editBurst(value, shift);                            notifyShapingEdit(); break;
        case BurstCount:    sequence.setBurstCount(sequence.burstCount() + value);       notifyShapingEdit(); break;
        case BurstRate:     sequence.setBurstRate(sequence.burstRate() + value);         notifyShapingEdit(); break;
        case BurstPitch:    sequence.editBurstPitch(value, shift);                       notifyShapingEdit(); break;
        case Mask:          sequence.editMask(value, shift); break;
        case Tilt:          sequence.editTilt(value, shift); break;
        case Sleep:         sequence.editSleep(value, shift); break;
        case Patience:      sequence.editPatienceRhythm(value, shift); break;
        case PatienceMelody:sequence.editPatienceMelody(value, shift); break;
        case Mutate:        sequence.editMutate(value, shift); break;
        case Jump:          sequence.editJump(value, shift); break;
        case Size:          sequence.setSize(sequence.size() + value);    notifyWindowEdit(); break;
        case First:         sequence.setFirst(sequence.first() + value);  notifyWindowEdit(); break;
        case Rotate:        sequence.editRotate(value, shift); break;
        case Range:         sequence.setRange(sequence.range() + value); break;
        case MinDegree:     sequence.editMinDegree(value, shift); break;
        case MaxDegree:     sequence.editMaxDegree(value, shift); break;
        case Refresh:       sequence.refresh(); break;
        case LastItem:      break;
        }
    }

    // Engine notify helpers (Codex review 2026-05-22 finding 1+2). Mirror
    // the StochasticSequenceEditPage::notifyStochastic* pair: refresh cache
    // on cache-baked-field edits (Burst*), and snap+flush+refresh on window
    // edits (Size/First/Last). No-op when no engine reference was bound at
    // setTrack time, or when the selected track isn't currently running as
    // Stochastic, or when the engine's playing pattern isn't this edit's
    // target pattern.
    bool shouldNotifyEngine() const {
        if (!_engine || !_track || !_project) return false;
        int idx = _project->selectedTrackIndex();
        auto &eng = _engine->trackEngine(idx);
        if (eng.trackMode() != Track::TrackMode::Stochastic) return false;
        if (eng.pattern() != _project->selectedPatternIndex()) return false;
        return true;
    }
    void notifyWindowEdit() {
        if (!shouldNotifyEngine()) return;
        int idx = _project->selectedTrackIndex();
        static_cast<StochasticTrackEngine &>(_engine->trackEngine(idx)).syncWindowEdit();
    }
    void notifyShapingEdit() {
        if (!shouldNotifyEngine()) return;
        int idx = _project->selectedTrackIndex();
        static_cast<StochasticTrackEngine &>(_engine->trackEngine(idx)).refreshCacheNow();
    }

    StochasticTrack *_track = nullptr;
    Project *_project = nullptr;
    Engine *_engine = nullptr;   // optional — only callers that have engine access pass it
};

// Phase 11 — single flat list, grouped semantically.
// Dropped from visibility (model fields/enum members kept as reserved slots):
//   Level    — UI no longer gates by level
//   Mode     — coupled Loop/Live (use split Rhythm + Melody instead)
//   Character — duplicate of Complexity (same field)
//   Shape    — Marbles toggle (engine always runs the distribution now)
//   RepeatProb — folded into Complexity kernel (slot repurposed as Repeat probability)
//   Contour    — restored Phase 12 as directional Drift bias relative to lastDegree
//   Density  — slot repurposed as Gate Length (visible row)
// Sections (top to bottom):
//   1. Playback   — Rhythm, Melody, Refresh
//   2. Pitch      — Complexity, Bias, Spread, Steps + Range/MinDegree/MaxDegree
//   3. Rhythm     — NoteDuration, Variation, Rest, articulation probs
//   4. Burst      — Burst + Count/Rate/Pitch
//   5. Pattern    — Mask, Tilt
//   6. Window     — Size, First, Rotate
//   7. Evolution  — Sleep, Patience, Mutate, Jump
inline const StochasticPerformanceListModel::Item StochasticPerformanceListModel::items[] = {
    // Playback
    Rhythm,
    Melody,
    Refresh,
    // Pitch
    Complexity,
    Contour,
    Repeat,
    Bias,
    Spread,
    Steps,
    Range,
    // Rhythm
    NoteDuration,
    Variation,
    Rest,
    GateLength,
    SlideProb,
    LegatoProb,
    // Burst
    Burst,
    BurstCount,
    BurstRate,
    BurstPitch,
    // Pattern sieve
    Mask,
    Tilt,
    // Sequence window
    Size,
    First,
    Rotate,
    // State evolution
    Sleep,
    Patience,
    PatienceMelody,
    Mutate,
    Jump,
    LastItem
};