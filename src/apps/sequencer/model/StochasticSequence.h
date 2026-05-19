#pragma once

#include "Config.h"
#include "Bitfield.h"
#include "Serialize.h"
#include "ModelUtils.h"
#include "Types.h"
#include "Scale.h"

#include "core/math/Math.h"

#include "Routing.h"
#include "StochasticTypes.h"

class StochasticSequence {
public:
    //----------------------------------------
    // Properties
    //----------------------------------------

    // trackIndex
    int trackIndex() const { return _trackIndex; }
    void setTrackIndex(int trackIndex) { _trackIndex = trackIndex; }

    // scale
    int scale() const { return _scale; }
    void setScale(int scale) { _scale = clamp(scale, -1, Scale::Count - 1); }
    const Scale &selectedScale(int defaultScale) const {
        return Scale::get(_scale != -1 ? _scale : defaultScale);
    }

    // rootNote
    int rootNote() const { return _rootNote; }
    void setRootNote(int rootNote) { _rootNote = clamp(rootNote, -1, 11); }
    int selectedRootNote(int defaultRootNote) const {
        return _rootNote != -1 ? _rootNote : defaultRootNote;
    }

    // divisor
    int divisor() const { return _divisor; }
    void setDivisor(int divisor) { _divisor = ModelUtils::clampDivisor(divisor); }

    // resetMeasure
    int resetMeasure() const { return _resetMeasure; }
    void setResetMeasure(int resetMeasure) { _resetMeasure = clamp(resetMeasure, 0, 128); }

    void printDivisor(StringBuilder &str) const {
        ModelUtils::printDivisor(str, _divisor);
    }

    void editDivisor(int value, bool shift) {
        setDivisor(ModelUtils::adjustedByDivisor(divisor(), value, shift));
    }

    void printResetMeasure(StringBuilder &str) const {
        if (_resetMeasure == 0) {
            str("Off");
        } else {
            str("%d", _resetMeasure);
        }
    }

    void editResetMeasure(int value, bool shift) {
        setResetMeasure(resetMeasure() + value);
    }

    void printScale(StringBuilder &str) const {
        if (scale() == -1) {
            str("Default");
        } else {
            str(Scale::name(scale()));
        }
    }

    void editScale(int value, bool shift) {
        setScale(scale() + value);
    }


    inline bool isRouted(Routing::Target target) const { return Routing::isRouted(target, _trackIndex); }
    inline void printRouted(StringBuilder &str, Routing::Target target) const { Routing::printRouted(str, target, _trackIndex); }

    void writeRouted(Routing::Target target, int intValue, float floatValue) {
        switch (target) {
        case Routing::Target::StochasticDensity: setDensity(intValue, true); break;
        case Routing::Target::StochasticTilt: setTilt(intValue, true); break;
        case Routing::Target::StochasticBurst: setBurst(intValue, true); break;
        case Routing::Target::StochasticComplexity: setComplexity(intValue, true); break;
        case Routing::Target::StochasticContour: setContour(intValue, true); break;
        case Routing::Target::StochasticRate: setRate(intValue, true); break;
        case Routing::Target::StochasticVariation: setVariation(intValue, true); break;
        case Routing::Target::StochasticRest: setRest(intValue, true); break;
        case Routing::Target::StochasticSlide: setSlide(intValue, true); break;
        case Routing::Target::StochasticSleep: setSleep(intValue, true); break;
        case Routing::Target::StochasticPatience: setPatience(intValue, true); break;
        case Routing::Target::StochasticMutate: setMutate(intValue, true); break;
        case Routing::Target::StochasticJump: setJump(intValue, true); break;
        case Routing::Target::Rotate: setRotate(intValue, true); break;
        default: break;
        }
    }

    // Phase 8.1 Source Modes
    StochasticSourceMode rhythmMode() const { return _rhythmMode; }
    void setRhythmMode(StochasticSourceMode mode) { _rhythmMode = ModelUtils::clampedEnum(mode); }

    StochasticSourceMode melodyMode() const { return _melodyMode; }
    void setMelodyMode(StochasticSourceMode mode) { _melodyMode = ModelUtils::clampedEnum(mode); }

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
    void setDegreeTicket(int degree, int tickets) { _degreeTickets[degree] = clamp(tickets, -1, 100); }

