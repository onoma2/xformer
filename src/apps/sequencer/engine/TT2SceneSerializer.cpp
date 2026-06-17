#include "TT2SceneSerializer.h"

#include "TT2Printer.h"
#include "TT2ScriptLoader.h"   // setScriptCommand, parseLine (pulls Types.h first)

#include "model/Types.h"
#include "model/Scale.h"

#include <cstring>
#include <cstdio>
#include <cstdlib>

namespace {

void wstr(Tt2SceneWrite w, void *c, const char *s) { w(c, s, std::strlen(s)); }
void wch(Tt2SceneWrite w, void *c, char ch) { w(c, &ch, 1); }
void wint(Tt2SceneWrite w, void *c, int v) {
    char b[12];
    int n = std::snprintf(b, sizeof(b), "%d", v);
    if (n > 0) w(c, b, size_t(n));
}

// Parse up to `max` whitespace/tab-separated ints from `line`. Returns count.
int parseInts(const char *line, long *out, int max) {
    int k = 0;
    const char *p = line;
    while (*p && k < max) {
        while (*p == ' ' || *p == '\t') ++p;
        if (!*p) break;
        char *end = nullptr;
        long v = std::strtol(p, &end, 10);
        if (end == p) break;
        out[k++] = v;
        p = end;
    }
    return k;
}

bool isBlankLine(const char *s) {
    for (const char *q = s; *q; ++q) {
        if (*q != ' ' && *q != '\t') return false;
    }
    return true;
}

enum Section { SEC_NONE, SEC_SCRIPT, SEC_PAT, SEC_TI, SEC_CI, SEC_CO, SEC_UNKNOWN };

} // namespace

bool tt2SerializeScene(const TeletypeProgram &p, Tt2SceneWrite w, void *c) {
    char line[TT2_PRINT_LINE_MAX];

    for (int s = 0; s < TT2_SCRIPT_COUNT; ++s) {
        wch(w, c, '#');
        if (s == TT2_METRO_SCRIPT)      wch(w, c, 'M');
        else if (s == TT2_INIT_SCRIPT)  wch(w, c, 'I');
        else                            wch(w, c, char('1' + s));
        wch(w, c, '\n');

        const TT2Script &sc = p.scripts[s];
        int len = sc.length > TT2_COMMANDS_PER_SCRIPT ? TT2_COMMANDS_PER_SCRIPT : sc.length;
        for (int l = 0; l < len; ++l) {
            if (!tt2PrintCommand(sc.commands[l], line, sizeof(line))) return false;
            wstr(w, c, line);
            wch(w, c, '\n');
        }
    }

    // #P: 4 patterns. Rows: len, wrap, start, end, then 64 value rows.
    wstr(w, c, "#P\n");
    for (int b = 0; b < TT2_PATTERN_COUNT; ++b) { wint(w, c, int(p.patterns[b].len));   wch(w, c, b == TT2_PATTERN_COUNT - 1 ? '\n' : '\t'); }
    for (int b = 0; b < TT2_PATTERN_COUNT; ++b) { wint(w, c, int(p.patterns[b].wrap));  wch(w, c, b == TT2_PATTERN_COUNT - 1 ? '\n' : '\t'); }
    for (int b = 0; b < TT2_PATTERN_COUNT; ++b) { wint(w, c, int(p.patterns[b].start)); wch(w, c, b == TT2_PATTERN_COUNT - 1 ? '\n' : '\t'); }
    for (int b = 0; b < TT2_PATTERN_COUNT; ++b) { wint(w, c, int(p.patterns[b].end));   wch(w, c, b == TT2_PATTERN_COUNT - 1 ? '\n' : '\t'); }
    for (int r = 0; r < TT2_PATTERN_LENGTH; ++r) {
        for (int b = 0; b < TT2_PATTERN_COUNT; ++b) {
            wint(w, c, int(p.patterns[b].val[r]));
            wch(w, c, b == TT2_PATTERN_COUNT - 1 ? '\n' : '\t');
        }
    }

    // #TI / #CI: input source enums, one per line.
    wstr(w, c, "#TI\n");
    for (int i = 0; i < TT2_TRIGGER_INPUT_COUNT; ++i) { wint(w, c, int(p.triggerSource[i])); wch(w, c, '\n'); }
    wstr(w, c, "#CI\n");
    for (int i = 0; i < TT2_CV_INPUT_COUNT; ++i) { wint(w, c, int(p.cvInputSource[i])); wch(w, c, '\n'); }

    // #CO: per-output shaping rows "range scale offset root".
    wstr(w, c, "#CO\n");
    for (int i = 0; i < TT2_CV_OUTPUT_COUNT; ++i) {
        wint(w, c, int(p.cvOutputRange[i]));        wch(w, c, ' ');
        wint(w, c, int(p.cvOutputQuantizeScale[i])); wch(w, c, ' ');
        wint(w, c, int(p.cvOutputOffset[i]));        wch(w, c, ' ');
        wint(w, c, int(p.cvOutputRootNote[i]));      wch(w, c, '\n');
    }

    return true;
}

