#include "DiscreteMapSequence.h"
#include "ProjectVersion.h"

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
    _divisor = 192;
    _gateLength = 1;
    _loop = true;
    _thresholdMode = ThresholdMode::Position;
    _scaleSource = ScaleSource::Project;
    _scale = 0;
    _rootNote = 0;
    _slewEnabled = false;

    for (auto &stage : _stages) {
        stage.clear();
    }
}

void DiscreteMapSequence::clearStage(int index) {
    if (index >= 0 && index < StageCount) {
        _stages[index].clear();
    }
}

void DiscreteMapSequence::write(VersionedSerializedWriter &writer) const {
    writer.write(static_cast<uint8_t>(_clockSource));
    writer.write(_divisor);
    writer.write(_gateLength);
    writer.write(_loop);
    writer.write(static_cast<uint8_t>(_thresholdMode));
    writer.write(static_cast<uint8_t>(_scaleSource));
    writer.write(_scale);
    writer.write(_rootNote);
    writer.write(_slewEnabled);

    for (const auto &stage : _stages) {
        stage.write(writer);
    }
}

void DiscreteMapSequence::read(VersionedSerializedReader &reader) {
    uint8_t clockSource, thresholdMode, scaleSource;

    reader.read(clockSource);
    _clockSource = static_cast<ClockSource>(clockSource);

    reader.read(_divisor);
    reader.read(_gateLength);
    reader.read(_loop);

    reader.read(thresholdMode);
    _thresholdMode = static_cast<ThresholdMode>(thresholdMode);

    reader.read(scaleSource);
    _scaleSource = static_cast<ScaleSource>(scaleSource);

    if (reader.dataVersion() >= ProjectVersion::Version58) {
        reader.read(_scale);
    } else {
        _scale = 0;
    }

    reader.read(_rootNote);
    reader.read(_slewEnabled);

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
        setScaleSource(ScaleSource::Track);
        break;
    case Routing::Target::RootNote:
        setRootNote(intValue);
        break;
    default:
        break;
    }
}