    // degreeRotation
    int degreeRotation() const { return _degreeRotation; }
    void setDegreeRotation(int rotation) { _degreeRotation = clamp(rotation, -32, 32); }

    // maskRotation
    int maskRotation() const { return _maskRotation; }
    void setMaskRotation(int rotation) { _maskRotation = clamp(rotation, -32, 32); }

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

    // til
    int tilt() const { return _tilt.get(isRouted(Routing::Target::StochasticTilt)); }
    void setTilt(int tilt, bool routed = false) { _tilt.set(clamp(tilt, -100, 100), routed); }

    // reservedJitter
    int reservedJitter() const { return _reservedJitter.get(isRouted(Routing::Target::StochasticReserved)); }
    void setReservedJitter(int jitter, bool routed = false) { _reservedJitter.set(clamp(jitter, 0, 100), routed); }

    // burs
    int burst() const { return _burst.get(isRouted(Routing::Target::StochasticBurst)); }
    void setBurst(int burst, bool routed = false) { _burst.set(clamp(burst, 0, 100), routed); }

    // minDegree
    int minDegree() const { return _minDegree; }
    void setMinDegree(int degree) { _minDegree = clamp(degree, 0, 127); }

    // maxDegree
    int maxDegree() const { return _maxDegree; }
    void setMaxDegree(int degree) { _maxDegree = clamp(degree, 0, 127); }

    // rotate
    int rotate() const { return _rotate.get(isRouted(Routing::Target::Rotate)); }
    void setRotate(int rotate, bool routed = false) { _rotate.set(clamp(rotate, -64, 64), routed); }

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

    void printMinDegree(StringBuilder &str) const { str("%d", minDegree()); }
    void editMinDegree(int value, bool shift) { setMinDegree(minDegree() + value); }

    void printMaxDegree(StringBuilder &str) const { str("%d", maxDegree()); }
    void editMaxDegree(int value, bool shift) { setMaxDegree(maxDegree() + value); }

    void printAccentProb(StringBuilder &str) const { str("%d%%", accentProb()); }
    void editAccentProb(int value, bool shift) { setAccentProb(ModelUtils::adjustedByStep(accentProb(), value, 1, !shift)); }

    void printLegatoProb(StringBuilder &str) const { str("%d%%", legatoProb()); }
    void editLegatoProb(int value, bool shift) { setLegatoProb(ModelUtils::adjustedByStep(legatoProb(), value, 1, !shift)); }

    void printComplexity(StringBuilder &str) const { printRouted(str, Routing::Target::StochasticComplexity); str("%d%%", complexity()); }
    void editComplexity(int value, bool shift) { if (!isRouted(Routing::Target::StochasticComplexity)) setComplexity(complexity() + value); }

    void printContour(StringBuilder &str) const { printRouted(str, Routing::Target::StochasticContour); str("%+d%%", contour()); }
    void editContour(int value, bool shift) { if (!isRouted(Routing::Target::StochasticContour)) setContour(contour() + value); }

    void printRate(StringBuilder &str) const { printRouted(str, Routing::Target::StochasticRate); str("%d%%", rate()); }
    void editRate(int value, bool shift) { if (!isRouted(Routing::Target::StochasticRate)) setRate(rate() + value); }

    void printVariation(StringBuilder &str) const { printRouted(str, Routing::Target::StochasticVariation); str("%+d%%", variation()); }
    void editVariation(int value, bool shift) { if (!isRouted(Routing::Target::StochasticVariation)) setVariation(variation() + value); }

    void printRest(StringBuilder &str) const { printRouted(str, Routing::Target::StochasticRest); str("%d%%", rest()); }
    void editRest(int value, bool shift) { if (!isRouted(Routing::Target::StochasticRest)) setRest(rest() + value); }

    void printSlide(StringBuilder &str) const { printRouted(str, Routing::Target::StochasticSlide); str("%d%%", slide()); }
    void editSlide(int value, bool shift) { if (!isRouted(Routing::Target::StochasticSlide)) setSlide(slide() + value); }

