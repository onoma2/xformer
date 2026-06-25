#include "UnitTest.h"
#include "model/TT2Config.h"
#include "model/TeletypeProgram.h"
#include "model/TeletypeRuntime.h"
#include "engine/TeletypeOutputState.h"
#include "engine/TT2Evaluator.h"

extern "C" {
#include "command.h"
#include "tt_parser.h"
#include "ops/op_enum.h"
}

namespace {

template<typename Cfg>
int16_t getvT(const char *text, TT2RuntimeT<Cfg> &runtime, TT2OutputState &output) {
    tele_command_t src = {};
    char errMsg[TELE_ERROR_MSG_LENGTH] = {};
    parse(text, &src, errMsg);
    TT2Command cmd = {};
    lowerCommand(src, cmd);
    return evaluateCommand(cmd, runtime, output).value;
}

template<typename Cfg>
void evalTextT(const char *text, TT2RuntimeT<Cfg> &runtime, TT2OutputState &output) {
    tele_command_t src = {};
    char errMsg[TELE_ERROR_MSG_LENGTH] = {};
    parse(text, &src, errMsg);
    TT2Command cmd = {};
    lowerCommand(src, cmd);
    evaluateCommand(cmd, runtime, output);
}

} // namespace

UNIT_TEST("TeletypeMini") {
CASE("Mini struct sizes fit budget") {
    expect(sizeof(TeletypeProgramT<TT2ConfigMini>) < 1700, "program");
    expect(sizeof(TT2RuntimeT<TT2ConfigMini>) < 2600, "runtime");
}
CASE("Mini op table covers every op Full covers") {
    auto *full = tt2OpTable<TT2ConfigFull>();
    auto *mini = tt2OpTable<TT2ConfigMini>();
    for (int i = 0; i < E_OP__LENGTH; ++i)
        if (full[i]) expect(mini[i] != nullptr, "coverage");
}
CASE("LAST 0 on Mini returns 0 with no init script") {
    TT2RuntimeT<TT2ConfigMini> runtime = {}; init(runtime);
    TT2OutputState output = {};
    expectEqual(int(getvT("LAST 0", runtime, output)), 0, "LAST 0 -> 0 (no OOB)");
}
CASE("MI.$ 1 1 on Mini binds script 0") {
    TT2RuntimeT<TT2ConfigMini> runtime = {}; init(runtime);
    TT2OutputState output = {};
    evalTextT("MI.$ 1 1", runtime, output);
    expectEqual(int(runtime.midi.on_script), 0, "on_script == 1-1");
}
}
