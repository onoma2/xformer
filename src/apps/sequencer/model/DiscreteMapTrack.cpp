#include "DiscreteMapTrack.h"
#include "ProjectVersion.h"

void DiscreteMapTrack::gateOutputName(int index, StringBuilder &str) const {
    str("G%d", _trackIndex + 1);
}

void DiscreteMapTrack::cvOutputName(int index, StringBuilder &str) const {
    str("CV%d", _trackIndex + 1);
}

void DiscreteMapTrack::clear() {
    _routedInput = 0.f;
    _routedThresholdBias = 0.f;
    _routedSync = 0.f;
    _octave = 0;
    _transpose = 0;
    _offset = 0;
    setCvUpdateMode(CvUpdateMode::Gate);  // Initialize default value
    for (auto &sequence : _sequences) {
        sequence.clear();
    }
}

void DiscreteMapTrack::write(VersionedSerializedWriter &writer) const {
    writer.write(_octave);
    writer.write(_transpose);
    writer.write(_cvUpdateMode);
    writeArray(writer, _sequences);
}

void DiscreteMapTrack::read(VersionedSerializedReader &reader) {
    if (reader.dataVersion() >= ProjectVersion::Version59) {
        reader.read(_octave);
        reader.read(_transpose);
    }
    if (reader.dataVersion() >= ProjectVersion::Version62) { // Added DiscreteMapTrack cvUpdateMode
        reader.read(_cvUpdateMode);
    } else {
        setCvUpdateMode(CvUpdateMode::Gate); // Default for older projects
    }
    readArray(reader, _sequences);
}

void DiscreteMapTrack::writeRouted(Routing::Target target, int intValue, float floatValue) {
    switch (target) {
    case Routing::Target::DiscreteMapInput:
        _routedInput = floatValue;
        break;
    case Routing::Target::DiscreteMapThreshold:
        _routedThresholdBias = floatValue;
        break;
    case Routing::Target::DiscreteMapSync:
        _routedSync = floatValue;
        break;
    case Routing::Target::DiscreteMapRangeHigh:
    case Routing::Target::DiscreteMapRangeLow:
        // Sequence-level params: apply to all patterns
        for (auto &sequence : _sequences) {
            sequence.writeRouted(target, intValue, floatValue);
        }
        break;
    case Routing::Target::Octave:
        _octave = intValue;
        break;
    case Routing::Target::Transpose:
        _transpose = intValue;
        break;
    case Routing::Target::Offset:
        _offset = intValue;
        break;
    default:
        break;
    }
}
