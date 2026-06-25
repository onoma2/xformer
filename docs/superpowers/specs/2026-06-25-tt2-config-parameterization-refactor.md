# TT2 Config Parameterization Refactor — Spec

**Status:** spec, pre-implementation. Foundation for the Teletype Mini scene-track (`docs/plans/2026-06-25-teletype-mini-scene-track.md`), independently shippable.

## Goal

Every Teletype-V2 size/index/type currently baked into the infra is sourced from one place, so a second struct configuration can exist later. The existing full TT2 track is **behavior-identical and byte-identical** after this refactor — no `TrackMode`, no UI, no behavior changes ship here.

## Scope

In scope: the TT2 model structs, the interpreter (runner/evaluator/ops), serializer/printer/loader, and the UI type-coupling seam **for the script and IO pages only** — converted to be config-parameterized, instantiated once (`Full`), behavior-preserving. The pattern page is explicitly NOT touched (see Work item 5).

**Out of scope (belongs to the Mini-add plan, not this refactor):** the `TT2MiniTrack` model, a `TeletypeMini` `TrackMode`, the ~20 `switch(trackMode)` dispatch sites, `FileManager` overloads, `gTeletypeLoadScratch` duplication, output routing. None of those contain hardcoded *counts* — they are mode-dispatch and only matter once a new mode exists. This refactor adds no new mode.

## The config model

A traits struct carries **only the values that will differ between configurations**. Eight (the original six + the two pattern dimensions, added by the pattern de-hardcode follow-up — see `docs/plans/2026-06-25-tt2-pattern-dehardcode-plan.md`):

```cpp
// model/TT2Config.h
struct TT2ConfigFull {
    static constexpr int ScriptCount      = 10;   // 8 numbered + metro + init
    static constexpr int DelayDepth       = 64;
    static constexpr int TriggerInputCount = 8;
    static constexpr int MetroScript      = 8;
    static constexpr int InitScript       = 9;
    static constexpr int SceneCount       = 1;
    static constexpr int PatternCount     = 4;    // tt-patterns (TT2PatternT)
    static constexpr int PatternLength    = 64;
};
```

Everything else is identical across configurations and **stays a plain shared constant** (no Cfg member, untouched at its use sites): `TT2_COMMANDS_PER_SCRIPT=6`, `TT2_COMMAND_MAX_LENGTH=16`, `TT2_STACK_DEPTH=16`, `TT2_EXEC_DEPTH=8`, `TT2_Q_LENGTH=64`, `TT2_CV_INPUT_COUNT=6`, `TT2_CV_OUTPUT_COUNT=8`, `TT2_CV_COUNT=8`, `TT2_TR_COUNT=8`, `TT2_RNG_COUNT=5`, `TT2_NB_SCALES=16`, `TT2_PRINT_SLOT_COUNT=16`, `TT2_MIDI_EVENTS=10`, `TT2_VARIABLE_COUNT=20`, plus `TT2_OUTPUT_CV_COUNT=8` / `TT2_OUTPUT_TR_COUNT=8` (`TeletypeOutputState.h:6-7` — fixed by hardware: 8 CV + 8 gate jacks; `TeletypeOutputState` and `TT2OutputState` are not templated).

**Metro/init stay explicit Cfg members, not derived** (`count-2`/`count-1`). Full is metro=8/init=9/count=10; Mini will be metro=2/init=−1/count=3 — the derivation does not survive a config with no init, so explicit is correct.

**Language:** C++11 (`src/CMakeLists.txt:22`). `tt2OpTable<Cfg>()` is a *function* template (not a variable template — those are C++14). No `if constexpr`, `auto`-return, or generic lambdas.

## Work item 1 — constant→Cfg substitution (the six varying values)

Replace every use of the six varying constants with the `Cfg::` member. **The authoritative site list is the grep, not a hand-enumeration** (hand lists drift — they already did):

```
grep -rn 'TT2_SCRIPT_COUNT\|TT2_DELAY_DEPTH\|TT2_TRIGGER_INPUT_COUNT\|TT2_TRIGGER_INPUTS\|TT2_METRO_SCRIPT\|TT2_INIT_SCRIPT\|TT2_PATTERN_COUNT\|TT2_PATTERN_LENGTH' src/apps/sequencer/{engine,model,ui}
```

