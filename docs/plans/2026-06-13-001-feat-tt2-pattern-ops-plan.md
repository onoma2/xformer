---
id: feat-tt2-pattern-ops
schema: plan
title: "feat: TT2 pattern op family (P/PN)"
type: feat
status: completed
date: 2026-06-13
depth: standard
---

# feat: TT2 pattern op family (P/PN)

## Summary

Wire the full Teletype pattern op family (~73 ops: the `P.*` working-pattern set
and their `PN.*` explicit-pattern mirrors) into the TT2 native op table. This is
a **pure-ops batch** over data that already exists — `TeletypeProgram.patterns[4]`
(struct `TT2Pattern { idx, len, wrap, start, end, val[64] }`), the `p_n` working
selector in `TT2Variables`, init defaults, serialization (raw struct write), and a
`patternVal()` accessor are all already in the model. No data-model, serialization,
or RAM changes are needed. Values and edge-case behavior are matched to upstream
`teletype/src/ops/patterns.c` so copy-pasted Teletype scenes behave identically.

Same shape as the already-landed Q (`feat(tt2): wire queue op family`) and stack
batches: handlers in `src/apps/sequencer/engine/TeletypeNativeOps.cpp`, registered
in the op table, tested in a new `TestTeletypeV2Pattern.cpp`.

---

## Scope Boundaries

**In scope:** all 73 `P.*`/`PN.*`/`RND.P`/`RND.PN` ops, plus a small set of shared
pattern accessor helpers (index/length clamping, P-vs-PN target resolution,
navigation-with-wrap). Semantics matched to `patterns.c`.

**Out of scope / deferred:**

### Deferred to Follow-Up Work
- Wiring `TeletypePatternViewPage` (exists for orig Teletype) to TT2 patterns — the on-device pattern editor. Ops-only now, consistent with the Q/stack/N/V batches.

---

## Key Technical Decisions

- **P vs PN sharing.** Each `PN.X` is `P.X` with a leading explicit pattern-index pop; `P.X` uses the working `p_n`. Implement one shared helper per concept that takes a resolved pattern reference + remaining args; `P.X` calls it with `patterns[p_n]`, `PN.X` pops+clamps an index then calls it. Halves the real logic across ~37 concepts × 2 forms.
- **Match upstream byte-for-byte.** Index advance/wrap, insert/remove shifting, and length updates must mirror `patterns.c` (copy-paste scene compatibility). The implementer reads each op body in `patterns.c` and reproduces its semantics — the test scenarios below pin the load-bearing cases.
- **No store/serialization work.** `patterns[]` already serializes via the raw `writer.write(_program)` blob. Do not add migration/versioning (project rule: dev files break freely).
- **RND ops use the TT2 rng** (`runtime.rng` slots), not C `rand()`. Tests assert range membership, not exact values.

---

## Implementation Units

### U1. Pattern accessor helpers + P.N selector

**Goal:** Shared helpers the rest of the ops build on, plus the working-pattern selector.
**Dependencies:** none.
**Files:** `src/apps/sequencer/engine/TeletypeNativeOps.cpp`, `src/tests/unit/sequencer/TestTeletypeV2Pattern.cpp` (new), `src/tests/unit/sequencer/CMakeLists.txt`.
**Approach:** Add static helpers — `normalisePn(int16_t)` (clamp 0..3), a resolver that returns the target `TT2Pattern&` for P (working `p_n`) vs PN (popped arg), length-bounded get/set of `val[idx]`, and an index step honoring `wrap`/`start`/`end`. Wire `P.N` (get/set `runtime.variables.p_n`, normalised). Register in the op table.
**Patterns to follow:** the Q ops in the same file (`opQ`, `opQN`); the existing `patternVal()` in `src/apps/sequencer/model/TeletypeProgram.h`.
**Test scenarios:**
- `P.N` get returns current `p_n`; `P.N 2` sets it to 2; `P.N 9` clamps to 3; `P.N -1` clamps to 0.
- Helper: PN target resolution clamps an out-of-range pattern arg into 0..3.

### U2. Value access — P / PN / P.I / P.L / P.HERE

