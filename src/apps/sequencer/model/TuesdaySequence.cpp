#include "TuesdaySequence.h"
#include "ProjectVersion.h"

// Algorithm names for display
static const char *algorithmNames[] = {
    "SIMPLE",     // 0
    "TRITRANCE",  // 1
    "STOMPER",    // 2
    "MARKOV",     // 3
    "CHIPARP",    // 4
    "GOACID",     // 5
    "SNH",        // 6
    "WOBBLE",     // 7
    "TECHNO",     // 8
    "FUNK",       // 9
    "DRONE",      // 10
    "PHASE",      // 11
    "RAGA",       // 12
    "AMBIENT",    // 13
    "ACID",       // 14
    "DRILL",      // 15
    "MINIMAL",    // 16
    "KRAFT",      // 17
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
    int algo = algorithm();
    if (algo < 21) {
        str(algorithmNames[algo]);
    } else {
        str("???");
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
    case Routing::Target::Octave:
        setOctave(intValue, true);
        break;
    case Routing::Target::Transpose:
        setTranspose(intValue, true);
        break;
    case Routing::Target::Divisor:
        setDivisor(intValue, true);
        break;
    case Routing::Target::Scan:
        setScan(intValue, true);
        break;
    case Routing::Target::Rotate:
        setRotate(intValue, true);
        break;
    case Routing::Target::GateOffset:
        setGateOffset(intValue, true);
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
    _loopLength = 0;  // Default: infinite (evolving patterns)
    setGlide(0);
    setTrill(0);
    _useScale = false;
    _skew = 0;
    _cvUpdateMode = Free;
    setOctave(0);
    setTranspose(0);
    setDivisor(12);
    _resetMeasure = 8;
    _scale = -1;
    _rootNote = -1;
    setScan(0);
    setRotate(0);
    setGateOffset(0);
}

void TuesdaySequence::write(VersionedSerializedWriter &writer) const {
    writer.write(_algorithm.base);
    writer.write(_flow.base);
    writer.write(_ornament.base);
    writer.write(_power.base);
    writer.write(_loopLength);
    writer.write(_glide.base);
    writer.write(_trill.base);
    writer.write(_useScale);
    writer.write(_skew);
    writer.write(_cvUpdateMode);
    writer.write(_octave.base);
    writer.write(_transpose.base);
    writer.write(_divisor.base);
    writer.write(_resetMeasure);
    writer.write(_scale);
    writer.write(_rootNote);
    writer.write(_scan.base);
    writer.write(_rotate.base);
    writer.write(_gateOffset.base);
}

void TuesdaySequence::read(VersionedSerializedReader &reader) {
    // All fields need version guards since TuesdayTrack is new in Version35
    // Defaults come from member initialization in TuesdayTrack.h
    reader.read(_algorithm.base, ProjectVersion::Version35);
    reader.read(_flow.base, ProjectVersion::Version35);
    reader.read(_ornament.base, ProjectVersion::Version35);
    reader.read(_power.base, ProjectVersion::Version35);
    reader.read(_loopLength, ProjectVersion::Version35);
    reader.read(_glide.base, ProjectVersion::Version35);
    reader.read(_trill.base, ProjectVersion::Version41);
    reader.read(_useScale, ProjectVersion::Version35);
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
    reader.read(_scan.base, ProjectVersion::Version35);
    reader.read(_rotate.base, ProjectVersion::Version35);
    reader.read(_gateOffset.base, ProjectVersion::Version40); // New in version 40
}
