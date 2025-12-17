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
    for (auto &sequence : _sequences) {
        sequence.clear();
    }
}

void DiscreteMapTrack::write(VersionedSerializedWriter &writer) const {
    writer.write(_octave);
    writer.write(_transpose);
    writeArray(writer, _sequences);
}

void DiscreteMapTrack::read(VersionedSerializedReader &reader) {
    if (reader.dataVersion() >= ProjectVersion::Version59) {
        reader.read(_octave);
        reader.read(_transpose);
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