**Goal:** Read/write pattern values and the working index/length.
**Dependencies:** U1.
**Files:** `src/apps/sequencer/engine/TeletypeNativeOps.cpp`, `TestTeletypeV2Pattern.cpp`.
**Approach:** `P`/`PN` get reads `val[idx]` then advances `idx` (upstream auto-advance); set writes `val[idx]` then advances. `P.I`/`PN.I` get/set the working `idx` (clamped). `P.L`/`PN.L` get/set `len` (clamp 0..64). `P.HERE`/`PN.HERE` read/write at `idx` without advancing. Match `patterns.c` exactly for the advance/wrap interaction with `start`/`end`/`wrap`.
**Patterns to follow:** `patterns.c` `op_P_get`/`op_P_set`, `op_P_HERE_*`, `op_P_I_*`, `op_P_L_*`.
**Test scenarios:**
- Seed `val={10,20,30}`, `len=3`, `idx=0`: `P` reads 10 and advances idx to 1; next `P` reads 20.
- `P 99` (set) writes 99 at idx then advances.
- `P.HERE` reads current idx without advancing; `P.HERE 5` writes without advancing.
- `P.I 2` sets idx; `P.I` reads 2. `P.L 4` sets len; `P.L` reads 4; `P.L 99` clamps to 64.
- `PN.I 1 2` / `PN 1` operate on pattern 1, leaving the working pattern untouched.
- Index advance honors `wrap`/`start`/`end` per upstream (pin the wrap-at-end case).

### U3. Bounds + navigation — P.WRAP / P.START / P.END / P.NEXT / P.PREV

**Goal:** Per-pattern bounds and index navigation.
**Dependencies:** U1, U2.
**Files:** `src/apps/sequencer/engine/TeletypeNativeOps.cpp`, `TestTeletypeV2Pattern.cpp`.
**Approach:** get/set `wrap`, `start`, `end` (clamped to valid ranges). `P.NEXT`/`P.PREV` step `idx` forward/back with the wrap/bounds rule from `patterns.c` (wrap within `[start,end]` when `wrap`, else clamp). `+PN` mirrors.
**Test scenarios:**
- `P.START 1`, `P.END 3`, `P.WRAP 1`: `P.NEXT` from idx 3 wraps to 1; from idx 2 → 3.
- `P.WRAP 0`: `P.NEXT` at `end` stays at `end` (clamp, no wrap).
- `P.PREV` symmetric (wrap from start to end / clamp at start).
- `P.START`/`P.END`/`P.WRAP` get return set values; out-of-range set clamps.

### U4. Insert / remove / push / pop — P.INS / P.RM / P.PUSH / P.POP

**Goal:** Structural edits with element shifting and length updates.
**Dependencies:** U1, U2.
**Files:** `src/apps/sequencer/engine/TeletypeNativeOps.cpp`, `TestTeletypeV2Pattern.cpp`.
**Approach:** `P.INS i v` shifts right from i, inserts v, bumps len (capped 64); `P.RM i` shifts left, drops len; `P.PUSH v` appends at len; `P.POP` removes+returns last. Match `patterns.c` shifting + len-clamp behavior exactly. `+PN`.
**Test scenarios:**
- `val={1,2,3}` len 3: `P.INS 1 9` → `{1,9,2,3}` len 4; `P.RM 1` → `{1,2,3}` len 3.
- `P.PUSH 7` appends at index len, len→4; `P.POP` returns 7, len→3.
- INS at/over len cap (64) does not overflow; RM on empty (len 0) is a safe no-op.
- Edge: `P.INS` at index ≥ len appends; negative index clamps to 0.

### U5. Reorder — P.REV / P.SHUF / P.ROT / P.CYC

**Goal:** Whole-pattern reordering over the active length.
**Dependencies:** U1, U2.
**Files:** `src/apps/sequencer/engine/TeletypeNativeOps.cpp`, `TestTeletypeV2Pattern.cpp`.
**Approach:** `P.REV` reverses `[0,len)`; `P.ROT n` rotates by n; `P.CYC n` cycles; `P.SHUF` shuffles via the TT2 rng. Match `patterns.c` rotation direction/sign. `+PN`.
**Test scenarios:**
- `val={1,2,3,4}` len 4: `P.REV` → `{4,3,2,1}`.
- `P.ROT 1` → upstream direction (pin exact result vs `patterns.c`); `P.CYC` likewise.
- `P.SHUF`: every output element is a member of the input multiset; len unchanged. (rng — assert permutation, not exact order.)

### U6. Whole-pattern arithmetic + scale — P.+ / P.+W / P.- / P.-W / P.SCALE

