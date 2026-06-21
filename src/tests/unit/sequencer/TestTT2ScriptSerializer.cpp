#include "UnitTest.h"

#include "engine/TT2SceneSerializer.h"
#include "engine/TT2ScriptLoader.h"   // loadScriptText, init(program)

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

std::string serializeScript(const TeletypeProgram &p, int idx) {
    WriteBuf wb;
    tt2SerializeScript(p, idx, writeCb, &wb);
    return wb.s;
}

bool deserializeScript(const std::string &text, TeletypeProgram &p, int idx) {
    ReadBuf rb{ &text, 0 };
    return tt2DeserializeScript(p, idx, readCb, &rb);
}

} // namespace

UNIT_TEST("TT2ScriptSerializer") {

CASE("script text round-trips") {
    TeletypeProgram p; init(p);
    loadScriptText(p, 3, "A 8\nCV 1 V 1\n");
    std::string t1 = serializeScript(p, 3);

    TeletypeProgram p2; init(p2);
    expectTrue(deserializeScript(t1, p2, 3), "deserialize succeeds");
    expectEqual(serializeScript(p2, 3).c_str(), t1.c_str(), "serialize->deserialize->serialize is stable");
}

CASE("load replaces only the target slot") {
    TeletypeProgram src; init(src);
    loadScriptText(src, 3, "B 2\n");
    std::string s3 = serializeScript(src, 3);

    TeletypeProgram p2; init(p2);
    loadScriptText(p2, 0, "X 9\n");
    std::string before0 = serializeScript(p2, 0);

    deserializeScript(s3, p2, 3);
    expectEqual(serializeScript(p2, 0).c_str(), before0.c_str(), "slot 0 untouched");
    expectEqual(serializeScript(p2, 3).c_str(), s3.c_str(), "slot 3 loaded");
}

CASE("empty script serializes empty and clears on load") {
    TeletypeProgram p; init(p);
    std::string empty = serializeScript(p, 5);
    expectEqual(int(empty.size()), 0, "empty script -> empty text");

    TeletypeProgram p2; init(p2);
    loadScriptText(p2, 5, "A 1\n");
    deserializeScript(empty, p2, 5);
    expectEqual(int(serializeScript(p2, 5).size()), 0, "slot cleared by empty load");
}

}
