#pragma once

#include "MidiConfig.h"
#include "TT2Config.h"
#include "Types.h"

#include <cassert>
#include <cstdint>
#include <cstring>

extern "C" {
#include "command.h"
}

// Varying dimensions live on the config; these compat aliases keep the ~30
// other files + tests that reference the bare names compiling unchanged.
static constexpr int TT2_SCRIPT_COUNT        = TT2ConfigFull::ScriptCount;   // 8 numbered + Metro + Init
static constexpr int TT2_COMMANDS_PER_SCRIPT = 6;
static constexpr int TT2_COMMAND_MAX_LENGTH  = 16;
static constexpr int TT2_PATTERN_COUNT       = TT2ConfigFull::PatternCount;
static constexpr int TT2_PATTERN_LENGTH      = TT2ConfigFull::PatternLength;

static constexpr int TT2_METRO_SCRIPT     = TT2ConfigFull::MetroScript;
static constexpr int TT2_INIT_SCRIPT      = TT2ConfigFull::InitScript;

enum class TT2TimeBase : uint8_t {
    Ms,
    Clock,
};

static_assert(TT2_COMMAND_MAX_LENGTH == 16, "v2 command max must be 16");
static_assert(COMMAND_MAX_LENGTH == TT2_COMMAND_MAX_LENGTH, "v2 and parser command max must match");
static_assert(TT2_COMMANDS_PER_SCRIPT == 6, "v2 expects 6 commands per script");
static_assert(TT2_PATTERN_COUNT == 4, "v2 expects 4 patterns");
static_assert(TT2_PATTERN_LENGTH == 64, "v2 expects 64 pattern entries");
static_assert(TT2_INIT_SCRIPT < TT2_SCRIPT_COUNT, "init script index must be valid");

struct TT2Command {
    uint8_t length;
    uint8_t commented;   // line disabled in place: runner skips it, editor toggles it
    uint8_t tag[TT2_COMMAND_MAX_LENGTH];
    int16_t value[TT2_COMMAND_MAX_LENGTH];
};

template<typename Cfg>
struct TT2PatternT {
    int16_t idx;
    uint16_t len;
    uint16_t wrap;
    int16_t start;
    int16_t end;
    int16_t val[Cfg::PatternLength];
};

struct TT2Script {
    uint8_t length;
    TT2Command commands[TT2_COMMANDS_PER_SCRIPT];
};

static constexpr int TT2_TRIGGER_INPUT_COUNT = TT2ConfigFull::TriggerInputCount;   // 8 trigger inputs -> scripts 1-8

// Per trigger-input source. Mirrors TeletypeTrack::TriggerInputSource so the
// (deferred) editor I/O grid edits the same shape. Engine reads it to decide
// which Performer input drives each trigger script. Defaults to CvIn1-4.
enum class TT2TriggerSource : uint8_t {
    None,
    CvIn1, CvIn2, CvIn3, CvIn4,
    GateOut1, GateOut2, GateOut3, GateOut4,
    GateOut5, GateOut6, GateOut7, GateOut8,
    LogicalGate1, LogicalGate2, LogicalGate3, LogicalGate4,
    LogicalGate5, LogicalGate6, LogicalGate7, LogicalGate8,
    Last
};

// Six analog/engine inputs feed runtime values: IN and PARAM (scaled, read via
// the IN/PARAM ops) plus the working variables X/Y/Z/T. Each maps to a
// configurable CV source mirroring TeletypeTrack::CvInputSource. UI deferred to
// the editor I/O grid; defaults wire IN/PARAM/X/Y to CvIn1-4.
enum class TT2CvInput : uint8_t { In, Param, X, Y, Z, T };
static constexpr int TT2_CV_INPUT_COUNT = 6;
static constexpr int TT2_CV_OUTPUT_COUNT = 8;  // per-output shaping (range/offset/quantize/root)

enum class TT2CvInputSource : uint8_t {
    None,
    CvIn1, CvIn2, CvIn3, CvIn4,
    CvOut1, CvOut2, CvOut3, CvOut4,
    CvOut5, CvOut6, CvOut7, CvOut8,
    CvRoute1, CvRoute2, CvRoute3, CvRoute4,
    LogicalCv1, LogicalCv2, LogicalCv3, LogicalCv4,
    LogicalCv5, LogicalCv6, LogicalCv7, LogicalCv8,
    Last
};

