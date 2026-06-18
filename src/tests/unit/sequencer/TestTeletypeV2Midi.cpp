#include "UnitTest.h"

#include "engine/TeletypeOutputState.h"
#include "engine/TT2Evaluator.h"

#include "model/Types.h"

extern "C" {
#include "command.h"
#include "tt_parser.h"
#include "ops/op_enum.h"
}

namespace {

int16_t getv(const char *text, TT2Runtime &runtime, TT2OutputState &output) {
    tele_command_t src = {};
    char errMsg[TELE_ERROR_MSG_LENGTH] = {};
    parse(text, &src, errMsg);
    TT2Command cmd = {};
    lowerCommand(src, cmd);
    return evaluateCommand(cmd, runtime, output).value;
}

void evalText(const char *text, TT2Runtime &runtime, TT2OutputState &output) {
    tele_command_t src = {};
    char errMsg[TELE_ERROR_MSG_LENGTH] = {};
    parse(text, &src, errMsg);
    TT2Command cmd = {};
    lowerCommand(src, cmd);
    evaluateCommand(cmd, runtime, output);
}

} // namespace

UNIT_TEST("TeletypeV2Midi") {

    CASE("buffer_note_on_append") {
        TT2Runtime runtime = {}; init(runtime);
        tt2MidiClear(runtime.midi);
        tt2MidiNoteOn(runtime.midi, 3, 60, 100);
        expectEqual(int(runtime.midi.on_count), 1, "on_count incremented");
        expectEqual(int(runtime.midi.note_on[0]), 60, "note stored");
        expectEqual(int(runtime.midi.note_vel[0]), 100, "vel stored");
        expectEqual(int(runtime.midi.on_channel[0]), 3, "channel stored");
        expectEqual(int(runtime.midi.last_note), 60, "last_note");
        expectEqual(int(runtime.midi.last_velocity), 100, "last_velocity");
        expectEqual(int(runtime.midi.last_channel), 3, "last_channel");
        expectEqual(int(runtime.midi.last_event_type), 1, "last_event_type on");
    }

    CASE("buffer_overflow_bounded") {
        TT2Runtime runtime = {}; init(runtime);
        tt2MidiClear(runtime.midi);
        for (int i = 0; i < 20; ++i) tt2MidiNoteOn(runtime.midi, 0, uint8_t(i), 64);
        expectEqual(int(runtime.midi.on_count), TT2_MIDI_EVENTS, "count capped");
    }

    CASE("buffer_off_and_cc") {
        TT2Runtime runtime = {}; init(runtime);
        tt2MidiClear(runtime.midi);
        tt2MidiNoteOff(runtime.midi, 1, 48);
        expectEqual(int(runtime.midi.off_count), 1, "off_count");
        expectEqual(int(runtime.midi.note_off[0]), 48, "off note");
        expectEqual(int(runtime.midi.last_event_type), 0, "last_event_type off");
        tt2MidiClear(runtime.midi);
        tt2MidiCc(runtime.midi, 2, 74, 99);
        expectEqual(int(runtime.midi.cc_count), 1, "cc_count");
        expectEqual(int(runtime.midi.cn[0]), 74, "cc number");
        expectEqual(int(runtime.midi.cc[0]), 99, "cc value");
        expectEqual(int(runtime.midi.last_controller), 74, "last_controller");
        expectEqual(int(runtime.midi.last_cc), 99, "last_cc");
        expectEqual(int(runtime.midi.last_event_type), 2, "last_event_type cc");
    }

    CASE("buffer_clear") {
        TT2Runtime runtime = {}; init(runtime);
        tt2MidiNoteOn(runtime.midi, 0, 60, 100);
        tt2MidiClear(runtime.midi);
        expectEqual(int(runtime.midi.on_count), 0, "on_count cleared");
        expectEqual(int(runtime.midi.last_note), 0, "last_note cleared");
    }

    // --- MI.* ops (U4) ---

    CASE("mi_last_event_reads") {
        TT2Runtime runtime = {}; init(runtime);
        TT2OutputState output = {}; init(output);
        tt2MidiClear(runtime.midi);
        tt2MidiNoteOn(runtime.midi, 4, 64, 120);  // ch 4 (0-based), note 64, vel 120
        expectEqual(int(getv("MI.LE", runtime, output)), 1, "MI.LE = on");
        expectEqual(int(getv("MI.LN", runtime, output)), 64, "MI.LN last note");
        expectEqual(int(getv("MI.LV", runtime, output)), 120, "MI.LV last velocity");
        expectEqual(int(getv("MI.LVV", runtime, output)), 120 * 129, "MI.LVV scaled");
        expectEqual(int(getv("MI.LCH", runtime, output)), 5, "MI.LCH channel +1");
    }

    CASE("mi_indexed_reads_via_I") {
        TT2Runtime runtime = {}; init(runtime);
        TT2OutputState output = {}; init(output);
        tt2MidiClear(runtime.midi);
        tt2MidiNoteOn(runtime.midi, 2, 50, 80);
        // MI.N/MI.V read the active frame's I (1-based); set it on frame 0.
        runtime.exec.frames[0].i = 1;
        expectEqual(int(getv("MI.NL", runtime, output)), 1, "MI.NL on_count");
        expectEqual(int(getv("MI.N", runtime, output)), 50, "MI.N note at I=1");
        expectEqual(int(getv("MI.V", runtime, output)), 80, "MI.V velocity at I=1");
        expectEqual(int(getv("MI.NCH", runtime, output)), 3, "MI.NCH channel +1");
        runtime.exec.frames[0].i = 5;  // out of range
        expectEqual(int(getv("MI.N", runtime, output)), 0, "MI.N out of range -> 0");
    }

    CASE("mi_cc_reads") {
        TT2Runtime runtime = {}; init(runtime);
        TT2OutputState output = {}; init(output);
        tt2MidiClear(runtime.midi);
        tt2MidiCc(runtime.midi, 0, 74, 99);
        runtime.exec.frames[0].i = 1;
        expectEqual(int(getv("MI.CL", runtime, output)), 1, "MI.CL cc_count");
        expectEqual(int(getv("MI.C", runtime, output)), 74, "MI.C controller at I=1");
        expectEqual(int(getv("MI.CC", runtime, output)), 99, "MI.CC value at I=1");
        expectEqual(int(getv("MI.LCC", runtime, output)), 99, "MI.LCC last cc");
    }

    CASE("mi_empty_buffer_zero") {
        TT2Runtime runtime = {}; init(runtime);
        TT2OutputState output = {}; init(output);
        tt2MidiClear(runtime.midi);
        expectEqual(int(getv("MI.NL", runtime, output)), 0, "empty on_count 0");
        expectEqual(int(getv("MI.LN", runtime, output)), 0, "empty last note 0");
        runtime.exec.frames[0].i = 1;
        expectEqual(int(getv("MI.N", runtime, output)), 0, "empty MI.N 0");
    }

    CASE("midi_config_defaults") {
        TT2Runtime runtime = {}; init(runtime);
        expectEqual(int(runtime.midi.on_script), -1, "on_script disabled by default");
        expectEqual(int(runtime.midi.off_script), -1, "off_script disabled");
        expectEqual(int(runtime.midi.cc_script), -1, "cc_script disabled");
        TeletypeProgram program; init(program);
        expectTrue(program.midiSource.port() == MidiSourceConfig().port(), "midiSource default port");
    }

    CASE("mi_dollar_script_binding") {
        TT2Runtime runtime = {}; init(runtime);
        TT2OutputState output = {}; init(output);
        evalText("MI.$ 1 3", runtime, output);   // note-on fires script 3
        expectEqual(int(getv("MI.$ 1", runtime, output)), 3, "MI.$ on-script round-trip");
        evalText("MI.$ 3 2", runtime, output);   // cc fires script 2
        expectEqual(int(getv("MI.$ 3", runtime, output)), 2, "MI.$ cc-script round-trip");
    }
}