    void printSleep(StringBuilder &str) const { printRouted(str, Routing::Target::StochasticSleep); str("%d%%", sleep()); }
    void editSleep(int value, bool shift) { if (!isRouted(Routing::Target::StochasticSleep)) setSleep(sleep() + value); }

    void printPatience(StringBuilder &str) const { printRouted(str, Routing::Target::StochasticPatience); str("%d%%", patience()); }
    void editPatience(int value, bool shift) { if (!isRouted(Routing::Target::StochasticPatience)) setPatience(patience() + value); }

    void printMutate(StringBuilder &str) const { printRouted(str, Routing::Target::StochasticMutate); str("%d%%", mutate()); }
    void editMutate(int value, bool shift) { if (!isRouted(Routing::Target::StochasticMutate)) setMutate(mutate() + value); }

    void printJump(StringBuilder &str) const { printRouted(str, Routing::Target::StochasticJump); str("%d%%", jump()); }
    void editJump(int value, bool shift) { if (!isRouted(Routing::Target::StochasticJump)) setJump(jump() + value); }

    void printBurstPitch(StringBuilder &str) const { str(burstPitch() == StochasticBurstPitch::Parent ? "Parent" : "Gen"); }
    void editBurstPitch(int value, bool shift) { setBurstPitch(ModelUtils::adjustedEnum(burstPitch(), value)); }

    void printRotate(StringBuilder &str) const { printRouted(str, Routing::Target::Rotate); str("%+d", rotate()); }
    void editRotate(int value, bool shift) { if (!isRouted(Routing::Target::Rotate)) setRotate(rotate() + value); }

    void printRootNote(StringBuilder &str) const {
        if (rootNote() == -1) {
            str("Default");
        } else {
            Types::printNote(str, rootNote());
        }
    }

    void editRootNote(int value, bool shift) {
        setRootNote(rootNote() + value);
    }

    // Phase 7 Pattern controls (Per-pattern metadata)
    int size() const { return _size; }
    void setSize(int size) {
        _size = clamp(size, 2, CONFIG_STEP_COUNT);
        _first = clamp(int(_first), 0, int(_size) - 1);
        _last = clamp(int(_last), int(_first), int(_size) - 1);
    }

    int first() const { return _first; }
    void setFirst(int first) { _first = clamp(first, 0, int(_size) - 1); }

    int last() const { return clamp(int(_last), first(), int(_size) - 1); }
    void setLast(int last) { _last = clamp(last, first(), int(_size) - 1); }

    bool patternValid() const { return rhythmValid() && melodyValid(); }
    void setPatternValid(bool valid) { setRhythmValid(valid); setMelodyValid(valid); }

    uint32_t seed() const { return rhythmSeed(); }
    void setSeed(uint32_t seed) { setRhythmSeed(seed); }

    void printNote(StringBuilder &str, int note, int defaultRootNote, int defaultScale) const {
        int rootNote = selectedRootNote(defaultRootNote);
        const auto &scale = selectedScale(defaultScale);
        scale.noteName(str, note, rootNote, Scale::Short1);
    }

    // Phase 6 scaffolding stubs for UI list (to keep current pages building)
    struct StepStub {
        int note() const { return 0; }
        bool gate() const { return false; }
        int gateProbability() const { return 0; }
        int noteVariationProbability() const { return 0; }
        int noteOctaveProbability() const { return 0; }
        int length() const { return 0; }
        int lengthVariationRange() const { return 0; }
        int lengthVariationProbability() const { return 0; }
        int retrigger() const { return 0; }
        int retriggerProbability() const { return 0; }
        Types::Condition condition() const { return Types::Condition::Off; }
        int gateOffset() const { return 0; }
        bool slide() const { return false; }
        bool accent() const { return false; }
        bool legato() const { return false; }
        void setGate(bool) {}
        void setGateProbability(int) {}
        void setNote(int) {}
        void setNoteVariationProbability(int) {}
        void setNoteOctaveProbability(int) {}
        void setLength(int) {}
        void setLengthVariationRange(int) {}
        void setLengthVariationProbability(int) {}
        void setRetrigger(int) {}
        void setRetriggerProbability(int) {}
        void setCondition(Types::Condition) {}
        void setGateOffset(int) {}
        void setSlide(bool) {}
        void setAccent(bool) {}
        void setLegato(bool) {}
    };

