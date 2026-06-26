# TT Shared Variable Bank (`TV i`)

**Goal:** A bank of 16 `int16` variables — `TV 0` … `TV 15` — **shared across all TT-family tracks** (full TT2 + Mini), so scripts on different TT tracks can pass values to each other. Like `BUS`, but: **ints not CV, TT-only (no routing), last-writer-wins (not summed)**.

- **One indexed op.** `TV i` (read) pushes slot `i`'s value; `TV i v` (set) writes it. The slot is a numeric stack arg (**0-based, `0..15`**) — so a single op, and **computed indices work** (`TV X`, `TV ADD I 1`). **Out-of-range index is a silent no-op / pushes 0 — NO error.** This is intentionally **unlike `BUS`**, which is 1-based and returns `OutOfRange` via `popOutputIndex` (`TeletypeNativeOps.cpp:21`); `TV` skips `popOutputIndex` and lets the host's `tvGet/tvSet` bounds-check the raw 0-based index.
- **Shared:** one array on `Engine`; every TT track's `TV` op hits the same 16 slots.
- **Ephemeral** = not serialized: `TV` values are never saved to or loaded from the project file. **Session-global** = zeroed once at `Engine` construction (boot), then holds its value across script runs, frames, transport stop, **and project load**, until overwritten or reboot. Never re-seeded per frame (unlike `BUS`). (`_tv` lives on `Engine`, outside `_model`, so a project load — which overwrites `_model` — doesn't touch it; per-project isolation would need new plumbing and is deferred.)
- **Last-writer-wins:** a plain store. If two tracks write `TV 0` in one frame, the last write stands (no accumulation).

> **Path convention:** `engine/…`/`model/…` are under `src/apps/sequencer/`; `teletype/…` are repo-root-relative.

## Why this mirrors existing mechanisms (the patterns to copy)

- **`BUS` is the closest shape** — one indexed op over an `Engine` array, reached through the host. `BUS i` reads `Engine::_busCv[4]` (`engine/Engine.h:135-160`) via `hostBusCv` (`TT2TrackEngine.cpp:258` → `_engine.busCv(index)`). `TV` copies that wiring but differs in **five** ways:
  1. **`int16` store**, no volts conversion (BUS is CV/float ±5V).
  2. **No `Routing` bridge** → TT-only (BUS is system-wide *because* of `CvRoute::InputSource/OutputDest::Bus`, `Routing.h:131`).
  3. **No per-frame re-seed** — `_busCvWriters` is re-filled each frame (`Engine.cpp:110`); `_tv` is not, so it's a persistent variable, not a summed lane.
  4. **0-based** (`TV 0..15`); BUS is 1-based.
  5. **No `popOutputIndex`** — `opTv` pops the raw index; out-of-range is a **silent no-op / 0, error stays `None`** (BUS returns `OutOfRange`). Do NOT route `TV` through `popOutputIndex`.
- The op body mirrors `opBus`/`opWp` (pop the index, null-guard the host, branch get/set on `isSetPosition`).

## The store — `Engine`

Add to `Engine`:
```cpp
static constexpr int TvCount = 16;
int16_t _tv[TvCount] = {};                          // zero-init at construction; TT-only, not routed
int16_t tvGet(int i) const { return (i >= 0 && i < TvCount) ? _tv[i] : 0; }
void    tvSet(int i, int16_t v) { if (i >= 0 && i < TvCount) _tv[i] = v; }
```
- **Not re-seeded each frame** (BUS re-seeds `_busCvWriters` at `Engine.cpp:110`; `_tv` does not). No per-frame code.
- **Lifetime = session-global.** Zero-init at construction (the member initializer above) and nowhere else. Do NOT clear on transport `reset()` (`Engine.cpp:875` — must persist through stop), and there is **no project-load clear** — `readProject` overwrites `_model` but `_tv` is on `Engine`, so it survives loads (`updateTrackSetups` only recreates an engine when `trackMode()` changes, `Engine.cpp:580`). Minimal, no-new-plumbing design satisfying "ephemeral (not serialized) + persists." Per-project clear is Deferred.

## Host bridge — 2 methods

Add to `TT2Host` (`engine/TT2Host.h`, pure virtual) + override in **both** engines (no shared base — each implements the vtable):
```cpp
int16_t hostTvGet(uint8_t slot);
void    hostTvSet(uint8_t slot, int16_t v);
```
Engine overrides are one-liners forwarding to the engine (mirroring `hostBusCv`):
```cpp
int16_t TT2TrackEngine::hostTvGet(uint8_t slot)            { return _engine.tvGet(slot); }
void    TT2TrackEngine::hostTvSet(uint8_t slot, int16_t v) { _engine.tvSet(slot, v); }
// identical bodies in TT2MiniTrackEngine.cpp
```
(2 methods × 5 sites: 1 `TT2Host.h` decl + 2 engine `.h` override decls + 2 engine `.cpp` defs.) `_tv[slot]` is bounds-checked in `tvGet/tvSet`, so a wrapped `uint8_t(negative)` or `≥16` index safely returns 0 / no-ops.

## The op — one templated function

In `engine/TeletypeNativeOps.cpp`, mirroring `opBus`:
```cpp
template<typename Cfg>
static void opTv(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                 int16_t *stack, uint8_t &stackSize, bool isSetPosition, TT2EvalError &error) {
    int16_t i = 0;
    if (!popStack(stack, stackSize, i, error)) return;
    TT2Host *h = tt2ActiveHost();
    if (isSetPosition && stackSize >= 1) {
        int16_t v = 0; if (!popStack(stack, stackSize, v, error)) return;
        if (h) h->hostTvSet(uint8_t(i), v);
    } else {
        pushStack(stack, stackSize, h ? h->hostTvGet(uint8_t(i)) : 0, error);
    }
}
```
Null-guard `h` (null in editor/tests) like every host op. **No macro, no 16 ops** — one function. Note it pops the raw index directly (0-based), **not** via `popOutputIndex` — so out-of-range is a silent no-op (host-bounds-checked), not an `OutOfRange` error. That's the deliberate divergence from `opBus`.

## Registration — through the teletype/ tooling (do NOT hand-edit generated files)

`match_token.rl` is the source of truth; `match_token.c`/`TT2OpNames.cpp` are generated. **One entry each:**
1. **Enum** — add `E_OP_TV` to `tele_op_idx_t` in `teletype/src/ops/op_enum.h`, before `E_OP__LENGTH`.
2. **Token rule** — add `"TV" => { MATCH_OP(E_OP_TV); };` to `teletype/src/match_token.rl`. (No `TV*` token exists — grep confirms; Ragel scanner is longest-match, no ordering hazard.)
3. **Regenerate** — `( cd teletype/src && ragel -C -G2 match_token.rl -o match_token.c )` (gitignored, `PROJECT.md:407`); then `python3 teletype/utils/tt2_op_names.py` (regenerates `engine/TT2OpNames.cpp`). Never hand-edit the generated files.
4. **Vtable** — `table[E_OP_TV] = opTv<Cfg>;` in the registration block of `engine/TeletypeNativeOps.cpp` (next to `table[E_OP_BUS]` at `:4483`).

## Tests

- **Stub the 2 new pure virtuals in the existing fake hosts (compile-breaking otherwise).** `TT2Host`'s methods are pure virtual, and four tests define full fake hosts: `TestTeletypeV2Modulator.cpp` (`struct ModStubHost : TT2Host`), `TestTeletypeV2Geode.cpp`, `TestTeletypeV2EnvelopeOps.cpp`, `TestTeletypeV2LfoOps.cpp`. Add `int16_t hostTvGet(uint8_t) override { return 0; }` + `void hostTvSet(uint8_t, int16_t) override {}` to each (else they stay abstract and don't compile).
- **`TestTeletypeV2ParserContract.cpp`** — `expectToken("TV", E_OP_TV, …)` round-trip (catches a broken tokenizer entry).
- **`TestTT2OpNames.cpp`** — auto-passes once the name is regenerated (iterates `0..E_OP__LENGTH`).
- **Functional via a fake `TT2Host`** backed by a single `int16_t[16]`: `TV 0 42` writes slot 0, `TV 0` reads 42 back; **sharing** — two op invocations against the same fake host see each other's writes; **last-writer-wins** (`TV 0 5` then `TV 0 9` → reads 9); **out-of-range** (`TV 99 …`, `TV -1`) no-ops / pushes 0 **and leaves `error == None`** (assert it — this is the 0-based/no-error contract that diverges from `BUS`); `h==null` → get pushes 0. The engine-level "all tracks hit one `_tv`" is sim/manual.

## Budget + RAM acceptance gate

Expected: `Engine._tv` = **32 bytes direct RAM** (a member, per PROJECT.md's "prefer engine-member placement" — `:305`), + 1 op-table slot × 2 configs (CCMRAM) + 1 small op body × 2 instantiations (`.text`). No per-track/runtime growth.

**Required verification (PROJECT.md:307 — direct RAM still matters):** build STM32 release (`make -C build/stm32/release sequencer`) and confirm the deltas vs baseline are as expected — `.bss`/`.data` (+~32 B for `_tv` on `Engine`), `.ccmram_bss` (op-table noise only), and `sizeof(Engine)` unchanged beyond the 32 B. No region overflow.

## Deferred (propose only)
- **Per-project clear** — zero `_tv` on project load/clear (so values don't carry between projects). Needs a new `Engine` load hook (`readProject`/`resume` have none today); v1 is session-global.
- Persist `TV` with the project (the opposite — drop "ephemeral"): add serialization; not in v1.
- Named aliases `TV.A`..`TV.P` over the same slots — 16 extra op tokens if scripts want letter-named access; the indexed op covers all uses.