bool tt2DeserializeScene(TeletypeProgram &p, Tt2SceneRead rd, void *c) {
    init(p);  // defaults; omitted sections stay default

    char line[TT2_PRINT_LINE_MAX];
    Section section = SEC_NONE;
    int scriptIdx = -1, scriptLine = 0;
    int patPhase = 0;     // 0=len 1=wrap 2=start 3=end 4..=value rows
    int tiIdx = 0, ciIdx = 0, coIdx = 0;
    bool ok = true;

    for (;;) {
        int n = 0, ch = 0;
        bool eof = false;
        for (;;) {
            ch = rd(c);
            if (ch < 0) { eof = true; break; }
            if (ch == '\n') break;
            if (ch == '\r') continue;
            if (n < int(sizeof(line)) - 1) line[n++] = char(ch);
        }
        line[n] = '\0';

        if (!(eof && n == 0)) {
            if (line[0] == '#') {
                // New section header; reset per-section counters.
                const char *t = line + 1;
                if (t[0] >= '1' && t[0] <= '8' && t[1] == '\0') {
                    section = SEC_SCRIPT; scriptIdx = t[0] - '1'; scriptLine = 0;
                } else if (std::strcmp(t, "M") == 0) {
                    section = SEC_SCRIPT; scriptIdx = TT2_METRO_SCRIPT; scriptLine = 0;
                } else if (std::strcmp(t, "I") == 0) {
                    section = SEC_SCRIPT; scriptIdx = TT2_INIT_SCRIPT; scriptLine = 0;
                } else if (std::strcmp(t, "P") == 0) {
                    section = SEC_PAT; patPhase = 0;
                } else if (std::strcmp(t, "TI") == 0) {
                    section = SEC_TI; tiIdx = 0;
                } else if (std::strcmp(t, "CI") == 0) {
                    section = SEC_CI; ciIdx = 0;
                } else if (std::strcmp(t, "CO") == 0) {
                    section = SEC_CO; coIdx = 0;
                } else {
                    section = SEC_UNKNOWN;
                }
            } else if (section == SEC_SCRIPT) {
                // Skip blank lines (matches loadScriptText; absorbs upstream
                // inter-section blank separators). Cap at the script length.
                if (!isBlankLine(line) && scriptLine < TT2_COMMANDS_PER_SCRIPT) {
                    if (!setScriptCommand(p, scriptIdx, scriptLine, line)) ok = false;
                    ++scriptLine;
                }
            } else if (section == SEC_PAT) {
                if (!isBlankLine(line)) {
                    long v[TT2_PATTERN_COUNT];
                    int got = parseInts(line, v, TT2_PATTERN_COUNT);
                    if (got < TT2_PATTERN_COUNT) {
                        ok = false;
                    } else if (patPhase < 4) {
                        for (int b = 0; b < TT2_PATTERN_COUNT; ++b) {
                            switch (patPhase) {
                            case 0: p.patterns[b].len   = uint16_t(v[b]); break;
                            case 1: p.patterns[b].wrap  = uint16_t(v[b]); break;
                            case 2: p.patterns[b].start = int16_t(v[b]); break;
                            case 3: p.patterns[b].end   = int16_t(v[b]); break;
                            }
                        }
                        ++patPhase;
                    } else {
                        int r = patPhase - 4;
                        if (r < TT2_PATTERN_LENGTH) {
                            for (int b = 0; b < TT2_PATTERN_COUNT; ++b) {
                                p.patterns[b].val[r] = int16_t(v[b]);
                            }
                        }
                        ++patPhase;
                    }
                }
            } else if (section == SEC_TI) {
                if (!isBlankLine(line) && tiIdx < TT2_TRIGGER_INPUT_COUNT) {
                    long v[1];
                    if (parseInts(line, v, 1) == 1 &&
                        v[0] >= 0 && v[0] < int(TT2TriggerSource::Last)) {
                        p.triggerSource[tiIdx] = TT2TriggerSource(v[0]);
                    }
                    ++tiIdx;
                }
            } else if (section == SEC_CI) {
                if (!isBlankLine(line) && ciIdx < TT2_CV_INPUT_COUNT) {
                    long v[1];
                    if (parseInts(line, v, 1) == 1 &&
                        v[0] >= 0 && v[0] < int(TT2CvInputSource::Last)) {
                        p.cvInputSource[ciIdx] = TT2CvInputSource(v[0]);
                    }
                    ++ciIdx;
                }
            } else if (section == SEC_CO) {
                if (!isBlankLine(line) && coIdx < TT2_CV_OUTPUT_COUNT) {
                    long v[4];
                    if (parseInts(line, v, 4) < 4) {
                        ok = false;
                    } else {
                        // Clamp persisted shaping to valid ranges — a hand-edited or
                        // corrupt scene must not feed out-of-bounds indices into the
                        // live output path (voltageRangeInfo / Scale::get).
                        long range = v[0];
                        if (range < 0 || range >= long(Types::VoltageRange::Last)) {
                            range = long(Types::VoltageRange::Bipolar5V);
                        }
                        long scale = v[1] < -1 ? -1 : (v[1] >= Scale::Count ? Scale::Count - 1 : v[1]);
                        long offset = v[2] < -500 ? -500 : (v[2] > 500 ? 500 : v[2]);
                        long root = v[3] < -1 ? -1 : (v[3] > 11 ? 11 : v[3]);
                        p.cvOutputRange[coIdx] = Types::VoltageRange(range);
                        p.cvOutputQuantizeScale[coIdx] = int8_t(scale);
                        p.cvOutputOffset[coIdx] = int16_t(offset);
                        p.cvOutputRootNote[coIdx] = int8_t(root);
                    }
                    ++coIdx;
                }
            }
        }

        if (eof) break;
    }

    return ok;
}
