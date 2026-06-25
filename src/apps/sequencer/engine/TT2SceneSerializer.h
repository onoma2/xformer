#pragma once

#include "model/TeletypeProgram.h"

#include <cstddef>

// Streaming read/write of a TeletypeProgram as an upstream-compatible teletype
// scene .txt (sections #1-#8 / #M / #I scripts, #P patterns) plus appended TT2
// extension sections (#TI trigger sources, #CI CV-in sources, #CO per-output
// shaping). No whole-section buffers: one small line buffer, char/line sinks
// supplied by the caller so this stays decoupled from the SD/fs layer.
//
// Metro period / M.C clock-sync are NOT serialized: like upstream, they are
// runtime state reproduced by the INIT/metro script at load, not scene data.

// Write `len` bytes of scene text. ctx is opaque caller state.
using Tt2SceneWrite = void (*)(void *ctx, const char *data, size_t len);
// Return the next scene-text byte, or a negative value at end of input.
using Tt2SceneRead = int (*)(void *ctx);

// Serialize program -> text via write(ctx, ...). Returns false only on an
// internal line-buffer overflow (a single command longer than the buffer).
template<typename Cfg>
bool tt2SerializeScene(const TeletypeProgramT<Cfg> &program, Tt2SceneWrite write, void *ctx);

// Deserialize text (pulled via read(ctx)) into `staging`. The function init()s
// `staging` first, so any omitted section keeps defaults; unknown #-sections are
// skipped. Returns false on malformed input (script parse error, short numeric
// row). The caller swaps `staging` into the live track only on true.
template<typename Cfg>
bool tt2DeserializeScene(TeletypeProgramT<Cfg> &staging, Tt2SceneRead read, void *ctx);

// Serialize/deserialize a single script slot's text (its command lines only, no
// #-section header). Deserialize replaces just that slot, leaving the rest of
// the program untouched. Used by per-script save/load.
template<typename Cfg>
bool tt2SerializeScript(const TeletypeProgramT<Cfg> &program, int scriptIndex, Tt2SceneWrite write, void *ctx);
template<typename Cfg>
bool tt2DeserializeScript(TeletypeProgramT<Cfg> &program, int scriptIndex, Tt2SceneRead read, void *ctx);
