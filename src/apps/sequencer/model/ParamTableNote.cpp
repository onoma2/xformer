#include "ParamTableNote.h"
#include "RouteParamKey.h"

#include "Track.h"
#include "Config.h"

#include <cmath>

namespace {

inline NoteTrack &noteTrack(const RouteParam::Scope &scope) {
    return static_cast<Track *>(scope.object)->noteTrack();
}

inline int denormI(const RouteParam::Range &r, float n) {
    return int(std::round(n * (r.max - r.min) + r.min));
}

// track-level: single field
void applySlideTime(const RouteParam::Scope &s, const RouteParam::Range &r, float n) { noteTrack(s).setSlideTime(denormI(r, n), true); }
void applyOctave(const RouteParam::Scope &s, const RouteParam::Range &r, float n)    { noteTrack(s).setOctave(denormI(r, n), true); }
void applyTranspose(const RouteParam::Scope &s, const RouteParam::Range &r, float n) { noteTrack(s).setTranspose(denormI(r, n), true); }
void applyRotate(const RouteParam::Scope &s, const RouteParam::Range &r, float n)    { noteTrack(s).setRotate(denormI(r, n), true); }
void applyGateBias(const RouteParam::Scope &s, const RouteParam::Range &r, float n)  { noteTrack(s).setGateProbabilityBias(denormI(r, n), true); }
void applyRetrigBias(const RouteParam::Scope &s, const RouteParam::Range &r, float n){ noteTrack(s).setRetriggerProbabilityBias(denormI(r, n), true); }
void applyLengthBias(const RouteParam::Scope &s, const RouteParam::Range &r, float n){ noteTrack(s).setLengthBias(denormI(r, n), true); }
void applyNoteBias(const RouteParam::Scope &s, const RouteParam::Range &r, float n)  { noteTrack(s).setNoteProbabilityBias(denormI(r, n), true); }

// sequence-level: fan out to every pattern (matches writeTarget)
void applyScale(const RouteParam::Scope &s, const RouteParam::Range &r, float n) {
    int v = denormI(r, n);
    for (int p = 0; p < CONFIG_PATTERN_COUNT; ++p) noteTrack(s).sequence(p).setScale(v, true);
}
void applyRootNote(const RouteParam::Scope &s, const RouteParam::Range &r, float n) {
    int v = denormI(r, n);
    for (int p = 0; p < CONFIG_PATTERN_COUNT; ++p) noteTrack(s).sequence(p).setRootNote(v, true);
}
void applyDivisor(const RouteParam::Scope &s, const RouteParam::Range &r, float n) {
    int v = denormI(r, n);
    for (int p = 0; p < CONFIG_PATTERN_COUNT; ++p) noteTrack(s).sequence(p).setDivisor(v, true);
}
void applyClockMult(const RouteParam::Scope &s, const RouteParam::Range &r, float n) {
    int v = denormI(r, n);
    for (int p = 0; p < CONFIG_PATTERN_COUNT; ++p) noteTrack(s).sequence(p).setClockMultiplier(v, true);
}
void applyRunMode(const RouteParam::Scope &s, const RouteParam::Range &r, float n) {
    int v = denormI(r, n);
    for (int p = 0; p < CONFIG_PATTERN_COUNT; ++p) noteTrack(s).sequence(p).setRunMode(Types::RunMode(v), true);
}
void applyFirstStep(const RouteParam::Scope &s, const RouteParam::Range &r, float n) {
    int v = denormI(r, n);
    for (int p = 0; p < CONFIG_PATTERN_COUNT; ++p) noteTrack(s).sequence(p).setFirstStep(v, true);
}
void applyLastStep(const RouteParam::Scope &s, const RouteParam::Range &r, float n) {
    int v = denormI(r, n);
    for (int p = 0; p < CONFIG_PATTERN_COUNT; ++p) noteTrack(s).sequence(p).setLastStep(v, true);
}

// Ranges mirror Routing::targetInfos so denormalization matches writeTarget.
constexpr RouteParam::Row kRows[] = {
    { ParamKey::SlideTime, "Slide Time",  {   0.f,  100.f }, RouteParam::Continuous, applySlideTime },
    { ParamKey::Octave,    "Octave",      { -10.f,   10.f }, RouteParam::Continuous, applyOctave },
    { ParamKey::Transpose, "Transpose",   { -60.f,   60.f }, RouteParam::Continuous, applyTranspose },
    { ParamKey::Rotate,    "Rotate",      { -64.f,   64.f }, RouteParam::Continuous, applyRotate },
    { ParamKey::GateProbabilityBias,      "Gate P. Bias",   { -8.f, 8.f }, RouteParam::Continuous, applyGateBias },
    { ParamKey::RetriggerProbabilityBias, "Retrig P. Bias", { -8.f, 8.f }, RouteParam::Continuous, applyRetrigBias },
    { ParamKey::LengthBias,               "Length Bias",    { -8.f, 8.f }, RouteParam::Continuous, applyLengthBias },
    { ParamKey::NoteProbabilityBias,      "Note P. Bias",   { -8.f, 8.f }, RouteParam::Continuous, applyNoteBias },
    { ParamKey::Scale,     "Scale",       {   0.f,   23.f }, RouteParam::Discrete,   applyScale },
    { ParamKey::RootNote,  "Root Note",   {   0.f,   11.f }, RouteParam::Discrete,   applyRootNote },
    { ParamKey::Divisor,   "Divisor",     {   1.f,  768.f }, RouteParam::Discrete,   applyDivisor },
    { ParamKey::ClockMultiplier, "Clock Mult",  {  50.f,  150.f }, RouteParam::Continuous, applyClockMult },
    { ParamKey::RunMode,   "Run Mode",    {   0.f,    5.f }, RouteParam::Discrete,   applyRunMode },
    { ParamKey::FirstStep, "First Step",  {   0.f,   63.f }, RouteParam::Discrete,   applyFirstStep },
    { ParamKey::LastStep,  "Last Step",   {   0.f,   63.f }, RouteParam::Discrete,   applyLastStep },
};

constexpr RouteParam::Table kTable{ RouteParam::Scope::Kind::Track, kRows, sizeof(kRows) / sizeof(kRows[0]) };

} // namespace

const RouteParam::Table &NoteParamTable::table() {
    return kTable;
}
