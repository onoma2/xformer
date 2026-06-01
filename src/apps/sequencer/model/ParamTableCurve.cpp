#include "ParamTableCurve.h"
#include "RouteParamKey.h"

#include "Track.h"
#include "Config.h"

#include <cmath>
#include <cstdint>

namespace {

inline CurveTrack &curveTrack(const RouteParam::Scope &scope) {
    return static_cast<Track *>(scope.object)->curveTrack();
}

inline float denormF(const RouteParam::Range &r, float n) {
    return n * (r.max - r.min) + r.min;
}

inline int denormI(const RouteParam::Range &r, float n) {
    return int(std::round(denormF(r, n)));
}

// track-level: single field
void applySlideTime(const RouteParam::Scope &s, const RouteParam::Range &r, float n) { curveTrack(s).setSlideTime(denormI(r, n), true); }
void applyOffset(const RouteParam::Scope &s, const RouteParam::Range &r, float n)    { curveTrack(s).setOffset(denormI(r, n), true); }
void applyRotate(const RouteParam::Scope &s, const RouteParam::Range &r, float n)    { curveTrack(s).setRotate(denormI(r, n), true); }
void applyShapeBias(const RouteParam::Scope &s, const RouteParam::Range &r, float n) { curveTrack(s).setShapeProbabilityBias(denormI(r, n), true); }
void applyGateBias(const RouteParam::Scope &s, const RouteParam::Range &r, float n)  { curveTrack(s).setGateProbabilityBias(denormI(r, n), true); }
// CurveRate: writeTarget feeds 0..400 then scales to 0..4.
void applyCurveRate(const RouteParam::Scope &s, const RouteParam::Range &r, float n) { curveTrack(s).setCurveRate(denormF(r, n) / 100.f, true); }
// Phase: launch addition (no writeTarget equivalent); plain field, clamp 0..1.
void applyPhase(const RouteParam::Scope &s, const RouteParam::Range &r, float n)     { curveTrack(s).setGlobalPhase(denormF(r, n)); }

// sequence-level: fan out to every pattern (matches writeTarget)
void applyDivisor(const RouteParam::Scope &s, const RouteParam::Range &r, float n) {
    int v = denormI(r, n);
    for (int p = 0; p < CONFIG_PATTERN_COUNT; ++p) curveTrack(s).sequence(p).setDivisor(v, true);
}
void applyClockMult(const RouteParam::Scope &s, const RouteParam::Range &r, float n) {
    int v = denormI(r, n);
    for (int p = 0; p < CONFIG_PATTERN_COUNT; ++p) curveTrack(s).sequence(p).setClockMultiplier(v, true);
}
void applyRunMode(const RouteParam::Scope &s, const RouteParam::Range &r, float n) {
    int v = denormI(r, n);
    for (int p = 0; p < CONFIG_PATTERN_COUNT; ++p) curveTrack(s).sequence(p).setRunMode(Types::RunMode(v), true);
}
void applyFirstStep(const RouteParam::Scope &s, const RouteParam::Range &r, float n) {
    int v = denormI(r, n);
    for (int p = 0; p < CONFIG_PATTERN_COUNT; ++p) curveTrack(s).sequence(p).setFirstStep(v, true);
}
void applyLastStep(const RouteParam::Scope &s, const RouteParam::Range &r, float n) {
    int v = denormI(r, n);
    for (int p = 0; p < CONFIG_PATTERN_COUNT; ++p) curveTrack(s).sequence(p).setLastStep(v, true);
}
// Wavefolder/DjFilter store centi-units; writeTarget truncates the float (not round).
void applyWavefolderFold(const RouteParam::Scope &s, const RouteParam::Range &r, float n) {
    int16_t v = int16_t(denormF(r, n));
    for (int p = 0; p < CONFIG_PATTERN_COUNT; ++p) curveTrack(s).sequence(p).setWavefolderFold(v, true);
}
void applyWavefolderGain(const RouteParam::Scope &s, const RouteParam::Range &r, float n) {
    int16_t v = int16_t(denormF(r, n));
    for (int p = 0; p < CONFIG_PATTERN_COUNT; ++p) curveTrack(s).sequence(p).setWavefolderGain(v, true);
}
void applyDjFilter(const RouteParam::Scope &s, const RouteParam::Range &r, float n) {
    int16_t v = int16_t(denormF(r, n));
    for (int p = 0; p < CONFIG_PATTERN_COUNT; ++p) curveTrack(s).sequence(p).setDjFilter(v, true);
}
void applyChaosAmount(const RouteParam::Scope &s, const RouteParam::Range &r, float n) {
    int v = denormI(r, n);
    for (int p = 0; p < CONFIG_PATTERN_COUNT; ++p) curveTrack(s).sequence(p).setChaosAmount(v, true);
}
void applyChaosRate(const RouteParam::Scope &s, const RouteParam::Range &r, float n) {
    int v = denormI(r, n);
    for (int p = 0; p < CONFIG_PATTERN_COUNT; ++p) curveTrack(s).sequence(p).setChaosRate(v, true);
}
void applyChaosParam1(const RouteParam::Scope &s, const RouteParam::Range &r, float n) {
    int v = denormI(r, n);
    for (int p = 0; p < CONFIG_PATTERN_COUNT; ++p) curveTrack(s).sequence(p).setChaosParam1(v, true);
}
void applyChaosParam2(const RouteParam::Scope &s, const RouteParam::Range &r, float n) {
    int v = denormI(r, n);
    for (int p = 0; p < CONFIG_PATTERN_COUNT; ++p) curveTrack(s).sequence(p).setChaosParam2(v, true);
}

// Ranges mirror Routing::targetInfos so denormalization matches writeTarget.
constexpr RouteParam::Row kRows[] = {
    // track-level
    { ParamKey::SlideTime,           "Slide Time",   {    0.f,  100.f }, RouteParam::Continuous, applySlideTime },
    { ParamKey::Offset,              "Offset",       { -500.f,  500.f }, RouteParam::Continuous, applyOffset },
    { ParamKey::Rotate,              "Rotate",       {  -64.f,   64.f }, RouteParam::Continuous, applyRotate },
    { ParamKey::ShapeProbabilityBias,"Shape P. Bias",{   -8.f,    8.f }, RouteParam::Continuous, applyShapeBias },
    { ParamKey::GateProbabilityBias, "Gate P. Bias", {   -8.f,    8.f }, RouteParam::Continuous, applyGateBias },
    { ParamKey::CurveRate,           "Curve Rate",   {    0.f,  400.f }, RouteParam::Continuous, applyCurveRate },
    { ParamKey::Phase,               "Phase",        {    0.f,    1.f }, RouteParam::Continuous, applyPhase },
    // sequence-level (fan-out)
    { ParamKey::Divisor,             "Divisor",      {    1.f,  768.f }, RouteParam::Discrete,   applyDivisor },
    { ParamKey::ClockMultiplier,     "Clock Mult",   {   50.f,  150.f }, RouteParam::Continuous, applyClockMult },
    { ParamKey::RunMode,             "Run Mode",     {    0.f,    5.f }, RouteParam::Discrete,   applyRunMode },
    { ParamKey::FirstStep,           "First Step",   {    0.f,   63.f }, RouteParam::Discrete,   applyFirstStep },
    { ParamKey::LastStep,            "Last Step",    {    0.f,   63.f }, RouteParam::Discrete,   applyLastStep },
    { ParamKey::WavefolderFold,      "Fold",         {    0.f,  100.f }, RouteParam::Continuous, applyWavefolderFold },
    { ParamKey::WavefolderGain,      "Gain",         {    0.f,  200.f }, RouteParam::Continuous, applyWavefolderGain },
    { ParamKey::DjFilter,            "DJ Filter",    { -100.f,  100.f }, RouteParam::Continuous, applyDjFilter },
    { ParamKey::ChaosAmount,         "Chaos Amt",    {    0.f,  100.f }, RouteParam::Continuous, applyChaosAmount },
    { ParamKey::ChaosRate,           "Chaos Rate",   {    0.f,  127.f }, RouteParam::Continuous, applyChaosRate },
    { ParamKey::ChaosParam1,         "Chaos P1",     {    0.f,  100.f }, RouteParam::Continuous, applyChaosParam1 },
    { ParamKey::ChaosParam2,         "Chaos P2",     {    0.f,  100.f }, RouteParam::Continuous, applyChaosParam2 },
};

constexpr RouteParam::Table kTable{ RouteParam::Scope::Kind::Track, kRows, sizeof(kRows) / sizeof(kRows[0]) };

} // namespace

const RouteParam::Table &CurveParamTable::table() {
    return kTable;
}
