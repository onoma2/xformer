---
id: refactor-ragel-parser-island
schema: plan
title: "refactor: keep only the Ragel tokenizer; delete the vendored C op-world + C VM"
type: refactor
status: completed
date: 2026-06-18
depth: deep
---

# Ragel Parser Island — Delete the Vendored C Op-World

> **For Claude:** REQUIRED SUB-SKILL: use superpowers:executing-plans to implement this plan unit-by-unit.

**Goal:** Stop juggling two languages. Keep **only** the Ragel tokenizer from the vendored C
library; delete the op table, the op implementations, the C VM, the C scene serializer, and the
host-hook stubs. The target pipeline is:

```
Ragel tokenize  →  tele_command_t (flat tokens)  →  lowerCommand  →  TT2Command  →  native dispatch
```

No `tele_ops[]`, no `tele_mods[]`, no `validate()`/`run_script()`/`process()` C VM, no
`print_command`, no C scene serialization, no `teletype_io` host-hook stubs. The `E_OP_*` enum is
the single frozen contract between the tokenizer and native dispatch.

**Resolved scope decision (editor preflight):** when `validate()` leaves the editor, the
commit-line preflight **drops op-shape validation and matches the runtime** — `parse()` still
catches syntax errors; arity/settable errors surface at runtime exactly as the engine already
handles them (`TT2Evaluator.h:105`: stack underflow, unsupported-op, mod-prefix stack checks).
This means **no native op-metadata table is built** — `tele_ops[]` was the wrong target entirely.
Editor and engine now agree on what is valid; today the editor is *stricter* than the engine
(`validate()` rejects extra operands the runtime silently ignores), so this also removes an
existing inconsistency.

---

## What the research established (load-bearing facts)

Verified against the tree; these shape every unit.

- **`tele_ops[]`/`tele_mods[]` are already dead for TT2.** `lowerCommand`
  (`src/apps/sequencer/model/TeletypeProgram.h:149`) only length-checks and copies the flat
  `tele_command_t` `tag[]`/`value[]` arrays; it reads no op table. The native printer and name
  registry (`TT2Printer.h`, `TT2OpNames.h`) are already "no `tele_ops[]`". Dispatch is native
  (`tt2NativeOpTable`). The only real `tele_ops[]` consumers are `print_command` (`command.c`) and
  the C VM (`teletype.c`) — both deletable.
- **One live `validate()` caller: the editor.** `TeletypeScriptViewPage::commitLine()`
  (`src/apps/sequencer/ui/pages/TeletypeScriptViewPage.cpp:686`) runs `parse()` then `validate()`
  on each committed line, surfacing `tele_error()` text via `showMessage`. This is the only real
  path keeping `validate()` (and its slice of `teletype.c`) alive. Per the resolved decision, the
  `validate()` call is removed; `parse()` stays for syntax errors.
- **`parse()` lives inside `teletype.c`** (`teletype/src/teletype.c:40`) alongside `validate()`
  (`:49`) and the `run_script`/`process` C VM (`:170+`). `parse()` is essentially `scanner(...)`.
  But the carve is deeper than repointing `scanner.h` — the **Ragel sources themselves** pull the
  VM/op-world: `scanner.rl:9` includes `teletype.h`; `match_token.rl:7` includes `ops/op.h` (which
  pulls `state.h`) + `helpers.h`. So U1 must edit those `.rl` includes **and regenerate the
  matching `scanner.c`/`match_token.c`** (Ragel) so the island depends only on a parser-only header
  + `op_enum.h`.
- **Two symbols must survive the deletion of `teletype.c`/`helpers.c`.** `tele_error()` lives at
  `teletype/src/teletype.c:429` and is still called by `commitLine()`
  (`src/apps/sequencer/ui/pages/TeletypeScriptViewPage.cpp:696`) to render `parse()` failures.
  `rev_bitstring_to_int()` lives at `teletype/src/helpers.c:105` and is called by
  `match_token.rl:542`. Both must move into the parser island (or callers change).
