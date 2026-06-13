#pragma once

#include "Config.h"
#include "CurveTrack.h"
#include "DiscreteMapTrack.h"
#include "IndexedTrack.h"
#include "MidiCvTrack.h"
#include "ModelUtils.h"
#include "NoteTrack.h"
#include "PhaseFluxTrack.h"
#include "Serialize.h"
#include "StochasticTrack.h"
#include "TeletypeTrack.h"
#include "TT2Track.h"
#include "TuesdayTrack.h"
#include "Types.h"

#include "core/Debug.h"
#include "core/math/Math.h"
#include "core/utils/Container.h"
#include "core/utils/StringUtils.h"

#include <cstdint>
#include <cstring>

#if CONFIG_ENABLE_SANITIZE
#define SANITIZE_TRACK_MODE(_actual_, _expected_)                              \
  ASSERT(_actual_ == _expected_, "invalid track mode");
#else // CONFIG_ENABLE_SANITIZE
#define SANITIZE_TRACK_MODE(_actual_, _expected_)                              \
  {                                                                            \
  }
#endif // CONFIG_ENABLE_SANITIZE

class Project;

class Track {
public:
  //----------------------------------------
  // Types
  //----------------------------------------

  enum class TrackMode : uint8_t {
    Note,
    Curve,
    MidiCv,
    Tuesday,
    DiscreteMap,
    Indexed,
    Teletype,
    Stochastic,
    PhaseFlux,
    TeletypeV2,
    Last,
    Default = Note
  };

  static const char *trackModeName(TrackMode trackMode) {
    switch (trackMode) {
    case TrackMode::Note:
      return "Note Circus";
    case TrackMode::Curve:
      return "Curve Studio";
    case TrackMode::MidiCv:
      return "MIDI/CV";
    case TrackMode::Tuesday:
      return "Algo(Tuesday)";
    case TrackMode::DiscreteMap:
      return "Discrete";
    case TrackMode::Indexed:
      return "Indexed";
    case TrackMode::Teletype:
      return "T9type";
    case TrackMode::Stochastic:
      return "Stochastic";
    case TrackMode::PhaseFlux:
      return "PhaseFlux";
    case TrackMode::TeletypeV2:
      return "T++ (v2)";
    case TrackMode::Last:
      break;
    }
    return nullptr;
  }

  // Single-letter engine abbreviation (matrix column headers, SPREAD labels).
  static char trackModeLetter(TrackMode trackMode) {
    switch (trackMode) {
    case TrackMode::Note:        return 'N';
    case TrackMode::Curve:       return 'C';
    case TrackMode::MidiCv:      return 'M';
    case TrackMode::Tuesday:     return 'A';
    case TrackMode::DiscreteMap: return 'D';
    case TrackMode::Indexed:     return 'I';
    case TrackMode::Teletype:    return 'T';
    case TrackMode::Stochastic:  return 'S';
    case TrackMode::PhaseFlux:   return 'P';
    case TrackMode::TeletypeV2:  return '2';
    case TrackMode::Last:        break;
    }
    return '?';
  }

  static uint8_t trackModeSerialize(TrackMode trackMode) {
    switch (trackMode) {
    case TrackMode::Note:
      return 0;
    case TrackMode::Curve:
      return 1;
    case TrackMode::MidiCv:
      return 2;
    case TrackMode::Tuesday:
      return 3;
    case TrackMode::DiscreteMap:
      return 4;
    case TrackMode::Indexed:
      return 5;
    case TrackMode::Teletype:
      return 6;
    case TrackMode::Stochastic:
      return 7;
    case TrackMode::PhaseFlux:
      return 8;
    case TrackMode::TeletypeV2:
      return 9;
    case TrackMode::Last:
      break;
    }
    return 0;
  }

  //----------------------------------------
  // Properties
  //----------------------------------------

  // trackIndex

  int trackIndex() const { return _trackIndex; }

  // trackMode

  TrackMode trackMode() const { return _trackMode; }

  void printTrackMode(StringBuilder &str) const {
    str(trackModeName(trackMode()));
  }

  // _cvOutputRotate is vestigial (base always 0, never user-set): CV output rotation is a
  // route-level group rotation in RoutingEngine now (spec 019). Field kept for format compat.

  // runGate

  bool runGate() const {
      // Logic: If not routed, return base (default 1/true).
      // If routed, return value > 0 (High = Run, Low = Stop).
      return _runGate.get(Routing::isRouted(Routing::Target::Run, _trackIndex)) > 0;
  }

  void setRunGate(bool run, bool routed = false) {
      _runGate.set(run ? 1 : 0, routed);
  }

  void printRunGate(StringBuilder &str) const {
       Routing::printRouted(str, Routing::Target::Run, _trackIndex);
       str(runGate() ? "On" : "Off");
  }

