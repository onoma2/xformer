#pragma once

#include "Config.h"
#include "Types.h"
#include "StochasticSequence.h"
#include "Serialize.h"
#include "Routing.h"

class StochasticTrack {
public:
    //----------------------------------------
    // Types
    //----------------------------------------

    using StochasticSequenceArray = std::array<StochasticSequence, CONFIG_PATTERN_COUNT + CONFIG_SNAPSHOT_COUNT>;

    // FillMode

    enum class FillMode : uint8_t {
        None,
        Gates,
        NextPattern,
        Condition,
        Last
    };

    static const char *fillModeName(FillMode fillMode) {
        switch (fillMode) {
        case FillMode::None:        return "None";
        case FillMode::Gates:       return "Gates";
        case FillMode::NextPattern: return "Next Pattern";
        case FillMode::Condition:   return "Condition";
        case FillMode::Last:        break;
        }
        return nullptr;
    }

    // CvUpdateMode

    enum class CvUpdateMode : uint8_t {
        Gate,
        Always,
        Last
    };

    static const char *cvUpdateModeName(CvUpdateMode mode) {
        switch (mode) {
        case CvUpdateMode::Gate:    return "Gate";
        case CvUpdateMode::Always:  return "Always";
        case CvUpdateMode::Last:    break;
        }
        return nullptr;
    }

    // MarblesMode
    enum class MarblesMode : uint8_t {
        Off,
        On,
        Last
    };

    //----------------------------------------
    // Properties
    //----------------------------------------

    // Phase 8.1 Source Modes
    StochasticSourceMode rhythmMode() const { return _rhythmMode; }
    void setRhythmMode(StochasticSourceMode mode) { _rhythmMode = ModelUtils::clampedEnum(mode); }

    StochasticSourceMode melodyMode() const { return _melodyMode; }
    void setMelodyMode(StochasticSourceMode mode) { _melodyMode = ModelUtils::clampedEnum(mode); }

    // Phase 7b Generator controls
    StochasticMode mode() const { return _mode; }
    void setMode(StochasticMode mode) { _mode = ModelUtils::clampedEnum(mode); }

    int complexity() const { return _complexity.get(isRouted(Routing::Target::StochasticComplexity)); }
    void setComplexity(int complexity, bool routed = false) { _complexity.set(clamp(complexity, 0, 100), routed); }

    int contour() const { return _contour.get(isRouted(Routing::Target::StochasticContour)); }
    void setContour(int contour, bool routed = false) { _contour.set(clamp(contour, -100, 100), routed); }

    int linearity() const { return _linearity; }
    void setLinearity(int linearity) { _linearity = clamp(linearity, 0, 100); }

    int rate() const { return _rate.get(isRouted(Routing::Target::StochasticRate)); }
    void setRate(int rate, bool routed = false) { _rate.set(clamp(rate, 1, 400), routed); }

    int variation() const { return _variation.get(isRouted(Routing::Target::StochasticVariation)); }
    void setVariation(int variation, bool routed = false) { _variation.set(clamp(variation, -100, 100), routed); }

    int rest() const { return _rest.get(isRouted(Routing::Target::StochasticRest)); }
    void setRest(int rest, bool routed = false) { _rest.set(clamp(rest, 0, 100), routed); }

    int slide() const { return _slide.get(isRouted(Routing::Target::StochasticSlide)); }
    void setSlide(int slide, bool routed = false) { _slide.set(clamp(slide, 0, 100), routed); }

    int burstRate() const { return _burstRate; }
    void setBurstRate(int rate) { _burstRate = clamp(rate, 0, 100); }

    int burstCount() const { return _burstCount; }
    void setBurstCount(int count) { _burstCount = clamp(count, 0, 100); }

    StochasticBurstPitch burstPitch() const { return _burstPitch; }
    void setBurstPitch(StochasticBurstPitch pitch) { _burstPitch = ModelUtils::clampedEnum(pitch); }

    int sleep() const { return _sleep.get(isRouted(Routing::Target::StochasticSleep)); }
    void setSleep(int sleep, bool routed = false) { _sleep.set(clamp(sleep, 0, 100), routed); }

