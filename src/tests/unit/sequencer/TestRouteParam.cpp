#include "UnitTest.h"

#include "apps/sequencer/model/RouteParam.h"

// U1 of the routing mod-matrix overhaul: the name-agnostic addressing core.
// A route delivers a normalized value into an addressed param slot. The engine
// resolves (scope, paramKey) -> Row and calls the row's apply hook; it never
// sees the target's name, range, or meaning. paramKey is an append-only numeric
// key, NOT the array index, so adding/reordering rows never shifts saved routes.
// See docs/plans/2026-05-31-002-routing-mod-matrix-overhaul-plan.md.

namespace {

// Stand-in for a concrete scope object (a track engine / sequence / engine).
// The apply hook writes into it without the engine knowing what it is.
struct FakeScopeObject {
    float lastValue = -1.f;
    int writeCount = 0;
};

void applyToFake(const RouteParam::Scope &scope, const RouteParam::Range &, float normalized) {
    auto *obj = static_cast<FakeScopeObject *>(scope.object);
    obj->lastValue = normalized;
    obj->writeCount += 1;
}

void applyNoop(const RouteParam::Scope &, const RouteParam::Range &, float) {}

// Rows are declared with keys in NON-index order to prove find() resolves by
// key, not by position. Key 0 is reserved for "None", so real keys start at 1.
constexpr RouteParam::Row kRows[] = {
    { 5, "five",       { 0.f, 1.f }, RouteParam::Continuous, applyToFake },
    { 2, "two",        { 0.f, 1.f }, RouteParam::Continuous, applyToFake },
    { 9, "structural", { 0.f, 1.f }, RouteParam::Structural, applyNoop   },
};

// A track-scoped test table; applyRouted scopes below must match this kind.
constexpr RouteParam::Table kTable{ RouteParam::Scope::Kind::Track, kRows, sizeof(kRows) / sizeof(kRows[0]) };

RouteParam::Scope trackScope(void *object) {
    RouteParam::Scope scope;
    scope.kind = RouteParam::Scope::Kind::Track;
    scope.object = object;
    return scope;
}

} // namespace

UNIT_TEST("RouteParam") {

CASE("find resolves by append-only key, not array index") {
    const RouteParam::Row *row = kTable.find(2);
    expectTrue(row != nullptr, "key 2 found");
    expectEqual(row->name, "two", "key 2 maps to the second declared row");

    const RouteParam::Row *first = kTable.find(5);
    expectTrue(first != nullptr, "key 5 found");
    expectEqual(first->name, "five", "key 5 maps to the first declared row");
}

CASE("unknown key returns nullptr") {
    expectTrue(kTable.find(7) == nullptr, "key 7 is not in the table");
    expectTrue(kTable.find(0) == nullptr, "key 0 (None) is not a row");
}

CASE("applyRouted invokes the row hook with the normalized value") {
    FakeScopeObject obj;
    RouteParam::Scope scope = trackScope(&obj);
    scope.trackIndex = 3;

    bool applied = kTable.applyRouted(scope, 5, 0.75f);
    expectTrue(applied, "applyRouted returns true for a live key");
    expectEqual(obj.lastValue, 0.75f, "hook wrote the normalized value");
    expectEqual(obj.writeCount, 1, "hook fired exactly once");
}

CASE("applyRouted on unknown key writes nothing and returns false") {
    FakeScopeObject obj;
    bool applied = kTable.applyRouted(trackScope(&obj), 7, 0.5f);
    expectFalse(applied, "unknown key is not a silent success");
    expectEqual(obj.writeCount, 0, "no hook fired");
}

CASE("structural rows are never applied") {
    FakeScopeObject obj;
    bool applied = kTable.applyRouted(trackScope(&obj), 9, 0.5f);
    expectFalse(applied, "structural param is UI-only, never routable");
    expectEqual(obj.writeCount, 0, "structural hook never fired");
}

CASE("null object fails closed, hook never dereferences") {
    bool applied = kTable.applyRouted(trackScope(nullptr), 5, 0.75f);
    expectFalse(applied, "null scope object is rejected before the hook");
}

CASE("scope-kind mismatch fails closed") {
    FakeScopeObject obj;
    RouteParam::Scope wrong;
    wrong.kind = RouteParam::Scope::Kind::Global;   // table expects Track
    wrong.object = &obj;

    bool applied = kTable.applyRouted(wrong, 5, 0.75f);
    expectFalse(applied, "wrong scope kind is rejected before the hook");
    expectEqual(obj.writeCount, 0, "mismatched-kind hook never fired");
}

CASE("flags read back per row") {
    expectTrue((kTable.find(5)->flags & RouteParam::Continuous) != 0, "key 5 is continuous");
    expectTrue((kTable.find(9)->flags & RouteParam::Structural) != 0, "key 9 is structural");
    expectTrue((kTable.find(5)->flags & RouteParam::Structural) == 0, "key 5 is not structural");
}

} // UNIT_TEST