  // _gateOutputRotate is vestigial (base always 0, never user-set): gate output rotation is a
  // route-level group rotation in RoutingEngine now (spec 018). Field kept for format compat.

  // noteTrack

  const NoteTrack &noteTrack() const {
    SANITIZE_TRACK_MODE(_trackMode, TrackMode::Note);
    return _container.as<NoteTrack>();
  }
  NoteTrack &noteTrack() {
    SANITIZE_TRACK_MODE(_trackMode, TrackMode::Note);
    return _container.as<NoteTrack>();
  }

  // curveTrack

  const CurveTrack &curveTrack() const {
    SANITIZE_TRACK_MODE(_trackMode, TrackMode::Curve);
    return _container.as<CurveTrack>();
  }
  CurveTrack &curveTrack() {
    SANITIZE_TRACK_MODE(_trackMode, TrackMode::Curve);
    return _container.as<CurveTrack>();
  }

  // midiCvTrack

  const MidiCvTrack &midiCvTrack() const {
    SANITIZE_TRACK_MODE(_trackMode, TrackMode::MidiCv);
    return _container.as<MidiCvTrack>();
  }
  MidiCvTrack &midiCvTrack() {
    SANITIZE_TRACK_MODE(_trackMode, TrackMode::MidiCv);
    return _container.as<MidiCvTrack>();
  }

  // tuesdayTrack

  const TuesdayTrack &tuesdayTrack() const {
    SANITIZE_TRACK_MODE(_trackMode, TrackMode::Tuesday);
    return _container.as<TuesdayTrack>();
  }
  TuesdayTrack &tuesdayTrack() {
    SANITIZE_TRACK_MODE(_trackMode, TrackMode::Tuesday);
    return _container.as<TuesdayTrack>();
  }

  // discreteMapTrack

  const DiscreteMapTrack &discreteMapTrack() const {
    SANITIZE_TRACK_MODE(_trackMode, TrackMode::DiscreteMap);
    return _container.as<DiscreteMapTrack>();
  }
  DiscreteMapTrack &discreteMapTrack() {
    SANITIZE_TRACK_MODE(_trackMode, TrackMode::DiscreteMap);
    return _container.as<DiscreteMapTrack>();
  }

  // indexedTrack

  const IndexedTrack &indexedTrack() const {
    SANITIZE_TRACK_MODE(_trackMode, TrackMode::Indexed);
    return _container.as<IndexedTrack>();
  }
  IndexedTrack &indexedTrack() {
    SANITIZE_TRACK_MODE(_trackMode, TrackMode::Indexed);
    return _container.as<IndexedTrack>();
  }

  // teletypeTrack

  const TeletypeTrack &teletypeTrack() const {
    SANITIZE_TRACK_MODE(_trackMode, TrackMode::Teletype);
    return _container.as<TeletypeTrack>();
  }
  TeletypeTrack &teletypeTrack() {
    SANITIZE_TRACK_MODE(_trackMode, TrackMode::Teletype);
    return _container.as<TeletypeTrack>();
  }

  // stochasticTrack

  const StochasticTrack &stochasticTrack() const {
    SANITIZE_TRACK_MODE(_trackMode, TrackMode::Stochastic);
    return _container.as<StochasticTrack>();
  }
  StochasticTrack &stochasticTrack() {
    SANITIZE_TRACK_MODE(_trackMode, TrackMode::Stochastic);
    return _container.as<StochasticTrack>();
  }

  // phaseFluxTrack

  const PhaseFluxTrack &phaseFluxTrack() const {
    SANITIZE_TRACK_MODE(_trackMode, TrackMode::PhaseFlux);
    return _container.as<PhaseFluxTrack>();
  }
  PhaseFluxTrack &phaseFluxTrack() {
    SANITIZE_TRACK_MODE(_trackMode, TrackMode::PhaseFlux);
    return _container.as<PhaseFluxTrack>();
  }

  // tt2Track

  const TT2Track &tt2Track() const {
    SANITIZE_TRACK_MODE(_trackMode, TrackMode::TeletypeV2);
    return _container.as<TT2Track>();
  }
  TT2Track &tt2Track() {
    SANITIZE_TRACK_MODE(_trackMode, TrackMode::TeletypeV2);
    return _container.as<TT2Track>();
  }

  //----------------------------------------
  // Methods
  //----------------------------------------

  Track() : _trackIndex(-1), _trackMode(TrackMode::Default) { initContainer(); }

  void clear();
  void clearPattern(int patternIndex);
  void copyPattern(int src, int dst);
  bool duplicatePattern(int patternIndex);

  void gateOutputName(int index, StringBuilder &str) const;
  void cvOutputName(int index, StringBuilder &str) const;

  void write(VersionedSerializedWriter &writer) const;
  void read(VersionedSerializedReader &reader);

