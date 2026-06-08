#pragma once

#include "MidiLearn.h"

#include "model/Routing.h"
#include "model/Types.h"

// Map a MidiLearn capture onto a route's MidiSource — the proven legacy mapping:
// knob -> CC absolute, endless encoder -> CC relative, wheel -> pitch bend,
// key -> note momentary. Port/channel come straight off the capture.
inline void assignMidiLearn(const MidiLearn::Result &result, Routing::MidiSource &midiSource) {
    midiSource.source().setPort(Types::MidiPort(result.port));
    midiSource.source().setChannel(result.channel);
    switch (result.event) {
    case MidiLearn::Event::ControlAbsolute:
        midiSource.setEvent(Routing::MidiSource::Event::ControlAbsolute);
        midiSource.setControlNumber(result.controlNumber);
        break;
    case MidiLearn::Event::ControlRelative:
        midiSource.setEvent(Routing::MidiSource::Event::ControlRelative);
        midiSource.setControlNumber(result.controlNumber);
        break;
    case MidiLearn::Event::PitchBend:
        midiSource.setEvent(Routing::MidiSource::Event::PitchBend);
        break;
    case MidiLearn::Event::Note:
        midiSource.setEvent(Routing::MidiSource::Event::NoteMomentary);
        midiSource.setNote(result.note);
        break;
    case MidiLearn::Event::Last:
        break;
    }
}
