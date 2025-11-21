#include "TuesdayTrack.h"
#include "ProjectVersion.h"

// Algorithm names for display
static const char *algorithmNames[] = {
    "TEST",       // 0
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
};

// Loop length values: Inf (0), 1-16, 19, 21, 24, 32, 35, 42, 48, 56, 64
static const int loopLengthValues[] = {
    0,   // Inf (infinite/evolving)
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
    19, 21, 24, 32, 35, 42, 48, 56, 64
};

void TuesdayTrack::printAlgorithm(StringBuilder &str) const {
    if (_algorithm < 13) {
        str(algorithmNames[_algorithm]);
    } else {
        str("???");
    }
}

void TuesdayTrack::printLoopLength(StringBuilder &str) const {
    if (_loopLength == 0) {
        str("Inf");
    } else if (_loopLength < 26) {
        str("%d", loopLengthValues[_loopLength]);
    } else {
        str("???");
    }
}

int TuesdayTrack::actualLoopLength() const {
    if (_loopLength < 26) {
        return loopLengthValues[_loopLength];
    }
    return 16; // Default fallback
}

void TuesdayTrack::gateOutputName(int index, StringBuilder &str) const {
    str("G%d", _trackIndex + 1);
}

void TuesdayTrack::cvOutputName(int index, StringBuilder &str) const {
    str("CV%d", _trackIndex + 1);
}

void TuesdayTrack::clear() {
    _algorithm = 0;
    _flow = 0;
    _ornament = 0;
    _power = 0;
    _loopLength = 16;
    _glide = 0;
    _useScale = false;
    _skew = 0;
    _cvUpdateMode = Free;
    _octave = 0;
    _transpose = 0;
    _divisor = 12;
    _resetMeasure = 8;
    _scale = -1;
    _rootNote = -1;
    _scan = 0;
    _rotate = 0;
}

void TuesdayTrack::write(VersionedSerializedWriter &writer) const {
    writer.write(_algorithm);
    writer.write(_flow);
    writer.write(_ornament);
    writer.write(_power);
    writer.write(_loopLength);
    writer.write(_glide);
    writer.write(_useScale);
    writer.write(_skew);
    writer.write(_cvUpdateMode);
    writer.write(_octave);
    writer.write(_transpose);
    writer.write(_divisor);
    writer.write(_resetMeasure);
    writer.write(_scale);
    writer.write(_rootNote);
    writer.write(_scan);
    writer.write(_rotate);
}

void TuesdayTrack::read(VersionedSerializedReader &reader) {
    // All fields need version guards since TuesdayTrack is new in Version35
    reader.read(_algorithm, 0, ProjectVersion::Version35);
    reader.read(_flow, 0, ProjectVersion::Version35);
    reader.read(_ornament, 0, ProjectVersion::Version35);
    reader.read(_power, 0, ProjectVersion::Version35);
    reader.read(_loopLength, 16, ProjectVersion::Version35);
    reader.read(_glide, 0, ProjectVersion::Version35);
    reader.read(_useScale, false, ProjectVersion::Version35);
    reader.read(_skew, 0, ProjectVersion::Version35);
    reader.read(_cvUpdateMode, Free, ProjectVersion::Version35);
    reader.read(_octave, 0, ProjectVersion::Version35);
    reader.read(_transpose, 0, ProjectVersion::Version35);
    reader.read(_divisor, 12, ProjectVersion::Version35);
    reader.read(_resetMeasure, 8, ProjectVersion::Version35);
    reader.read(_scale, -1, ProjectVersion::Version35);
    reader.read(_rootNote, -1, ProjectVersion::Version35);
    reader.read(_scan, 0, ProjectVersion::Version35);
    reader.read(_rotate, 0, ProjectVersion::Version35);
}
