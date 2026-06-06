#include "UnitTest.h"

#include "apps/sequencer/model/RouteParam.h"

// U1 of the routing mod-matrix overhaul: the name-agnostic addressing core.
// A route addresses a param slot by (scope, paramKey). The engine resolves to a
// Row to read its key, range, name and flags. paramKey is an append-only numeric
// key, NOT the array index, so adding/reordering rows never shifts saved routes.
// See docs/plans/2026-05-31-002-routing-mod-matrix-overhaul-plan.md.

namespace {

// Rows are declared with keys in NON-index order to prove find() resolves by
// key, not by position. Key 0 is reserved for "None", so real keys start at 1.
constexpr RouteParam::Row kRows[] = {
    { 5, "five",       { 0.f, 1.f }, RouteParam::Continuous },
    { 2, "two",        { 0.f, 1.f }, RouteParam::Continuous },
    { 9, "structural", { 0.f, 1.f }, RouteParam::Structural },
};

constexpr RouteParam::Table kTable{ RouteParam::Scope::Kind::Track, kRows, sizeof(kRows) / sizeof(kRows[0]) };

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

CASE("gate rising edge fires once, re-arms after falling") {
    uint8_t mask = 0;
    expectTrue(RouteParam::gateRisingEdge(mask, 0, 1.0f), "low->high fires");
    expectFalse(RouteParam::gateRisingEdge(mask, 0, 1.0f), "held high does not refire");
    expectFalse(RouteParam::gateRisingEdge(mask, 0, 0.0f), "high->low is not a rising edge");
    expectTrue(RouteParam::gateRisingEdge(mask, 0, 1.0f), "re-armed after falling");
}

CASE("gate tracks are independent within one mask") {
    uint8_t mask = 0;
    expectTrue(RouteParam::gateRisingEdge(mask, 2, 1.0f), "track 2 fires");
    expectFalse(RouteParam::gateRisingEdge(mask, 2, 1.0f), "track 2 holds");
    expectTrue(RouteParam::gateRisingEdge(mask, 5, 1.0f), "track 5 fires independently");
    expectFalse(RouteParam::gateRisingEdge(mask, 2, 1.0f), "track 2 still held");
}

CASE("gate threshold is 0.5") {
    uint8_t mask = 0;
    expectFalse(RouteParam::gateRisingEdge(mask, 0, 0.5f), "exactly 0.5 is not high");
    expectTrue(RouteParam::gateRisingEdge(mask, 0, 0.51f), "just above 0.5 is high");
}

CASE("flags read back per row") {
    expectTrue((kTable.find(5)->flags & RouteParam::Continuous) != 0, "key 5 is continuous");
    expectTrue((kTable.find(9)->flags & RouteParam::Structural) != 0, "key 9 is structural");
    expectTrue((kTable.find(5)->flags & RouteParam::Structural) == 0, "key 5 is not structural");
}

} // UNIT_TEST
