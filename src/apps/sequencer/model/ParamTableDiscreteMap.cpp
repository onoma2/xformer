#include "ParamTableDiscreteMap.h"
#include "RouteParamKey.h"

#include "Track.h"
#include "Config.h"

#include <cmath>

namespace {

inline DiscreteMapTrack &dmTrack(const RouteParam::Scope &scope) {
    return static_cast<Track *>(scope.object)->discreteMapTrack();
}

inline float denormF(const RouteParam::Range &r, float n) {
    return n * (r.max - r.min) + r.min;
}

inline int denormI(const RouteParam::Range &r, float n) {
    return int(std::round(denormF(r, n)));
}

// inlets: track-level routed-CV fields, raw denormalized float (no scaling, no fan-out)
void applyInput(const RouteParam::Scope &s, const RouteParam::Range &r, float n)   { dmTrack(s).setRoutedInput(denormF(r, n)); }
void applyScanner(const RouteParam::Scope &s, const RouteParam::Range &r, float n) { dmTrack(s).setRoutedScanner(denormF(r, n)); }
void applySync(const RouteParam::Scope &s, const RouteParam::Range &r, float n)    { dmTrack(s).setRoutedSync(denormF(r, n)); }

// Two fan-out scopes, matching writeTarget exactly:
//  - track-routed params (Octave/Transpose/Offset/SlideTime/RangeHigh/RangeLow)
//    reach DiscreteMapTrack::writeRouted, which fans to ALL sequences() -- patterns
//    AND snapshot slots (CONFIG_PATTERN_COUNT + CONFIG_SNAPSHOT_COUNT).
//  - sequence-targets (Divisor/ClockMult/Scale/RootNote) reach the writeTarget
//    pattern loop, so they touch only CONFIG_PATTERN_COUNT patterns.
// Octave/Transpose/Offset/Divisor/Scale/RootNote/RangeHigh/RangeLow write the BASE
// (no routed flag); SlideTime(slewTime) and ClockMult write the routed slot.
void applyOctave(const RouteParam::Scope &s, const RouteParam::Range &r, float n) {
    int v = denormI(r, n);
    for (auto &seq : dmTrack(s).sequences()) seq.setOctave(v);
}
void applyTranspose(const RouteParam::Scope &s, const RouteParam::Range &r, float n) {
    int v = denormI(r, n);
    for (auto &seq : dmTrack(s).sequences()) seq.setTranspose(v);
}
void applyOffset(const RouteParam::Scope &s, const RouteParam::Range &r, float n) {
    int v = denormI(r, n);
    for (auto &seq : dmTrack(s).sequences()) seq.setOffset(v);
}
void applySlideTime(const RouteParam::Scope &s, const RouteParam::Range &r, float n) {
    int v = denormI(r, n);
    for (auto &seq : dmTrack(s).sequences()) seq.setSlewTime(v, true);
}
void applyRangeHigh(const RouteParam::Scope &s, const RouteParam::Range &r, float n) {
    float v = denormF(r, n);
    for (auto &seq : dmTrack(s).sequences()) seq.setRangeHigh(v);
}
void applyRangeLow(const RouteParam::Scope &s, const RouteParam::Range &r, float n) {
    float v = denormF(r, n);
    for (auto &seq : dmTrack(s).sequences()) seq.setRangeLow(v);
}
void applyDivisor(const RouteParam::Scope &s, const RouteParam::Range &r, float n) {
    int v = denormI(r, n);
    for (int p = 0; p < CONFIG_PATTERN_COUNT; ++p) dmTrack(s).sequence(p).setDivisor(v);
}
void applyClockMult(const RouteParam::Scope &s, const RouteParam::Range &r, float n) {
    int v = denormI(r, n);
    for (int p = 0; p < CONFIG_PATTERN_COUNT; ++p) dmTrack(s).sequence(p).setClockMultiplier(v, true);
}
void applyScale(const RouteParam::Scope &s, const RouteParam::Range &r, float n) {
    int v = denormI(r, n);
    for (int p = 0; p < CONFIG_PATTERN_COUNT; ++p) dmTrack(s).sequence(p).setScale(v);
}
void applyRootNote(const RouteParam::Scope &s, const RouteParam::Range &r, float n) {
    int v = denormI(r, n);
    for (int p = 0; p < CONFIG_PATTERN_COUNT; ++p) dmTrack(s).sequence(p).setRootNote(v);
}

// Ranges mirror Routing::targetInfos so denormalization matches writeTarget.
constexpr RouteParam::Row kRows[] = {
    // sequence-level direct (fan-out)
    { ParamKey::Octave,          "Octave",     { -10.f,  10.f }, RouteParam::Continuous, applyOctave },
    { ParamKey::Transpose,       "Transpose",  { -60.f,  60.f }, RouteParam::Continuous, applyTranspose },
    { ParamKey::Offset,          "Offset",     { -500.f, 500.f }, RouteParam::Continuous, applyOffset },
    { ParamKey::SlideTime,       "Slide Time", {   0.f, 100.f }, RouteParam::Continuous, applySlideTime },
    { ParamKey::Divisor,         "Divisor",    {   1.f, 768.f }, RouteParam::Discrete,   applyDivisor },
    { ParamKey::ClockMultiplier, "Clock Mult", {  50.f, 150.f }, RouteParam::Continuous, applyClockMult },
    { ParamKey::Scale,           "Scale",      {   0.f,  23.f }, RouteParam::Discrete,   applyScale },
    { ParamKey::RootNote,        "Root Note",  {   0.f,  11.f }, RouteParam::Discrete,   applyRootNote },
    { ParamKey::DiscreteMapRangeHigh, "Range Hi", { -5.f, 5.f }, RouteParam::Continuous, applyRangeHigh },
    { ParamKey::DiscreteMapRangeLow,  "Range Lo", { -5.f, 5.f }, RouteParam::Continuous, applyRangeLow },
    // inlets (track-level)
    { ParamKey::DiscreteMapInput,   "Inlet In",  { -5.f,  5.f }, RouteParam::Inlet, applyInput },
    { ParamKey::DiscreteMapScanner, "Inlet Scan",{  0.f, 34.f }, RouteParam::Inlet, applyScanner },
    { ParamKey::DiscreteMapSync,    "Inlet Sync",{  0.f,  1.f }, RouteParam::Inlet, applySync },
};

constexpr RouteParam::Table kTable{ RouteParam::Scope::Kind::Track, kRows, sizeof(kRows) / sizeof(kRows[0]) };

} // namespace

const RouteParam::Table &DiscreteMapParamTable::table() {
    return kTable;
}