template<typename Cfg>
struct TeletypeProgramT {
    uint8_t dialectVersion;
    uint8_t bootScriptIndex;
    uint8_t bootEnabled;
    uint8_t timeBase;
    int16_t clockDivisor;
    int16_t clockMultiplier;
    uint8_t resetMetroOnLoad;
    TT2Script scripts[Cfg::ScriptCount];
    TT2PatternT<Cfg> patterns[Cfg::PatternCount];
    TT2TriggerSource triggerSource[Cfg::TriggerInputCount];
    TT2CvInputSource cvInputSource[TT2_CV_INPUT_COUNT];
    // Per-CV-output shaping (mirrors TT1 TeletypeTrack); applied in TT2TrackEngine::cvOutput.
    Types::VoltageRange cvOutputRange[TT2_CV_OUTPUT_COUNT];
    int16_t cvOutputOffset[TT2_CV_OUTPUT_COUNT];       // ×0.01 V, -500..500
    int8_t cvOutputQuantizeScale[TT2_CV_OUTPUT_COUNT]; // -1 = off, else scale index
    int8_t cvOutputRootNote[TT2_CV_OUTPUT_COUNT];      // -1 = project root
    MidiSourceConfig midiSource;  // per-track MIDI filter (port + channel); UI deferred
};

using TT2Pattern = TT2PatternT<TT2ConfigFull>;
using TeletypeProgram = TeletypeProgramT<TT2ConfigFull>;

template<typename Cfg>
inline void init(TeletypeProgramT<Cfg> &p) {
    memset(&p, 0, sizeof(TeletypeProgramT<Cfg>));
    p.dialectVersion = 2;
    p.bootScriptIndex = Cfg::InitScript >= 0 ? Cfg::InitScript : 0;
    p.bootEnabled = 0;
    p.timeBase = uint8_t(TT2TimeBase::Ms);
    p.clockDivisor = 12;
    p.clockMultiplier = 100;
    p.resetMetroOnLoad = 1;
    for (int i = 0; i < Cfg::PatternCount; i++) {
        p.patterns[i].len = 0;                       // match current Teletype default
        p.patterns[i].wrap = 1;
        p.patterns[i].start = 0;
        p.patterns[i].end = Cfg::PatternLength - 1;
    }
    // Default each trigger input to its matching CV input (CvIn1-4).
    for (int i = 0; i < Cfg::TriggerInputCount; i++) {
        p.triggerSource[i] = TT2TriggerSource(int(TT2TriggerSource::CvIn1) + i);
    }
    // Default IN/PARAM/X/Y to CvIn1-4; Z/T unmapped (memset leaves them None).
    p.cvInputSource[int(TT2CvInput::In)]    = TT2CvInputSource::CvIn1;
    p.cvInputSource[int(TT2CvInput::Param)] = TT2CvInputSource::CvIn2;
    p.cvInputSource[int(TT2CvInput::X)]     = TT2CvInputSource::CvIn3;
    p.cvInputSource[int(TT2CvInput::Y)]     = TT2CvInputSource::CvIn4;
    // Per-output shaping defaults: ±5V range, no quantize, 0 offset/root.
    for (int i = 0; i < TT2_CV_OUTPUT_COUNT; i++) {
        p.cvOutputRange[i] = Types::VoltageRange::Bipolar5V;
        p.cvOutputQuantizeScale[i] = -1;
    }
    p.midiSource = MidiSourceConfig();  // proper default (memset cleared it above)
}

inline TT2Command *scriptCommand(TT2Script &s, uint8_t index) {
    return (index < TT2_COMMANDS_PER_SCRIPT) ? &s.commands[index] : nullptr;
}

// Lower a parsed tele_command_t into the native v2 TT2Command storage.
// Returns false only if the source length exceeds TT2_COMMAND_MAX_LENGTH.
// separator and comment are intentionally dropped.
inline bool lowerCommand(const tele_command_t &src, TT2Command &dst) {
    if (src.length > TT2_COMMAND_MAX_LENGTH) {
        return false;
    }
    dst.length = src.length;
    dst.commented = src.comment ? 1 : 0;
    for (uint8_t i = 0; i < src.length; ++i) {
        dst.tag[i] = src.tag[i];
        dst.value[i] = src.value[i];
    }
    // Zero trailing slots for determinism.
    for (uint8_t i = src.length; i < TT2_COMMAND_MAX_LENGTH; ++i) {
        dst.tag[i] = 0;
        dst.value[i] = 0;
    }
    return true;
}

inline int16_t *patternVal(TT2Pattern &pat, uint16_t index) {
    return (index < TT2_PATTERN_LENGTH) ? &pat.val[index] : nullptr;
}

// sizeof guards are <= bounds verified on ARM STM32 release builds.
static_assert(sizeof(TT2Command) <= 52, "TT2Command size drift");
static_assert(sizeof(TT2Pattern) <= 140, "TT2Pattern size drift");
static_assert(sizeof(TT2Script) <= 304, "TT2Script size drift");
static_assert(sizeof(TeletypeProgram) <= 3640, "TeletypeProgram size drift");
