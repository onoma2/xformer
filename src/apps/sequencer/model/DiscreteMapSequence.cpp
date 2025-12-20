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
    _slewEnabled = false;
    _rangeHigh = 5.0f;
    _rangeLow = -5.0f;

    for (auto &stage : _stages) {
        stage.clear();
    }
}

void DiscreteMapSequence::clearStage(int index) {
    if (index >= 0 && index < StageCount) {
        _stages[index].clear();
    }
}

void DiscreteMapSequence::clearThresholds() {
    for (auto &stage : _stages) {
        stage.setThreshold(0);
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
        auto randomDir = static_cast<DiscreteMapSequence::Stage::TriggerDir>(rng.nextRange(3));
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
        // Randomly select one of the three direction options: Rise, Fall, or Off
        auto randomDir = static_cast<DiscreteMapSequence::Stage::TriggerDir>(rng.nextRange(3));
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
    writer.write(_slewEnabled);
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

    if (reader.dataVersion() >= ProjectVersion::Version60) {
        reader.read(syncMode);
        _syncMode = ModelUtils::clampedEnum(static_cast<SyncMode>(syncMode));
    } else {
        _syncMode = SyncMode::Off;
    }

    reader.read(_divisor);
    reader.read(_gateLength);
    reader.read(_loop);

    if (reader.dataVersion() >= ProjectVersion::Version60) {
        reader.read(_resetMeasure);
    } else {
        _resetMeasure = 8;
    }

    reader.read(thresholdMode);
    _thresholdMode = static_cast<ThresholdMode>(thresholdMode);

    if (reader.dataVersion() >= ProjectVersion::Version58) {
        reader.read(_scale);
    } else {
        _scale = -1;
    }

    reader.read(_rootNote);
    reader.read(_slewEnabled);

    if (reader.dataVersion() >= ProjectVersion::Version61) {
        reader.read(_rangeHigh);
        reader.read(_rangeLow);
    } else {
        // Backward compatibility: default to full ±5V range
        _rangeHigh = 5.0f;
        _rangeLow = -5.0f;
    }

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