    const StepStub step(int index) const { return StepStub(); }
          StepStub step(int index)       { return StepStub(); }

    // Phase 8.2 Split Source Buffers
    const std::array<StochasticSourceEvent, CONFIG_STEP_COUNT> &events() const { return _events; }
          std::array<StochasticSourceEvent, CONFIG_STEP_COUNT> &events()       { return _events; }

    bool rhythmValid() const { return _rhythmValid; }
    void setRhythmValid(bool valid) { _rhythmValid = valid; }

    bool melodyValid() const { return _melodyValid; }
    void setMelodyValid(bool valid) { _melodyValid = valid; }

    uint32_t rhythmSeed() const { return _rhythmSeed; }
    void setRhythmSeed(uint32_t seed) { _rhythmSeed = seed; }

    uint32_t melodySeed() const { return _melodySeed; }
    void setMelodySeed(uint32_t seed) { _melodySeed = seed; }

    void clear() {
        _scale = -1;
        _rootNote = -1;
        _divisor = 12;
        _resetMeasure = 0;

        _size = 16;
        _first = 0;
        _last = 15;

        _rhythmValid = false;
        _melodyValid = false;
        _rhythmSeed = 0;
        _melodySeed = 0;

        for (int i = 0; i < CONFIG_USER_SCALE_SIZE; ++i) {
            _degreeTickets[i] = 10;
        }
        _linearity = 0;
        _degreeRotation = 0;
        _maskRotation = 0;
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
        _rotate.setBase(0);
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


        for (auto &event : _events) event.clear();
    }

    void write(VersionedSerializedWriter &writer) const {
        writer.write(_scale);
        writer.write(_rootNote);
        writer.write(_divisor);
        writer.write(_resetMeasure);

        writer.write(_size);
        writer.write(_first);
        writer.write(_last);

        writer.write(_rhythmValid);
        writer.write(_melodyValid);
        writer.write(_rhythmSeed);
        writer.write(_melodySeed);

        for (int i = 0; i < CONFIG_USER_SCALE_SIZE; ++i) writer.write(_degreeTickets[i]);
        writer.write(_linearity);
        writer.write(_degreeRotation);
        writer.write(_maskRotation);
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
        _rotate.write(writer);
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


        for (const auto &event : _events) {
            for (int i = 0; i < 6; ++i) {
                writer.write(event.bytes[i]);
            }
        }
    }