    int patience() const { return _patience.get(isRouted(Routing::Target::StochasticPatience)); }
    void setPatience(int patience, bool routed = false) { _patience.set(clamp(patience, 0, 100), routed); }

    int mutate() const { return _mutate.get(isRouted(Routing::Target::StochasticMutate)); }
    void setMutate(int mutate, bool routed = false) { _mutate.set(clamp(mutate, 0, 100), routed); }

    int jump() const { return _jump.get(isRouted(Routing::Target::StochasticJump)); }
    void setJump(int jump, bool routed = false) { _jump.set(clamp(jump, 0, 100), routed); }

    int range() const { return _range; }
    void setRange(int range) { _range = clamp(range, 1, 4); }

    // degreeTickets
    int degreeTicket(int degree) const { return _degreeTickets[degree]; }
    void setDegreeTicket(int degree, int tickets) { _degreeTickets[degree] = clamp(tickets, 0, 100); }

    // degreeRotation
    int degreeRotation() const { return _degreeRotation; }
    void setDegreeRotation(int rotation) { _degreeRotation = clamp(rotation, -12, 12); }

    // maskRotation
    int maskRotation() const { return _maskRotation; }
    void setMaskRotation(int rotation) { _maskRotation = clamp(rotation, -12, 12); }

    // lock
    bool lock() const { return _lock; }
    void setLock(bool lock) { _lock = lock; }

    // fillMuted
    bool fillMuted() const { return _fillMuted; }
    void setFillMuted(bool fillMuted) { _fillMuted = fillMuted; }

    // loopFirst
    int loopFirst() const { return _loopFirst; }
    void setLoopFirst(int first) { _loopFirst = clamp(first, 0, CONFIG_STEP_COUNT - 1); }

    // loopLast
    int loopLast() const { return _loopLast; }
    void setLoopLast(int last) { _loopLast = clamp(last, 0, CONFIG_STEP_COUNT - 1); }

    // accentProb
    int accentProb() const { return _accentProb; }
    void setAccentProb(int prob) { _accentProb = clamp(prob, 0, 100); }

    // legatoProb
    int legatoProb() const { return _legatoProb; }
    void setLegatoProb(int prob) { _legatoProb = clamp(prob, 0, 100); }

    // marblesMode
    MarblesMode marblesMode() const { return _marblesMode; }
    void setMarblesMode(MarblesMode mode) { _marblesMode = ModelUtils::clampedEnum(mode); }

    // marblesSpread
    int marblesSpread() const { return _marblesSpread; }
    void setMarblesSpread(int spread) { _marblesSpread = clamp(spread, 0, 100); }

    // marblesBias
    int marblesBias() const { return _marblesBias; }
    void setMarblesBias(int bias) { _marblesBias = clamp(bias, 0, 100); }

    // marblesSteps
    int marblesSteps() const { return _marblesSteps; }
    void setMarblesSteps(int steps) { _marblesSteps = clamp(steps, 1, 100); }

    // density
    int density() const { return _density.get(isRouted(Routing::Target::StochasticDensity)); }
    void setDensity(int density, bool routed = false) { _density.set(clamp(density, 0, 100), routed); }

    // tilt
    int tilt() const { return _tilt.get(isRouted(Routing::Target::StochasticTilt)); }
    void setTilt(int tilt, bool routed = false) { _tilt.set(clamp(tilt, -100, 100), routed); }

    // reservedJitter
    int reservedJitter() const { return _reservedJitter.get(isRouted(Routing::Target::StochasticReserved)); }
    void setReservedJitter(int jitter, bool routed = false) { _reservedJitter.set(clamp(jitter, 0, 100), routed); }

    // burst
    int burst() const { return _burst.get(isRouted(Routing::Target::StochasticBurst)); }
    void setBurst(int burst, bool routed = false) { _burst.set(clamp(burst, 0, 100), routed); }

    // minDegree
    int minDegree() const { return _minDegree; }
    void setMinDegree(int degree) { _minDegree = clamp(degree, 0, 127); }