**Goal:** Apply arithmetic across the active length.
**Dependencies:** U1, U2.
**Files:** `src/apps/sequencer/engine/TeletypeNativeOps.cpp`, `TestTeletypeV2Pattern.cpp`.
**Approach:** `P.+ n` adds n to all `[0,len)`; `P.+W n lo hi` adds with wrap into `[lo,hi]`; `P.-`/`P.-W` subtract. `P.SCALE` quantizes values to a scale. **`P.SCALE` depends on a scale-quantize helper that does not exist yet** (only the N/V raw tables landed) — implement a minimal quantize for it or, if that balloons, split `P.SCALE` to a follow-up unit and land the rest of U6. Decide at execution time.
**Test scenarios:**
- `val={1,2,3}` len 3: `P.+ 10` → `{11,12,13}`.
- `P.+W 3 0 4` wraps each sum into [0,4] per upstream wrap math (pin against `patterns.c`).
- `P.- 1` subtracts; `P.-W` wraps.
- `P.SCALE`: covered once the quantize approach is chosen; pin one known input→output against `patterns.c`.

### U7. Per-index ops + query — P.PA/PS/PM/PD/PMOD, P.MIN/MAX, P.MINV/MAXV, P.SUM/AVG, P.FND

**Goal:** Per-element arithmetic and whole-pattern queries.
**Dependencies:** U1, U2.
**Files:** `src/apps/sequencer/engine/TeletypeNativeOps.cpp`, `TestTeletypeV2Pattern.cpp`.
**Approach:** `P.PA i n`/`PS`/`PM`/`PD`/`PMOD` apply add/sub/mul/div/mod at one index (div/mod guard zero, mirroring the Q.* arithmetic guards). `P.MIN`/`P.MAX` clamp all elements to a floor/ceiling; `P.MINV`/`P.MAXV` return the min/max value over `[0,len)`; `P.SUM`/`P.AVG` reduce; `P.FND v` returns the index of the first element equal to v (or upstream's not-found sentinel). `+PN`.
**Test scenarios:**
- `P.PA 1 10` on `{1,2,3}` → `{1,12,3}`; `P.PD i 0` / `P.PMOD i 0` are no-ops (zero guard).
- `P.MINV`/`P.MAXV` over `{5,2,8}` → 2 / 8; `P.SUM` → 15; `P.AVG` rounds per upstream.
- `P.MIN 4` floors elements below 4; `P.MAX 6` caps elements above 6 (mirror Q.MIN/MAX set semantics — verify against `patterns.c`).
- `P.FND 8` on `{5,2,8}` → 2; not-found returns upstream sentinel.

### U8. Random — P.RND / PN.RND / RND.P / RND.PN

**Goal:** Random pattern access using the TT2 rng.
**Dependencies:** U1, U2.
**Files:** `src/apps/sequencer/engine/TeletypeNativeOps.cpp`, `TestTeletypeV2Pattern.cpp`.
**Approach:** `P.RND` returns a random element over `[0,len)`; `RND.P` returns a random index. Use `runtime.rng` (the established TT2 rng), not C `rand()`. `+PN`/`RND.PN`.
**Test scenarios:**
- `P.RND` over `{5,2,8}` len 3: result is always one of {5,2,8}.
- `RND.P` returns an index in `[0,len)`. (rng — assert membership/range, not exact value.)

---

## System-Wide Impact

- Touches only `TeletypeNativeOps.cpp` (handlers + table) and the new test file + its CMake registration. No model, serialization, runner, lowering, or RAM changes.
- The two `unsupported_op` probes in `TestTeletypeV2NativeOps.cpp` and `TestTeletypeV2ScriptRunner.cpp` currently use `P 0` as their "still-unsupported" op (chosen because pattern was unwired). Wiring P makes those pass-through — **repoint both probes to another unwired op** (e.g. a not-yet-wired pitch/scale op such as `HZ 0`) as part of U2, the same way N's wiring repointed them off `N`.

---

## Risks & Mitigations

- **Probe breakage** (above) — repoint in U2; full TT2 suite must stay green each unit.
- **P.SCALE scale dependency** — no quantize helper exists yet; U6 decides minimal-impl vs split-to-follow-up at execution time.
- **Upstream-fidelity drift** — advance/wrap, rotate sign, insert/remove shifting, P.FND sentinel, P.AVG rounding are the error-prone cases; each is pinned by a test scenario against `patterns.c`.
- **RND determinism** — tested by range/permutation membership, not exact value.

---

## Verification

- New `TestTeletypeV2Pattern.cpp` green (per-unit cases above).
- Full `TestTeletypeV2*` suite green after each unit (esp. NativeOps + ScriptRunner after the probe repoint).
- STM32 release builds clean (flash is no longer a constraint post-`-Os`).
- Execution posture: TDD per unit (RED before GREEN), matching the Q/stack/N/V batches.
