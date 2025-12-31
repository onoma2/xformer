#include "TuesdaySequence.h"
#include "ProjectVersion.h"

// Algorithm names for display
static const char *algorithmNames[] = {
    "SIMPLE",     // 0
    "TRITRANCE",  // 1
    "STOMPER",    // 2
    "MARKOV",     // 3
    "CHIPARP1",   // 4
    "CHIPARP2",   // 5
    "WOBBLE",     // 6
    "SCALEWALK",  // 7
    "WINDOW",     // 8
    "MINIMAL",    // 9
    "GANZ",       // 10
    "BLAKE",      // 11
    "APHEX",      // 12
    "AUTECH",     // 13
    "STEPWAVE",   // 14
    "RESERVED15", // 15
    "RESERVED16", // 16
    "RESERVED17", // 17
    "RESERVED18", // 18
    "RESERVED19", // 19
    "RESERVED20", // 20
};

// Loop length values: Inf (0), 1-16, 19, 21, 24, 32, 35, 42, 48, 56, 64, 95, 96, 127, 128
static const int loopLengthValues[] = {
    0,   // Inf (infinite/evolving)
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
    19, 21, 24, 32, 35, 42, 48, 56, 64, 95, 96, 127, 128
};

void TuesdaySequence::printAlgorithm(StringBuilder &str) const {
    printRouted(str, Routing::Target::Algorithm);
    switch (algorithm()) {
    case 0:  str("Test"); break;
    case 1:  str("TriTrance"); break;
    case 2:  str("Stomper"); break;
    case 3:  str("Markov"); break;
    case 4:  str("Chip1"); break;
    case 5:  str("Chip2"); break;
    case 6:  str("Wobble"); break;
    case 7:  str("ScaleWlk"); break;
    case 8:  str("Window"); break;
    case 9:  str("Minimal"); break;
    case 10: str("Ganz"); break;
    case 11: str("Blake"); break;
    case 12: str("Aphex"); break;
    case 13: str("Autech"); break;
    case 14: str("StepWave"); break;
    default: str("%d", algorithm()); break;
    }
}

void TuesdaySequence::printLoopLength(StringBuilder &str) const {
    if (_loopLength == 0) {
        str("Inf");
    } else if (_loopLength < 30) {
        str("%d", loopLengthValues[_loopLength]);
    } else {
        str("???");
    }
}

int TuesdaySequence::actualLoopLength() const {
    if (_loopLength < 30) {
        return loopLengthValues[_loopLength];
    }
    return 16; // Default fallback
}

void TuesdaySequence::writeRouted(Routing::Target target, int intValue, float floatValue) {
    switch (target) {
    case Routing::Target::Algorithm:
        setAlgorithm(intValue, true);
        break;
    case Routing::Target::Flow:
        setFlow(intValue, true);
        break;
    case Routing::Target::Ornament:
        setOrnament(intValue, true);
        break;
    case Routing::Target::Power:
        setPower(intValue, true);
        break;
    case Routing::Target::Glide:
        setGlide(intValue, true);
        break;
    case Routing::Target::Trill:
        setTrill(intValue, true);
        break;
    case Routing::Target::StepTrill:
        setStepTrill(intValue, true);
        break;
    case Routing::Target::Octave:
        setOctave(intValue, true);
        break;
    case Routing::Target::Transpose:
        setTranspose(intValue, true);
        break;
    case Routing::Target::Divisor:
        setDivisor(intValue, true);
        break;
    case Routing::Target::ClockMult:
        setClockMultiplier(intValue, true);
        break;
    case Routing::Target::Rotate:
        setRotate(intValue, true);
        break;
    case Routing::Target::GateLength:
        _gateLength.write(intValue);
        break;
    case Routing::Target::GateOffset:
        _gateOffset.write(intValue);
        break;
    default:
        break;
    }
}

void TuesdaySequence::clear() {
    setAlgorithm(0);
    setFlow(0);
    setOrnament(0);
    setPower(0);
    _start = 0;
    _loopLength = 0;
    _glide.clear();
    _glide.setBase(50); // Default 50%
    _trill.clear();
    _trill.setBase(50); // Default 50%
    _skew = 0;
    _cvUpdateMode = Free;

    _octave.clear();
    _transpose.clear();
    _divisor.clear();
    _divisor.setBase(12); // 1/16
    _clockMultiplier.clear();
    _clockMultiplier.setBase(100);
    _resetMeasure = 0;
    _scale = -1;  // Project
    _rootNote = -1;
    _rotate.clear();
    _gateLength.clear();
    _gateLength.setBase(50);
    _gateOffset.clear();
    _gateOffset.setBase(0); // Default 0% (Quantized)
    _maskParameter = 0; // Default: ALL (no skipping)
    _timeMode = 0; // Default: FREE mode
    _maskProgression = 0; // Default: no progression
}

void TuesdaySequence::write(VersionedSerializedWriter &writer) const {
    writer.write(_algorithm.base);
    writer.write(_flow.base);
    writer.write(_ornament.base);
    writer.write(_power.base);
    writer.write(_start);
    writer.write(_loopLength);
    writer.write(_glide.base);
    writer.write(_trill.base);
    writer.write(_stepTrill.base);
    writer.write(_skew);
    writer.write(_cvUpdateMode);
    writer.write(_octave.base);
    writer.write(_transpose.base);
    writer.write(_divisor.base);
    writer.write(_clockMultiplier.base);
    writer.write(_resetMeasure);
    writer.write(_scale);
    writer.write(_rootNote);
    _rotate.write(writer);
    _gateLength.write(writer);
    _gateOffset.write(writer);
    writer.write(_maskParameter);
    writer.write(_timeMode);
    writer.write(_maskProgression);
}

void TuesdaySequence::read(VersionedSerializedReader &reader) {
    // All fields need version guards since TuesdayTrack is new in Version35
    // Defaults come from member initialization in TuesdayTrack.h
    reader.read(_algorithm.base);
    reader.read(_flow.base);
    reader.read(_ornament.base);
    reader.read(_power.base);
    reader.read(_start);
    reader.read(_loopLength);
    reader.read(_glide.base);
    reader.read(_trill.base);
    reader.read(_stepTrill.base);
    reader.read(_skew);
    reader.read(_cvUpdateMode);
    reader.read(_octave.base);
    reader.read(_transpose.base);
    reader.read(_divisor.base);
    reader.read(_clockMultiplier.base);
    reader.read(_resetMeasure);
    reader.read(_scale);
    reader.read(_rootNote);
    _rotate.read(reader);
    _gateLength.read(reader);
    _gateOffset.read(reader);
    reader.read(_maskParameter);
    reader.read(_timeMode);
    reader.read(_maskProgression);
}