Current totals as a sanity figure (verified): `TT2_SCRIPT_COUNT` **33**, `TT2_DELAY_DEPTH` 4 (1 def + 3 uses), `TT2_TRIGGER_INPUT_COUNT`/`TT2_TRIGGER_INPUTS` ~19, `TT2_METRO_SCRIPT` ~12, `TT2_INIT_SCRIPT` ~6. Every hit converts; the static_asserts in `TeletypeProgram.h`/`TeletypeRuntime.h` are the only definitional sites that stay. Collapse the `TT2_TRIGGER_INPUTS` alias into the one `Cfg::TriggerInputCount`.

**High-risk clusters the first audit under-counted — call these out so they are not missed (each is `Cfg::ScriptCount`/`Cfg::DelayDepth` once the op/struct is `template<Cfg>`):**
- `TeletypeNativeOps.cpp` script-count sites (≥10, not 1): opJ/opK bounds (`:115,:128`), SCRIPT-arg bound (`:225`), the per-script EVERY/S.ALL loop (`:1026`), turtle script-number clamp (`:1290,:1294`), `tt2ClearScriptN` bound (`:1777`), SCRIPT.CLR all-clear loop (`:1789`), LAST reset loop (`:1799`), the SCRIPT op bound (`:2529`). On the Full alias these would silently keep Full bounds for a Mini track's re-entrant/runtime behavior.
- `TeletypeRuntime.h` inline delay helpers `tt2DelayAdd` (`:395`) and `tt2DelayClear` (`:412`) index `runtime.delay.entries` over `TT2_DELAY_DEPTH` directly — they MUST become function templates with the rest of the runtime, or templating the struct without them is a compile break / Full-only bound.
- **Script-polarity state is trigger-count-sized:** `script_pol[TT2_TRIGGER_INPUTS]` (`TeletypeRuntime.h:60`), its init loop (`:288`), and the SCRIPT.POL op (`TeletypeNativeOps.cpp:2394,2395,2400`) all bound on `TT2_TRIGGER_INPUTS` → `Cfg::TriggerInputCount`. Grep catches them, but name them — easy to miss and Full-only builds hide it.
- **`Project.cpp` sim demo seed** (`:37,:44`) calls `loadScriptText(p, TT2_METRO_SCRIPT, …)` / `TT2_INIT_SCRIPT` — `Project.cpp` is in the grep scope (`model/`). It seeds a Full TT2 track, so convert to `TT2ConfigFull::MetroScript`/`InitScript` (not excluded — the acceptance gate wants zero un-converted varying-constant hits).

The 16 *fixed* constants keep their existing `TT2_*` references unchanged — do NOT route them through Cfg.

## Work item 2 — bare-literal cleanup (no symbol today)

- **Metro/init markers in the serializer** `TT2SceneSerializer.cpp:55-56,129-131` compare/emit script indices — confirm they read `Cfg::MetroScript`/`Cfg::InitScript`, not literal `8`/`9`.
- **Known-deferred literal:** `TT2SceneSerializer.cpp:131` `t[0] >= '1' && t[0] <= '8'` hardcodes "8 numbered scripts" (= `ScriptCount − 2`) as a char-range bound. The serializer becomes `template<Cfg>` (WI3) but this char range is text-format parsing; for Full it's correct and this refactor is Full-only. Flag it: it must derive from `Cfg` when Mini's scene text format is defined (Mini plan), not here. This is the one bare literal acceptance #2 explicitly exempts.
- **`TT2TrackEngine.cpp:114-115`** — `int16_t *target[4]` + `for (k<4)` samples the X/Y/Z/T working-var inputs. This is a *fixed* 4-element subset (X,Y,Z,T), NOT a varying count and NOT `TT2_CV_INPUT_COUNT`(6). Leave the behavior; optionally name a `TT2_XYZT_INPUT_COUNT=4` constant for readability. Explicitly flag it so it is not "fixed" by mistake.
- Confirm no other literal `10/64/9/8/2` is used as script-count / delay-depth / metro / init / trigger-count. The acceptance grep (below) enforces this.

## Work item 3 — type parameterization (the structs + interpreter)

The varying counts are array dimensions, so the structs become templates and everything touching them follows.