  Track &operator=(const Track &other) {
    ASSERT(_trackMode == other._trackMode, "invalid track mode");
    _container = other._container;
    setTrackIndex(other._trackIndex); // Call the private setTrackIndex
    return *this;
  }

private:
  void setTrackIndex(int trackIndex) {
    _trackIndex = trackIndex; // Set _trackIndex first
    // Move logic from setContainerTrackIndex here
    switch (_trackMode) {
    case TrackMode::Note:
      _container.as<NoteTrack>().setTrackIndex(trackIndex);
      break;
    case TrackMode::Curve:
      _container.as<CurveTrack>().setTrackIndex(trackIndex);
      break;
    case TrackMode::DiscreteMap:
      _container.as<DiscreteMapTrack>().setTrackIndex(trackIndex);
      break;
    case TrackMode::MidiCv:
      _container.as<MidiCvTrack>().setTrackIndex(trackIndex);
      break;
    case TrackMode::Tuesday:
      _container.as<TuesdayTrack>().setTrackIndex(trackIndex);
      break;
    case TrackMode::Indexed:
      _container.as<IndexedTrack>().setTrackIndex(trackIndex);
      break;
    case TrackMode::Teletype:
      _container.as<TeletypeTrack>().setTrackIndex(trackIndex);
      break;
    case TrackMode::Stochastic:
      _container.as<StochasticTrack>().setTrackIndex(trackIndex);
      break;
    case TrackMode::PhaseFlux:
      _container.as<PhaseFluxTrack>().setTrackIndex(trackIndex);
      break;
    case TrackMode::TeletypeV2:
      _container.as<TT2Track>().setTrackIndex(trackIndex);
      break;
    case TrackMode::Last:
      break;
    }
  }

  void setTrackMode(TrackMode trackMode) {
    trackMode = ModelUtils::clampedEnum(trackMode);
    if (trackMode != _trackMode) {
      _trackMode = trackMode;
      initContainer();
    }
  }
  void initContainer() {
    switch (_trackMode) {
    case TrackMode::Note:
      _track.note = _container.create<NoteTrack>();
      _track.note->setTrackIndex(_trackIndex); // Set track index here
      break;
    case TrackMode::Curve:
      _track.curve = _container.create<CurveTrack>();
      _track.curve->setTrackIndex(_trackIndex); // Set track index here
      break;
    case TrackMode::MidiCv:
      _track.midiCv = _container.create<MidiCvTrack>();
      _track.midiCv->setTrackIndex(_trackIndex); // Set track index here
      break;
    case TrackMode::Tuesday:
      _track.tuesday = _container.create<TuesdayTrack>();
      _track.tuesday->setTrackIndex(_trackIndex); // Set track index here
      break;
    case TrackMode::DiscreteMap:
      _track.discreteMap = _container.create<DiscreteMapTrack>();
      _track.discreteMap->setTrackIndex(_trackIndex); // Set track index here
      break;
    case TrackMode::Indexed:
      _track.indexed = _container.create<IndexedTrack>();
      _track.indexed->setTrackIndex(_trackIndex); // Set track index here
      break;
    case TrackMode::Teletype:
      _track.teletype = _container.create<TeletypeTrack>();
      _track.teletype->setTrackIndex(_trackIndex); // Set track index here
      break;
    case TrackMode::Stochastic:
      _track.stochastic = _container.create<StochasticTrack>();
      _track.stochastic->setTrackIndex(_trackIndex); // Set track index here
      break;
    case TrackMode::PhaseFlux:
      _track.phaseFlux = _container.create<PhaseFluxTrack>();
      _track.phaseFlux->setTrackIndex(_trackIndex);
      break;
    case TrackMode::TeletypeV2:
      _track.tt2 = _container.create<TT2Track>();
      _track.tt2->setTrackIndex(_trackIndex);
      break;
    case TrackMode::Last:
      break;
    }
  }

  uint8_t _trackIndex = -1;
  TrackMode _trackMode;
  Routable<uint8_t> _runGate;
  Routable<int8_t> _cvOutputRotate;
  Routable<int8_t> _gateOutputRotate;

  Container<NoteTrack, CurveTrack, MidiCvTrack, TuesdayTrack, DiscreteMapTrack, IndexedTrack, TeletypeTrack, StochasticTrack, PhaseFluxTrack, TT2Track> _container;
  union {
    NoteTrack *note;
    CurveTrack *curve;
    MidiCvTrack *midiCv;
    TuesdayTrack *tuesday;
    DiscreteMapTrack *discreteMap;
    IndexedTrack *indexed;
    TeletypeTrack *teletype;
    StochasticTrack *stochastic;
    PhaseFluxTrack *phaseFlux;
    TT2Track *tt2;
  } _track;

  friend class Project;
  friend class ClipBoard;
};

#undef SANITIZE_TRACK_MODE
