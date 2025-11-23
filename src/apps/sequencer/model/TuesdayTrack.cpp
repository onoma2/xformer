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
    "AMBIENT",    // 13
    "ACID",       // 14
    "DRILL",      // 15
    "MINIMAL",    // 16
    "KRAFT",      // 17
    "APHEX",      // 18
    "AUTECH",     // 19
};

// Loop length values: Inf (0), 1-16, 19, 21, 24, 32, 35, 42, 48, 56, 64, 95, 96, 127, 128
static const int loopLengthValues[] = {
    0,   // Inf (infinite/evolving)
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
    19, 21, 24, 32, 35, 42, 48, 56, 64, 95, 96, 127, 128
};

void TuesdayTrack::printAlgorithm(StringBuilder &str) const {
    if (_algorithm < 20) {
        str(algorithmNames[_algorithm]);
    } else {
        str("???");
    }
}

void TuesdayTrack::printLoopLength(StringBuilder &str) const {
    if (_loopLength == 0) {
        str("Inf");
    } else if (_loopLength < 30) {
        str("%d", loopLengthValues[_loopLength]);
    } else {
        str("???");
    }
}

int TuesdayTrack::actualLoopLength() const {
    if (_loopLength < 30) {
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
    writer.write(_trill);
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
    writer.write(_gateOffset);
}

void TuesdayTrack::read(VersionedSerializedReader &reader) {
    // All fields need version guards since TuesdayTrack is new in Version35
    // Defaults come from member initialization in TuesdayTrack.h
    reader.read(_algorithm, ProjectVersion::Version35);
    reader.read(_flow, ProjectVersion::Version35);
    reader.read(_ornament, ProjectVersion::Version35);
    reader.read(_power, ProjectVersion::Version35);
    reader.read(_loopLength, ProjectVersion::Version35);
    reader.read(_glide, ProjectVersion::Version35);
    reader.read(_trill, ProjectVersion::Version41);
    reader.read(_useScale, ProjectVersion::Version35);
    reader.read(_skew, ProjectVersion::Version35);
    reader.read(_cvUpdateMode, ProjectVersion::Version35);
    reader.read(_octave, ProjectVersion::Version35);
    reader.read(_transpose, ProjectVersion::Version35);
    reader.read(_divisor, ProjectVersion::Version35);
    reader.read(_resetMeasure, ProjectVersion::Version35);
    reader.read(_scale, ProjectVersion::Version35);
    reader.read(_rootNote, ProjectVersion::Version35);
    reader.read(_scan, ProjectVersion::Version35);
    reader.read(_rotate, ProjectVersion::Version35);
    reader.read(_gateOffset, ProjectVersion::Version40); // New in version 40
}