    // maxDegree
    int maxDegree() const { return _maxDegree; }
    void setMaxDegree(int degree) { _maxDegree = clamp(degree, 0, 127); }

    // slideTime
    int slideTime() const { return _slideTime.get(isRouted(Routing::Target::SlideTime)); }
    void setSlideTime(int slideTime, bool routed = false) { _slideTime.set(clamp(slideTime, 0, 100), routed); }

    // octave
    int octave() const { return _octave.get(isRouted(Routing::Target::Octave)); }
    void setOctave(int octave, bool routed = false) { _octave.set(clamp(octave, -10, 10), routed); }

    // transpose
    int transpose() const { return _transpose.get(isRouted(Routing::Target::Transpose)); }
    void setTranspose(int transpose, bool routed = false) { _transpose.set(clamp(transpose, -100, 100), routed); }

    // rotate
    int rotate() const { return _rotate.get(isRouted(Routing::Target::Rotate)); }
    void setRotate(int rotate, bool routed = false) { _rotate.set(clamp(rotate, -64, 64), routed); }

    // fillMode
    FillMode fillMode() const { return _fillMode; }
    void setFillMode(FillMode mode) { _fillMode = ModelUtils::clampedEnum(mode); }

    // cvUpdateMode
    CvUpdateMode cvUpdateMode() const { return _cvUpdateMode; }
    void setCvUpdateMode(CvUpdateMode mode) { _cvUpdateMode = ModelUtils::clampedEnum(mode); }

    // gateBias
    int gateBias() const { return _gateBias.get(isRouted(Routing::Target::GateProbabilityBias)); }
    void setGateBias(int bias, bool routed = false) { _gateBias.set(clamp(bias, -8, 8), routed); }

    // retriggerBias
    int retriggerBias() const { return _retriggerBias.get(isRouted(Routing::Target::RetriggerProbabilityBias)); }
    void setRetriggerBias(int bias, bool routed = false) { _retriggerBias.set(clamp(bias, -8, 8), routed); }

    // lengthBias
    int lengthBias() const { return _lengthBias.get(isRouted(Routing::Target::LengthBias)); }
    void setLengthBias(int bias, bool routed = false) { _lengthBias.set(clamp(bias, -8, 8), routed); }

    // noteBias
    int noteBias() const { return _noteBias.get(isRouted(Routing::Target::NoteProbabilityBias)); }
    void setNoteBias(int bias, bool routed = false) { _noteBias.set(clamp(bias, -8, 8), routed); }

    // UI Helpers
    void printLinearity(StringBuilder &str) const { str("%d%%", linearity()); }
    void editLinearity(int value, bool shift) { setLinearity(linearity() + value); }

    void printMarblesMode(StringBuilder &str) const { str(marblesMode() == MarblesMode::Off ? "Off" : "On"); }
    void editMarblesMode(int value, bool shift) { setMarblesMode(ModelUtils::adjustedEnum(marblesMode(), value)); }

    void printMarblesSpread(StringBuilder &str) const { str("%d%%", marblesSpread()); }
    void editMarblesSpread(int value, bool shift) { setMarblesSpread(marblesSpread() + value); }

    void printMarblesBias(StringBuilder &str) const { str("%d%%", marblesBias()); }
    void editMarblesBias(int value, bool shift) { setMarblesBias(marblesBias() + value); }

    void printMarblesSteps(StringBuilder &str) const { str("%d", marblesSteps()); }
    void editMarblesSteps(int value, bool shift) { setMarblesSteps(marblesSteps() + value); }

    void printDensity(StringBuilder &str) const { printRouted(str, Routing::Target::StochasticDensity); str("%d%%", density()); }
    void editDensity(int value, bool shift) { if (!isRouted(Routing::Target::StochasticDensity)) setDensity(density() + value); }

    void printTilt(StringBuilder &str) const { printRouted(str, Routing::Target::StochasticTilt); str("%+d%%", tilt()); }
    void editTilt(int value, bool shift) { if (!isRouted(Routing::Target::StochasticTilt)) setTilt(tilt() + value); }