- **TT2 native helper keep-set** (`extern "C"` decls at `src/apps/sequencer/engine/TeletypeNativeOps.cpp:11`):
  `tresillo`, `drum`, `velocity` (from `drum_helpers.c`); `chaos_*` set/get for val/r/alg (from
  `chaos.c`); `table_nr`, `table_n_s`, `table_n_c`, `table_n_cs`, `table_n_b` (from `table.c`,
  const-table-backed); **`euclidean` (from `teletype/libavr32/src/euclidean/euclidean.c`, backed by
  `euclidean/data.c`)**. Port exactly these + copy their const tables into native C++.
- **The compiled vendored set spans two roots.** `src/apps/sequencer/CMakeLists.txt:177-204` lists
  the `teletype/src` files (15 `ops/*.c` + core); **`:205-210` additionally compiles the libavr32
  block**: `euclidean/data.c`, `euclidean/euclidean.c`, `music.c`, `random.c`, `util.c`. The final
  "only `scanner.c` + `match_token.c` + parse TU" gate must account for the libavr32 files too —
  `euclidean`+`data.c` are ported (U3); `music.c`/`random.c`/`util.c` resolved by the link audit.
- **`#include "teletype.h"` has many parse-only callers — all must migrate.** Production:
  `src/apps/sequencer/engine/TT2ScriptLoader.h:15` and `ui/pages/TeletypeScriptViewPage.cpp`. Tests:
  ~23 `TestTeletypeV2*`/`TestTT2*` units. If `teletype.h` survives, the "island" still leaks
  VM/state into native TT2; if it's deleted while a caller still includes it, that caller breaks.
  So the refactor includes a **global include migration**: every parse-only caller moves from
  `teletype.h` to the parser-only header. The only files allowed to keep `teletype.h` are the C-VM
  parity tests, and only until U4 retires them.

---

## Problem Frame

Removing TT1 left TT2 depending on the vendored C *more* than before: a 94-function `teletype_io`
stub file (`TeletypeBridge.cpp`) exists solely so the vendored op implementations and C VM link.
TT2 already dispatches, prints, names, and serializes natively — the vendored C contributes
nothing TT2 runs except the Ragel tokenizer. The op table and C VM persist only because (a) the
editor calls `validate()` and (b) `parse()` is trapped in the same file as the VM. Resolve both
and the entire C op-world deletes, leaving the one artifact worth keeping: the Ragel lexer/parser.

---

## Scope Boundaries

**In scope:**
- A **parser-only header/API** exposing `parse`/`scanner`/`tele_command_t`/token+error enums
  without pulling `teletype.h` (VM/state). Carve `parse()` out of `teletype.c` into its own TU,
  **and edit `scanner.rl`/`match_token.rl` to drop their VM/op-world includes + regenerate the
  `.c`** so the island has no VM/state/op-table dependency. Move `tele_error()` and
  `rev_bitstring_to_int()` into the island.
- **Global include migration** — every parse-only caller (`TT2ScriptLoader.h`,
  `TeletypeScriptViewPage.cpp`, all parse/lower/print TT2 tests) switches `#include "teletype.h"`
  to the parser-only header, so `teletype.h` has no native referent left when it's deleted.
- **Remove `validate()` from the editor** (`commitLine()`); keep `parse()` + `tele_error()` for
  syntax-error preflight. No native shape validation added (resolved decision).
- **Port the helper/table keep-set** to native C++: `tresillo`/`drum`/`velocity`, `chaos_*`,
  `table_n*` (from `teletype/src`), and `euclidean` + `euclidean/data.c` (from libavr32) + their
  const tables.
- **Freeze `op_enum.h`** as a committed hand-maintained header (stop generating from `op.c`);
  retire **`teletype/utils/op_enums.py` only** (it reads `op.c`). **Keep `tt2_op_names.py`** — it
  derives enum→name from `match_token.rl` with no `tele_ops[]` dependency, and stays as the
  lexer→printer drift guard. Ordinals unchanged.
- **Replace or retire the C parity tests** — any test that asserts native dispatch against the
  vendored C VM can't run once the VM is gone; convert to pure-native assertions or mark
  pre-removal and drop.
- **Delete** the C op-world: all 15 `ops/*.c` (incl. `op.c`), the C-VM remainder of `teletype.c`,
  `command.c`, `scene_serialization.c`, `TeletypeBridge.cpp`, and every other vendored `.c` the
  link audit proves unreferenced. The `tele_*` host hooks go with them.
- Verify ordinals frozen, full suite green, both trees link, RAM/flash re-measured, sim auditioned.

