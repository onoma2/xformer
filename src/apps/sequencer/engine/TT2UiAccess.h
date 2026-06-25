#pragma once

#include "model/Track.h"
#include "model/TT2Config.h"

// Seam-1 model accessors: the single place UI pages reach a TT2 track's
// program. Only the Full variant exists today, so each wraps tt2Track();
// a future Mini variant adds its branch here, not in the pages.

inline int tt2ScriptCount(const Track &) { return TT2ConfigFull::ScriptCount; }
inline int tt2TriggerInputCount(const Track &) { return TT2ConfigFull::TriggerInputCount; }

inline TT2Command *tt2ScriptCommand(Track &t, int scriptIdx, int lineIdx) { return &t.tt2Track().program().scripts[scriptIdx].commands[lineIdx]; }
inline int tt2ScriptLength(const Track &t, int scriptIdx) { return t.tt2Track().program().scripts[scriptIdx].length; }
inline void tt2SetScriptLength(Track &t, int scriptIdx, int len) { t.tt2Track().program().scripts[scriptIdx].length = uint8_t(len); }

inline Types::VoltageRange tt2CvOutputRange(const Track &t, int i) { return t.tt2Track().program().cvOutputRange[i]; }
inline void tt2SetCvOutputRange(Track &t, int i, Types::VoltageRange v) { t.tt2Track().program().cvOutputRange[i] = v; }

inline int16_t tt2CvOutputOffset(const Track &t, int i) { return t.tt2Track().program().cvOutputOffset[i]; }
inline void tt2SetCvOutputOffset(Track &t, int i, int16_t v) { t.tt2Track().program().cvOutputOffset[i] = v; }

inline int8_t tt2CvOutputQuantizeScale(const Track &t, int i) { return t.tt2Track().program().cvOutputQuantizeScale[i]; }
inline void tt2SetCvOutputQuantizeScale(Track &t, int i, int8_t v) { t.tt2Track().program().cvOutputQuantizeScale[i] = v; }

inline int8_t tt2CvOutputRootNote(const Track &t, int i) { return t.tt2Track().program().cvOutputRootNote[i]; }
inline void tt2SetCvOutputRootNote(Track &t, int i, int8_t v) { t.tt2Track().program().cvOutputRootNote[i] = v; }

inline TT2TriggerSource tt2TriggerSource(const Track &t, int i) { return t.tt2Track().program().triggerSource[i]; }
inline void tt2SetTriggerSource(Track &t, int i, TT2TriggerSource v) { t.tt2Track().program().triggerSource[i] = v; }

inline TT2CvInputSource tt2CvInputSource(const Track &t, int i) { return t.tt2Track().program().cvInputSource[i]; }
inline void tt2SetCvInputSource(Track &t, int i, TT2CvInputSource v) { t.tt2Track().program().cvInputSource[i] = v; }
