#include "TT2Printer.h"
#include "TT2OpNames.h"

extern "C" {
#include "command.h"   // token-tag enum (tele_word_t): NUMBER/XNUMBER/BNUMBER/RNUMBER/OP/MOD/PRE_SEP/SUB_SEP
}

#include <cstdio>
#include <cstring>
#include <cstdint>

namespace {

// Append s into out[pos..cap), advancing pos. Returns false (NUL-terminated at the
// prior pos) if it would not fit.
bool appendStr(char *out, size_t cap, size_t &pos, const char *s) {
    size_t n = std::strlen(s);
    if (pos + n + 1 > cap) { out[pos] = '\0'; return false; }
    std::memcpy(out + pos, s, n);
    pos += n;
    out[pos] = '\0';
    return true;
}

bool appendChar(char *out, size_t cap, size_t &pos, char c) {
    if (pos + 2 > cap) { out[pos] = '\0'; return false; }
    out[pos++] = c;
    out[pos] = '\0';
    return true;
}

// Native number formatters — match teletype/src/helpers.c exactly so the lexer
// re-accepts them: signed decimal; X/B/R-prefixed uint16 bit pattern with leading
// (hex/bin) / trailing (rbin) zeros stripped to a minimum of one digit.
void fmtDec(int16_t value, char *buf) {
    std::snprintf(buf, 8, "%d", int(value));
}
void fmtHex(uint16_t value, char *buf) {
    static const char *D = "0123456789ABCDEF";
    int i = 0; buf[i++] = 'X';
    bool nz = false;
    for (int s = 3; s >= 0; --s) {
        uint8_t v = (value >> (s * 4)) & 0xf;
        if (nz || v) { buf[i++] = D[v]; nz = true; }
    }
    if (!nz) buf[i++] = '0';
    buf[i] = '\0';
}
void fmtBin(uint16_t value, char *buf) {
    int i = 0; buf[i++] = 'B';
    bool nz = false;
    for (int s = 15; s >= 0; --s) {
        uint8_t v = (value >> s) & 1;
        if (nz || v) { buf[i++] = char('0' + v); nz = true; }
    }
    if (!nz) buf[i++] = '0';
    buf[i] = '\0';
}
void fmtRbin(uint16_t value, char *buf) {
    char bits[16];
    for (int i = 0; i < 16; ++i) bits[i] = char('0' + ((value >> i) & 1));
    int hi = -1;
    for (int i = 15; i >= 0; --i) { if (bits[i] == '1') { hi = i; break; } }
    int n = (hi < 0) ? 1 : (hi + 1);   // value 0 -> "R0"
    if (hi < 0) bits[0] = '0';
    int j = 0; buf[j++] = 'R';
    for (int i = 0; i < n; ++i) buf[j++] = bits[i];
    buf[j] = '\0';
}

} // namespace

bool tt2PrintCommand(const TT2Command &cmd, char *out, size_t cap) {
    if (cap == 0) return false;
    out[0] = '\0';
    // tag[]/value[] are fixed TT2_COMMAND_MAX_LENGTH slots; reject a length that
    // would read past them (malformed project / future bank blob / bad caller).
    if (cmd.length > TT2_COMMAND_MAX_LENGTH) return false;
    size_t pos = 0;
    char num[20];
    for (uint8_t i = 0; i < cmd.length; ++i) {
        uint8_t tag = cmd.tag[i];
        int16_t value = cmd.value[i];
        bool ok = true;
        switch (tag) {
        case OP: {
            const char *n = tt2OpName(value);
            ok = appendStr(out, cap, pos, n ? n : "?");
            break;
        }
        case MOD: {
            const char *n = tt2ModName(value);
            ok = appendStr(out, cap, pos, n ? n : "?");
            break;
        }
        case NUMBER:  fmtDec(value, num);            ok = appendStr(out, cap, pos, num); break;
        case XNUMBER: fmtHex(uint16_t(value), num);  ok = appendStr(out, cap, pos, num); break;
        case BNUMBER: fmtBin(uint16_t(value), num);  ok = appendStr(out, cap, pos, num); break;
        case RNUMBER: fmtRbin(uint16_t(value), num); ok = appendStr(out, cap, pos, num); break;
        case PRE_SEP: ok = appendChar(out, cap, pos, ':'); break;
        case SUB_SEP: ok = appendChar(out, cap, pos, ';'); break;
        default: break;  // unknown tag: skip
        }
        if (!ok) return false;
        // space unless last token or the next is a separator (mirrors the lexer)
        if (i + 1 < cmd.length) {
            uint8_t next = cmd.tag[i + 1];
            if (next != PRE_SEP && next != SUB_SEP) {
                if (!appendChar(out, cap, pos, ' ')) return false;
            }
        }
    }
    return true;
}

bool tt2PrintScript(const TT2Script &script, char *out, size_t cap) {
    if (cap == 0) return false;
    out[0] = '\0';
    size_t pos = 0;
    int n = script.length;
    if (n > TT2_COMMANDS_PER_SCRIPT) n = TT2_COMMANDS_PER_SCRIPT;
    for (int i = 0; i < n; ++i) {
        size_t lineStart = pos;
        if (i > 0 && !appendChar(out, cap, pos, '\n')) { out[lineStart] = '\0'; return false; }
        if (!tt2PrintCommand(script.commands[i], out + pos, cap - pos)) {
            out[lineStart] = '\0';
            return false;
        }
        pos += std::strlen(out + pos);
    }
    return true;
}
