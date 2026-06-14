---
id: feat-tt2-native-printer
schema: plan
title: "feat: TT2 native op registry + command/script text printer"
type: feat
status: completed
date: 2026-06-14
branch: feat/tt2-v2-native
depth: standard
---

# feat: TT2 native op registry + command/script text printer

> **Not the persistence design.** This plan delivers the shared *text primitive* —
> ops/script → text — that the editor and a future scene exporter both reuse. Internal
> storage stays native binary `TeletypeProgram`/`TT2Command`; a human-readable scene
> file format is a separate serializer plan (see Future Persistence Boundary).

## Problem

TT2 stores programs as **lowered ops** (`TT2Command{length, tag[], value[]}`), never
source text. The forward path exists (`parse()` → `lowerCommand()` →
`TT2Script`, via `TT2ScriptLoader`), but there is no reverse: ops → text. Without
it, an on-device editor cannot display or re-edit a script, and the supertrack scene
bank (8–16 op-only programs) is unreadable. The round-trip — `TT2Command → text` — is
the missing half of the editor.

The cheap path (reuse upstream `print_command`/`tele_ops[]`/`tele_command_t`) is
**rejected on contract grounds**: the endgame is a native C++ language (op registry,
parser-assembly, printer, VM), with the `tele_*` C runtime deleted alongside the
bridge. Leaning on `print_command` re-anchors the editor to code that must die.

## Contract boundary (standing)

- **Kept:** Ragel (`teletype/src/match_token.rl` → `match_token.c`) as the tokenizer
  (name→enum); the generated `op_enum.h` / mod enum; the token-tag enum
  (`tele_word_t` in `teletype/src/command.h`) — these are the token model the kept
  lexer emits and `TT2Command.tag[]` already stores.
- **Out (dies with the bridge):** `tele_ops[]`/`tele_mods[]` tables, `print_command`,
  `command.c` parse/print, the `scene_state_t` VM. The printer must not depend on any
  of these.

---

## Scope

In scope:
- A **generated native op/mod name registry** (`enum → name`), sourced from
  `match_token.rl` (the same grammar Ragel compiles), so names cannot drift from the
  lexer.
- A **native C++ printer**: `TT2Command → text` line and `TT2Script → text` (per-script,
  `\n`-separated lines), walking `tag[]`/`value[]` — no `tele_*`. Whole-program views
  (script labels, ordering, separators) are the editor's to compose from per-script
  output — **not** a printer helper here (that format is a UI decision, deferred).
- A round-trip idempotence test harness.

### Deferred to Follow-Up Work
- Native C++ **parser-assembly** (tokens → `TT2Command`), replacing `command.c parse()`.
  The printer's name registry is what that port will reuse (forward lookup), but the
  port itself is a separate, larger unit. TT2 keeps using `parse()` until then.
- Editor UI wiring (display/edit pages) — consumes this printer; lives in the
  editor-repoint plan (`docs/plans/2026-06-14-001-feat-tt2-editor-repoint-plan.md`).

### Out of scope (non-goals — keep persistence out)
- Byte-exact text reproduction. Comments are not stored (stripped at parse) and
  whitespace is normalized — this is a **semantic**, command/script-level round-trip.
- Arity tables. Printing is tag-driven and does not need op arity (parser-port concern).
- **No project save/load format.**
- **No Supertrack scene-bank format / bank metadata.**
- **No human scene-file grammar** (section headers, config/pattern/IO/timing/MIDI sections).
- **No source-text storage in the model** — the model stays binary ops; text is
  reconstructed on demand.

---

## Future Persistence Boundary

This printer is a **text primitive**, not the persistence layer. Two mechanisms, mirroring
real Teletype (internal binary scene storage + a text scene serializer):

- **Internal storage stays native binary** — `TeletypeProgram` / `TT2Command`. The
  Supertrack scene bank is binary, fast, bounded (per the supertrack design — scenes are
  program-only, no text field). This plan does not touch it.
