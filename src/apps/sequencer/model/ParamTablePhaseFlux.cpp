#include "ParamTablePhaseFlux.h"
#include "RouteParamKey.h"

#include "Track.h"
#include "Config.h"

#include <cmath>

namespace {

inline PhaseFluxTrack &pfTrack(const RouteParam::Scope &scope) {
    return static_cast<Track *>(scope.object)->phaseFluxTrack();
}

inline float denormF(const RouteParam::Range &r, float n) {
    return n * (r.max - r.min) + r.min;
}

inline int denormI(const RouteParam::Range &r, float n) {
    return int(std::round(denormF(r, n)));
}

// track-level, routed slot (PhaseFluxTrack::writeRouted)
void applyOctave(const RouteParam::Scope &s, const RouteParam::Range &r, float n)    { pfTrack(s).setOctave(denormI(r, n), true); }
void applyTranspose(const RouteParam::Scope &s, const RouteParam::Range &r, float n) { pfTrack(s).setTranspose(denormI(r, n), true); }
void applySlideTime(const RouteParam::Scope &s, const RouteParam::Range &r, float n) { pfTrack(s).setSlideTime(denormI(r, n), true); }

// sequence-level routed, pattern fan-out (PhaseFluxSequence::writeRouted)
void applyDivisor(const RouteParam::Scope &s, const RouteParam::Range &r, float n) {
    int v = denormI(r, n);
    for (int p = 0; p < CONFIG_PATTERN_COUNT; ++p) pfTrack(s).sequence(p).setDivisor(v, true);
}
void applyClockMult(const RouteParam::Scope &s, const RouteParam::Range &r, float n) {
    int v = denormI(r, n);
    for (int p = 0; p < CONFIG_PATTERN_COUNT; ++p) pfTrack(s).sequence(p).setClockMultiplier(v, true);
}
// sequence-level BASE write (defect-parity; closes at U6b)
void applyScale(const RouteParam::Scope &s, const RouteParam::Range &r, float n) {
    int v = denormI(r, n);
    for (int p = 0; p < CONFIG_PATTERN_COUNT; ++p) pfTrack(s).sequence(p).setScale(v);
}
void applyRootNote(const RouteParam::Scope &s, const RouteParam::Range &r, float n) {
    int v = denormI(r, n);
    for (int p = 0; p < CONFIG_PATTERN_COUNT; ++p) pfTrack(s).sequence(p).setRootNote(v);
}

// LAUNCH additions: new routables, sequence-level fan-out, plain (ungated) fields.
void applyPhase(const RouteParam::Scope &s, const RouteParam::Range &r, float n) {
    float v = denormF(r, n);
    for (int p = 0; p < CONFIG_PATTERN_COUNT; ++p) pfTrack(s).sequence(p).setGlobalPhase(v);
}
#define PF_NUDGE_HOOK(NAME, SETTER)                                                        \
    void NAME(const RouteParam::Scope &s, const RouteParam::Range &r, float n) {           \
        int v = denormI(r, n);                                                             \
        for (int p = 0; p < CONFIG_PATTERN_COUNT; ++p) pfTrack(s).sequence(p).SETTER(v);   \
    }
PF_NUDGE_HOOK(applyWarpNudge, setWarpNudge)
PF_NUDGE_HOOK(applyResponseNudge, setResponseNudge)
PF_NUDGE_HOOK(applyLenNudge, setLenNudge)
PF_NUDGE_HOOK(applyCyclePhaseWarp, setCyclePhaseWarp)
PF_NUDGE_HOOK(applyPulseNudge, setPulseNudge)
#undef PF_NUDGE_HOOK

// Ranges mirror Routing::targetInfos (parity rows) / the field clamp (LAUNCH rows).
constexpr RouteParam::Row kRows[] = {
    // parity: track-level
    { ParamKey::Octave,          "Octave",     { -10.f,  10.f }, RouteParam::Continuous, applyOctave },
    { ParamKey::Transpose,       "Transpose",  { -60.f,  60.f }, RouteParam::Continuous, applyTranspose },
    { ParamKey::SlideTime,       "Slide Time", {   0.f, 100.f }, RouteParam::Continuous, applySlideTime },
    // parity: sequence-level routed
    { ParamKey::Divisor,         "Divisor",    {   1.f, 768.f }, RouteParam::Discrete,   applyDivisor },
    { ParamKey::ClockMultiplier, "Clock Mult", {  50.f, 150.f }, RouteParam::Continuous, applyClockMult },
    // parity: sequence-level base write
    { ParamKey::Scale,           "Scale",      {   0.f,  23.f }, RouteParam::Discrete,   applyScale },
    { ParamKey::RootNote,        "Root Note",  {   0.f,  11.f }, RouteParam::Discrete,   applyRootNote },
    // LAUNCH: new routables
    { ParamKey::Phase,           "Phase",      {   0.f,   1.f }, RouteParam::Continuous, applyPhase },
    { ParamKey::WarpNudge,       "Warp Nudge", { -64.f,  64.f }, RouteParam::Continuous, applyWarpNudge },
    { ParamKey::ResponseNudge,   "Resp Nudge", { -64.f,  64.f }, RouteParam::Continuous, applyResponseNudge },
    { ParamKey::LenNudge,        "Len Nudge",  { -64.f,  64.f }, RouteParam::Continuous, applyLenNudge },
    { ParamKey::CyclePhaseWarp,  "Cyc Warp",   { -64.f,  64.f }, RouteParam::Continuous, applyCyclePhaseWarp },
    { ParamKey::PulseNudge,      "Pulse Nudge",{ -15.f,  15.f }, RouteParam::Continuous, applyPulseNudge },
};

constexpr RouteParam::Table kTable{ RouteParam::Scope::Kind::Track, kRows, sizeof(kRows) / sizeof(kRows[0]) };

} // namespace

const RouteParam::Table &PhaseFluxParamTable::table() {
    return kTable;
}
