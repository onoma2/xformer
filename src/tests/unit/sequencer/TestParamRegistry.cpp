#include "UnitTest.h"

#include "model/ParamTableGlobal.h"
#include "model/ParamTableNote.h"

// Polices the shared ParamKey registry invariant (F6): within any one table no
// two rows may share a key, and no row may use key 0 (None). Table::find()
// resolves by linear scan and returns the FIRST match, so a duplicate would
// silently alias one concept onto another instead of failing -- this sweep turns
// that comment-only contract into a compile-and-run guard. Every per-type table
// added at U4 onward must be appended to the sweep below.

namespace {

void expectUniqueNonZeroKeys(const RouteParam::Table &table, const char *name) {
    const RouteParam::Row *rows = table.rows();
    size_t count = table.count();
    for (size_t i = 0; i < count; ++i) {
        expectTrue(rows[i].key != 0, name);   // key 0 is None, never a row
        for (size_t j = i + 1; j < count; ++j) {
            expectTrue(rows[i].key != rows[j].key, name);
        }
    }
}

} // namespace

UNIT_TEST("ParamRegistry") {

CASE("every table has unique, non-zero row keys") {
    expectUniqueNonZeroKeys(GlobalParamTable::table(), "Global table key uniqueness");
    expectUniqueNonZeroKeys(NoteParamTable::table(), "Note table key uniqueness");
}

} // UNIT_TEST
