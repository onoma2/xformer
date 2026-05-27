#include "UnitTest.h"

#include "engine/TT2TrackEngine.h"

// Pre-include model/Types.h in C++ mode so that when teletype.h pulls in
// state.h -> types.h inside extern "C", the C++ templates are already
// processed and skipped by #pragma once.
#include "model/Types.h"

extern "C" {
#include "command.h"
#include "teletype.h"
#include "ops/op_enum.h"
}

namespace {

struct ParseResult {
    tele_error_t error;
    char errorMsg[TELE_ERROR_MSG_LENGTH];
    tele_command_t cmd;
};

ParseResult tryParse(const char *text) {
    ParseResult result;
    result.cmd = {};
    result.errorMsg[0] = '\0';
    result.error = parse(text, &result.cmd, result.errorMsg);
    return result;
}

TT2Command lower(const tele_command_t &src) {
    TT2Command dst = {};
    lowerCommand(src, dst);
    return dst;
}

void writeLine(TT2Script &script, uint8_t lineIndex, const char *text) {
    auto r = tryParse(text);
    expectEqual(int(r.error), int(E_OK), "parse line");
    script.commands[lineIndex] = lower(r.cmd);
}

} // namespace

UNIT_TEST("TeletypeV2TrackEngineSmoke") {

    CASE("cv_1_5000_reaches_performer_output") {
        TT2Track track;
        track.clear();

        writeLine(track.program().scripts[0], 0, "CV 1 5000");
        track.program().scripts[0].length = 1;

        TT2TrackEngine engine;
        engine.setTrack(&track);

        auto result = engine.runScript(0);
        expectEqual(int(result.error), int(TT2EvalError::None), "script ok");

        // TT2OutputState should reflect the write.
        expectEqual(engine.output().cv[0].targetRaw, int16_t(5000), "targetRaw");
        expectEqual(int(engine.output().cvDirty), 1, "cvDirty bit 0");

        // Performer cvOutput() should convert raw 5000 to volts.
        float expected = TT2TrackEngine::rawToVolts(5000);
        expectEqual(engine.cvOutput(0), expected, "cv voltage");
    }

    CASE("tr_1_1_reaches_performer_output") {
        TT2Track track;
        track.clear();

        writeLine(track.program().scripts[0], 0, "TR 1 1");
        track.program().scripts[0].length = 1;

        TT2TrackEngine engine;
        engine.setTrack(&track);

        auto result = engine.runScript(0);
        expectEqual(int(result.error), int(TT2EvalError::None), "script ok");

        expectEqual(int(engine.output().tr[0].level), 1, "tr level");
        expectEqual(int(engine.output().trDirty), 1, "trDirty bit 0");
        expectTrue(engine.gateOutput(0), "gate high");
    }

    CASE("tr_1_0_reaches_performer_output") {
        TT2Track track;
        track.clear();

        writeLine(track.program().scripts[0], 0, "TR 1 0");
        track.program().scripts[0].length = 1;

        TT2TrackEngine engine;
        engine.setTrack(&track);

        auto result = engine.runScript(0);
        expectEqual(int(result.error), int(TT2EvalError::None), "script ok");

        expectEqual(int(engine.output().tr[0].level), 0, "tr level");
        expectEqual(int(engine.output().trDirty), 1, "trDirty bit 0");
        expectFalse(engine.gateOutput(0), "gate low");
    }

    CASE("multi_cv_script_reaches_performer_output") {
        TT2Track track;
        track.clear();

        writeLine(track.program().scripts[0], 0, "CV 1 1000");
        writeLine(track.program().scripts[0], 1, "CV 2 2000");
        track.program().scripts[0].length = 2;

        TT2TrackEngine engine;
        engine.setTrack(&track);

        auto result = engine.runScript(0);
        expectEqual(int(result.error), int(TT2EvalError::None), "script ok");

        expectEqual(engine.cvOutput(0), TT2TrackEngine::rawToVolts(1000), "cv0");
        expectEqual(engine.cvOutput(1), TT2TrackEngine::rawToVolts(2000), "cv1");
        expectEqual(int(engine.output().cvDirty), 0x03, "both dirty");
    }

    CASE("engine_without_track_is_noop") {
        TT2TrackEngine engine;
        // No track set.
        auto result = engine.runScript(0);
        expectEqual(int(result.error), int(TT2EvalError::NoTrack), "noop reports NoTrack");
        // Unset CV defaults to 0V, not -5V.
        expectEqual(engine.cvOutput(0), 0.f, "default cv is 0V");
        expectFalse(engine.gateOutput(0), "default gate low");
    }

    CASE("cv_explicit_zero_is_negative_five") {
        // CV 1 0 writes raw 0, which is -5V. This must be distinguishable
        // from an unwritten output (which returns 0V).
        TT2Track track;
        track.clear();

        writeLine(track.program().scripts[0], 0, "CV 1 0");
        track.program().scripts[0].length = 1;

        TT2TrackEngine engine;
        engine.setTrack(&track);

        auto result = engine.runScript(0);
        expectEqual(int(result.error), int(TT2EvalError::None), "script ok");
        expectEqual(engine.cvOutput(0), -5.f, "CV 1 0 = -5V");
    }

    CASE("cv_voltage_bounds") {
        // Verify conversion at extremes.
        expectEqual(TT2TrackEngine::rawToVolts(0), -5.f, "0 raw = -5V");
        expectEqual(TT2TrackEngine::rawToVolts(16383), 5.f, "16383 raw = +5V");
    }
}
