#include "TuesdaySequence.h"
#include "ProjectVersion.h"

// Algorithm names for display
static const char *algorithmNames[] = {
    "SIMPLE",     // 0
    "TRITRANCE",  // 1
    "STOMPER",    // 2
    "RESERVED3",  // 3 (MARKOV - not kept)
    "RESERVED4",  // 4 (CHIPARP - not kept)
    "RESERVED5",  // 5 (GOACID - not kept)
    "RESERVED6",  // 6 (SNH - not kept)
    "RESERVED7",  // 7 (WOBBLE - not kept)
    "RESERVED8",  // 8 (TECHNO - not kept)
    "RESERVED9",  // 9 (FUNK - not kept)
    "RESERVED10", // 10 (DRONE - not kept)
    "RESERVED11", // 11 (PHASE - not kept)
    "RESERVED12", // 12 (RAGA - not kept)
    "RESERVED13", // 13 (AMBIENT - not kept)
    "RESERVED14", // 14 (ACID - not kept)
    "RESERVED15", // 15 (DRILL - not kept)
    "RESERVED16", // 16 (MINIMAL - not kept)
    "RESERVED17", // 17 (KRAFT - not kept)
    "APHEX",      // 18
    "AUTECH",     // 19
    "STEPWAVE",   // 20
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
    case 3:  str("Aphex"); break; // Remapped 18
    case 4:  str("Autech"); break; // Remapped 19
    case 5:  str("StepWave"); break; // Remapped 20
    case 6:  str("Markov"); break;
    case 7:  str("Chip1"); break;
    case 8:  str("Chip2"); break;
    case 9:  str("Wobble"); break;
    case 10: str("ScaleWlk"); break;
    case 11: str("Window"); break;
    case 12: str("Minimal"); break;
    case 13: str("Ganz"); break;
    case 14: str("Blake"); break;
    case 18: str("Aphex"); break;
    case 19: str("Autech"); break;
    case 20: str("StepWave"); break;
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
    _resetMeasure = 0;
    _scale = -1;  // Project
    _rootNote = -1;
    _rotate.clear();
    _gateLength.clear();
    _gateLength.setBase(50);
    _gateOffset.clear();
    _gateOffset.setBase(0); // Default 0% (Quantized)
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
    writer.write(_resetMeasure);
    writer.write(_scale);
    writer.write(_rootNote);
    _rotate.write(writer);
    _gateLength.write(writer);
    _gateOffset.write(writer);
}

void TuesdaySequence::read(VersionedSerializedReader &reader) {
    // All fields need version guards since TuesdayTrack is new in Version35
    // Defaults come from member initialization in TuesdayTrack.h
    reader.read(_algorithm.base, ProjectVersion::Version35);
    reader.read(_flow.base, ProjectVersion::Version35);
    reader.read(_ornament.base, ProjectVersion::Version35);
    reader.read(_power.base, ProjectVersion::Version35);
    reader.read(_start, ProjectVersion::Version35);
    reader.read(_loopLength, ProjectVersion::Version35);
    reader.read(_glide.base, ProjectVersion::Version35);
    reader.read(_trill.base, ProjectVersion::Version41);
    reader.read(_stepTrill.base, ProjectVersion::Version51);
    reader.read(_skew, ProjectVersion::Version35);
    reader.read(_cvUpdateMode, ProjectVersion::Version35);
    reader.read(_octave.base, ProjectVersion::Version35);
    reader.read(_transpose.base, ProjectVersion::Version35);
    if (reader.dataVersion() < ProjectVersion::Version10) {
        reader.readAs<uint8_t>(_divisor.base);
    } else {
        reader.read(_divisor.base);
    }
    reader.read(_resetMeasure, ProjectVersion::Version35);
    reader.read(_scale, ProjectVersion::Version35);
    reader.read(_rootNote, ProjectVersion::Version35);
    _rotate.read(reader);
    _gateLength.read(reader);
    _gateOffset.read(reader);
}
