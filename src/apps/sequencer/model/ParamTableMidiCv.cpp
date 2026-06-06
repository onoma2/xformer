#include "ParamTableMidiCv.h"
#include "RouteParamKey.h"

namespace {

// Ranges mirror Routing::targetInfos so denormalization matches writeTarget.
constexpr RouteParam::Row kRows[] = {
    { ParamKey::Transpose, "Transpose",  { -60.f, 60.f }, RouteParam::Continuous },
    { ParamKey::SlideTime, "Slide Time", {   0.f, 100.f }, RouteParam::Continuous },
};

constexpr RouteParam::Table kTable{ RouteParam::Scope::Kind::Track, kRows, sizeof(kRows) / sizeof(kRows[0]) };

} // namespace

const RouteParam::Table &MidiCvParamTable::table() {
    return kTable;
}
