#include "Track.h"
#include "Project.h"
#include "ProjectVersion.h"

void Track::clear() {
  _trackMode = TrackMode::Default;
  _runGate.clear();
  _runGate.base = 1; // Default to Run = On
  _cvOutputRotate.clear();
  _gateOutputRotate.clear();

  initContainer();
}

void Track::clearPattern(int patternIndex) {
    switch (_trackMode) {
    case TrackMode::Note:
        _track.note->sequence(patternIndex).clear();
        break;
    case TrackMode::Curve:
        _track.curve->sequence(patternIndex).clear();
        break;
    case TrackMode::MidiCv:
        break;
    case TrackMode::Tuesday:
        _track.tuesday->sequence(patternIndex).clear();
        break;
    case TrackMode::DiscreteMap:
        _track.discreteMap->sequence(patternIndex).clear();
        break;
    case TrackMode::Indexed:
        _track.indexed->sequence(patternIndex).clear();
        break;
    case TrackMode::Stochastic:
        _track.stochastic->sequence(patternIndex).clear();
        break;
    case TrackMode::Fractal:
        _track.fractal->sequence(patternIndex).clear();
        break;
    case TrackMode::PhaseFlux:
        _track.phaseFlux->sequence(patternIndex).clear();
        break;
    case TrackMode::TeletypeV2:
        // TT2 is not pattern-based — nothing per-pattern to clear.
        break;
    case TrackMode::TeletypeMini:
        break;
    case TrackMode::Last:
        break;
    }
}

void Track::copyPattern(int src, int dst) {
    switch (_trackMode) {
    case TrackMode::Note:
        _track.note->sequence(dst) = _track.note->sequence(src);
        break;
    case TrackMode::Curve:
        _track.curve->sequence(dst) = _track.curve->sequence(src);
        break;
    case TrackMode::MidiCv:
        break;
    case TrackMode::Tuesday:
        _track.tuesday->sequence(dst) = _track.tuesday->sequence(src);
        break;
    case TrackMode::DiscreteMap:
        _track.discreteMap->sequence(dst) = _track.discreteMap->sequence(src);
        break;
    case TrackMode::Indexed:
        _track.indexed->sequence(dst) = _track.indexed->sequence(src);
        break;
    case TrackMode::Stochastic:
        _track.stochastic->sequence(dst) = _track.stochastic->sequence(src);
        break;
    case TrackMode::Fractal:
        _track.fractal->sequence(dst) = _track.fractal->sequence(src);
        break;
    case TrackMode::PhaseFlux:
        _track.phaseFlux->sequence(dst) = _track.phaseFlux->sequence(src);
        break;
    case TrackMode::TeletypeV2:
        // TT2 is not pattern-based — nothing per-pattern to copy.
        break;
    case TrackMode::TeletypeMini:
        break;
    case TrackMode::Last:
        break;
    }
}

bool Track::duplicatePattern(int patternIndex) {
    if (patternIndex < CONFIG_PATTERN_COUNT - 1) {
        copyPattern(patternIndex, patternIndex + 1);
        return true;
    }
    return false;
}

void Track::gateOutputName(int index, StringBuilder &str) const {
    switch (_trackMode) {
    case TrackMode::Note:
    case TrackMode::Curve:
        str("Gate");
        break;
    case TrackMode::MidiCv:
        _track.midiCv->gateOutputName(index, str);
        break;
    case TrackMode::Tuesday:
        _track.tuesday->gateOutputName(index, str);
        break;
    case TrackMode::DiscreteMap:
        _track.discreteMap->gateOutputName(index, str);
        break;
    case TrackMode::Indexed:
        _track.indexed->gateOutputName(index, str);
        break;
    case TrackMode::Stochastic:
        str("Gate");
        break;
    case TrackMode::Fractal:
        str("Gate");
        break;
    case TrackMode::PhaseFlux:
        str("Gate");
        break;
    case TrackMode::TeletypeV2:
        str("Gate");
        break;
    case TrackMode::TeletypeMini:
        str("Gate");
        break;
    case TrackMode::Last:
        break;
    }
}