**Out of scope / deferred:**
- **Replacing Ragel itself** with a hand-written native tokenizer — explicitly kept. A native
  lexer is a separate, larger effort if ever wanted.
- The native scene `.txt` format — TT2 already owns it (`TT2SceneSerializer`);
  `scene_serialization.c` is removed, not replaced.
- Any native op-shape/arity preflight in the editor — explicitly dropped (resolved decision).

---

## Key Technical Decisions

- **Parser island = `scanner.c` + `match_token.c` + carved `parse()` + a parser-only header.**
  The new header declares `parse`/`scanner`/`tele_command_t`/`cmd_*` helpers/token+error enums +
  `tele_error()` + `rev_bitstring_to_int()`, and includes `op_enum.h` (the `E_OP_*` enum
  `match_token` needs) — but **not** `teletype.h`/`ops/op.h`/`state.h`. The carve has three parts:
  (a) `parse()` moves from `teletype.c` into its own TU; (b) `scanner.rl` drops `#include "teletype.h"`
  and `match_token.rl` drops `#include "ops/op.h"` + `helpers.h`, both repointing to the parser-only
  header, then `scanner.c`/`match_token.c` are **regenerated with Ragel**; (c) `tele_error()` and
  `rev_bitstring_to_int()` move into the island so their callers (editor, `match_token`) keep
  linking. The VM half of `teletype.c` then has no remaining referent and is deleted.
- **No native op-metadata table.** `tele_ops[]` is unneeded: names are already native
  (`TT2OpNames`), dispatch is native (`tt2NativeOpTable`), and op-shape validation is dropped.
  Building a native arity/settable table would only serve an editor preflight we are deliberately
  removing.
- **Editor preflight = `parse()` only.** `commitLine()` keeps the `parse()` call (syntax errors
  still show); the `validate()` call and its error branch are deleted. The editor includes the
  parser-only header instead of `teletype.h`.
- **`op_enum.h` is frozen, not generated.** Ordinals are the serialize values and dispatch keys;
  they must not move. Freeze the current header and drop the `op.c`-driven generator.
  `match_token.rl` references `E_OP_` *names* (not order), so it stays consistent. Adding a future
  op becomes a deliberate edit of `op_enum.h` + `match_token.rl` + the native dispatch table.
  **Ordinals are sacred** — verification asserts the enum is unchanged.
- **Port the helpers, don't keep their `.c`.** Reimplement the keep-set
  (`tresillo`/`drum`/`velocity`/`chaos_*`/`table_n*`/`euclidean`) as native C++ and copy the const
  tables from `table.c`/`drum_helpers.c` and libavr32 `euclidean/data.c` into native source, so
  `drum_helpers.c`/`chaos.c`/`table.c` + libavr32 `euclidean/euclidean.c`+`data.c` can be deleted.
  `music.c`/`random.c`/`util.c` (libavr32) are resolved by the link audit — port if referenced,
  delete if not. If one helper is large/fiddly enough that porting is risky, that single `.c` may
  stay vendored — flagged at the audit, not assumed away.
- **Drop-set by link audit, not by guess.** After the carve, `validate()` removal, and helper
  port, the linker names every still-referenced vendored symbol. Delete files until only the
  parser island remains and zero `tele_*` host-hook symbols are referenced — which is what lets
  `TeletypeBridge.cpp` delete cleanly.

---

## Implementation Units

### U1. Parser-only header + carve `parse()` out of `teletype.c`

**Goal:** Expose the tokenizer (`parse`/`scanner`/`tele_command_t`/enums) through a header free of
`teletype.h`, and move `parse()` into its own TU, so the C VM in `teletype.c` becomes unreferenced.

**Dependencies:** none.

**Files:**
- Create: a parser-only header (e.g. `teletype/src/tt_parser.h`) declaring `parse`/`scanner`,
  `tele_command_t` + `cmd_*` helpers, token + `tele_error_t` enums, `tele_error()`, and
  `rev_bitstring_to_int()`; includes `op_enum.h`, not `teletype.h`/`ops/op.h`/`state.h`.
- Create: a parser TU (e.g. `teletype/src/tt_parser.c`) holding `parse()` carved from
  `teletype/src/teletype.c:40`, `tele_error()` (moved from `teletype.c:429`), and
  `rev_bitstring_to_int()` (moved from `helpers.c:105`) + any VM-free glue.
