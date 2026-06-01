#include "ParamTableMidiCv.h"
#include "RouteParamKey.h"

#include "Track.h"

#include <cmath>

namespace {

inline MidiCvTrack &midiCvTrack(const RouteParam::Scope &scope) {
    return static_cast<Track *>(scope.object)->midiCvTrack();
}

inline int denormI(const RouteParam::Range &r, float n) {
    return int(std::round(n * (r.max - r.min) + r.min));
}

// track-level, routed slot, no fan-out (matches MidiCvTrack::writeRouted)
void applyTranspose(const RouteParam::Scope &s, const RouteParam::Range &r, float n) { midiCvTrack(s).setTranspose(denormI(r, n), true); }
void applySlideTime(const RouteParam::Scope &s, const RouteParam::Range &r, float n) { midiCvTrack(s).setSlideTime(denormI(r, n), true); }

// Ranges mirror Routing::targetInfos so denormalization matches writeTarget.
constexpr RouteParam::Row kRows[] = {
    { ParamKey::Transpose, "Transpose",  { -60.f, 60.f }, RouteParam::Continuous, applyTranspose },
    { ParamKey::SlideTime, "Slide Time", {   0.f, 100.f }, RouteParam::Continuous, applySlideTime },
};

constexpr RouteParam::Table kTable{ RouteParam::Scope::Kind::Track, kRows, sizeof(kRows) / sizeof(kRows[0]) };

} // namespace

const RouteParam::Table &MidiCvParamTable::table() {
    return kTable;
}
