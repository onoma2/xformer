#include "ParamTableIndexed.h"
#include "RouteParamKey.h"

#include "Track.h"
#include "Config.h"

#include <cmath>

namespace {

inline IndexedTrack &indexedTrack(const RouteParam::Scope &scope) {
    return static_cast<Track *>(scope.object)->indexedTrack();
}

inline float denormF(const RouteParam::Range &r, float n) {
    return n * (r.max - r.min) + r.min;
}

inline int denormI(const RouteParam::Range &r, float n) {
    return int(std::round(denormF(r, n)));
}

// track-level direct: routed slot
void applyOctave(const RouteParam::Scope &s, const RouteParam::Range &r, float n)    { indexedTrack(s).setOctave(denormI(r, n), true); }
void applyTranspose(const RouteParam::Scope &s, const RouteParam::Range &r, float n) { indexedTrack(s).setTranspose(denormI(r, n), true); }
void applySlideTime(const RouteParam::Scope &s, const RouteParam::Range &r, float n) { indexedTrack(s).setSlideTime(denormI(r, n), true); }

// sequence-level direct: fan out to every pattern (matches writeTarget).
// Divisor/Scale/RootNote write the BASE (no routed flag) -- mirror writeRouted.
void applyDivisor(const RouteParam::Scope &s, const RouteParam::Range &r, float n) {
    int v = denormI(r, n);
    for (int p = 0; p < CONFIG_PATTERN_COUNT; ++p) indexedTrack(s).sequence(p).setDivisor(v);
}
void applyClockMult(const RouteParam::Scope &s, const RouteParam::Range &r, float n) {
    int v = denormI(r, n);
    for (int p = 0; p < CONFIG_PATTERN_COUNT; ++p) indexedTrack(s).sequence(p).setClockMultiplier(v, true);
}
void applyScale(const RouteParam::Scope &s, const RouteParam::Range &r, float n) {
    int v = denormI(r, n);
    for (int p = 0; p < CONFIG_PATTERN_COUNT; ++p) indexedTrack(s).sequence(p).setScale(v);
}
void applyRootNote(const RouteParam::Scope &s, const RouteParam::Range &r, float n) {
    int v = denormI(r, n);
    for (int p = 0; p < CONFIG_PATTERN_COUNT; ++p) indexedTrack(s).sequence(p).setRootNote(v);
}
void applyFirstStep(const RouteParam::Scope &s, const RouteParam::Range &r, float n) {
    int v = denormI(r, n);
    for (int p = 0; p < CONFIG_PATTERN_COUNT; ++p) indexedTrack(s).sequence(p).setFirstStep(v, true);
}
void applyRunMode(const RouteParam::Scope &s, const RouteParam::Range &r, float n) {
    int v = denormI(r, n);
    for (int p = 0; p < CONFIG_PATTERN_COUNT; ++p) indexedTrack(s).sequence(p).setRunMode(Types::RunMode(v), true);
}

// inlets: fill the per-track routed-CV field, -100..100 normalized to -1..1, fan-out
void applyIndexedA(const RouteParam::Scope &s, const RouteParam::Range &r, float n) {
    float v = denormF(r, n) * 0.01f;
    for (int p = 0; p < CONFIG_PATTERN_COUNT; ++p) indexedTrack(s).sequence(p).setRoutedIndexedA(v);
}
void applyIndexedB(const RouteParam::Scope &s, const RouteParam::Range &r, float n) {
    float v = denormF(r, n) * 0.01f;
    for (int p = 0; p < CONFIG_PATTERN_COUNT; ++p) indexedTrack(s).sequence(p).setRoutedIndexedB(v);
}

// Ranges mirror Routing::targetInfos so denormalization matches writeTarget.
constexpr RouteParam::Row kRows[] = {
    // track-level direct
    { ParamKey::Octave,          "Octave",     { -10.f,  10.f }, RouteParam::Continuous, applyOctave },
    { ParamKey::Transpose,       "Transpose",  { -60.f,  60.f }, RouteParam::Continuous, applyTranspose },
    { ParamKey::SlideTime,       "Slide Time", {   0.f, 100.f }, RouteParam::Continuous, applySlideTime },
    // sequence-level direct (fan-out)
    { ParamKey::Divisor,         "Divisor",    {   1.f, 768.f }, RouteParam::Discrete,   applyDivisor },
    { ParamKey::ClockMultiplier, "Clock Mult", {  50.f, 150.f }, RouteParam::Continuous, applyClockMult },
    { ParamKey::Scale,           "Scale",      {   0.f,  23.f }, RouteParam::Discrete,   applyScale },
    { ParamKey::RootNote,        "Root Note",  {   0.f,  11.f }, RouteParam::Discrete,   applyRootNote },
    { ParamKey::FirstStep,       "First Step", {   0.f,  63.f }, RouteParam::Discrete,   applyFirstStep },
    { ParamKey::RunMode,         "Run Mode",   {   0.f,   5.f }, RouteParam::Discrete,   applyRunMode },
    // inlets
    { ParamKey::IndexedA,        "Inlet A",    { -100.f, 100.f }, RouteParam::Inlet, applyIndexedA },
    { ParamKey::IndexedB,        "Inlet B",    { -100.f, 100.f }, RouteParam::Inlet, applyIndexedB },
};

constexpr RouteParam::Table kTable{ RouteParam::Scope::Kind::Track, kRows, sizeof(kRows) / sizeof(kRows[0]) };

} // namespace

const RouteParam::Table &IndexedParamTable::table() {
    return kTable;
}