    void printBurst(StringBuilder &str) const { printRouted(str, Routing::Target::StochasticBurst); str("%d%%", burst()); }
    void editBurst(int value, bool shift) { if (!isRouted(Routing::Target::StochasticBurst)) setBurst(burst() + value); }

    void printRotate(StringBuilder &str) const { printRouted(str, Routing::Target::Rotate); str("%+d", rotate()); }
    void editRotate(int value, bool shift) { if (!isRouted(Routing::Target::Rotate)) setRotate(rotate() + value); }

    void printLock(StringBuilder &str) const { ModelUtils::printYesNo(str, lock()); }
    void editLock(int value, bool shift) { setLock(value > 0); }

    void printMinDegree(StringBuilder &str) const { str("%d", minDegree()); }
    void editMinDegree(int value, bool shift) { setMinDegree(minDegree() + value); }

    void printMaxDegree(StringBuilder &str) const { str("%d", maxDegree()); }
    void editMaxDegree(int value, bool shift) { setMaxDegree(maxDegree() + value); }

    void printAccentProb(StringBuilder &str) const { str("%d%%", accentProb()); }
    void editAccentProb(int value, bool shift) { setAccentProb(ModelUtils::adjustedByStep(accentProb(), value, 1, !shift)); }

    void printLegatoProb(StringBuilder &str) const { str("%d%%", legatoProb()); }
    void editLegatoProb(int value, bool shift) { setLegatoProb(ModelUtils::adjustedByStep(legatoProb(), value, 1, !shift)); }

    void printOctave(StringBuilder &str) const { printRouted(str, Routing::Target::Octave); str("%+d", octave()); }
    void editOctave(int value, bool shift) { if (!isRouted(Routing::Target::Octave)) setOctave(octave() + value); }

    void printTranspose(StringBuilder &str) const { printRouted(str, Routing::Target::Transpose); str("%+d", transpose()); }
    void editTranspose(int value, bool shift) { if (!isRouted(Routing::Target::Transpose)) setTranspose(transpose() + value); }

    void printCvUpdateMode(StringBuilder &str) const { str(cvUpdateModeName(cvUpdateMode())); }
    void editCvUpdateMode(int value, bool shift) { setCvUpdateMode(ModelUtils::adjustedEnum(cvUpdateMode(), value)); }

    void printSlideTime(StringBuilder &str) const { printRouted(str, Routing::Target::SlideTime); str("%d%%", slideTime()); }
    void editSlideTime(int value, bool shift) { if (!isRouted(Routing::Target::SlideTime)) setSlideTime(ModelUtils::adjustedByStep(slideTime(), value, 5, !shift)); }

    int8_t activePatternIndex() const { return -1; }
    void setActivePatternIndex(int index) {}

    uint32_t activeSeed() const { return 0; }
    void setActiveSeed(uint32_t seed) {}

    // sequences
    const StochasticSequenceArray &sequences() const { return _sequences; }
          StochasticSequenceArray &sequences()       { return _sequences; }

    const StochasticSequence &sequence(int index) const { return _sequences[index]; }
          StochasticSequence &sequence(int index)       { return _sequences[index]; }

    //----------------------------------------
    // Routing
    //----------------------------------------

    inline bool isRouted(Routing::Target target) const { return Routing::isRouted(target, _trackIndex); }
    inline void printRouted(StringBuilder &str, Routing::Target target) const { Routing::printRouted(str, target, _trackIndex); }
    void writeRouted(Routing::Target target, int intValue, float floatValue);

    //----------------------------------------
    // Methods
    //----------------------------------------

    StochasticTrack() { clear(); }

