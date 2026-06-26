# TT2 Cross-Track Ops — poke another track's tt-patterns + scripts

**Goal:** Two new cross-track Teletype ops so any TT2-family track (full **or** Mini) can reach into another TT2-family track's data — the parity-with-`WNG`/`WNN` the NoteTrack ops already have, but for TT2 targets:

- **`WPN <track> <bank> <idx> [<v>]`** — get/set cell `idx` of pattern `bank` on track `track` (the target's *active* program — for a Mini target, its active scene). The cross-track form of same-track `PN bank idx [v]`, with **identical bank/idx normalisation** (see below) so it's a true `PN` parity op.
- **`W.SCRIPT <track> <n>`** — trigger script `n` on track `track`. Range matches native `SCRIPT` = `1..Cfg::ScriptCount` (**Full 1–10** = 8 trigger + Metro 9 + Init 10; **Mini 1–3** = 2 trigger + Metro), out-of-range sets `OutOfRange` like `SCRIPT`.

Both work in every direction (full↔full, full↔Mini, Mini↔Mini) because the op functions are `template<Cfg>` and instantiate for both configs automatically. (Op names are proposals — they follow the `W*` "reach another track" family; adjust if you prefer `W.PN`/`W.SCR`.)

> **Path convention for this spec:** `engine/…` and `model/…` citations are under `src/apps/sequencer/` (e.g. `engine/TT2Host.h` = `src/apps/sequencer/engine/TT2Host.h`). `teletype/…` citations are repo-root-relative.

## How the existing cross-track bridge works (the pattern to mirror)

TT2 ops reach other tracks through the **`TT2Host` virtual interface** (`engine/TT2Host.h:13`), implemented **separately** by `TT2TrackEngine` and `TT2MiniTrackEngine` (no shared base — each overrides the full vtable). An op gets the active engine via `tt2ActiveHost()` and calls a `host*` method with a 1-based track arg.

- **`opWng`** (`engine/TeletypeNativeOps.cpp:2021`) is the template: pop `track`, `step` (and `v` when `isSet`), null-guard the host, branch get/set, `t-1` for the 1-based→0-based track. `hostNoteGateGet` (`TT2TrackEngine.cpp:219`) reaches the model via `tt2NoteSequence(model, track)`.
- **`tt2NoteSequence`** (`TT2TrackEngine.cpp:19`) selects the target's **currently-selected w-pattern**: `playState().trackState(track).pattern()` → `noteTrack().sequence(patternIndex)`. The new pattern op mirrors this selection (active program, not a fixed one).
- The same logic is **duplicated** (identical body, different name `tt2MiniNoteSequence`) in `TT2MiniTrackEngine.cpp:13` (the two `.cpp`s don't share statics). The new work should put the shared cross-track logic in a header helper to avoid a third copy (see Work item 1).

## New host methods (the model-reaching layer)

Add to `TT2Host` (`engine/TT2Host.h`, pure virtual) and override in **both** engines. To avoid duplicating the body in two `.cpp`s, put the real logic in **free helpers in a shared header** (e.g. `engine/TT2HostCrossTrack.h`) that each override calls in one line — mirroring `tt2NoteSequence` but written once.

```cpp
// bank/idx are SIGNED — normalised exactly like PN (clamp bank, normalise idx); 0 if not a TT2 track
int16_t       hostTrackPatternVal(uint8_t track, int16_t bank, int16_t idx);
void          hostSetTrackPatternVal(uint8_t track, int16_t bank, int16_t idx, int16_t v);
// returns the error (None / OutOfRange / ExecDepthOverflow) so the op can report it
TT2EvalError  hostTriggerTrackScript(uint8_t track, int16_t script);
```

That's **3 host methods**, each at 5 sites (1 `TT2Host.h` pure-virtual decl + 2 engine `.h` override decls + 2 engine `.cpp` one-line defs forwarding to the shared helper). (My earlier "~2" estimate undercounted — pattern needs get+set, script needs trigger.)

### Cross-track program access (the shared helper logic)

Reach another track's TT2 program by index, branching on its mode (the two program types are distinct templates, but `patterns[b].val[i]` is `int16_t` in both — so one `int16_t` signature covers both):

```cpp
// pseudo — in TT2HostCrossTrack.h. bank/idx SIGNED; returns the cell ptr after PN-style
// normalisation, or nullptr if the target isn't a TT2-family track (op then reads/writes nothing).
static inline int16_t* tt2CrossPatternCell(Model &model, int t, int16_t bank, int16_t idx) {
    if (t < 0 || t >= CONFIG_TRACK_COUNT) return nullptr;
    Track &tk = model.project().track(t);
    if (tk.trackMode() == Track::TrackMode::TeletypeV2)
        return tt2PatternCell<TT2ConfigFull>(tk.tt2Track().program(), bank, idx);   // see below
    if (tk.trackMode() == Track::TrackMode::TeletypeMini) {
        int scene = model.project().playState().trackState(t).pattern() % TT2ConfigMini::SceneCount;
        return tt2PatternCell<TT2ConfigMini>(tk.tt2MiniTrack().program(scene), bank, idx); // active scene
    }
    return nullptr;   // not a TT2-family track — safe no-op
}
```
- **Bank/idx normalisation must match same-track `PN` exactly** (this is the parity claim). `PN` clamps the bank via `normalisePn<Cfg>` (`TeletypeNativeOps.cpp:3287`: `<0→0`, `>=PatternCount→PatternCount-1`) and reads/writes through `patternGetVal`/`patternSetVal` (`:3385/3387`), which normalise the index. The cross-track `tt2PatternCell<Cfg>(program, bank, idx)` helper must reuse that same normalisation — **do NOT cast to `uint8_t` or hard-reject out-of-range** (that would break `PN`'s negative-index/clamp semantics). Easiest: refactor `normalisePn` + the `patternGetVal`/`patternSetVal` index logic into a shared header both the ops TU and the cross-track helper include (they're currently `static` in the ops TU), so WPN and PN are guaranteed identical; or replicate the exact clamp/normalise in `tt2PatternCell`.
- Mini's active scene is recomputed from `playState().trackState(t).pattern() % SceneCount` — the **same formula** the Mini engine uses (`tt2SceneIndex`, `TT2MiniTrackEngine.h:20,48`). Do NOT reach the other engine's private `_activeScene`.
- Both configs share `TT2_PATTERN_COUNT == 4` (`model/TeletypeProgram.h:20`/:34) and `PatternLength == 64`.

### Script-trigger reaches the other engine

`_engine.trackEngine(t)` returns a base `TrackEngine&` (`Engine.h:195`). **Do NOT call `runScript` bare** — the triggered script's own `W*`/`BUS`/`MO` ops resolve against `tt2ActiveHost()`, which is still the *caller's* engine. The target's script must run with the **target** installed as active host. `ScopedHost` is private (`TT2TrackEngine.h:196`), so add a small **public** method on each engine that wraps its own `ScopedHost` (a host-pointer swap only) — mirroring the update-path firing at `TT2TrackEngine.cpp:359`:
```cpp
// public, on TT2TrackEngine + TT2MiniTrackEngine — returns the eval error so the op can report it
TT2EvalError triggerScriptFromHost(int16_t oneBased) {
    int idx = oneBased - 1;
    if (idx < 0 || idx >= Cfg::ScriptCount) return TT2EvalError::OutOfRange;   // matches SCRIPT range
    ScopedHost host(this);                                                     // run with THIS track as active host
    return runScript(_track.program(...), _runtime, _output, uint8_t(idx)).error;  // ExecDepthOverflow propagates
}
```
`hostTriggerTrackScript` checks the global cross-track cap, then branches + downcasts (the `as<>()` pattern is already used, e.g. `TT2TrackEngine.cpp:243`) and returns the error:
```cpp
if (crossDepth >= TT2_CROSS_DEPTH) return TT2EvalError::ExecDepthOverflow;   // global cap (see Recursion)
auto m = model.project().track(t).trackMode();
if (m == TeletypeV2)        return engine.trackEngine(t).as<TT2TrackEngine>().triggerScriptFromHost(n);
if (m == TeletypeMini)      return engine.trackEngine(t).as<TT2MiniTrackEngine>().triggerScriptFromHost(n);
return TT2EvalError::None;   // non-TT2 target: silent no-op
```
(`n` stays 1-based into `triggerScriptFromHost`, which does the `-1` + range check itself — single source of truth for the range.)

> **Locking — RESOLVED, no deferral (corrects the earlier worry).** `ScopedHost` does **NOT** take `Engine::lock()` — it only swaps the active-host pointer (`tt2SetActiveHost`, `TT2TrackEngine.h:196-199`). `Engine::lock()` is held only by the UI-thread entry points (`runLiveCommand`/`triggerScript`, `TT2TrackEngine.cpp:126,140`), never on the op-execution path (which already runs inside `Engine::update()`). So a cross-track trigger fired from inside an op touches no lock → no deadlock → **inline trigger is feasible, no deferral.** (`Engine::lock()` is a non-reentrant busy-spin at `Engine.cpp:326`, but it is simply not in play here.) The real requirement is the **active-host swap** above, which a bare `runScript` would miss.
>
> **Recursion — already bounded by the existing guard; add one global cap for stack safety.** `W.SCRIPT` goes through the same `runScript`, which increments the *target's* per-runtime `exec.depth` (`TT2_EXEC_DEPTH = 8`, `model/TeletypeRuntime.h:16`). Re-entering a track accumulates its own depth (the prior invocation is still on the C++ stack), so each track still caps at 8 → A↔B ping-pong is **bounded, not infinite**. The only residual is the *aggregate* C++ stack depth across a cycle of N distinct tracks (≈ N×8), which can exceed what the single-track depth-8 budget assumed. **Decision: reuse the per-runtime guard AND add a small engine-level counter `crossDepth`** (cap `TT2_CROSS_DEPTH` ≈ 4): bump it before the cross-track call, restore after; past the cap `hostTriggerTrackScript` returns `ExecDepthOverflow` (the op propagates it via its `error` param). Caps aggregate stack depth regardless of track count — inline, no deferral. `WPN` (data read/write) has no recursion at all.

## The two op functions (`engine/TeletypeNativeOps.cpp`, templated)

Mirror `opWng`/`opWp` exactly:
```cpp
template<typename Cfg>
static void opWpn(TT2RuntimeT<Cfg>&, TT2OutputState&, const TeletypeProgramT<Cfg>*,
                  int16_t *stack, uint8_t &stackSize, bool isSet, TT2EvalError &error) {
    int16_t t=0,b=0,i=0;
    if (!popStack(stack,stackSize,t,error)) return;
    if (!popStack(stack,stackSize,b,error)) return;
    if (!popStack(stack,stackSize,i,error)) return;
    TT2Host *h = tt2ActiveHost();
    if (isSet && stackSize >= 1) {
        int16_t v=0; if (!popStack(stack,stackSize,v,error)) return;
        if (h) h->hostSetTrackPatternVal(uint8_t(t-1), b, i, v);   // b/i SIGNED → PN normalisation
    } else {
        pushStack(stack,stackSize, h ? h->hostTrackPatternVal(uint8_t(t-1), b, i) : 0, error);
    }
}
template<typename Cfg>
static void opWScript(TT2RuntimeT<Cfg>&, TT2OutputState&, const TeletypeProgramT<Cfg>*,
                      int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t t=0,n=0;
    if (!popStack(stack,stackSize,t,error)) return;
    if (!popStack(stack,stackSize,n,error)) return;
    if (TT2Host *h = tt2ActiveHost()) error = h->hostTriggerTrackScript(uint8_t(t-1), n);  // propagate OutOfRange/ExecDepthOverflow
}
```

## Op-registration — through the teletype/ generator tooling (do NOT hand-edit generated files)

> **Paths:** `engine/…` and `model/…` below are under `src/apps/sequencer/`; `teletype/…` are repo-root-relative.

`teletype/src/match_token.rl` (op text → enum) is the **source of truth**. The lexer (`match_token.c`) and the name table (`TT2OpNames.cpp`) are **generated** from it — never edit them by hand: `match_token.c` is **gitignored** (`teletype/.gitignore:48`) and gets clobbered on the next worktree bootstrap. The flow, mirroring how `WNG` exists:

1. **Enum** (hand-edited, tracked) — add `E_OP_WPN` and `E_OP_W_SCRIPT` to `tele_op_idx_t` in `teletype/src/ops/op_enum.h`, before the `E_OP__LENGTH` sentinel (`:441`; `W*` block at `:179-186`). **No `_SET` variant** — `opWpn` is a single fused get/set op like `opWng` (there is no `E_OP_WNG_SET`; `isSet` is set positionally at dispatch, `TT2Evaluator.h:127`).
2. **Token rules** (hand-edited, tracked) — add spellings to `teletype/src/match_token.rl` (`W*` at `:206-213`): `"WPN" => { MATCH_OP(E_OP_WPN); };`, `"W.SCRIPT" => { MATCH_OP(E_OP_W_SCRIPT); };`. No ordering hazard — `match_token.rl` is a Ragel *scanner* (longest-match is native, so `WPN` beats `WP` regardless of order; grep confirms no collision with existing tokens).
3. **Regenerate via the tooling (the canonical path — the only correct way):**
   - `( cd teletype/src && ragel -C -G2 match_token.rl -o match_token.c )` — regenerates the lexer (gitignored, per `PROJECT.md:407-410`; `ragel` is on PATH). **Never hand-edit `match_token.c`.**
   - `python3 teletype/utils/tt2_op_names.py` — regenerates `src/apps/sequencer/engine/TT2OpNames.cpp` (enum→name) from the `.rl`. Run **after** the `.rl` edit. **Never hand-add the `case`.**
   - *(optional)* `python3 teletype/utils/docs.py` / `cheatsheet.py` to include the op in generated docs.
4. **Op vtable** (hand-edited, tracked) — register the op functions in `src/apps/sequencer/engine/TeletypeNativeOps.cpp` (the `table[E_OP_*] = op…<Cfg>;` block, `:4474-4481`): `table[E_OP_WPN] = opWpn<Cfg>; table[E_OP_W_SCRIPT] = opWScript<Cfg>;`. **Silent-no-op trap** — enum + name resolve but a null slot makes the op do nothing.

Hand-edited files: `op_enum.h`, `match_token.rl`, `TeletypeNativeOps.cpp` (+ the op fns + host methods). Generated (by the tools, never by hand): `match_token.c`, `TT2OpNames.cpp`.

## Tests

- **`TestTeletypeV2ParserContract.cpp`** — add `expectToken("WPN", E_OP_WPN, "...")` and `expectToken("W.SCRIPT", E_OP_W_SCRIPT, ...)` round-trips (catches a broken tokenizer entry, trap #1).
- **`TestTT2OpNames.cpp`** — iterates `0..E_OP__LENGTH` asserting every enum has a name; passes automatically once step 3 is done (catches a missing name).
- **New functional test** (host level — the engines aren't host-constructible, so test the helper + op via a stub host, mirroring how cross-track is reachable): exercise `opWpn` get/set through a fake `TT2Host` (or the shared `tt2CrossPatternCell` helper directly against a `Model` with a Mini track) — assert a full track writing `WPN <miniTrack> 0 5 42` lands in the Mini track's active-scene `patterns[0].val[5]`, and reading it back. For `W.SCRIPT`, a helper-level test that the right target `runScript` index is selected per `trackMode` (the inline-run itself is sim/manual given the engine isn't host-constructible).

## Budget

Negligible. Per config: +2–3 op-table pointer slots (`E_OP_*` entries) in CCMRAM × 2 configs (~a few dozen bytes), + the op-function bodies in `.text` (small, ×2 instantiations). No struct/runtime growth. The host methods add no per-track state.

## Work order

1. **Shared cross-track helper** (`src/apps/sequencer/engine/TT2HostCrossTrack.h`): `tt2CrossPatternCell(model, t, bank, idx) -> int16_t*` + a script-target resolver. Unit-test the pattern helper directly (host-constructible — it takes `Model&`).
2. **`WPN` first** (low-risk, data only): enum + token rules + regen (ragel + python) + vtable + `opWpn` + 2 host methods (get/set) forwarding to the helper + parser-contract test + functional test. Ship.
3. **`W.SCRIPT` plumbing**: public `triggerScriptFromHost` (target-`ScopedHost` swap) on each engine + the small global cross-track-nesting cap in `hostTriggerTrackScript`. (The per-runtime depth-8 guard already bounds it; the lock is not in play.)
4. **`W.SCRIPT`**: enum + token rules + regen + vtable + `opWScript` + `hostTriggerTrackScript` (1 host method) per the chosen recursion strategy + tests.

## Deferred (propose only)
- Cross-track variable peek/poke (`W.A`-style) — same bridge, another host method pair.
- A dedicated `WPN.SET` explicit-set form (the fused `isSet` op covers it; add only if the parser ergonomics need it).
