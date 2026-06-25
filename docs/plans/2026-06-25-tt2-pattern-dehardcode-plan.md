# TT2 Pattern De-Hardcode — TDD Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Move `TT2_PATTERN_COUNT` (4) and `TT2_PATTERN_LENGTH` (64) into the compile-time config traits (`TT2ConfigFull`), so a future "supermini" variant can have different tt-pattern dimensions. Behavior- and byte-identical for Full/Mini (both stay 4×64).

**Architecture:** Extends the gate-clean config-parameterization refactor (`docs/superpowers/specs/2026-06-25-tt2-config-parameterization-refactor.md`), which deliberately left these two as fixed constants. Same playbook: add config members → template the one remaining fixed struct (`TT2Pattern` → `TT2PatternT<Cfg>`) → ripple `template<Cfg>` through the pattern-op family that names it → keep `= TT2ConfigFull::` compat aliases so Full-only consumers (UI pattern page, tests) compile unchanged.

**Tech Stack:** C++11 (no variable templates / `if constexpr` / `auto`-return). Host unit tests `make -C build sequencer && ctest --test-dir build`; ARM `make -C build/stm32/release sequencer`. IDE clang diagnostics (`-mthumb-interwork`, missing `size_t`) are spurious — trust only real `make`.

**TDD shape:** behavior-preserving refactor — the existing pattern-op tests + `TestTT2SceneSerializer` are the characterization suite (must stay green at every commit). New `static_assert(==)` byte-identity guards + a grep gate are the structural invariants. Only Full is instantiated; no `supermini` yet.

**Scope boundary:** No new files. **`ui/pages/TeletypePatternViewPage.cpp` is NOT touched** — its ~14 `TT2_PATTERN_LENGTH` uses + `const TT2Pattern&` (`:88`) are a Full-only consumer; the compat aliases resolve (same treatment as the script/IO pages). No `supermini` config, no behavior change.

---

## sizeof baselines (derivable — no probe needed)

`sizeof(TeletypeProgramT<TT2ConfigFull>) == 3638` (parent refactor) and `sizeof(TT2PatternT<TT2ConfigFull>) == 138` (pure POD: `idx,len,wrap,start,end` = 5×2 + `val[64]` = 128, alignment 2, zero padding — identical host/ARM, confirmed by both gate reviews). Write the `== 138` / `== 3638` guards directly; no compiler-error probe.

---

## Task 0 — add the two config members

**Files:** Modify `src/apps/sequencer/model/TT2Config.h`; Test `src/tests/unit/sequencer/TestTT2Config.cpp`.

**Step 1 — failing test** (extend `TestTT2Config.cpp`):
```cpp
expectEqual(TT2ConfigFull::PatternCount, 4, "PatternCount");
expectEqual(TT2ConfigFull::PatternLength, 64, "PatternLength");
```
**Step 2** run → fails (members don't exist).
**Step 3** add to `struct TT2ConfigFull`: `static constexpr int PatternCount = 4;` and `static constexpr int PatternLength = 64;`.
**Step 4** run → PASS.
**Step 5 commit:** `feat(tt2): add PatternCount/PatternLength to TT2ConfigFull`

---

## Task 1 — template `TT2Pattern`, alias to Full

**Files:** Modify `src/apps/sequencer/model/TeletypeProgram.h`; Test `TestTT2Config.cpp`.

**Step 1 — failing test** (extend `TestTT2Config.cpp`):
```cpp
static_assert(sizeof(TT2PatternT<TT2ConfigFull>) == 138, "");
static_assert(sizeof(TeletypeProgramT<TT2ConfigFull>) == 3638, "");   // unchanged
expect(std::is_trivially_copyable<TT2PatternT<TT2ConfigFull>>::value, "");
```
**Step 2** run → fails (`TT2PatternT` undefined).
**Step 3 — implement** in `TeletypeProgram.h`:
- `:45` `struct TT2Pattern` → `template<typename Cfg> struct TT2PatternT`; `:51` `int16_t val[Cfg::PatternLength];` (other fields unchanged).
- `:103` inside `TeletypeProgramT<Cfg>`: `TT2PatternT<Cfg> patterns[Cfg::PatternCount];`.
- `:126,:130` `init(...)` pattern loop bounds → `Cfg::PatternCount` / `Cfg::PatternLength`.
- `:174-175` `patternVal` — **leave AS-IS.** It's dead (zero callers tree-wide, confirmed by both gates); `inline int16_t *patternVal(TT2Pattern &pat, ...)` keeps compiling via the `using TT2Pattern = TT2PatternT<Full>` alias. Do NOT template dead code (it'd never instantiate anyway). Out-of-scope to delete here.
- `:20-21` convert `TT2_PATTERN_COUNT`/`TT2_PATTERN_LENGTH` to `= TT2ConfigFull::PatternCount` / `PatternLength` (permanent compat aliases). `:34-35` asserts still hold (alias = 4/64).
- Add `using TT2Pattern = TT2PatternT<TT2ConfigFull>;`. `:180` keep `static_assert(sizeof(TT2Pattern) <= 140)` (now via the alias) and the new `== <BASE>` guard.
**Step 4** run full suite → PASS; the two `==` guards confirm byte-identity. Whole `sequencer` target builds (every consumer of `TT2Pattern`/`TT2_PATTERN_*` binds through the aliases).
**Step 5 commit:** `refactor(tt2): template TT2Pattern on config, alias Full`

