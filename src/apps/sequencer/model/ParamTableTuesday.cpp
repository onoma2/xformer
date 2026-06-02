#include "ParamTableTuesday.h"
#include "RouteParamKey.h"

#include "Track.h"
#include "Config.h"

#include <cmath>

namespace {

inline TuesdayTrack &tuesdayTrack(const RouteParam::Scope &scope) {
    return static_cast<Track *>(scope.object)->tuesdayTrack();
}

inline int denormI(const RouteParam::Range &r, float n) {
    return int(std::round(n * (r.max - r.min) + r.min));
}

// All Tuesday routed params are sequence-level, routed slot, pattern fan-out.
#define TUESDAY_SEQ_HOOK(NAME, SETTER)                                                      \
    void NAME(const RouteParam::Scope &s, const RouteParam::Range &r, float n) {            \
        int v = denormI(r, n);                                                              \
        for (int p = 0; p < CONFIG_PATTERN_COUNT; ++p) tuesdayTrack(s).sequence(p).SETTER(v, true); \
    }

TUESDAY_SEQ_HOOK(applyAlgorithm, setAlgorithm)
TUESDAY_SEQ_HOOK(applyFlow, setFlow)
TUESDAY_SEQ_HOOK(applyOrnament, setOrnament)
TUESDAY_SEQ_HOOK(applyPower, setPower)
TUESDAY_SEQ_HOOK(applyGlide, setGlide)
TUESDAY_SEQ_HOOK(applyTrill, setTrill)
TUESDAY_SEQ_HOOK(applyStepTrill, setStepTrill)
TUESDAY_SEQ_HOOK(applyOctave, setOctave)
TUESDAY_SEQ_HOOK(applyTranspose, setTranspose)
TUESDAY_SEQ_HOOK(applyRotate, setRotate)
TUESDAY_SEQ_HOOK(applyDivisor, setDivisor)
TUESDAY_SEQ_HOOK(applyClockMult, setClockMultiplier)
TUESDAY_SEQ_HOOK(applyGateLength, setGateLength)
TUESDAY_SEQ_HOOK(applyGateOffset, setGateOffset)

#undef TUESDAY_SEQ_HOOK

// Scale/RootNote are plain ints (base write, no routed flag), like the other
// pitched tracks; the base-write defect closes at U6b (override table).
void applyScale(const RouteParam::Scope &s, const RouteParam::Range &r, float n) {
    int v = denormI(r, n);
    for (int p = 0; p < CONFIG_PATTERN_COUNT; ++p) tuesdayTrack(s).sequence(p).setScale(v);
}
void applyRootNote(const RouteParam::Scope &s, const RouteParam::Range &r, float n) {
    int v = denormI(r, n);
    for (int p = 0; p < CONFIG_PATTERN_COUNT; ++p) tuesdayTrack(s).sequence(p).setRootNote(v);
}

// Ranges mirror Routing::targetInfos so denormalization matches writeTarget.
constexpr RouteParam::Row kRows[] = {
    { ParamKey::Algorithm,       "Algorithm",  {   0.f,  14.f }, RouteParam::Discrete,   applyAlgorithm },
    { ParamKey::Flow,            "Flow",       {   0.f,  16.f }, RouteParam::Discrete,   applyFlow },
    { ParamKey::Ornament,        "Ornament",   {   0.f,  16.f }, RouteParam::Discrete,   applyOrnament },
    { ParamKey::Power,           "Power",      {   0.f,  16.f }, RouteParam::Discrete,   applyPower },
    { ParamKey::Glide,           "Glide",      {   0.f, 100.f }, RouteParam::Continuous, applyGlide },
    { ParamKey::Trill,           "Trill",      {   0.f, 100.f }, RouteParam::Continuous, applyTrill },
    { ParamKey::StepTrill,       "Step Trill", {   0.f, 100.f }, RouteParam::Continuous, applyStepTrill },
    { ParamKey::GateLength,      "Gate Length",{   0.f, 100.f }, RouteParam::Continuous, applyGateLength },
    { ParamKey::GateOffset,      "Gate Offset",{   0.f, 100.f }, RouteParam::Continuous, applyGateOffset },
    { ParamKey::Octave,          "Octave",     { -10.f,  10.f }, RouteParam::Continuous, applyOctave },
    { ParamKey::Transpose,       "Transpose",  { -60.f,  60.f }, RouteParam::Continuous, applyTranspose },
    { ParamKey::Rotate,          "Rotate",     { -64.f,  64.f }, RouteParam::Continuous, applyRotate },
    { ParamKey::Divisor,         "Divisor",    {   1.f, 768.f }, RouteParam::Discrete,   applyDivisor },
    { ParamKey::ClockMultiplier, "Clock Mult", {  50.f, 150.f }, RouteParam::Continuous, applyClockMult },
    { ParamKey::Scale,           "Scale",      {   0.f,  23.f }, RouteParam::Discrete,   applyScale },
    { ParamKey::RootNote,        "Root Note",  {   0.f,  11.f }, RouteParam::Discrete,   applyRootNote },
};

constexpr RouteParam::Table kTable{ RouteParam::Scope::Kind::Track, kRows, sizeof(kRows) / sizeof(kRows[0]) };

} // namespace

const RouteParam::Table &TuesdayParamTable::table() {
    return kTable;
}