- **A human-readable TT2 scene file is a separate serializer/deserializer layer** (its own
  future plan). That layer owns the scene-file grammar: section headers (e.g. the
  Teletype-style `#1`/`#2`/…/`#M`/`#I` script markers, pattern sections, config/IO/timing/
  MIDI sections, bank labels and ordering) — and calls **`tt2PrintScript`** for the script
  bodies. The printer supplies bodies; the serializer owns layout.

**Future consumers of this printer** (why it exists, without it growing into storage):
1. editor line display / per-script edit (`docs/plans/2026-06-14-001-...editor-repoint`),
2. script-body export inside a future TT2 scene serializer,
3. the round-trip test oracle (parse↔print idempotence).

A full **scene round-trip** (text file ↔ whole program incl. patterns/config) is **not**
this plan's claim — that belongs to the serializer plan. This plan only claims
command/script-level semantic idempotence.

---

## Key Technical Decisions

**D1 — Name registry generated from `match_token.rl`.** A Python generator (sibling to
`teletype/utils/op_enums.py`) parses the `"NAME" => { MATCH_OP(E_OP_X); }` and
`MATCH_MOD(E_MOD_X)` lines and emits a native C++ `enum → name` lookup. `match_token.rl`
is the single source of name↔enum (Ragel uses it forward; we generate the reverse), so
adding an op updates lexer + name table from one edit. No `tele_ops[]`.

**D2 — Emit as a `switch` (order-independent, compiler-checked).** The generated lookup
is `const char *tt2OpName(int op)` / `tt2ModName(int mod)` returning string literals via
`switch`, `default: return nullptr`. A `switch` needs no agreement with the enum's
numeric order (C++ lacks array designated initializers). It catches a **removed** enum
value at compile time, but does **not** catch a **newly added** enum value missing from
`match_token.rl` (that case just falls to `default`/nullptr) — the U1 full-coverage test
is the guard for that. Generated file is tracked (like `op_enum.h`), regenerated when
ops change.

**D2a — Alias canonicalization (multiple spellings → one enum).** `match_token.rl`
maps several spellings to one enum: e.g. both `W.ACT` and `WR.ACT` →
`E_OP_WR_ACT` (`match_token.rl:208-209`). Measured invariant: **419 op enum values, 420
spellings, 1 duplicate, 0 missing.** A naive line-for-line generator emits two
`case E_OP_WR_ACT:` and fails to compile. The generator **dedups by enum**, emitting one
case per enum value with a **canonical spelling = first occurrence in `match_token.rl`**
(deterministic; a tiny override map can pin a preferred spelling if ever needed). Either
spelling re-parses to the same enum, so printing the canonical one keeps the round-trip
idempotent.

**D3 — Printer is tag-driven, native, allocation-free, bounded.** `tt2PrintCommand`
walks `TT2Command.tag[]`/`value[]` and appends per token: `OP`→`tt2OpName`,
`MOD`→`tt2ModName`, `PRE_SEP`→`":"`, `SUB_SEP`→`";"`,
`NUMBER`/`XNUMBER`/`BNUMBER`/`RNUMBER`→ native dec/hex/bin/rev-bin formatters. Spacing
mirrors the lexer: a space between tokens except before a separator.

**Bounded buffer + truncation status.** A command is ≤16 tokens
(`TT2_COMMAND_MAX_LENGTH`); the widest token is a 16-bit binary/reverse-binary literal
(`B` + 16 digits = 17 chars). Worst case ≈ 16×17 + 15 spaces ≈ **287 chars**. Define
`TT2_PRINT_LINE_MAX = 320` (headroom) and have `tt2PrintCommand` take a sized buffer and
**return a bool fits/truncated** (do not rely on `StringBuilder` silently advancing past
the end — `core/utils/StringBuilder.h` has no overflow flag). Callers size to
`TT2_PRINT_LINE_MAX`; truncation is tested, not assumed impossible.

