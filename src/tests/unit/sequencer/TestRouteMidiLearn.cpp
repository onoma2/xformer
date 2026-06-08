#include "UnitTest.h"

#include "apps/sequencer/engine/RouteMidiLearn.h"

// Maps a MidiLearn capture onto a route's MidiSource. The four auto-detected event
// types (knob/encoder/wheel/key) map to CC absolute/relative, pitch bend, note
// momentary; port and channel come straight off the capture.

static MidiLearn::Result resultFor(MidiLearn::Event event, MidiPort port, uint8_t channel, uint8_t numOrNote) {
    MidiLearn::Result r;
    r.port = port;
    r.channel = channel;
    r.event = event;
    r.controlNumber = numOrNote;     // union with note
    return r;
}

UNIT_TEST("RouteMidiLearn") {

CASE("ControlAbsolute -> CC absolute, port/channel/CC captured") {
    auto r = resultFor(MidiLearn::Event::ControlAbsolute, MidiPort::UsbMidi, 5, 74);
    Routing::MidiSource ms;
    ms.clear();
    assignMidiLearn(r, ms);
    expectEqual(int(ms.event()), int(Routing::MidiSource::Event::ControlAbsolute), "CC absolute");
    expectEqual(int(ms.source().port()), int(Types::MidiPort::UsbMidi), "USB port");
    expectEqual(ms.source().channel(), 5, "channel 5");
    expectEqual(ms.controlNumber(), 74, "CC 74");
}

CASE("ControlRelative -> CC relative, CC captured") {
    auto r = resultFor(MidiLearn::Event::ControlRelative, MidiPort::Midi, 0, 16);
    Routing::MidiSource ms;
    ms.clear();
    assignMidiLearn(r, ms);
    expectEqual(int(ms.event()), int(Routing::MidiSource::Event::ControlRelative), "CC relative");
    expectEqual(int(ms.source().port()), int(Types::MidiPort::Midi), "MIDI port");
    expectEqual(ms.controlNumber(), 16, "CC 16");
}

CASE("PitchBend -> pitch bend, no number field") {
    auto r = resultFor(MidiLearn::Event::PitchBend, MidiPort::Midi, 3, 0);
    Routing::MidiSource ms;
    ms.clear();
    assignMidiLearn(r, ms);
    expectEqual(int(ms.event()), int(Routing::MidiSource::Event::PitchBend), "pitch bend");
    expectEqual(ms.source().channel(), 3, "channel 3");
}

CASE("Note -> note momentary, note captured") {
    MidiLearn::Result r;
    r.port = MidiPort::Midi;
    r.channel = 9;
    r.event = MidiLearn::Event::Note;
    r.note = 60;
    Routing::MidiSource ms;
    ms.clear();
    assignMidiLearn(r, ms);
    expectEqual(int(ms.event()), int(Routing::MidiSource::Event::NoteMomentary), "note momentary");
    expectEqual(ms.note(), 60, "note 60");
}

} // UNIT_TEST
