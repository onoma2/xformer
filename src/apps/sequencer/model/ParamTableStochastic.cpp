#include "ParamTableStochastic.h"
#include "RouteParamKey.h"

#include "Track.h"
#include "Config.h"

#include <cmath>

namespace {

inline StochasticTrack &stochTrack(const RouteParam::Scope &scope) {
    return static_cast<Track *>(scope.object)->stochasticTrack();
}

inline int denormI(const RouteParam::Range &r, float n) {
    return int(std::round(n * (r.max - r.min) + r.min));
}

// track-level, routed slot (StochasticTrack::writeRouted)
void applyOctave(const RouteParam::Scope &s, const RouteParam::Range &r, float n)    { stochTrack(s).setOctave(denormI(r, n), true); }
void applyTranspose(const RouteParam::Scope &s, const RouteParam::Range &r, float n) { stochTrack(s).setTranspose(denormI(r, n), true); }
void applySlideTime(const RouteParam::Scope &s, const RouteParam::Range &r, float n) { stochTrack(s).setSlideTime(denormI(r, n), true); }

// sequence-level, routed slot, pattern fan-out (StochasticSequence::writeRouted)
#define STOCH_SEQ_ROUTED(NAME, SETTER)                                                     \
    void NAME(const RouteParam::Scope &s, const RouteParam::Range &r, float n) {           \
        int v = denormI(r, n);                                                             \
        for (int p = 0; p < CONFIG_PATTERN_COUNT; ++p) stochTrack(s).sequence(p).SETTER(v, true); \
    }
STOCH_SEQ_ROUTED(applyComplexity, setComplexity)
STOCH_SEQ_ROUTED(applyVariation, setVariation)
STOCH_SEQ_ROUTED(applyRest, setRest)
STOCH_SEQ_ROUTED(applySlide, setSlide)
STOCH_SEQ_ROUTED(applyBurst, setBurst)
STOCH_SEQ_ROUTED(applySleep, setSleep)
STOCH_SEQ_ROUTED(applyMutate, setMutate)
STOCH_SEQ_ROUTED(applyJump, setJump)
STOCH_SEQ_ROUTED(applyContour, setContour)
STOCH_SEQ_ROUTED(applyMaskRhythm, setMaskRhythm)
STOCH_SEQ_ROUTED(applyTiltRhythm, setTiltRhythm)
STOCH_SEQ_ROUTED(applyGateLength, setGateLength)
STOCH_SEQ_ROUTED(applyPatienceRhythm, setPatienceRhythm)
STOCH_SEQ_ROUTED(applyNoteDuration, setNoteDuration)
STOCH_SEQ_ROUTED(applyRotate, setRotate)
STOCH_SEQ_ROUTED(applyClockMult, setClockMultiplier)
STOCH_SEQ_ROUTED(applyFeel, setFeel)   // dead-slot fix: wired (no-op in old dispatch)
#undef STOCH_SEQ_ROUTED

// sequence-level BASE write, pattern fan-out -- mirrors the writeTarget special
// case. _scale/_rootNote/_divisor are plain ints (no routed slot); the base-write
// defect closes structurally at U6b (override table), not here.
void applyScale(const RouteParam::Scope &s, const RouteParam::Range &r, float n) {
    int v = denormI(r, n);
    for (int p = 0; p < CONFIG_PATTERN_COUNT; ++p) stochTrack(s).sequence(p).setScale(v);
}
void applyRootNote(const RouteParam::Scope &s, const RouteParam::Range &r, float n) {
    int v = denormI(r, n);
    for (int p = 0; p < CONFIG_PATTERN_COUNT; ++p) stochTrack(s).sequence(p).setRootNote(v);
}
void applyDivisor(const RouteParam::Scope &s, const RouteParam::Range &r, float n) {
    int v = denormI(r, n);
    for (int p = 0; p < CONFIG_PATTERN_COUNT; ++p) stochTrack(s).sequence(p).setDivisor(v);
}

// Ranges mirror Routing::targetInfos so denormalization matches writeTarget.
constexpr RouteParam::Row kRows[] = {
    // track-level
    { ParamKey::Octave,          "Octave",     { -10.f,  10.f }, RouteParam::Continuous, applyOctave },
    { ParamKey::Transpose,       "Transpose",  { -60.f,  60.f }, RouteParam::Continuous, applyTranspose },
    { ParamKey::SlideTime,       "Slide Time", {   0.f, 100.f }, RouteParam::Continuous, applySlideTime },
    // sequence-level routed
    { ParamKey::Complexity,      "Complexity", {   0.f, 100.f }, RouteParam::Continuous, applyComplexity },
    { ParamKey::Variation,       "Variation",  { -100.f, 100.f }, RouteParam::Continuous, applyVariation },
    { ParamKey::Rest,            "Rest",       {   0.f, 100.f }, RouteParam::Continuous, applyRest },
    { ParamKey::Slide,           "Slide",      {   0.f, 100.f }, RouteParam::Continuous, applySlide },
    { ParamKey::Burst,           "Burst",      {   0.f, 100.f }, RouteParam::Continuous, applyBurst },
    { ParamKey::Sleep,           "Sleep",      {   0.f, 100.f }, RouteParam::Continuous, applySleep },
    { ParamKey::Mutate,          "Mutate",     {   0.f, 100.f }, RouteParam::Continuous, applyMutate },
    { ParamKey::Jump,            "Jump",       {   0.f, 100.f }, RouteParam::Continuous, applyJump },
    { ParamKey::Contour,         "Contour",    { -100.f, 100.f }, RouteParam::Continuous, applyContour },
    { ParamKey::MaskRhythm,      "Mask Rhy",   {   0.f, 100.f }, RouteParam::Continuous, applyMaskRhythm },
    { ParamKey::TiltRhythm,      "Tilt Rhy",   { -100.f, 100.f }, RouteParam::Continuous, applyTiltRhythm },
    { ParamKey::StochasticGateLength, "Gate Length", { 0.f, 100.f }, RouteParam::Continuous, applyGateLength },
    { ParamKey::PatienceRhythm,  "Patience",   {   0.f, 100.f }, RouteParam::Continuous, applyPatienceRhythm },
    { ParamKey::NoteDuration,    "Note Dur",   {   0.f,   7.f }, RouteParam::Discrete,   applyNoteDuration },
    { ParamKey::Rotate,          "Rotate",     { -64.f,  64.f }, RouteParam::Continuous, applyRotate },
    { ParamKey::ClockMultiplier, "Clock Mult", {  50.f, 150.f }, RouteParam::Continuous, applyClockMult },
    { ParamKey::Feel,            "Feel",       {   0.f, 100.f }, RouteParam::Continuous, applyFeel },
    // sequence-level base write (defect-parity; closes at U6b)
    { ParamKey::Scale,           "Scale",      {   0.f,  23.f }, RouteParam::Discrete,   applyScale },
    { ParamKey::RootNote,        "Root Note",  {   0.f,  11.f }, RouteParam::Discrete,   applyRootNote },
    { ParamKey::Divisor,         "Divisor",    {   1.f, 768.f }, RouteParam::Discrete,   applyDivisor },
};

constexpr RouteParam::Table kTable{ RouteParam::Scope::Kind::Track, kRows, sizeof(kRows) / sizeof(kRows[0]) };

} // namespace

const RouteParam::Table &StochasticParamTable::table() {
    return kTable;
}