**D4 — Number formats must match the kept lexer's syntax.** The hex/binary/reverse-bin
prefixes the printer emits must be re-accepted by `match_token.rl`'s number patterns
(else the round-trip breaks). The native formatters copy those exact prefixes; the
idempotence test (D5) is what proves the match.

**D5 — Semantic round-trip, command/script-level only, not byte-exact.** Correctness =
`parse → lower → print → parse → lower` yields an **identical `TT2Command`** (idempotent),
not identical text. Comments and exact spacing are not preserved by design. This claim is
scoped to commands and scripts — a **full scene round-trip** (text file ↔ whole program
incl. patterns/config/IO) is the future serializer's correctness target, not this plan's.

**D6 — Script line model: print exactly `length` commands, index = line.** A
`TT2Script` is `{length, commands[6]}` (`TeletypeProgram.h:51`); the runner executes
`0..length-1` (`TT2Runner.h`) and the loader skips blank source lines on store
(`TT2ScriptLoader.h:38`), so stored commands are **contiguous and non-empty** — no blank
middle lines exist in storage. `tt2PrintScript` prints exactly commands `0..length-1`,
one text line each, so **line index == command index** (stable for the editor); an
empty script (`length 0`) prints nothing. The printer does not invent blank slots; any
fixed-grid / blank-line editing affordance is the editor's concern, not the printer's.

---

## High-Level Technical Design

Directional, not implementation spec.

```
match_token.rl  ──(generator: tt2_op_names.py)──▶  TT2OpNames.{h,cpp}
  "MO.SHAPE" => MATCH_OP(E_OP_MO_SHAPE)              tt2OpName(E_OP_MO_SHAPE) -> "MO.SHAPE"
  "IF"       => MATCH_MOD(E_MOD_IF)                  tt2ModName(E_MOD_IF)     -> "IF"

TT2Command{tag[],value[]} ──tt2PrintCommand──▶ "MO.SHAPE 3 5"
  per tag: OP/MOD -> name | NUMBER.. -> dec/hex/bin | PRE_SEP ':' | SUB_SEP ';'

forward (kept):  text ──Ragel parse()──▶ tele_command_t ──lowerCommand──▶ TT2Command
reverse (new):   TT2Command ──tt2PrintCommand──▶ text
round-trip test: text -> ... -> TT2Command -> print -> parse -> lower == same TT2Command
```

---

## Implementation Units

### U1. Generated native op/mod name registry

**Goal:** A native `enum → name` lookup for ops and mods, generated from the Ragel
grammar — the printer's only new dependency, and the seed of the future parser port.

**Requirements:** D1, D2; contract (no `tele_ops[]`).

**Dependencies:** none.

**Files:**
- `teletype/utils/tt2_op_names.py` (create — parses `match_token.rl`, emits the C++)
- `src/apps/sequencer/engine/TT2OpNames.h` + `TT2OpNames.cpp` (generated, tracked)
- `src/apps/sequencer/CMakeLists.txt` (modify — add `TT2OpNames.cpp`)
- `src/tests/unit/sequencer/TestTT2OpNames.cpp` (create)
- `src/tests/unit/sequencer/CMakeLists.txt` (modify)

**Approach:** The generator regexes `"<NAME>"\s*=>\s*{\s*MATCH_OP\(E_OP_<X>\)` and the
`MATCH_MOD` variant out of `match_token.rl`, emitting `const char *tt2OpName(int op)`
and `tt2ModName(int mod)` as `switch` statements returning the literal name, with a
`default` returning `nullptr`. **Dedup by enum (D2a):** keep the first spelling seen per
`E_OP_*`/`E_MOD_*` and skip later ones, so multiple spellings (e.g. `W.ACT`/`WR.ACT` →
`E_OP_WR_ACT`) emit exactly one `case` — a naive line-for-line generator would emit a
duplicate `case` and fail to compile. Include `ops/op_enum.h` for the case labels.
Document the regen command at the top of the generated file (mirrors `op_enums.py`).
Run it once to produce the tracked headers.