    void read(VersionedSerializedReader &reader) {
        reader.read(_scale);
        reader.read(_rootNote);
        reader.read(_divisor);
        reader.read(_resetMeasure);

        reader.read(_size);
        reader.read(_first);
        reader.read(_last);

        reader.read(_rhythmValid);
        reader.read(_melodyValid);
        reader.read(_rhythmSeed);
        reader.read(_melodySeed);

        for (int i = 0; i < CONFIG_USER_SCALE_SIZE; ++i) reader.read(_degreeTickets[i]);
        for (int i = 0; i < CONFIG_USER_SCALE_SIZE; ++i) _degreeTickets[i] = clamp(int(_degreeTickets[i]), -1, 100);
        reader.read(_linearity);
        _linearity = clamp(int(_linearity), 0, 100);
        reader.read(_degreeRotation);
        _degreeRotation = clamp(int(_degreeRotation), -32, 32);
        reader.read(_maskRotation);
        _maskRotation = clamp(int(_maskRotation), -32, 32);
        reader.read(_accentProb);
        _accentProb = clamp(int(_accentProb), 0, 100);
        reader.read(_legatoProb);
        _legatoProb = clamp(int(_legatoProb), 0, 100);
        uint8_t marblesMode;
        reader.read(marblesMode);
        _marblesMode = marblesMode < uint8_t(MarblesMode::Last) ? static_cast<MarblesMode>(marblesMode) : MarblesMode::Off;
        reader.read(_marblesSpread);
        _marblesSpread = clamp(int(_marblesSpread), 0, 100);
        reader.read(_marblesBias);
        _marblesBias = clamp(int(_marblesBias), 0, 100);
        reader.read(_marblesSteps);
        _marblesSteps = clamp(int(_marblesSteps), 1, 100);
        _density.read(reader);
        _tilt.read(reader);
        _reservedJitter.read(reader);
        _burst.read(reader);
        reader.read(_minDegree);
        _minDegree = clamp(int(_minDegree), 0, 127);
        reader.read(_maxDegree);
        _maxDegree = clamp(int(_maxDegree), 0, 127);
        _rotate.read(reader);
        _complexity.read(reader);
        _contour.read(reader);
        _rate.read(reader);
        _variation.read(reader);
        _rest.read(reader);
        _slide.read(reader);
        reader.read(_burstRate);
        _burstRate = clamp(int(_burstRate), 0, 100);
        reader.read(_burstCount);
        _burstCount = clamp(int(_burstCount), 0, 100);
        uint8_t burstPitch;
        reader.read(burstPitch);
        _burstPitch = burstPitch < uint8_t(StochasticBurstPitch::Last) ? static_cast<StochasticBurstPitch>(burstPitch) : StochasticBurstPitch::Parent;
        _sleep.read(reader);
        _patience.read(reader);
        _mutate.read(reader);
        _jump.read(reader);
        reader.read(_range);
        _range = clamp(int(_range), 1, 4);

        uint8_t rhythmMode, melodyMode;
        reader.read(rhythmMode);
        _rhythmMode = rhythmMode < uint8_t(StochasticSourceMode::Last) ? static_cast<StochasticSourceMode>(rhythmMode) : StochasticSourceMode::Loop;
        reader.read(melodyMode);
        _melodyMode = melodyMode < uint8_t(StochasticSourceMode::Last) ? static_cast<StochasticSourceMode>(melodyMode) : StochasticSourceMode::Loop;


        for (auto &event : _events) {
            for (int i = 0; i < 6; ++i) {
                reader.read(event.bytes[i]);
            }
        }

        sanitizeAfterRead();
    }

private:
    void sanitizeAfterRead() {
        _scale = clamp(int(_scale), -1, Scale::Count - 1);
        _rootNote = clamp(int(_rootNote), -1, 11);
        _divisor = ModelUtils::clampDivisor(_divisor);
        _resetMeasure = clamp(int(_resetMeasure), 0, 128);

        if (_size < 2 || _size > CONFIG_STEP_COUNT) {
            _size = 16;
            _first = 0;
            _last = 15;
            _rhythmValid = false;
            _melodyValid = false;
            _rhythmSeed = 0;
            _melodySeed = 0;

        for (int i = 0; i < CONFIG_USER_SCALE_SIZE; ++i) {
            _degreeTickets[i] = 10;
        }
        _linearity = 0;
        _degreeRotation = 0;
        _maskRotation = 0;
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
        _rotate.setBase(0);
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

            for (auto &event : _events) event.clear();
            return;
        }

        _first = clamp(int(_first), 0, int(_size) - 1);
        _last = clamp(int(_last), int(_first), int(_size) - 1);
        _rhythmValid = _rhythmValid ? true : false;
        _melodyValid = _melodyValid ? true : false;

        for (int i = _size; i < CONFIG_STEP_COUNT; ++i) {
            _events[i].clear();
        }
    }

    int8_t _trackIndex = -1;
    int8_t _scale = -1;
    int8_t _rootNote = -1;
    uint8_t _divisor = 12;
    uint8_t _resetMeasure = 0;

    // Phase 7 Generation Parameters
    uint8_t _size;
    uint8_t _first;
    uint8_t _last;

    // Phase 8.2 Split Source Buffers
    std::array<StochasticSourceEvent, CONFIG_STEP_COUNT> _events;
    bool _rhythmValid;
    bool _melodyValid;
    uint32_t _rhythmSeed;
    uint32_t _melodySeed;

    int8_t _degreeTickets[CONFIG_USER_SCALE_SIZE];
    uint8_t _linearity;
    int8_t _degreeRotation;
    int8_t _maskRotation;
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

    Routable<int8_t> _rotate;
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


    friend class StochasticTrack;
};

static_assert(sizeof(StochasticSequence) <= 544, "StochasticSequence too large");
