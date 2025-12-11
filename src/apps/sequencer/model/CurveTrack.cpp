#include "CurveTrack.h"
#include "ProjectVersion.h"

void CurveTrack::writeRouted(Routing::Target target, int intValue, float floatValue) {
    switch (target) {
    case Routing::Target::SlideTime:
        setSlideTime(intValue, true);
        break;
    case Routing::Target::Offset:
        setOffset(intValue, true);
        break;
    case Routing::Target::Rotate:
        setRotate(intValue, true);
        break;
    case Routing::Target::ShapeProbabilityBias:
        setShapeProbabilityBias(intValue, true);
        break;
    case Routing::Target::GateProbabilityBias:
        setGateProbabilityBias(intValue, true);
        break;
    default:
        break;
    }
}

void CurveTrack::clear() {
    setPlayMode(Types::PlayMode::Aligned);
    setFillMode(FillMode::None);
    setMuteMode(MuteMode::LastValue);
    setSlideTime(0);
    setOffset(0);
    setRotate(0);
    setShapeProbabilityBias(0);
    setGateProbabilityBias(0);
    setGlobalPhase(0.f);

    for (auto &sequence : _sequences) {
        sequence.clear();
    }
}

void CurveTrack::write(VersionedSerializedWriter &writer) const {
    writer.write(_playMode);
    writer.write(_fillMode);
    writer.write(_muteMode);
    writer.write(_slideTime.base);
    writer.write(_offset.base);
    writer.write(_rotate.base);
    writer.write(_shapeProbabilityBias.base);
    writer.write(_gateProbabilityBias.base);
    writer.write(_globalPhase);
    writeArray(writer, _sequences);
}

void CurveTrack::read(VersionedSerializedReader &reader) {
    reader.read(_playMode);
    reader.read(_fillMode);
    reader.read(_muteMode, ProjectVersion::Version22);
    reader.read(_slideTime.base, ProjectVersion::Version8);
    reader.read(_offset.base, ProjectVersion::Version28);
    reader.read(_rotate.base);
    reader.read(_shapeProbabilityBias.base, ProjectVersion::Version15);
    reader.read(_gateProbabilityBias.base, ProjectVersion::Version15);
    if (reader.dataVersion() >= ProjectVersion::Version42) {
        reader.read(_globalPhase);
    } else {
        uint8_t phaseOffset;
        reader.read(phaseOffset, ProjectVersion::Version35);
        setGlobalPhase(float(phaseOffset) / 100.f);
    }
    if (reader.dataVersion() >= ProjectVersion::Version43) {
        float dummyFold;
        float dummyGain;
        reader.read(dummyFold);
        reader.read(dummyGain);
        // Discard old track-level wavefolder settings
        float dummy;
        reader.read(dummy); // _wavefolderSymmetry (or placeholder)
    }
    if (reader.dataVersion() >= ProjectVersion::Version44) {
        float dummyFilter;
        reader.read(dummyFilter);
    }
    if (reader.dataVersion() >= ProjectVersion::Version45) {
        float dummy;
        reader.read(dummy); // _foldF placeholder
        reader.read(dummy); // _filterF placeholder
    } 
    if (reader.dataVersion() >= ProjectVersion::Version46) {
        float dummyXFade;
        reader.read(dummyXFade);
    }
    if (reader.dataVersion() >= ProjectVersion::Version48) {
        int dummyChaosAmount;
        int dummyChaosRate;
        int dummyChaosParam1;
        int dummyChaosParam2;
        reader.read(dummyChaosAmount);
        reader.read(dummyChaosRate);
        reader.read(dummyChaosParam1);
        reader.read(dummyChaosParam2);
    }
    if (reader.dataVersion() >= ProjectVersion::Version49) {
        CurveSequence::ChaosAlgorithm dummyChaosAlgo;
        reader.read(dummyChaosAlgo);
    }
    readArray(reader, _sequences);
}

void CurveTrack::populateWithLfoShape(int sequenceIndex, Curve::Type shape, int firstStep, int lastStep) {
    if (sequenceIndex >= 0 && sequenceIndex < static_cast<int>(_sequences.size())) {
        _sequences[sequenceIndex].populateWithLfoShape(shape, firstStep, lastStep);
    }
}

void CurveTrack::populateWithLfoPattern(int sequenceIndex, Curve::Type shape, int firstStep, int lastStep) {
    if (sequenceIndex >= 0 && sequenceIndex < static_cast<int>(_sequences.size())) {
        _sequences[sequenceIndex].populateWithLfoPattern(shape, firstStep, lastStep);
    }
}

void CurveTrack::populateWithLfoWaveform(int sequenceIndex, Curve::Type upShape, Curve::Type downShape, int firstStep, int lastStep) {
    if (sequenceIndex >= 0 && sequenceIndex < static_cast<int>(_sequences.size())) {
        _sequences[sequenceIndex].populateWithLfoWaveform(upShape, downShape, firstStep, lastStep);
    }
}

void CurveTrack::populateWithSineWaveLfo(int sequenceIndex, int firstStep, int lastStep) {
    if (sequenceIndex >= 0 && sequenceIndex < static_cast<int>(_sequences.size())) {
        _sequences[sequenceIndex].populateWithSineWaveLfo(firstStep, lastStep);
    }
}

void CurveTrack::populateWithTriangleWaveLfo(int sequenceIndex, int firstStep, int lastStep) {
    if (sequenceIndex >= 0 && sequenceIndex < static_cast<int>(_sequences.size())) {
        _sequences[sequenceIndex].populateWithTriangleWaveLfo(firstStep, lastStep);
    }
}

void CurveTrack::populateWithSawtoothWaveLfo(int sequenceIndex, int firstStep, int lastStep) {
    if (sequenceIndex >= 0 && sequenceIndex < static_cast<int>(_sequences.size())) {
        _sequences[sequenceIndex].populateWithSawtoothWaveLfo(firstStep, lastStep);
    }
}

void CurveTrack::populateWithSquareWaveLfo(int sequenceIndex, int firstStep, int lastStep) {
    if (sequenceIndex >= 0 && sequenceIndex < static_cast<int>(_sequences.size())) {
        _sequences[sequenceIndex].populateWithSquareWaveLfo(firstStep, lastStep);
    }
}