void Track::cvOutputName(int index, StringBuilder &str) const {
    switch (_trackMode) {
    case TrackMode::Note:
    case TrackMode::Curve:
        str("CV");
        break;
    case TrackMode::MidiCv:
        _track.midiCv->cvOutputName(index, str);
        break;
    case TrackMode::Tuesday:
        _track.tuesday->cvOutputName(index, str);
        break;
    case TrackMode::DiscreteMap:
        _track.discreteMap->cvOutputName(index, str);
        break;
    case TrackMode::Indexed:
        _track.indexed->cvOutputName(index, str);
        break;
    case TrackMode::Stochastic:
        str("CV");
        break;
    case TrackMode::Fractal:
        str("CV");
        break;
    case TrackMode::PhaseFlux:
        str("CV");
        break;
    case TrackMode::TeletypeV2:
        str("CV");
        break;
    case TrackMode::TeletypeMini:
        str("CV");
        break;
    case TrackMode::Last:
        break;
    }
}

void Track::write(VersionedSerializedWriter &writer) const {
    writer.writeEnum(_trackMode, trackModeSerialize);
    int8_t linkReserved = 0; // reserved byte (was _linkTrack); keep for stream alignment
    writer.write(linkReserved);
    _runGate.write(writer);
    writer.write(_cvOutputRotate.base);
    writer.write(_gateOutputRotate.base);

    switch (_trackMode) {
    case TrackMode::Note:
        _track.note->write(writer);
        break;
    case TrackMode::Curve:
        _track.curve->write(writer);
        break;
    case TrackMode::MidiCv:
        _track.midiCv->write(writer);
        break;
    case TrackMode::Tuesday:
        _track.tuesday->write(writer);
        break;
    case TrackMode::DiscreteMap:
        _track.discreteMap->write(writer);
        break;
    case TrackMode::Indexed:
        _track.indexed->write(writer);
        break;
    case TrackMode::Stochastic:
        _track.stochastic->write(writer);
        break;
    case TrackMode::Fractal:
        _track.fractal->write(writer);
        break;
    case TrackMode::PhaseFlux:
        _track.phaseFlux->write(writer);
        break;
    case TrackMode::TeletypeV2:
        _track.tt2->write(writer);
        break;
    case TrackMode::TeletypeMini:
        tt2MiniTrack().write(writer);
        break;
    case TrackMode::Last:
        break;
    }
}

void Track::read(VersionedSerializedReader &reader) {
  reader.readEnum(_trackMode, trackModeSerialize);
  int8_t linkReserved; // reserved byte (was _linkTrack); consume to keep alignment
  reader.read(linkReserved);
  (void)linkReserved;

  _runGate.read(reader);

  _cvOutputRotate.read(reader);
  _gateOutputRotate.read(reader);

    initContainer();

    switch (_trackMode) {
    case TrackMode::Note:
        _track.note->read(reader);
        break;
    case TrackMode::Curve:
        _track.curve->read(reader);
        break;
    case TrackMode::MidiCv:
        _track.midiCv->read(reader);
        break;
    case TrackMode::Tuesday:
        _track.tuesday->read(reader);
        break;
    case TrackMode::DiscreteMap:
        _track.discreteMap->read(reader);
        break;
    case TrackMode::Indexed:
        _track.indexed->read(reader);
        break;
    case TrackMode::Stochastic:
        _track.stochastic->read(reader);
        break;
    case TrackMode::Fractal:
        _track.fractal->read(reader);
        break;
    case TrackMode::PhaseFlux:
        _track.phaseFlux->read(reader);
        break;
    case TrackMode::TeletypeV2:
        _track.tt2->read(reader);
        break;
    case TrackMode::TeletypeMini:
        tt2MiniTrack().read(reader);
        break;
    case TrackMode::Last:
        break;
    }
}
