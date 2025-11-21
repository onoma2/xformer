#include "TuesdayTrack.h"
#include "ProjectVersion.h"

// Algorithm names for display
static const char *algorithmNames[] = {
    "TEST",       // 0
    "TRITRANCE",  // 1
    "STOMPER",    // 2
    "MARKOV",     // 3
    "WOBBLE",     // 4
    "CHIPARP1",   // 5
    "CHIPARP2",   // 6
    "SNH",        // 7
    "SAIKO",      // 8
    "SAIKOLEAD",  // 9
    "SCALEWALK",  // 10
    "TOOEASY",    // 11
    "RANDOM",     // 12
    "CELLAUTO",   // 13
    "CHAOS",      // 14
    "FRACTAL",    // 15
    "WAVE",       // 16
    "DNA",        // 17
    "TURING",     // 18
    "PARTICLE",   // 19
    "NEURAL",     // 20
    "QUANTUM",    // 21
    "LSYSTEM",    // 22
    "TECHNO",     // 23
    "JUNGLE",     // 24
    "AMBIENT",    // 25
    "ACID",       // 26
    "FUNK",       // 27
    "DRILL",      // 28
    "MINIMAL",    // 29
    "KRAFTWERK",  // 30
    "APHEX",      // 31
    "BOARDS",     // 32
    "TANGERINE",  // 33
    "AUTECHRE",   // 34
    "SQUAREPUSH", // 35
    "DAFT",       // 36
};

// Loop length values: Inf (0), 1-16, 19, 21, 24, 32, 35, 42, 48, 56, 64
static const int loopLengthValues[] = {
    0,   // Inf (infinite/evolving)
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
    19, 21, 24, 32, 35, 42, 48, 56, 64
};

void TuesdayTrack::printAlgorithm(StringBuilder &str) const {
    if (_algorithm < 37) {
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
}

void TuesdayTrack::read(VersionedSerializedReader &reader) {
    reader.read(_algorithm);
    reader.read(_flow);
    reader.read(_ornament);
    reader.read(_power);
    reader.read(_loopLength);
    reader.read(_glide, 0);  // Default 0% for old projects
    reader.read(_useScale, false);  // Default free for old projects
    reader.read(_skew, 0);  // Default 0 for old projects
    reader.read(_cvUpdateMode, Free);  // Default Free for old projects (backward compatible)
}