---

## Task 2 — ripple `template<Cfg>` through the pattern-op family

**Files:** Modify `src/apps/sequencer/engine/TeletypeNativeOps.cpp`.

The pattern ops are **already** `template<Cfg>` (parent refactor); this task makes the 14 helpers they call template too, and replaces the `TT2Pattern` type name + `TT2_PATTERN_*` constants inside them.

**Step 1 — the edits:**
- **`normalisePn` (BLOCKER — must template, the gate caught this):** `normalisePn` (def `:3284`, forward decl `:3093`) is a plain `int16_t`-param function whose body uses `TT2_PATTERN_COUNT` at `:3286`. It is NOT in the helper list and has no `Cfg` in scope — so swapping `:3286` to `Cfg::PatternCount` as a bare edit **does not compile**. Since supermini changes pattern *count*, `normalisePn` must clamp to `Cfg::PatternCount`, not Full's 4. Fix: make it `template<typename Cfg> static int16_t normalisePn(int16_t pn)` (def + forward decl `:3093` both template), body `:3286` → `Cfg::PatternCount`, and update **every `normalisePn(pn)` call site (~30) → `normalisePn<Cfg>(pn)`** (they're inside `template<Cfg>` ops, so `Cfg` is in scope; explicit `<Cfg>` is required since the arg is `int16_t`). These are the `patterns[normalisePn(pn)]` / `mutablePattern`-internal sites.
- **15 helpers → `template<typename Cfg>` taking `TT2PatternT<Cfg>&` / `const TT2PatternT<Cfg>&`:** the 14 — `normaliseIdx` (`:3300`), `patternNextInc` (`:3517`), `patternPrevDec` (`:3529`), `patternReverse` (`:3809`), `patternRotate` (`:3819`), `patternShuffle` (`:3838`, also `TT2Rng&` — stays), `patternMinIdx` (`:4012`), `patternMaxIdx` (`:4018`), `patternMinVal` (`:4024`), `patternMaxVal` (`:4030`), `patternSum` (`:4036`), `patternAvg` (`:4042`), `patternFind` (`:4048`), `patternRndVal` (`:4109`) — **plus `normalisePn` above** (15th). `mutablePattern` (`:3294`) is already `template<Cfg>` — change its return type to `TT2PatternT<Cfg>*`.
- **Every `TT2Pattern` inside the (already-templated) ops → `TT2PatternT<Cfg>`** (~40 sites: the `TT2Pattern *p = mutablePattern(...)` locals, `const TT2Pattern &pat = ...`, at `:1859,:3103,:3118,:3328,:3335…:4118`). Required so a supermini op uses the mini pattern type, not Full's.
- **`TT2_PATTERN_COUNT`/`TT2_PATTERN_LENGTH` → `Cfg::PatternCount`/`Cfg::PatternLength`** (~16 sites: `:1858,:1860,:1861,:1895,:1932,:3104,:3119,:3286,:3306,:3343,:3396,:3524,:3700,:3702,:3716,:3728`). `:3286` only compiles once `normalisePn` is templated (above). Keep `TT2_Q_LENGTH` (fixed) at `:3104/:3119`.

