#include "TuesdaySequence.h"
#include "ProjectVersion.h"

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
    case 15: str("Turing"); break;
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

void TuesdaySequence::clear() {
    setAlgorithm(0);
    setFlow(0);
    setOrnament(0);
    setPower(0);
    _start = 0;
    _loopLength = 0;
    _glide = 50; // Default 50%
    _trill = 50; // Default 50%
    _skew = 0;
    _cvUpdateMode = Free;

    _octave = 0;
    _transpose = 0;
    _divisor = 12; // 1/16
    _clockMultiplier = 100;
    _resetMeasure = 0;
    _scaleGroup.raw = 0;
    setScale(-1);  // Project
    setRootNote(-1);
    setScaleRotate(-1);
    _rotate = 0;
    _gateLength = 50;
    _gateOffset = 0; // Default 0% (Quantized)
    _maskParameter = 0; // Default: ALL (no skipping)
    _timeMode = 0; // Default: FREE mode
    _maskProgression = 0; // Default: no progression
}

void TuesdaySequence::write(VersionedSerializedWriter &writer) const {
    writer.write(_algorithm);
    writer.write(_flow);
    writer.write(_ornament);
    writer.write(_power);
    writer.write(_start);
    writer.write(_loopLength);
    writer.write(_glide);
    writer.write(_trill);
    writer.write(_stepTrill);
    writer.write(_skew);
    writer.write(_cvUpdateMode);
    writer.write(_octave);
    writer.write(_transpose);
    writer.write(_divisor);
    writer.write(_clockMultiplier);
    writer.write(_resetMeasure);
    int8_t scaleField = rawScale();
    int8_t rootNoteField = rawRootNote();
    int8_t scaleRotateField = int8_t(scaleRotate());
    writer.write(scaleField);
    writer.write(rootNoteField);
    writer.write(scaleRotateField);
    writer.write(_rotate);
    writer.write(_gateLength);
    writer.write(_gateOffset);
    writer.write(_maskParameter);
    writer.write(_timeMode);
    writer.write(_maskProgression);
}

void TuesdaySequence::read(VersionedSerializedReader &reader) {
    // Defaults come from member initialization in TuesdayTrack.h
    reader.read(_algorithm);
    reader.read(_flow);
    reader.read(_ornament);
    reader.read(_power);
    reader.read(_start);
    reader.read(_loopLength);
    reader.read(_glide);
    reader.read(_trill);
    reader.read(_stepTrill);
    reader.read(_skew);
    reader.read(_cvUpdateMode);
    reader.read(_octave);
    reader.read(_transpose);
    reader.read(_divisor);
    reader.read(_clockMultiplier);
    reader.read(_resetMeasure);
    int8_t scaleField = 0;
    int8_t rootNoteField = 0;
    int8_t scaleRotateField = 0;
    reader.read(scaleField);
    reader.read(rootNoteField);
    reader.read(scaleRotateField);
    setScale(scaleField);
    setRootNote(rootNoteField);
    setScaleRotate(scaleRotateField);
    reader.read(_rotate);
    reader.read(_gateLength);
    reader.read(_gateOffset);
    reader.read(_maskParameter);
    reader.read(_timeMode);
    reader.read(_maskProgression);
}