- Modify + regenerate: `teletype/src/scanner.rl` (drop `#include "teletype.h"`),
  `teletype/src/match_token.rl` (drop `#include "ops/op.h"` + `helpers.h`), both including the
  parser-only header; **regenerate `scanner.c`/`match_token.c` with Ragel**.
- Modify: `teletype/src/scanner.h` (drop the `teletype.h` include; use the parser-only header);
  `src/apps/sequencer/engine/TT2ScriptLoader.h:15` (switch `teletype.h` → parser-only header — the
  production native parse caller); `src/apps/sequencer/CMakeLists.txt` (compile the parser TU).

**Approach:** Identify exactly what `parse()`/`scanner()`/`match_token` reference (the scanner
state, the `tele_command_t` builder, the token/error enums, `tele_error`, `rev_bitstring_to_int`)
and gather their declarations into the parser-only header. Move those definitions into the parser
TU. Edit the two `.rl` includes and regenerate the `.c` (requires the Ragel tool — if unavailable
locally, hand-edit the generated `#include` lines in the `.c`, which are above the state-machine
tables and safe to touch). Leave the rest of `teletype.c` compiling for now (still holds
`validate()`/VM) — its deletion happens in U4 once U2 removes the last `validate()` caller. Build
the sim to confirm the parser island compiles with no VM/state/op-table include.

**Execution note:** Characterization-first — assert `parse()` produces the same `tele_command_t`
for a representative script before and after the carve (token-level, or via lower→print).

**Patterns to follow:** the existing `scanner.h`/`command.h` declarations; the include structure
of `TT2ScriptLoader.h` (the native parse caller).

**Test scenarios:**
- Parser island compiles with no include of `teletype.h`/`ops/op.h`/`state.h` (grep the island's
  TUs + regenerated `scanner.c`/`match_token.c`).
- `parse()` of a representative multi-op script yields the identical `tele_command_t` (tags +
  values) as before the carve.
- `tele_error()` and `rev_bitstring_to_int()` resolve from the island (editor + `match_token`
  link without `teletype.c`/`helpers.c`).
- Sim + unit suite build.

---

### U2. Remove `validate()` from the editor preflight

**Goal:** Drop the only live `validate()` caller; keep `parse()` for syntax errors.

**Dependencies:** U1 (editor includes the parser-only header).

**Files:**
- Modify: `src/apps/sequencer/ui/pages/TeletypeScriptViewPage.cpp` (`commitLine()`, ~`:686-700`) —
  remove the `validate()` call + its error branch; keep the `parse()` call/error branch; switch
  the include from `teletype.h` to the parser-only header.

**Approach:** After `parse()` succeeds, proceed directly to `pushHistory`/commit — no `validate()`.
Malformed *syntax* still shows via the `parse()` error branch; malformed *op shape* (arity,
settable) now defers to runtime, matching the engine.

**Execution note:** Test-first where practical — a UI commit test asserting a syntactically bad
line still shows an error and a syntactically valid but over-arity line now commits.

**Test scenarios:**
- A syntax error (e.g. unmatched token) still shows a message and does not commit.
- A valid line commits.
- An over-arity line (e.g. `ADD 1 2 3`) now commits (matches runtime tolerance) instead of being
  rejected.

---

### U3. Port the helper/table keep-set to native C++

**Goal:** Reimplement the exact helper functions TT2 native ops call, plus their const tables, in
native C++ so `drum_helpers.c`/`chaos.c`/`table.c` can be deleted.

**Dependencies:** none (parallel with U1/U2); required before U4 deletion.

**Files:**
- Create/Modify: native helper source under `src/apps/sequencer/engine/` (e.g. fold into
  `TeletypeNativeOps.cpp` or a `TT2Helpers.*`) implementing `tresillo`, `drum`, `velocity`,
  `chaos_*` (val/r/alg set+get), `table_nr`, `table_n_s`, `table_n_c`, `table_n_cs`, `table_n_b`,
  and `euclidean`; copy the backing const tables from `teletype/src/table.c` + `drum_helpers.c`
  and **libavr32 `euclidean/data.c`**.
