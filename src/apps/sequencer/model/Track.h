#pragma once

#include "Config.h"
#include "CurveTrack.h"
#include "DiscreteMapTrack.h"
#include "IndexedTrack.h"
#include "MidiCvTrack.h"
#include "ModelUtils.h"
#include "NoteTrack.h"
#include "Serialize.h"
#include "TeletypeTrack.h"
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
    Last,
    Default = Note
  };

  static const char *trackModeName(TrackMode trackMode) {
    switch (trackMode) {
    case TrackMode::Note:
      return "Note";
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
    case TrackMode::Last:
      break;
    }
    return nullptr;
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

  // linkTrack

  int linkTrack() const { return _linkTrack; }
  void setLinkTrack(int linkTrack) {
    _linkTrack = clamp(linkTrack, -1, _trackIndex - 1);
  }

  void editLinkTrack(int value, bool shift) {
    setLinkTrack(linkTrack() + value);
  }

  void printLinkTrack(StringBuilder &str) const {
    if (linkTrack() == -1) {
      str("None");
    } else {
      str("Track%d", linkTrack() + 1);
    }
  }

  // cvOutputRotate

  int cvOutputRotate() const { return _cvOutputRotate.get(Routing::isRouted(Routing::Target::CvOutputRotate, _trackIndex)); }
  void setCvOutputRotate(int rotate, bool routed = false) {
      _cvOutputRotate.set(clamp(rotate, -8, 8), routed);
  }
  void editCvOutputRotate(int value, bool shift) {
      if (!Routing::isRouted(Routing::Target::CvOutputRotate, _trackIndex)) {
          setCvOutputRotate(cvOutputRotate() + value);
      }
  }
  void printCvOutputRotate(StringBuilder &str) const {
      Routing::printRouted(str, Routing::Target::CvOutputRotate, _trackIndex);
      str("%+d", cvOutputRotate());
  }
  bool isCvOutputRotated() const {
      return _cvOutputRotate.base != 0 || Routing::isRouted(Routing::Target::CvOutputRotate, _trackIndex);
  }

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

  // gateOutputRotate

  int gateOutputRotate() const { return _gateOutputRotate.get(Routing::isRouted(Routing::Target::GateOutputRotate, _trackIndex)); }
  void setGateOutputRotate(int rotate, bool routed = false) {
      _gateOutputRotate.set(clamp(rotate, -8, 8), routed);
  }
  void editGateOutputRotate(int value, bool shift) {
      if (!Routing::isRouted(Routing::Target::GateOutputRotate, _trackIndex)) {
          setGateOutputRotate(gateOutputRotate() + value);
      }
  }
  void printGateOutputRotate(StringBuilder &str) const {
      Routing::printRouted(str, Routing::Target::GateOutputRotate, _trackIndex);
      str("%+d", gateOutputRotate());
  }
  bool isGateOutputRotated() const {
      return _gateOutputRotate.base != 0 || Routing::isRouted(Routing::Target::GateOutputRotate, _trackIndex);
  }

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
    _linkTrack = other._linkTrack;
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
    case TrackMode::Last:
      break;
    }
  }

  uint8_t _trackIndex = -1;
  TrackMode _trackMode;
  int8_t _linkTrack;
  Routable<uint8_t> _runGate;
  Routable<int8_t> _cvOutputRotate;
  Routable<int8_t> _gateOutputRotate;

  Container<NoteTrack, CurveTrack, MidiCvTrack, TuesdayTrack, DiscreteMapTrack, IndexedTrack, TeletypeTrack> _container;
  union {
    NoteTrack *note;
    CurveTrack *curve;
    MidiCvTrack *midiCv;
    TuesdayTrack *tuesday;
    DiscreteMapTrack *discreteMap;
    IndexedTrack *indexed;
    TeletypeTrack *teletype;
  } _track;

  friend class Project;
  friend class ClipBoard;
};

#undef SANITIZE_TRACK_MODE
