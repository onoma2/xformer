#include "DiscreteMapSequence.h"
#include "ProjectVersion.h"
#include "core/utils/Random.h"
#include "os/os.h"

static Random rng;

//----------------------------------------
// Stage
//----------------------------------------

void DiscreteMapSequence::Stage::clear() {
    _threshold = 0;
    _direction = TriggerDir::Off;
    _noteIndex = 0;
}

void DiscreteMapSequence::Stage::write(VersionedSerializedWriter &writer) const {
    writer.write(_threshold);
    writer.write(static_cast<uint8_t>(_direction));
    writer.write(_noteIndex);
}

void DiscreteMapSequence::Stage::read(VersionedSerializedReader &reader) {
    int8_t threshold;
    reader.read(threshold);
    setThreshold(threshold);
    uint8_t dir;
    reader.read(dir);
    _direction = static_cast<TriggerDir>(dir);
    reader.read(_noteIndex);
}

//----------------------------------------
// DiscreteMapSequence
//----------------------------------------

void DiscreteMapSequence::clear() {
    _clockSource = ClockSource::Internal;
    _syncMode = SyncMode::Off;
    _divisor = 192;
    _gateLength = 0; // 1T default
    _loop = true;
    _resetMeasure = 8;
    _thresholdMode = ThresholdMode::Position;
    _scale = -1;
    _rootNote = 0;
    setSlewTime(0);
    _octave = 0;
    _transpose = 0;
    _offset = 0;
    _rangeHigh = 5.0f;
    _rangeLow = -5.0f;

    // Initialize with interleaved threshold distribution (fret pattern)
    // Uses round-robin interleaving across all 4 sections (pages)
    const int min_val = -100;
    const int max_val = 100;
    const int active_pages = 4;
    const int total_toggles = 8 * active_pages; // 32 stages
    const float step = (max_val - min_val) / float(total_toggles - 1);

    for (int i = 0; i < StageCount; ++i) {
        auto &stage = _stages[i];

        // Calculate page (1-based) and button (0-based)
        int page = (i / 8) + 1;      // 1-4
        int button = i % 8;          // 0-7

        // Calculate global index using round-robin interleaving
        int global_index = button * active_pages + (page - 1);

        // Calculate threshold value
        float value = min_val + global_index * step;
        int threshold = static_cast<int>(value + 0.5f); // Round to nearest

        // Clamp to range (handles rounding edge cases)
        if (threshold < min_val) threshold = min_val;
        if (threshold > max_val) threshold = max_val;

        stage.setThreshold(threshold);
        stage.setDirection(Stage::TriggerDir::Off); // All inactive by default
        stage.setNoteIndex(0);
    }
}

void DiscreteMapSequence::clearStage(int index) {
    if (index >= 0 && index < StageCount) {
        _stages[index].clear();
    }
}

void DiscreteMapSequence::clearThresholds() {
    // Initialize with interleaved threshold distribution (fret pattern)
    // Uses round-robin interleaving across all 4 sections (pages)
    const int min_val = -100;
    const int max_val = 100;
    const int active_pages = 4;
    const int total_toggles = 8 * active_pages; // 32 stages
    const float step = (max_val - min_val) / float(total_toggles - 1);

    for (int i = 0; i < StageCount; ++i) {
        auto &stage = _stages[i];

        // Calculate page (1-based) and button (0-based)
        int page = (i / 8) + 1;      // 1-4
        int button = i % 8;          // 0-7

        // Calculate global index using round-robin interleaving
        int global_index = button * active_pages + (page - 1);

        // Calculate threshold value
        float value = min_val + global_index * step;
        int threshold = static_cast<int>(value + 0.5f); // Round to nearest

        // Clamp to range (handles rounding edge cases)
        if (threshold < min_val) threshold = min_val;
        if (threshold > max_val) threshold = max_val;

        stage.setThreshold(threshold);
    }
}

void DiscreteMapSequence::clearNotes() {
    for (auto &stage : _stages) {
        stage.setNoteIndex(0);
    }
}

void DiscreteMapSequence::randomize() {
    for (auto &stage : _stages) {
        stage.setThreshold(rng.nextRange(199) - 99);
        stage.setNoteIndex(rng.nextRange(128) - 63);
        auto randomDir = static_cast<DiscreteMapSequence::Stage::TriggerDir>(rng.nextRange(4));
        stage.setDirection(randomDir);
    }
}

void DiscreteMapSequence::randomizeThresholds() {
    for (auto &stage : _stages) {
        stage.setThreshold(rng.nextRange(199) - 99);
    }
}

void DiscreteMapSequence::randomizeNotes() {
    for (auto &stage : _stages) {
        // Note index range is -63 to +64, so we need a range of 128 values
        stage.setNoteIndex(rng.nextRange(128) - 63);
    }
}

void DiscreteMapSequence::randomizeDirections() {
    for (auto &stage : _stages) {
        // Randomly select one of the direction options: Rise, Fall, Off, or Both
        auto randomDir = static_cast<DiscreteMapSequence::Stage::TriggerDir>(rng.nextRange(4));
        stage.setDirection(randomDir);
    }
}

void DiscreteMapSequence::write(VersionedSerializedWriter &writer) const {
    writer.write(static_cast<uint8_t>(_clockSource));
    writer.write(static_cast<uint8_t>(_syncMode));
    writer.write(_divisor);
    writer.write(_gateLength);
    writer.write(_loop);
    writer.write(_resetMeasure);
    writer.write(static_cast<uint8_t>(_thresholdMode));
    writer.write(_scale);
    writer.write(_rootNote);
    _slewTime.write(writer);
    writer.write(_octave);
    writer.write(_transpose);
    writer.write(_offset);
    writer.write(_rangeHigh);
    writer.write(_rangeLow);

    for (const auto &stage : _stages) {
        stage.write(writer);
    }
}

void DiscreteMapSequence::read(VersionedSerializedReader &reader) {
    uint8_t clockSource, thresholdMode;
    uint8_t syncMode;

    reader.read(clockSource);
    _clockSource = static_cast<ClockSource>(clockSource);

    reader.read(syncMode);
    _syncMode = ModelUtils::clampedEnum(static_cast<SyncMode>(syncMode));

    reader.read(_divisor);
    reader.read(_gateLength);
    reader.read(_loop);

    reader.read(_resetMeasure);

    reader.read(thresholdMode);
    _thresholdMode = static_cast<ThresholdMode>(thresholdMode);

    reader.read(_scale);

    reader.read(_rootNote);
    _slewTime.read(reader);
    reader.read(_octave);
    reader.read(_transpose);
    reader.read(_offset);

    reader.read(_rangeHigh);
    reader.read(_rangeLow);

    for (auto &stage : _stages) {
        stage.read(reader);
    }
}

void DiscreteMapSequence::writeRouted(Routing::Target target, int intValue, float floatValue) {
    switch (target) {
    case Routing::Target::Divisor:
        setDivisor(intValue);
        break;
    case Routing::Target::Scale:
        setScale(intValue);
        break;
    case Routing::Target::RootNote:
        setRootNote(intValue);
        break;
    case Routing::Target::Octave:
        setOctave(intValue);
        break;
    case Routing::Target::Transpose:
        setTranspose(intValue);
        break;
    case Routing::Target::Offset:
        setOffset(intValue);
        break;
    case Routing::Target::SlideTime:
        setSlewTime(intValue, true);
        break;
    case Routing::Target::DiscreteMapRangeHigh:
        setRangeHigh(floatValue);
        break;
    case Routing::Target::DiscreteMapRangeLow:
        setRangeLow(floatValue);
        break;
    default:
        break;
    }
}
