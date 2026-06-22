#include "UnitTest.h"

#include "engine/generators/HelicalAr.h"

#include <vector>

UNIT_TEST("HelicalAr") {

CASE("same seed + params reproduces the step stream") {
    HelicalAr a, b;
    a.seed(0x1234);
    b.seed(0x1234);
    for (int i = 0; i < 32; ++i) {
        auto ra = a.step(14, 7, 192, +1, 24, 768);
        auto rb = b.step(14, 7, 192, +1, 24, 768);
        expectEqual(ra.noteIndex, rb.noteIndex, "deterministic noteIndex");
        expectEqual(ra.durationTicks, rb.durationTicks, "deterministic duration");
        expectEqual(ra.gateLength, rb.gateLength, "deterministic gate");
    }
}

CASE("duration couples into the next pitch") {
    HelicalAr a, b;
    a.seed(0x55);
    b.seed(0x55);
    a.step(14, 7, 192, +1, 24, 768);
    b.step(14, 7, 192, +1, 24, 768);
    b.dur += 50.f; // perturb only the carried duration state
    auto na = a.step(14, 7, 192, +1, 24, 768);
    auto nb = b.step(14, 7, 192, +1, 24, 768);
    expectTrue(na.noteIndex != nb.noteIndex, "duration feeds the next pitch");
}

CASE("noteIndex stays within [0, span)") {
    HelicalAr h;
    h.seed(0xABCD);
    for (int i = 0; i < 200; ++i) {
        auto r = h.step(21, 7, 192, +1, 24, 768);
        expectTrue(r.noteIndex >= 0 && r.noteIndex < 21, "note within span");
    }
}

CASE("law direction makes higher notes monotonically longer or shorter") {
    int prev = -1;
    for (int n = 0; n < 14; ++n) {
        int up = HelicalAr::durationForNote(n, 7, 192, +1, 24, 768);
        if (prev >= 0) expectTrue(up >= prev, "lawDir + non-decreasing with note");
        prev = up;
    }
    int last = 1 << 30;
    for (int n = 0; n < 14; ++n) {
        int down = HelicalAr::durationForNote(n, 7, 192, -1, 24, 768);
        expectTrue(down <= last, "lawDir - non-increasing with note");
        last = down;
    }
}

CASE("duration and gate stay within bounds") {
    HelicalAr h;
    h.seed(0x777);
    for (int i = 0; i < 200; ++i) {
        auto r = h.step(14, 7, 192, +1, 24, 768);
        expectTrue(r.durationTicks >= 24 && r.durationTicks <= 768, "duration within [min,max]");
        expectTrue(r.gateLength >= 0 && r.gateLength <= r.durationTicks, "gate <= duration");
        expectTrue(r.gateLength * 100 >= r.durationTicks * 20 - 100, "gate at least ~20% of duration");
    }
}

CASE("does not collapse to a single repeated note") {
    HelicalAr h;
    h.seed(0);
    std::vector<int> notes;
    for (int i = 0; i < 48; ++i) {
        notes.push_back(h.step(14, 7, 192, +1, 24, 768).noteIndex);
    }
    bool varied = false;
    for (size_t i = 1; i < notes.size(); ++i) {
        if (notes[i] != notes[0]) { varied = true; break; }
    }
    expectTrue(varied, "sequence varies, no fixed-point collapse");
}

CASE("stays lively — no short cycle, varied notes") {
    for (int span : {14, 24}) {
        int scaleSize = span / 2;
        HelicalAr h;
        h.seed(0x1234);
        std::vector<int> notes;
        for (int i = 0; i < 96; ++i) {
            notes.push_back(h.step(span, scaleSize, 192, +1, 24, 768).noteIndex);
        }
        const int start = 48; // examine the tail, past any transient
        for (int p = 1; p <= 16; ++p) {
            bool periodic = true;
            for (int i = start; i + p < int(notes.size()); ++i) {
                if (notes[i] != notes[i + p]) { periodic = false; break; }
            }
            expectTrue(!periodic, "tail not periodic at any period 1..16");
        }
        std::vector<int> seen;
        for (int i = start; i < int(notes.size()); ++i) {
            bool found = false;
            for (int s : seen) if (s == notes[i]) { found = true; break; }
            if (!found) seen.push_back(notes[i]);
        }
        expectTrue(int(seen.size()) > 6, "tail visits many distinct notes");
    }
}

}