- Modify: `src/apps/sequencer/engine/TeletypeNativeOps.cpp` — replace the `extern "C"` vendored
  decls with calls to the native implementations.

**Approach:** Port each function 1:1 (same algorithm, same table data) so behavior is identical;
verify against the vendored output before deleting the `.c`. Keep the const tables in flash
(`static const`) — they are read-only data, not statics that grow RAM.

**Execution note:** Characterization-first — capture vendored outputs for representative inputs
(euclidean/tresillo/drum/velocity patterns, each `table_n*` across its domain, chaos sequences)
and assert the native ports match exactly.

**Test scenarios:**
- `euclidean`/`tresillo`/`drum`/`velocity` outputs match the vendored functions across sampled
  args.
- Each `table_n*` returns identical values across its input domain.
- `chaos_*` set/get round-trips and the chaos sequence matches vendored output.
- TT2 native ops that use these helpers behave unchanged in the sim.

---

### U4. Delete the C op-world; freeze the enum; link audit; verify

**Goal:** Remove every now-unreferenced vendored file, the host-hook stub, and the generators;
freeze `op_enum.h`; confirm correctness end-to-end and quantify the reduction.

**Dependencies:** U1, U2, U3.

**Files:**
- Delete: `ops/op.c` + all 14 op-impl category files; the C-VM remainder of `teletype.c`;
  `command.c`; `scene_serialization.c`; `drum_helpers.c`/`chaos.c`/`table.c`; libavr32
  `euclidean/euclidean.c`+`data.c` (all ported in U3); `src/apps/sequencer/engine/TeletypeBridge.cpp`;
  and every other vendored `.c` the link audit clears (incl. libavr32 `music.c`/`random.c`/`util.c`
  if unreferenced); their CMake entries (`:177-210`).
- Freeze: `teletype/src/ops/op_enum.h` (committed, hand-maintained); retire **`teletype/utils/op_enums.py`
  only**. **Keep `teletype/utils/tt2_op_names.py`** (reads `match_token.rl`, no `op.c`/`tele_ops[]`
  dependency) as the lexer→printer drift guard; update its header comment if it references the now-removed
  generator pipeline.
- Migrate: every remaining parse/lower/print TT2 test (`src/tests/unit/sequencer/TestTeletypeV2*`,
  `TestTT2*`) from `#include "teletype.h"` to the parser-only header. The only files left including
  `teletype.h` are the C-VM parity oracles — which this unit then retires.
- Replace/retire: the C-parity unit tests (those asserting native dispatch == vendored C VM) —
  convert to pure-native assertions or remove. After this, **no file under
  `src/apps/sequencer/{engine,model,ui}` or `src/tests` includes `teletype.h`.**

**Approach:** Build the sim; read the linker's undefined/remaining-reference report. Delete vendored
sources until the compiled vendored set is `scanner.c` + `match_token.c` + the carved parse TU and
**zero `tele_*` host-hook symbols** are referenced — then `TeletypeBridge.cpp` deletes clean.
Reconfigure, full `ctest`, build sim + STM32 release. Re-measure `arm-none-eabi-size -A` (expect a
`.text`/flash drop from removing ~15 op-impl files + C VM + C scene serializer + 94-stub; `.bss`/
`.ccmram` roughly flat — these were code, not statics). Audition the sim: a TT2 track parses + runs
scripts across the op surface (op-coverage demo), and a scene saves/loads.

**Approach (parity tests):** Locate tests that compare TT2 native dispatch to the vendored C VM
(`run_script`/`validate` reference oracles). Where the assertion is "native matches C," either
re-anchor it to a frozen expected-value table (pure native) or mark and remove it as a
pre-removal-only check — the C oracle no longer exists to compare against.

**Test scenarios:**
- Full suite 100% (parse/lower/print/dispatch all native, unchanged behavior on the op surface).
- `rg '#include "teletype.h"' src/apps/sequencer/{engine,model,ui} src/tests` returns nothing.
- Both trees link; STM32 no region overflow.
- Sim: TT2 demo track runs ops end-to-end; scene round-trips.
- Flash delta recorded; no `.bss`/`.ccmram` regression.

**Verification:** suite green, both trees clean, flash reduced, sim healthy; the binary defines no
`tele_*` host-hook symbol and no vendored op-exec/C-VM symbol; the CMake `:177-210` vendored block
is reduced to `scanner.c` + `match_token.c` + the carved parse TU (both `teletype/src` and the
libavr32 block emptied of everything else).