    void clear() {
        for (int i = 0; i < CONFIG_USER_SCALE_SIZE; ++i) {
            _degreeTickets[i] = 10;
        }
        _linearity = 0;
        _degreeRotation = 0;
        _maskRotation = 0;
        _lock = false;
        _fillMuted = true;
        _loopFirst = 0;
        _loopLast = CONFIG_STEP_COUNT - 1;
        _accentProb = 0;
        _legatoProb = 0;
        _marblesMode = MarblesMode::Off;
        _marblesSpread = 50;
        _marblesBias = 50;
        _marblesSteps = 100;
        _density.setBase(100);
        _tilt.setBase(0);
        _reservedJitter.setBase(0);
        _burst.setBase(0);
        _minDegree = 0;
        _maxDegree = 127;
        _slideTime.setBase(0);
        _octave.setBase(0);
        _transpose.setBase(0);
        _rotate.setBase(0);
        _fillMode = FillMode::None;
        _cvUpdateMode = CvUpdateMode::Gate;
        _gateBias.setBase(0);
        _retriggerBias.setBase(0);
        _lengthBias.setBase(0);
        _noteBias.setBase(0);

        _mode = StochasticMode::Dice;
        _complexity.setBase(50);
        _contour.setBase(0);
        _rate.setBase(50);
        _variation.setBase(0);
        _rest.setBase(0);
        _slide.setBase(0);
        _burstRate = 50;
        _burstCount = 0;
        _burstPitch = StochasticBurstPitch::Parent;
        _sleep.setBase(0);
        _patience.setBase(100);
        _mutate.setBase(0);
        _jump.setBase(0);
        _range = 1;

        _rhythmMode = StochasticSourceMode::Loop;
        _melodyMode = StochasticSourceMode::Loop;

        for (auto &sequence : _sequences) {
            sequence.clear();
        }
    }

    void write(VersionedSerializedWriter &writer) const {
        for (int i = 0; i < CONFIG_USER_SCALE_SIZE; ++i) writer.write(_degreeTickets[i]);
        writer.write(_linearity);
        writer.write(_degreeRotation);
        writer.write(_maskRotation);
        writer.write(_lock);
        writer.write(_fillMuted);
        writer.write(_loopFirst);
        writer.write(_loopLast);
        writer.write(_accentProb);
        writer.write(_legatoProb);
        writer.write(static_cast<uint8_t>(_marblesMode));
        writer.write(_marblesSpread);
        writer.write(_marblesBias);
        writer.write(_marblesSteps);
        _density.write(writer);
        _tilt.write(writer);
        _reservedJitter.write(writer);
        _burst.write(writer);
        writer.write(_minDegree);
        writer.write(_maxDegree);
        _slideTime.write(writer);
        _octave.write(writer);
        _transpose.write(writer);
        _rotate.write(writer);
        writer.write(static_cast<uint8_t>(_fillMode));
        writer.write(static_cast<uint8_t>(_cvUpdateMode));
        _gateBias.write(writer);
        _retriggerBias.write(writer);
        _lengthBias.write(writer);
        _noteBias.write(writer);

        // Phase 7b additions
        writer.write(static_cast<uint8_t>(_mode));
        _complexity.write(writer);
        _contour.write(writer);
        _rate.write(writer);
        _variation.write(writer);
        _rest.write(writer);
        _slide.write(writer);
        writer.write(_burstRate);
        writer.write(_burstCount);
        writer.write(static_cast<uint8_t>(_burstPitch));
        _sleep.write(writer);
        _patience.write(writer);
        _mutate.write(writer);
        _jump.write(writer);
        writer.write(_range);

        writer.write(static_cast<uint8_t>(_rhythmMode));
        writer.write(static_cast<uint8_t>(_melodyMode));

        for (const auto &sequence : _sequences) {
            sequence.write(writer);
        }
    }

