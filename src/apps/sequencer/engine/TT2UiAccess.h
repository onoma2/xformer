#pragma once

#include "model/Track.h"
#include "model/TT2Config.h"

// Seam-1 model accessors: the single place UI pages reach a TT2 track's
// program. Full uses tt2Track().program(); Mini uses
// tt2MiniTrack().program(scene). The scene index defaults to 0 so Full call
// sites stay byte-identical; the page passes the active scene for Mini.

inline bool tt2IsMini(const Track &t) { return t.trackMode() == Track::TrackMode::TeletypeMini; }

inline int tt2ScriptCount(const Track &t) {
    return tt2IsMini(t) ? TT2ConfigMini::ScriptCount : TT2ConfigFull::ScriptCount;
}
inline int tt2TriggerInputCount(const Track &t) {
    return tt2IsMini(t) ? TT2ConfigMini::TriggerInputCount : TT2ConfigFull::TriggerInputCount;
}
inline int tt2MetroScriptIndex(const Track &t) {
    return tt2IsMini(t) ? TT2ConfigMini::MetroScript : TT2ConfigFull::MetroScript;
}
inline int tt2InitScriptIndex(const Track &t) {
    return tt2IsMini(t) ? TT2ConfigMini::InitScript : TT2ConfigFull::InitScript;
}

inline TT2Script &tt2Script(Track &t, int scriptIdx, int scene = 0) {
    return tt2IsMini(t) ? t.tt2MiniTrack().program(scene).scripts[scriptIdx]
                        : t.tt2Track().program().scripts[scriptIdx];
}

inline TT2Command *tt2ScriptCommand(Track &t, int scriptIdx, int lineIdx, int scene = 0) {
    return tt2IsMini(t) ? &t.tt2MiniTrack().program(scene).scripts[scriptIdx].commands[lineIdx]
                        : &t.tt2Track().program().scripts[scriptIdx].commands[lineIdx];
}
inline int tt2ScriptLength(const Track &t, int scriptIdx, int scene = 0) {
    return tt2IsMini(t) ? t.tt2MiniTrack().program(scene).scripts[scriptIdx].length
                        : t.tt2Track().program().scripts[scriptIdx].length;
}
inline void tt2SetScriptLength(Track &t, int scriptIdx, int len, int scene = 0) {
    if (tt2IsMini(t)) t.tt2MiniTrack().program(scene).scripts[scriptIdx].length = uint8_t(len);
    else t.tt2Track().program().scripts[scriptIdx].length = uint8_t(len);
}

inline Types::VoltageRange tt2CvOutputRange(const Track &t, int i, int scene = 0) {
    return tt2IsMini(t) ? t.tt2MiniTrack().program(scene).cvOutputRange[i] : t.tt2Track().program().cvOutputRange[i];
}
inline void tt2SetCvOutputRange(Track &t, int i, Types::VoltageRange v, int scene = 0) {
    if (tt2IsMini(t)) t.tt2MiniTrack().program(scene).cvOutputRange[i] = v;
    else t.tt2Track().program().cvOutputRange[i] = v;
}

inline int16_t tt2CvOutputOffset(const Track &t, int i, int scene = 0) {
    return tt2IsMini(t) ? t.tt2MiniTrack().program(scene).cvOutputOffset[i] : t.tt2Track().program().cvOutputOffset[i];
}
inline void tt2SetCvOutputOffset(Track &t, int i, int16_t v, int scene = 0) {
    if (tt2IsMini(t)) t.tt2MiniTrack().program(scene).cvOutputOffset[i] = v;
    else t.tt2Track().program().cvOutputOffset[i] = v;
}

inline int8_t tt2CvOutputQuantizeScale(const Track &t, int i, int scene = 0) {
    return tt2IsMini(t) ? t.tt2MiniTrack().program(scene).cvOutputQuantizeScale[i] : t.tt2Track().program().cvOutputQuantizeScale[i];
}
inline void tt2SetCvOutputQuantizeScale(Track &t, int i, int8_t v, int scene = 0) {
    if (tt2IsMini(t)) t.tt2MiniTrack().program(scene).cvOutputQuantizeScale[i] = v;
    else t.tt2Track().program().cvOutputQuantizeScale[i] = v;
}

inline int8_t tt2CvOutputRootNote(const Track &t, int i, int scene = 0) {
    return tt2IsMini(t) ? t.tt2MiniTrack().program(scene).cvOutputRootNote[i] : t.tt2Track().program().cvOutputRootNote[i];
}
inline void tt2SetCvOutputRootNote(Track &t, int i, int8_t v, int scene = 0) {
    if (tt2IsMini(t)) t.tt2MiniTrack().program(scene).cvOutputRootNote[i] = v;
    else t.tt2Track().program().cvOutputRootNote[i] = v;
}

inline TT2TriggerSource tt2TriggerSource(const Track &t, int i, int scene = 0) {
    return tt2IsMini(t) ? t.tt2MiniTrack().program(scene).triggerSource[i] : t.tt2Track().program().triggerSource[i];
}
inline void tt2SetTriggerSource(Track &t, int i, TT2TriggerSource v, int scene = 0) {
    if (tt2IsMini(t)) t.tt2MiniTrack().program(scene).triggerSource[i] = v;
    else t.tt2Track().program().triggerSource[i] = v;
}

inline TT2CvInputSource tt2CvInputSource(const Track &t, int i, int scene = 0) {
    return tt2IsMini(t) ? t.tt2MiniTrack().program(scene).cvInputSource[i] : t.tt2Track().program().cvInputSource[i];
}
inline void tt2SetCvInputSource(Track &t, int i, TT2CvInputSource v, int scene = 0) {
    if (tt2IsMini(t)) t.tt2MiniTrack().program(scene).cvInputSource[i] = v;
    else t.tt2Track().program().cvInputSource[i] = v;
}
