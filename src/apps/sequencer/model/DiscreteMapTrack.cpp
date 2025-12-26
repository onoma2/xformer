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
    _routedScanner = 0.f;
    _routedSync = 0.f;
    setCvUpdateMode(CvUpdateMode::Gate);  // Initialize default value
    setPlayMode(Types::PlayMode::Aligned);
    for (auto &sequence : _sequences) {
        sequence.clear();
    }
}

void DiscreteMapTrack::write(VersionedSerializedWriter &writer) const {
    writer.write(_cvUpdateMode);
    writer.write(_playMode);
    writeArray(writer, _sequences);
}

void DiscreteMapTrack::read(VersionedSerializedReader &reader) {
    reader.read(_cvUpdateMode);
    reader.read(_playMode);
    readArray(reader, _sequences);
}

void DiscreteMapTrack::writeRouted(Routing::Target target, int intValue, float floatValue) {
    switch (target) {
    case Routing::Target::DiscreteMapInput:
        _routedInput = floatValue;
        break;
    case Routing::Target::DiscreteMapScanner:
        _routedScanner = floatValue;
        break;
    case Routing::Target::DiscreteMapSync:
        _routedSync = floatValue;
        break;
    case Routing::Target::DiscreteMapRangeHigh:
    case Routing::Target::DiscreteMapRangeLow:
    case Routing::Target::SlideTime:
    case Routing::Target::Octave:
    case Routing::Target::Transpose:
    case Routing::Target::Offset:
        // Sequence-level params: apply to all patterns
        for (auto &sequence : _sequences) {
            sequence.writeRouted(target, intValue, floatValue);
        }
        break;
    default:
        break;
    }
}
