#include "UnitTest.h"
#include "core/midi/MidiMessage.h"

UNIT_TEST("MidiMessage") {
    CASE("makePitchBend signed round-trips through pitchBend()") {
        expectEqual(MidiMessage::makePitchBend(0, 0).pitchBend(), 0, "center");
        expectEqual(MidiMessage::makePitchBend(0, 4096).pitchBend(), 4096, "up");
        expectEqual(MidiMessage::makePitchBend(0, -4096).pitchBend(), -4096, "down");
        expectEqual(MidiMessage::makePitchBend(0, 99999).pitchBend(), 8191, "clamp hi");
        expectEqual(MidiMessage::makePitchBend(0, -99999).pitchBend(), -8192, "clamp lo");
    }
}