    void read(VersionedSerializedReader &reader) {
        for (int i = 0; i < CONFIG_USER_SCALE_SIZE; ++i) reader.read(_degreeTickets[i]);
        reader.read(_linearity);
        reader.read(_degreeRotation);
        reader.read(_maskRotation);
        reader.read(_lock);
        reader.read(_fillMuted);
        reader.read(_loopFirst);
        reader.read(_loopLast);
        reader.read(_accentProb);
        reader.read(_legatoProb);
        uint8_t marblesMode;
        reader.read(marblesMode);
        _marblesMode = static_cast<MarblesMode>(marblesMode);
        reader.read(_marblesSpread);
        reader.read(_marblesBias);
        reader.read(_marblesSteps);
        _density.read(reader);
        _tilt.read(reader);
        _reservedJitter.read(reader);
        _burst.read(reader);
        reader.read(_minDegree);
        reader.read(_maxDegree);
        _slideTime.read(reader);
        _octave.read(reader);
        _transpose.read(reader);
        _rotate.read(reader);
        uint8_t fillMode;
        reader.read(fillMode);
        _fillMode = static_cast<FillMode>(fillMode);
        uint8_t cvUpdateMode;
        reader.read(cvUpdateMode);
        _cvUpdateMode = static_cast<CvUpdateMode>(cvUpdateMode);
        _gateBias.read(reader);
        _retriggerBias.read(reader);
        _lengthBias.read(reader);
        _noteBias.read(reader);

        // Phase 7b additions
        uint8_t mode, burstPitch;
        reader.read(mode); _mode = static_cast<StochasticMode>(mode);
        _complexity.read(reader);
        _contour.read(reader);
        _rate.read(reader);
        _variation.read(reader);
        _rest.read(reader);
        _slide.read(reader);
        reader.read(_burstRate);
        reader.read(_burstCount);
        reader.read(burstPitch); _burstPitch = static_cast<StochasticBurstPitch>(burstPitch);
        _sleep.read(reader);
        _patience.read(reader);
        _mutate.read(reader);
        _jump.read(reader);
        reader.read(_range);

        uint8_t rhythmMode, melodyMode;
        reader.read(rhythmMode); _rhythmMode = static_cast<StochasticSourceMode>(rhythmMode);
        reader.read(melodyMode); _melodyMode = static_cast<StochasticSourceMode>(melodyMode);

        for (auto &sequence : _sequences) {
            sequence.read(reader);
        }
    }

private:
    void setTrackIndex(int trackIndex) {
        _trackIndex = trackIndex;
        for (auto &sequence : _sequences) {
            sequence.setTrackIndex(trackIndex);
        }
    }

    int8_t _trackIndex = -1;

    int8_t _degreeTickets[CONFIG_USER_SCALE_SIZE];
    uint8_t _linearity;
    int8_t _degreeRotation;
    int8_t _maskRotation;
    bool _lock;
    bool _fillMuted;
    uint8_t _loopFirst;
    uint8_t _loopLast;
    uint8_t _accentProb;
    uint8_t _legatoProb;
    MarblesMode _marblesMode;
    uint8_t _marblesSpread;
    uint8_t _marblesBias;
    uint8_t _marblesSteps;
    
    Routable<uint8_t> _density;
    Routable<int8_t> _tilt;
    Routable<uint8_t> _reservedJitter;
    Routable<uint8_t> _burst;

    uint8_t _minDegree;
    uint8_t _maxDegree;
    
    Routable<uint8_t> _slideTime;
    Routable<int8_t> _octave;
    Routable<int8_t> _transpose;
    Routable<int8_t> _rotate;
    FillMode _fillMode;
    CvUpdateMode _cvUpdateMode;
    Routable<int8_t> _gateBias;
    Routable<int8_t> _retriggerBias;
    Routable<int8_t> _lengthBias;
    Routable<int8_t> _noteBias;

    // Phase 7b Generator controls
    StochasticMode _mode;
    Routable<uint8_t> _complexity;
    Routable<int8_t> _contour;
    Routable<uint8_t> _rate;
    Routable<int8_t> _variation;
    Routable<uint8_t> _rest;
    Routable<uint8_t> _slide;
    uint8_t _burstRate;
    uint8_t _burstCount;
    StochasticBurstPitch _burstPitch;
    Routable<uint8_t> _sleep;
    Routable<uint8_t> _patience;
    Routable<uint8_t> _mutate;
    Routable<uint8_t> _jump;
    uint8_t _range;

    StochasticSourceMode _rhythmMode;
    StochasticSourceMode _melodyMode;

    StochasticSequenceArray _sequences;

    friend class Track;
};

static_assert(sizeof(StochasticTrack) <= 9544, "StochasticTrack too large");