---

## Verification

- `op_enum.h` byte-identical to pre-change (ordinal freeze — the load-bearing invariant).
- Parser island (incl. regenerated `scanner.c`/`match_token.c`) compiles with no
  `teletype.h`/`ops/op.h`/`state.h` include; `parse()` output unchanged across the carve;
  `tele_error()`/`rev_bitstring_to_int()` resolve from the island.
- `tt2_op_names.py` still regenerates the native name registry from `match_token.rl` (drift guard
  retained); `op_enums.py` retired.
- `rg '#include "teletype.h"' src/apps/sequencer/{engine,model,ui} src/tests` returns **nothing**
  (all parse-only callers migrated; the C-VM parity tests that held it are retired).
- Editor: syntax errors still show; op-shape errors defer to runtime (no `validate()` linked).
- Ported helpers reproduce vendored output exactly (euclidean/tresillo/drum/velocity/chaos/table).
- `rg "tele_(cv|tr|env|lfo|g_|bus|metro|ii)\b"` finds no referenced symbol in the binary;
  `TeletypeBridge.o` is gone; no `validate`/`run_script`/`process` linked.
- Full `TestTeletypeV2*`/`TT2` + model + routing suites green; `TestTT2SceneSerializer` proves
  parse→lower→print is intact; C-parity tests re-anchored or removed.
- Sim + STM32 link clean; flash down, RAM not regressed.

## Risks

- **Ordinal drift** — any reorder of `op_enum.h` desyncs every `E_OP_*` consumer (dispatch, lower,
  printer, serialization). Mitigation: byte-identical `op_enum.h` gate. (The LFO.C lesson, at
  433× scale.)
- **`parse()` won't cleanly separate from `teletype.c`** — the carve drags VM internals.
  Mitigation: U1 gathers the exact `parse`/`scanner` referents into the parser-only header; if a
  clean split is impossible, keep a minimal stripped `teletype.c` holding only `parse()` + its
  non-VM deps and delete the rest — the host hooks still drop once the VM bodies go.
- **Ragel unavailable or `.rl` regen diverges** — the parser island still pulls VM/op-world via
  `scanner.rl`/`match_token.rl` includes if not regenerated. Mitigation: the include lines sit
  above the Ragel state tables, so a hand-edit of the generated `.c` `#include`s is safe if the
  tool is missing; verify with the "no `teletype.h`/`ops/op.h`/`state.h`" grep in U1.
- **Euclidean lives in libavr32, not `drum_helpers.c`** — porting the wrong source leaves
  `euclidean.c`+`data.c` linked. Mitigation: U3 explicitly ports `libavr32/src/euclidean/{euclidean,data}.c`;
  the U4 link audit confirms the libavr32 block links nothing.
- **A ported helper diverges from the vendored algorithm** — subtle behavior change in
  euclidean/drum/chaos/table output. Mitigation: U3 characterization tests assert exact parity
  against vendored output before the `.c` is deleted.
- **An editor feature beyond `commitLine()` relies on `validate()`** — e.g. another preflight path.
  Mitigation: grep all `validate(`/`tele_error` references in `ui/` during U2; remove or re-route
  each.
- **A migrated parse-only test secretly used a VM symbol** — switching its include to the
  parser-only header then fails to compile. Mitigation: that compile failure is the signal the test
  was a C-VM oracle, not parse-only — route it to the retire/re-anchor bucket instead of migrating.
- **`command.c`'s `copy_command`/`copy_post_command` is reached by the parser path** — would block
  deleting `command.c`. Mitigation: link audit; if reached, fold the needed copy helper into the
  parser island and still delete `print_command`.
- **Const tables inflate flash if mis-placed** — porting `table.c` data without `const`/`static`
  could land it in RAM. Mitigation: keep ported tables `static const`; re-measure in U4.

## Rollback

Each unit is a commit; revert in reverse. U1 (parser header + carve) and U3 (helper port) are
additive — they don't delete anything and are safe alone. U2 (editor `validate()` removal) is a
small isolated UI change. U4 is the structural deletion; reverting its CMake + file deletions
restores the old build while keeping the parser island, ported helpers, and editor change for a
retry.
