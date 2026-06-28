#pragma once

#include "model/Model.h"
#include "model/TeletypeProgram.h"

#include <cstdint>

// Increment cross-track recursion depth for the lifetime of a WS dispatch.
struct TT2CrossGuard {
    uint8_t &d;
    explicit TT2CrossGuard(uint8_t &x) : d(x) { ++d; }
    ~TT2CrossGuard() { --d; }
};

// Cross-track PN-cell resolver shared by both TT2 engines' host overrides.
// bank/idx SIGNED, normalised exactly like same-track PN (normalisePn on bank,
// normaliseIdx — negative-counts-from-p.len — on idx). nullptr if not a TT2 track.

template<typename Cfg>
static inline int16_t *tt2CrossPatternCellImpl(TeletypeProgramT<Cfg> &program, int16_t bank, int16_t idx) {
    if (bank < 0) bank = 0;
    else if (bank >= Cfg::PatternCount) bank = Cfg::PatternCount - 1;
    auto &p = program.patterns[bank];
    int16_t len = int16_t(p.len);
    if (idx < 0) {
        if (idx < -len) idx = 0;
        else idx = len + idx;
    }
    if (idx >= Cfg::PatternLength) idx = Cfg::PatternLength - 1;
    if (idx < 0) idx = 0;
    return &p.val[idx];
}

static inline int16_t *tt2CrossPatternCell(Model &model, int t, int16_t bank, int16_t idx) {
    if (t < 0 || t >= CONFIG_TRACK_COUNT) return nullptr;
    Track &tk = model.project().track(t);
    if (tk.trackMode() == Track::TrackMode::TeletypeV2)
        return tt2CrossPatternCellImpl<TT2ConfigFull>(tk.tt2Track().program(), bank, idx);
    if (tk.trackMode() == Track::TrackMode::TeletypeMini) {
        int scene = model.project().playState().trackState(t).pattern() % TT2ConfigMini::SceneCount;
        return tt2CrossPatternCellImpl<TT2ConfigMini>(tk.tt2MiniTrack().program(scene), bank, idx);
    }
    return nullptr;
}