**Patterns to follow:** `teletype/utils/op_enums.py` (generator structure, `common`
package parsing); `match_token.rl` MATCH_OP/MATCH_MOD line format.

**Test scenarios:**
- **full coverage** — every `E_OP_*` `< E_OP__LENGTH` and every `E_MOD_*` `<
  E_MOD__LENGTH` returns a non-null name. This is the **only** guard against a newly
  added enum value missing from `match_token.rl` (the `switch` `default` silently
  returns nullptr for it; the compiler won't flag it). Measured baseline: 419 op
  spellings cover 419 enums after dedup.
- dedup compiles — the generated file builds with no duplicate `case` (the
  `W.ACT`/`WR.ACT` → `E_OP_WR_ACT` collision); `tt2OpName(E_OP_WR_ACT)` returns one
  spelling
- spot-check dotted + plain: `tt2OpName(E_OP_MO_SHAPE)=="MO.SHAPE"`,
  `tt2OpName(E_OP_ADD)=="ADD"`, `tt2ModName(E_MOD_IF)=="IF"`
- out-of-range op/mod index returns nullptr (no OOB)

**Verification:** name lookup covers the full enum (coverage test green); generated file
compiles (dedup); no `tele_ops[]`/`tele_mods[]` reference anywhere in the new files.

---

### U2. Native printer — TT2Command → text + TT2Script → text

**Goal:** Reconstruct script text from lowered ops, natively.

**Requirements:** D3, D4, D5.

**Dependencies:** U1.

**Files:**
- `src/apps/sequencer/engine/TT2Printer.h` + `TT2Printer.cpp` (create)
- `src/tests/unit/sequencer/TestTT2Printer.cpp` (create)
- `src/tests/unit/sequencer/CMakeLists.txt` (modify)

**Approach:** `bool tt2PrintCommand(const TT2Command&, char *out, size_t cap)` (returns
false on truncation, D3) walks `tag[]`/`value[]`, appending per the D3 mapping. Define
`TT2_PRINT_LINE_MAX = 320`. Native compact number formatters for dec / hex / binary /
reverse-binary emit the prefixes `match_token.rl` accepts (D4) and **preserve the uint16
bit pattern** (the value is stored `int16_t`; hex/bin print the raw 16-bit pattern, as
upstream `helpers.c` does). Spacing: a space between adjacent tokens unless the next tag
is `PRE_SEP`/`SUB_SEP`. `bool tt2PrintScript(const TT2Script&, char *out, size_t cap)`
carries the **same cap→bool contract**: prints exactly commands `0..length-1`, one
`\n`-separated line each (D6), returns false if `cap` can't hold the result (no overrun,
no partial-line corruption — stop at the last line that fits). Define
`TT2_PRINT_SCRIPT_MAX = TT2_COMMANDS_PER_SCRIPT * TT2_PRINT_LINE_MAX + TT2_COMMANDS_PER_SCRIPT`
(6 lines × 320 + newlines/null ≈ **2048**); callers size to it. The public surface is
`tt2PrintCommand` + `tt2PrintScript` only — **no `tt2PrintProgram`** (whole-program text
layout is the editor's, built from per-script output). The token-tag constants
(`OP`/`NUMBER`/`XNUMBER`/`BNUMBER`/`RNUMBER`/`MOD`/`PRE_SEP`/`SUB_SEP`) come from the kept
token enum (`tele_word_t`, `command.h`) — the same tags `TT2Command` already stores;
mirror them into a small native `enum` in `TT2Printer.h` if a header dependency on
`command.h` is undesirable (decide at implementation; both are contract-clean since the
tag enum is lexer-side, not `tele_*` runtime).

**Patterns to follow:** the token-walk shape of upstream `print_command` (as a
*reference for tag handling only*, not a dependency); `core/utils/StringBuilder` usage
elsewhere in the engine; the leftmost-token order already used by the evaluator.

**Execution note:** test-first — write the per-tag formatting + round-trip tests before
the printer.

**Test scenarios:**
- each tag type formats correctly: `OP` (`MO.SHAPE`), `MOD` (`IF`), `NUMBER` (decimal,
  incl. negative), `XNUMBER` (hex), `BNUMBER` (binary), `RNUMBER` (reverse-bin),
  `PRE_SEP` (`:`), `SUB_SEP` (`;`)
- spacing: `EQ X 1` → `"EQ X 1"`; `IF EQ X 1: TR.P 1` keeps `:` tight, no space before it
- **number bit-pattern round-trips** (value stored `int16_t`, formats are uint16
  bit-pattern — these catch signed/format mistakes): `XFFFF`, `B1111111111111111`,
  `R`-pattern max and zero, and a **negative decimal** (e.g. `-1`, `-32768`) each
  `parse → lower → print → parse → lower` to an identical token
- **round-trip idempotence**: for a corpus of script lines (math, comparison, MOD with
  `:`, multi-statement with `;`, `MO.*`/`G.*`, hex/bin numbers, the alias `W.ACT`),
  `parse → lower → print → parse → lower` yields an identical `TT2Command`
  (compare `length` + `tag[]` + `value[]`)
- **alias canonicalization**: `W.ACT` and `WR.ACT` both parse to `E_OP_WR_ACT`; the
  printer emits the one canonical spelling, which re-parses to the same enum
- **command truncation**: a max-width line (16 tokens, binary literals) into a buffer
  smaller than `TT2_PRINT_LINE_MAX` returns false and does not overrun; into a
  `TT2_PRINT_LINE_MAX` buffer returns true and fits
- `tt2PrintScript` prints exactly `length` lines, `\n`-separated, index==line (D6);
  empty script (`length 0`) → no lines (empty output, returns true)
- **script truncation**: a full 6-line script of max-width commands into a buffer
  smaller than `TT2_PRINT_SCRIPT_MAX` returns false and does not overrun (stops at the
  last whole line that fits, no partial line); into a `TT2_PRINT_SCRIPT_MAX` buffer
  returns true and fits all 6 lines
- a command using every supported tag round-trips

**Verification:** round-trip idempotent over the corpus incl. the bit-pattern + alias
cases; truncation returns status (no overrun); printer references no `tele_*` runtime;
builds + STM32 release links.

---

## Risks

- **Name/enum drift.** Mitigated by D1 — names generated from the same `match_token.rl`
  Ragel compiles; adding an op regenerates both. The U1 full-coverage test fails loudly
  if the generated table lags the enum.
- **Number-syntax mismatch (D4).** If a native formatter emits a prefix the lexer
  doesn't accept, the round-trip breaks — caught by the U2 idempotence test, not latent.
- **Tag-enum coupling.** The printer needs the token-tag constants; they live in
  `command.h` (kept, lexer-side). If `command.h` is later split, mirror the 8 tag values
  into `TT2Printer.h`. Low risk, flagged.

---

## Verification (overall)

- Native `tt2OpName`/`tt2ModName` cover the full op/mod enums, generated from `match_token.rl`.
- `tt2PrintCommand`/`tt2PrintScript` reconstruct text; `parse→lower→print→parse→lower`
  is idempotent over the test corpus.
- No new **printer** dependency on `tele_ops[]`/`tele_mods[]`/`print_command`. The
  forward path (`TT2ScriptLoader::loadScriptText`, `TT2ScriptLoader.h:43`) and the
  round-trip tests still call the kept Ragel `parse()` — that's the lexer, not the
  dying `tele_*` runtime, and is in scope to keep.
- Full `TestTeletypeV2*` + new tests + STM32 release green.