**Step 2 — verify:**
- Host: `make -C build sequencer` builds + links.
- ARM: `make -C build/stm32/release sequencer` builds.
- `ctest --test-dir build -R 'TT2|TeletypeV2|Tt2'` → all pass (the pattern-op tests — `TestTeletypeV2` P/PN/P.NEXT cases — are the real coverage).
- **Grep gates:** (a) `grep -n 'static.*TT2Pattern\b' src/apps/sequencer/engine/TeletypeNativeOps.cpp` → every hit is inside `template<typename Cfg>` and names `TT2PatternT<Cfg>`, none bare `TT2Pattern`. (b) `grep -n 'normalisePn(' src/apps/sequencer/engine/TeletypeNativeOps.cpp` → no **bare** `normalisePn(pn)` call remains (all must be `normalisePn<Cfg>(pn)`); the only un-suffixed hits allowed are the template def + forward decl. (c) no `TT2_PATTERN_COUNT`/`TT2_PATTERN_LENGTH` left in the file.
**Step 3 commit:** `refactor(tt2): template pattern-op family on config`

---

## Task 3 — serializer pattern loops

**Files:** Modify `src/apps/sequencer/engine/TT2SceneSerializer.cpp` (already `template<Cfg>`).
**Step 1:** `TT2_PATTERN_COUNT`/`TT2_PATTERN_LENGTH` → `Cfg::PatternCount`/`Cfg::PatternLength` at `:72,:73,:74,:75,:76,:77,:79,:154,:155,:156,:159,:170,:171`. The local `long v[TT2_PATTERN_COUNT]` (`:154`) → `long v[Cfg::PatternCount]`.
**Step 2:** `ctest --test-dir build -R TestTT2SceneSerializer` → PASS (round-trip characterization). Host build links.
**Step 3 commit:** `refactor(tt2): serializer pattern loops via Cfg`

---

## Task 4 — spec amendment + acceptance gate

**Files:** Modify `docs/superpowers/specs/2026-06-25-tt2-config-parameterization-refactor.md`.
1. Move `TT2_PATTERN_COUNT`/`TT2_PATTERN_LENGTH` out of the "16 fixed constants" list into the config-traits list (now 8 varying: + PatternCount/PatternLength). Update the Work-item-1 grep + acceptance #2 wording to include them.
2. **Acceptance run:**
   - `static_assert`s hold: `TT2PatternT<Full>` == baseline, `TeletypeProgramT<Full>` == 3638 (byte-identical).
   - Grep gate: no `TT2_PATTERN_COUNT`/`TT2_PATTERN_LENGTH` inside templated infra (`TeletypeProgram.h` struct/loops, `TeletypeNativeOps.cpp`, `TT2SceneSerializer.cpp`) except the two alias definitions; consumers (UI pattern page, tests) keep the compat aliases — expected, not failures.
   - Full host suite green (only the known pre-existing `TestScaleGroupStorage` red); ARM release builds; `TestTT2Config` + pattern-op + serializer tests pass.
3. **Commit:** `docs(tt2): patterns now config-parameterized (spec amend + gate)`

---

## Verification gate (definition of done)
- Byte-identical Full: `sizeof(TT2PatternT<Full>)` == baseline, `sizeof(TeletypeProgramT<Full>)` == 3638 (compile-time `static_assert`).
- Pattern-op + serializer characterization tests green at every commit; no existing test edited (compat aliases hold).
- Grep gate: zero `TT2_PATTERN_*` in templated infra outside the alias definitions.
- Host build + link, ARM release build, both clean.
- `TeletypePatternViewPage.cpp` untouched.

## Notes
- After this, the config has 8 varying traits (script/delay/trigger/metro/init/scene + pattern count/length); the only remaining fixed constants are genuinely hardware/parser-bound (CV/TR=8, command width 16, stack/exec/Q/RNG/scales/midi).
- No `supermini` mode is created here — this only makes the dimensions parameterizable. The supermini config + track is a separate downstream step.
