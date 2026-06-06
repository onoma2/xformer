#include "UnitTest.h"

#include "model/ParamTableGlobal.h"
#include "model/RouteParamKey.h"

// U2 of the routing mod-matrix overhaul: the global (Tier-0) param table.
// Characterization: the table owns the expected keys and resolves each to its
// declared name/flags. Global scope. The apply hooks the table used to carry are
// gone (the override path reads rows by key, never applies via hooks).

UNIT_TEST("ParamTableGlobal") {

CASE("table owns the expected keys with declared names and flags") {
    struct Spec { uint8_t key; const char *name; uint8_t flag; };
    const Spec specs[] = {
        { ParamKey::Tempo,        "Tempo",   RouteParam::Continuous },
        { ParamKey::Swing,        "Swing",   RouteParam::Continuous },
        { ParamKey::CvRouteScan,  "CVR Scn", RouteParam::Continuous },
        { ParamKey::CvRouteRoute, "CVR Rte", RouteParam::Continuous },
    };
    const RouteParam::Table &t = GlobalParamTable::table();
    for (const auto &s : specs) {
        const RouteParam::Row *row = t.find(s.key);
        expectTrue(row != nullptr, s.name);
        expectEqual(row->name, s.name, s.name);
        expectTrue((row->flags & s.flag) != 0, s.name);
    }
}

CASE("table is global-scoped and rejects unowned keys") {
    const RouteParam::Table &t = GlobalParamTable::table();
    expectTrue(t.expectedKind() == RouteParam::Scope::Kind::Global, "global scope kind");
    expectTrue(t.find(0) == nullptr, "key 0 (None) not owned");
    expectTrue(t.find(200) == nullptr, "unknown key not owned");
}

} // UNIT_TEST
