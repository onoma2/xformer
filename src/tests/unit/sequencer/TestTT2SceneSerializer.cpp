#include "UnitTest.h"

#include "engine/TT2SceneSerializer.h"
#include "engine/TT2ScriptLoader.h"   // loadScriptText, init(program) (pulls Types.h first)
#include "model/Scale.h"

#include <string>

namespace {

struct WriteBuf { std::string s; };
void writeCb(void *ctx, const char *d, size_t n) {
    static_cast<WriteBuf *>(ctx)->s.append(d, n);
}

struct ReadBuf { const std::string *s; size_t pos; };
int readCb(void *ctx) {
    auto *r = static_cast<ReadBuf *>(ctx);
    return r->pos < r->s->size() ? int(static_cast<unsigned char>((*r->s)[r->pos++])) : -1;
}

std::string serialize(const TeletypeProgram &p) {
    WriteBuf wb;
    tt2SerializeScene(p, writeCb, &wb);
    return wb.s;
}

bool deserialize(const std::string &text, TeletypeProgram &p) {
    ReadBuf rb{ &text, 0 };
    return tt2DeserializeScene(p, readCb, &rb);
}

// A program exercising every serialized section.
void buildRepresentative(TeletypeProgram &p) {
    init(p);
    loadScriptText(p, 0, "A 8\nB ADD A 1\n");
    loadScriptText(p, 7, "CV 1 V 1\n");                 // numbered script 8
    loadScriptText(p, TT2_METRO_SCRIPT, "A ADD A 1\n");
    loadScriptText(p, TT2_INIT_SCRIPT, "M 500\n");
    p.patterns[0].len = 4;
    p.patterns[0].wrap = 1;
    p.patterns[0].start = 0;
    p.patterns[0].end = 3;
    p.patterns[0].val[0] = 10;
    p.patterns[0].val[1] = -5;
    p.patterns[2].val[63] = 99;
    p.triggerSource[0] = TT2TriggerSource::GateOut1;
    p.cvInputSource[0] = TT2CvInputSource::CvOut1;
    p.cvOutputRange[0] = Types::VoltageRange::Unipolar5V;
    p.cvOutputQuantizeScale[0] = 2;
    p.cvOutputOffset[0] = 12;
    p.cvOutputRootNote[0] = 5;
}

} // namespace

UNIT_TEST("TT2SceneSerializer") {

    CASE("round_trip_identity") {
        TeletypeProgram a;
        buildRepresentative(a);
        std::string s1 = serialize(a);

        TeletypeProgram b;
        expectTrue(deserialize(s1, b), "deserialize succeeds");
        std::string s2 = serialize(b);
        expectTrue(s1 == s2, "serialize(deserialize(x)) == x (text identity)");
    }

    CASE("round_trip_fields") {
        TeletypeProgram a;
        buildRepresentative(a);
        TeletypeProgram b;
        expectTrue(deserialize(serialize(a), b), "deserialize succeeds");

        expectEqual(int(b.scripts[0].length), 2, "script 0 length");
        expectEqual(int(b.scripts[7].length), 1, "script 8 length");
        expectEqual(int(b.scripts[TT2_METRO_SCRIPT].length), 1, "metro length");
        expectEqual(int(b.scripts[TT2_INIT_SCRIPT].length), 1, "init length");
        expectEqual(int(b.patterns[0].len), 4, "pattern len");
        expectEqual(int(b.patterns[0].val[0]), 10, "pattern val 0");
        expectEqual(int(b.patterns[0].val[1]), -5, "pattern val 1 (negative)");
        expectEqual(int(b.patterns[2].val[63]), 99, "pattern val last");
        expectEqual(int(b.triggerSource[0]), int(TT2TriggerSource::GateOut1), "trigger source");
        expectEqual(int(b.cvInputSource[0]), int(TT2CvInputSource::CvOut1), "cv-in source");
        expectEqual(int(b.cvOutputRange[0]), int(Types::VoltageRange::Unipolar5V), "out range");
        expectEqual(int(b.cvOutputQuantizeScale[0]), 2, "out scale");
        expectEqual(int(b.cvOutputOffset[0]), 12, "out offset");
        expectEqual(int(b.cvOutputRootNote[0]), 5, "out root");
    }

    CASE("stock_scene_defaults") {
        // Only common sections (scripts + patterns), no TT2 extension sections.
        std::string stock =
            "#1\nA 1\n"
            "#P\n0\t0\t0\t0\n0\t0\t0\t0\n0\t0\t0\t0\n0\t0\t0\t0\n";
        TeletypeProgram b;
        expectTrue(deserialize(stock, b), "stock scene loads");
        expectEqual(int(b.scripts[0].length), 1, "script 0 parsed");
        // Extension fields keep init() defaults.
        expectEqual(int(b.triggerSource[0]), int(TT2TriggerSource::CvIn1), "default trigger source");
        expectEqual(int(b.cvOutputRange[0]), int(Types::VoltageRange::Bipolar5V), "default range");
        expectEqual(int(b.cvOutputQuantizeScale[0]), -1, "default quantize off");
    }

    CASE("unknown_section_skipped") {
        std::string withGrid =
            "#1\nA 7\n"
            "#G\n0101010101010101\n0000\n"
            "#P\n2\t0\t0\t0\n0\t0\t0\t0\n0\t0\t0\t0\n0\t0\t0\t0\n";
        TeletypeProgram b;
        expectTrue(deserialize(withGrid, b), "unknown #G skipped, rest parses");
        expectEqual(int(b.scripts[0].length), 1, "script before #G parsed");
        expectEqual(int(b.patterns[0].len), 2, "pattern after #G parsed");
    }

    CASE("malformed_returns_false") {
        std::string bad = "#CO\nnot numbers here\n";
        TeletypeProgram b;
        expectTrue(!deserialize(bad, b), "malformed numeric row -> false");
    }

    CASE("co_out_of_range_values_clamped") {
        // Hand-edited/corrupt #CO must not produce an out-of-bounds VoltageRange
        // or quantize index that the live output path would dereference.
        std::string bad =
            "#1\nA 1\n"
            "#CO\n255 99 9999 100\n";
        TeletypeProgram b;
        expectTrue(deserialize(bad, b), "loads (clamped, not rejected)");
        expectTrue(int(b.cvOutputRange[0]) < int(Types::VoltageRange::Last), "range clamped in bounds");
        expectTrue(int(b.cvOutputQuantizeScale[0]) < Scale::Count, "scale clamped in bounds");
        expectTrue(int(b.cvOutputOffset[0]) <= 500, "offset clamped");
        expectTrue(int(b.cvOutputRootNote[0]) <= 11, "root clamped");
    }

    CASE("empty_program_round_trips") {
        TeletypeProgram a;
        init(a);
        TeletypeProgram b;
        expectTrue(deserialize(serialize(a), b), "empty deserialize ok");
        expectEqual(int(b.scripts[0].length), 0, "no script lines");
        expectTrue(serialize(a) == serialize(b), "empty round-trips");
    }
}
