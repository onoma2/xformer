#pragma once

#include "Config.h"
#include "Types.h"
#include "Serialize.h"
#include "ModelUtils.h"
#include "NoteTrack.h"
#include "CurveTrack.h"
#include "MidiCvTrack.h"
#include "TuesdayTrack.h"

#include "core/Debug.h"
#include "core/math/Math.h"
#include "core/utils/StringUtils.h"
#include "core/utils/Container.h"

#include <cstdint>
#include <cstring>

#if CONFIG_ENABLE_SANITIZE
# define SANITIZE_TRACK_MODE(_actual_, _expected_) ASSERT(_actual_ == _expected_, "invalid track mode");
#else // CONFIG_ENABLE_SANITIZE
# define SANITIZE_TRACK_MODE(_actual_, _expected_) {}
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
        Last,
        Default = Note
    };

    static const char *trackModeName(TrackMode trackMode) {
        switch (trackMode) {
        case TrackMode::Note:    return "Note";
        case TrackMode::Curve:   return "Curve";
        case TrackMode::MidiCv:  return "MIDI/CV";
        case TrackMode::Tuesday: return "Tuesday";
        case TrackMode::Last:    break;
        }
        return nullptr;
    }

    static uint8_t trackModeSerialize(TrackMode trackMode) {
        switch (trackMode) {
        case TrackMode::Note:    return 0;
        case TrackMode::Curve:   return 1;
        case TrackMode::MidiCv:  return 2;
        case TrackMode::Tuesday: return 3;
        case TrackMode::Last:    break;
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

    // noteTrack

    const NoteTrack &noteTrack() const { SANITIZE_TRACK_MODE(_trackMode, TrackMode::Note); return _container.as<NoteTrack>(); }
          NoteTrack &noteTrack()       { SANITIZE_TRACK_MODE(_trackMode, TrackMode::Note); return _container.as<NoteTrack>(); }

    // curveTrack

    const CurveTrack &curveTrack() const { SANITIZE_TRACK_MODE(_trackMode, TrackMode::Curve); return _container.as<CurveTrack>(); }
          CurveTrack &curveTrack()       { SANITIZE_TRACK_MODE(_trackMode, TrackMode::Curve); return _container.as<CurveTrack>(); }

    // midiCvTrack

    const MidiCvTrack &midiCvTrack() const { SANITIZE_TRACK_MODE(_trackMode, TrackMode::MidiCv); return _container.as<MidiCvTrack>(); }
          MidiCvTrack &midiCvTrack()       { SANITIZE_TRACK_MODE(_trackMode, TrackMode::MidiCv); return _container.as<MidiCvTrack>(); }

    // tuesdayTrack

    const TuesdayTrack &tuesdayTrack() const { SANITIZE_TRACK_MODE(_trackMode, TrackMode::Tuesday); return _container.as<TuesdayTrack>(); }
          TuesdayTrack &tuesdayTrack()       { SANITIZE_TRACK_MODE(_trackMode, TrackMode::Tuesday); return _container.as<TuesdayTrack>(); }

    //----------------------------------------
    // Methods
    //----------------------------------------

    Track() :
        _trackMode(TrackMode::Default),
        _trackIndex(-1)
    {
        initContainer();
    }

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
        case TrackMode::MidiCv:
            _container.as<MidiCvTrack>().setTrackIndex(trackIndex);
            break;
        case TrackMode::Tuesday:
            _container.as<TuesdayTrack>().setTrackIndex(trackIndex);
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
        case TrackMode::Last:
            break;
        }
    }

    uint8_t _trackIndex = -1;
    TrackMode _trackMode;
    int8_t _linkTrack;

    Container<NoteTrack, CurveTrack, MidiCvTrack, TuesdayTrack> _container;
    union {
        NoteTrack *note;
        CurveTrack *curve;
        MidiCvTrack *midiCv;
        TuesdayTrack *tuesday;
    } _track;

    friend class Project;
    friend class ClipBoard;
};

#undef SANITIZE_TRACK_MODE