- `TeletypeProgramT<Cfg>`, `TT2ScriptT<Cfg>`, `TT2RuntimeT<Cfg>` (and the `*T` sub-structs whose arrays use the six varying counts). Sub-structs whose arrays use only fixed constants stay non-templated.
- `init(...)` and the inline runtime helpers become **free function templates in the global namespace** (ADL must resolve `init(prog)`).
- Aliases preserve every existing consumer: `using TeletypeProgram = TeletypeProgramT<TT2ConfigFull>; using TT2Runtime = TT2RuntimeT<TT2ConfigFull>; using TT2Script = TT2ScriptT<TT2ConfigFull>;`. The legacy `TT2_*` *varying* constants are **kept permanently** as `= TT2ConfigFull::X` compatibility aliases (single-source — they alias the Cfg member, not an independent literal). 7 test files (`TestTeletypeV2{ScriptRunner,Delay,Metro,NativeOps,TriggerInput,Function}.cpp`, `TestTT2SceneSerializer.cpp`) use `TT2_SCRIPT_COUNT`/`TT2_DELAY_DEPTH`/`TT2_METRO_SCRIPT`/`TT2_INIT_SCRIPT`/`TT2_TRIGGER_INPUT_COUNT` directly — keeping the aliases is what makes "tests compile unchanged" true. The grep gate forbids these aliases inside the **templated infra** (which must use `Cfg::`), not their definitions or test/Full-only use.
- `TT2Runner.h` (runScript, tt2AdvanceDelays, tt2AdvanceMetro, tt2RunFunction*, runStoredCommand), `TT2Evaluator.h`, `TT2Printer.{h,cpp}`, `TT2SceneSerializer.{h,cpp}`, `TT2ScriptLoader.h` become `template<typename Cfg>`. The two `.cpp`s (serializer, printer) need explicit `<TT2ConfigFull>` instantiation at file end (out-of-line defs don't auto-instantiate).
- **Every `static`/anonymous-namespace function in `TeletypeNativeOps.cpp` whose signature names a templated alias** (`TT2Runtime`, `TeletypeProgram`, `TT2Pattern`, `TT2Script`) becomes `template<typename Cfg>` — not only the `op*` handlers. The non-op helper cluster MUST template alongside them: `tt2PushRandom` (`:232`), the turtle helpers (`:1177,:1184`), `tt2InitPatternN`/`tt2ClearScriptN` (`:1742,:1776`), `tt2MiIndex` (`:2235`), `runStoredCommand` (`:3020`), `mutablePattern` (`:3076`), and the whole `pattern*` family (`:3107–3840`). Under the Full alias these compile whether templated or not, so a Full-only build hides a miss — but Mini needs every one. The `TT2_SIMPLE_VAR_OP` macro emits a template. Body `Cfg::` edits are exactly the Work-item-1 grep hits inside the file (≥10 `TT2_SCRIPT_COUNT` sites + metro/init); everything else is signature-only. Scalar-op bodies and musical constants (`prime[6]`, `jiConst[6]`, scale/exp tables) are untouched.
- **Acceptance grep for the helper cluster:** `grep -n 'static.*\(TT2Runtime\|TeletypeProgram\|TT2Pattern\|TT2Script\)' src/apps/sequencer/engine/TeletypeNativeOps.cpp` — every hit must be a `template<typename Cfg>` (resolving the alias to `*T<Cfg>`) before Mini instantiates.

## Work item 4 — the op-table trait (re-entrant safety)

The op table must be selected by `Cfg`, reachable from re-entrant ops, with no global the path can silently fall back to.

```cpp
template<typename Cfg> using TT2OpFuncT = void (*)(TT2RuntimeT<Cfg>&, TT2OutputState&,
                            const TeletypeProgramT<Cfg>*, int16_t*, uint8_t&, bool, TT2EvalError&);
using TT2OpFunc = TT2OpFuncT<TT2ConfigFull>;                 // keeps TestTeletypeV2Evaluator + extern decl compiling
template<typename Cfg> const TT2OpFuncT<Cfg>* tt2OpTable();   // primary declared in evaluator header
template<> const TT2OpFuncT<TT2ConfigFull>* tt2OpTable<TT2ConfigFull>();  // BLOCKER B1: the explicit-specialization DECLARATION must also be in the header
```

- **B1 — the explicit-specialization *declaration* (not just the .cpp definition) goes in `TT2Evaluator.h`.** Per [temp.expl.spec], a function-template specialization must be declared before its first use in *every* TU that implicitly instantiates it, or the program is ill-formed (no diagnostic required). The `TestTeletypeV2*` TUs call `runScript`/`evaluateCommand` → instantiate `tt2OpTable<TT2ConfigFull>()` from the header; if they see only the primary template they may bind a wrong implicit instantiation or fail to link. (This is NOT the same as the existing `extern const TT2OpFunc* tt2NativeOpTable` plain-variable export — that mechanism is unaffected; the trap is specific to the function-template specialization.)
- `template<typename Cfg> struct OpTableBuilderT { TT2OpFuncT<Cfg> table[E_OP__LENGTH]; ... };`, `CCMRAM_BSS OpTableBuilderT<TT2ConfigFull> opTableBuilderFull;`, and the *definition* `template<> const TT2OpFuncT<TT2ConfigFull>* tt2OpTable<TT2ConfigFull>() { return opTableBuilderFull.table; }` in the ops `.cpp`.
- Keep legacy globals `tt2NativeOpTable = opTableBuilderFull.table` / `tt2NativeOpCount` so existing consumers and `TestTeletypeV2Evaluator.cpp`'s `fakeOpTable` (typed `TT2OpFunc`) compile.
- `runScript<Cfg>`/`evaluateCommand<Cfg>` resolve their table from `tt2OpTable<Cfg>()`; `evaluateCommand` keeps the table as a defaulted param so the fake-ops test can still inject one explicitly. **`Cfg` must deduce from the `runtime` argument** (`TT2RuntimeT<Cfg>&`), not from `program` — `TestTeletypeV2Evaluator`'s `evaluateWithFakeOps` passes no program (nullptr). The re-entrant ops (`opScript:2537`, `opF*:2561/2571/2582`, `opL*:2592–2645`, `opSAll/opSPop`→`runStoredCommand:3028`) call `runScript<Cfg>`/`evaluateCommand<Cfg>` with no table → they inherit the `Cfg` table automatically.

## Work item 5 — UI type-coupling seam (behavior-preserving, Full-only)

The script and IO pages bind concrete `tt2Track()` / `as<TT2TrackEngine>()` at ~90 sites (script page 15+9, IO page 2). Concentrate that coupling behind two seams so a later variant adds one branch per accessor, not 90. **`TeletypePatternViewPage` (7+ concrete `tt2Track()` sites) is OUT of scope for this refactor — it stays entirely Full-concrete and untouched.** The Mini plan blocks Mini from the pattern page in v1, so it never needs a seam; revisit only if Mini gains pattern editing later. Do not claim the seam covers the pattern page. **`TT2Command` is non-templated** (`TeletypeProgram.h:35-40`) — the command editor and `_undoCommand` (`TeletypeScriptViewPage.h:78`) keep using it unchanged; only the *container navigation* is type-specific.

- **Seam 1 — program access (model side).** Free accessors that return shared types/scalars: **`tt2ScriptCount(track)`** and **`tt2TriggerInputCount(track)`** (the count seam — the page must navigate/iterate via these, NOT `TT2_SCRIPT_COUNT`/`TT2_TRIGGER_INPUT_COUNT`, or a Mini editor walks to non-existent scripts/rows), `tt2ScriptCommand(track, scriptIdx, lineIdx) → TT2Command*`, `tt2ScriptLength/​setScriptLength`, plus the IO-grid getters/setters over `cvOutputRange/Offset/QuantizeScale/RootNote`, `triggerSource[]`, `cvInputSource[]`. Route the script-nav + edit sites (`TeletypeScriptViewPage.cpp:87,628,639,797,812,835,1091`), the HUD trigger loop (`:254`), and IO sites (`TT2IoConfigPage.cpp:102,115,165,223` + field access) through these. CV-in (6) and CV-out (8) counts are fixed — the grid keeps its plain constants. **Only the loop *count* routes through the seam; the HUD trigger-grid *geometry* (the `(i % 4)` / `(i / 4) * 7` 2×4 layout math tied to 8 inputs) stays Full-tuned — re-laying it out for 2 inputs is the Mini plan's concern, not this refactor.** The count seam alone does not make the HUD Mini-ready.
- **Seam 2 — engine control (UI→engine), input actions only.** A fixed interface of three input verbs both engine types implement: `triggerScript(int)`, `toggleScriptMute(int)`, `toggleMetroActive()`. The page calls these instead of `as<TT2TrackEngine>()` at the action sites `:396,420,909,917,925,968,975`. **`drawHud` is NOT in the seam and stays Full-concrete this refactor** — it reads rich TT2-specific state (`trackEngine.output()` at `:110,:234` returning `TT2OutputState&`, and `inputState(i)` at `:259`) that isn't worth abstracting for a Full-only pass. So `:231/:233` keep their concrete `tt2Track()`/`as<TT2TrackEngine>()`; making the HUD variant-aware is the Mini plan's concern (alongside the HUD grid geometry, deferred the same way). **RAM constraint (`.ccmram_bss` must not grow):** add the three verbs as **non-pure virtuals with default bodies on the existing `TrackEngine` base** (it already has a vtable; defaults are no-ops so the other 8 engines need ZERO new code — required by the non-goals), TT2TrackEngine overriding. No new vptr. Do NOT introduce a separate interface class multiply-inherited onto the engine — that adds a vptr and grows `sizeof`. Acceptance: `sizeof(TT2TrackEngine)`, every other engine's `sizeof`, and the engine `Container` size are unchanged.
- This refactor implements both seams and **routes the existing Full page through them with no behavior change** (Full TT2 still the only mode). It is the independently-testable step the Mini plan's Task 15 sits on. Live-exec (`:108,736`) and SD I/O (`:1189,1206`) stay on the concrete Full path — not part of the seam.

## Non-goals (explicit)

No `TeletypeMini` `TrackMode`; no `Track`/`ClipBoard`/`TopPage`/`OverviewPage`/`LaunchpadController`/`Engine` dispatch-case edits; no `FileManager` overloads; no `gTeletypeLoadScratch` change; no output-routing change; no second config instantiation. Those all live in the Mini-add plan. This refactor ships Full-only.

## Invariants / acceptance

1. **Behavior-identical Full:** full `TestTeletypeV2*` suite green; `sizeof(TeletypeProgram)`, `sizeof(TT2Runtime)`, `sizeof(TT2Track)`, **`sizeof(TT2TrackEngine)`, and the engine `Container` size** unchanged (the Seam-2 interface adds no vptr/storage); ARM release build (`make -C build/stm32/release sequencer`) `.data`/`.bss`/`.ccmram_bss` sizes unchanged (one config, one op table).
2. **Single source of truth:** the Work-item-1 grep returns zero `TT2_*` varying-constant hits **inside the templated infra** other than the alias definitions themselves and the `static_assert` definitional sites (the permanent compat aliases and tests are expected hits, not failures), and no bare literal `10`/`64`/`9`/`8`/`2` is used as script-count / delay-depth / metro-index / init-index / trigger-count anywhere in the **templated** TT2 infra (incl. the NativeOps re-entrant sites, the runtime inline delay helpers `:395/:412`, and the UI script/trigger nav). Each flows from `Cfg::` or its named fixed constant. One explicit exemption: the serializer char-range literal at `TT2SceneSerializer.cpp:131` (Full-only text format, deferred to the Mini plan). The helper-cluster grep (WI3) also returns zero un-templated `static` helpers naming the aliases.
3. **Tests compile unchanged:** the full `TestTeletypeV2*`/`TestTT2*` suite (which constructs `TeletypeProgram`/`TT2Runtime`/`TT2OpFunc`/`fakeOpTable` directly) compiles with ZERO edits — the `using` aliases preserve the type names and the free functions deduce `Cfg=Full` from their `runtime`/`program` arguments. This is the real compile gate; the WI1 grep scope (`engine,model,ui`) deliberately excludes `tests/` because tests touch only surviving aliases. If a test needs editing, an alias was dropped too early.
4. **Op-table identity:** `tt2OpTable<TT2ConfigFull>()` and the legacy `tt2NativeOpTable` point at the same table; the explicit-specialization declaration is in the header (B1); `TestTeletypeV2Evaluator` compiles and passes with its `fakeOpTable`.
5. **UI parity:** the Full script/IO pages behave identically after routing through Seam 1/2 (manual check + `ui-preview` where layout is touched).
6. **No new mode reachable:** `TrackMode` enum unchanged; no dispatch site altered.

## Notes
- No serialization versioning / migration (dev files break freely).
- After this lands, the Mini-add plan reduces to: `TT2ConfigMini` + Mini instantiations + `TT2MiniTrack` model + `TeletypeMini` mode + dispatch cases + Mini engine + the per-accessor Mini branch.
