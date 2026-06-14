---
id: feat-tt2-editor-repoint
schema: plan
title: "feat: TT2 on-device editor (repoint, transitional coexist + parity harness)"
type: feat
status: active
date: 2026-06-14
depth: standard
---

# feat: TT2 on-device editor (repoint, transitional coexist + parity harness)

## Summary

Give `TrackMode::TeletypeV2` an on-device editor by **reusing the existing,
working Teletype edit pages** (`TeletypeScriptViewPage`, `TeletypePatternViewPage`
— line editing, live mode, pattern editing all already work) rather than rebuilding
them. The pages today bind to the original Teletype's `scene_state_t` C-API; this
plan adds a **native TT2 data layer** beneath them and makes them **TrackMode-aware**
so the same UI drives either backend.

**Decided architecture (do not re-litigate):**
- **Option B** — keep the Teletype **Ragel lexer** (`parse()`) for text input (trusted dialect spec, already ships); display via the **native `TT2Command → text` printer** — **shipped** as `TT2Printer` + the generated `TT2OpNames` registry (plan 007). No `tele_command_t` round-trip, no `print_command`, no `tele_ops[]`. Names are **generated from `match_token.rl`**, not a hand-written table (see Key Decisions).
- **Transitional coexist** — orig Teletype (`scene_state_t`) and TT2 (native) backends live side-by-side behind a TrackMode branch in the pages, so both can be edited/run on one device for comparison. The orig backend + orig Teletype track mode are torn out in a **scheduled follow-up** once parity is confirmed (native-only end state).
- **Behavioral parity harness** — run identical script text through the orig C VM and the TT2 native runner, diff CV/gate output tick-by-tick. The rigorous, repeatable equivalence check; survives every future op change.

---

## Scope Boundaries

**In scope:** native printer + names table; native per-line script edit API on `TeletypeProgram`; TrackMode-aware rebind of the two pages (incl. live mode → native runner for TT2 tracks); TopPage routing + Pages registry for TeletypeV2; the parity harness.

### Deferred to Follow-Up Work
- **Teardown unit** — once parity is confirmed, delete the orig `scene_state_t` backend branches and retire `TrackMode::Teletype`, collapsing the pages to native-only. The coexist branch is a temporary scaffold with a scheduled death.
- **I/O routing grid** — TT2 has no I/O-routing data model (orig uses `TeletypeTrack::PatternSlot` mappings). The TT2 path omits the I/O grid until that model exists.
- **File save/load** — `FileManager` Teletype script/track overloads for TT2.

---

## Key Technical Decisions

- **Native names registry — generated, NOT `tele_ops[]`, NOT a hand-written merge (SHIPPED, decision revised).** `print_command` reads `tele_ops[id]->name`, but `tele_ops[]` carries function pointers into the C op implementations — referencing it relinks the foreign C op code TT2 dropped. 001 originally proposed merging into `tele2_ops[]{name,handler}` (hand-written names, replacing `tt2NativeOpTable`). **Revised on the way to shipping (plan 007):** the name side is **generated from `match_token.rl`** (`TT2OpNames`: `tt2OpName`/`tt2ModName`), the handler side stays `tt2NativeOpTable` (untouched). This avoids both the C-op relink *and* hand-sync drift — names are derived from the same grammar Ragel compiles, so they can't disagree with the lexer. No merge, no dispatch restructure. The hand-written `{name,handler}` array is **not** built. (Arity stays implicit / per-handler `stackSize` checks; a future cleanup if ever needed.)
- **Per-line edit is cheap and native.** Edit a line = `parse(text)` (Ragel) → `lowerCommand` → store at `program.scripts[s].commands[line]`. TT2 already has `parse` + `lowerCommand`; only thin set/insert/delete helpers on `TeletypeProgram` are new. No `tele_command_t` persists past the lower step.
- **Live mode reroutes to the native runner.** Orig live exec is `process_command(scene_state_t)`; the TT2 path parses → lowers → runs one command via the native evaluator (`evaluateCommand`) / `runScript`, against the track's `TT2Runtime`/`TT2OutputState`.
- **Coexist via a TrackMode branch, not an abstraction layer.** Inline `if (trackMode == TeletypeV2) { native } else { orig }` at each data touch — trivially deletable at teardown. No adapter interface (avoids premature abstraction; the dual path is temporary).

---

## High-Level Technical Design

This illustrates the intended approach and is directional guidance for review, not implementation specification.

```
              ┌─────────────────────────────────────────┐
              │  TeletypeScriptViewPage / PatternViewPage │  (existing UI, reused)
              │  draw / keys / editBuffer / live mode     │
              └───────────────┬───────────────────────────┘
                              │ TrackMode branch (temporary)
              ┌───────────────┴───────────────┐
        orig  │                                │  TeletypeV2
   ┌──────────▼─────────┐          ┌───────────▼──────────────────┐
   │ scene_state_t C-API │          │ NATIVE TT2 data layer (new)  │
   │ ss_*_script_command │          │ • printer: TT2Command→text   │
   │ print_command       │          │   (names-only op table)      │
   │ process_command     │          │ • per-line set/ins/del       │
   │ ss_*_pattern_val    │          │   (parse→lowerCommand)       │
   └─────────────────────┘          │ • pattern access program[]   │
                                     │ • live: evaluateCommand      │
                                     └──────────────────────────────┘

   Parity harness (separate, no UI): script text ─┬─► orig C VM  ──► CV/gate ─┐
                                                   └─► TT2 runner ──► CV/gate ─┴─► diff
```

---

## Implementation Units

### U1. Native op-name registry + TT2Command→text printer — ✅ SHIPPED (plan 007)

**Status: done** via `docs/plans/2026-06-14-007-feat-tt2-native-printer-plan.md`
(`TT2OpNames` registry + `TT2Printer`). The display keystone exists; U3 consumes it.

**Design note — the `tele2_ops[]` merge was reversed (superseded).** 001 proposed
merging names into the handler table (`tele2_ops[]{name,handler}` replacing
`tt2NativeOpTable`, names hand-written per registration). Shipped instead:
- **Names** — `TT2OpNames` (`tt2OpName`/`tt2ModName`), **generated** from
  `teletype/src/match_token.rl` (the kept Ragel grammar). Names can't drift from the
  lexer and aren't hand-synced.
- **Handlers** — `tt2NativeOpTable` **unchanged** (no restructure, no migration
  regression risk).
- **Printer** — `tt2PrintCommand`/`tt2PrintScript` (`TT2Printer.h`), bounded
  `cap→bool`, native dec/hex/bin/rev-bin, **no `tele_ops[]`/`print_command`**.

Two single-source structures (names from the grammar, handlers from registration) beat
one hand-written merged array; the merge bought nothing the generator doesn't. Round-trip
idempotence, alias canonicalization, truncation, and full op/mod coverage are tested
(`TestTT2OpNames`, `TestTT2Printer`); existing `TestTeletypeV2*` stayed green (no handler
restructure). The `Tele2Op{name,handler}` array and the dispatch repoint are **not**
needed.

### U2. Native per-line script edit API + pattern accessors on TeletypeProgram

**Goal:** The mutation primitives the pages need on the TT2 program.
**Dependencies:** none.
**Files:** `src/apps/sequencer/model/TeletypeProgram.h` (or `TT2ScriptLoader.h`), `src/tests/unit/sequencer/TestTeletypeV2ScriptEdit.cpp` (new).
**Approach:** `setScriptCommand(program, s, line, text)` (parse→lowerCommand→store, return parse error if any), `insertScriptCommand`/`deleteScriptCommand` (shift `commands[]`, adjust `scripts[s].length`, bounds to `TT2_COMMANDS_PER_SCRIPT`). Pattern read/write helpers likely already exist (`patternVal`) — add any the pattern page needs (len/idx access). No `tele_command_t` retained.
**Patterns to follow:** orig `ss_overwrite/insert/delete_script_command` for shift/length semantics; existing `loadScriptText` for the parse→lower pipeline.
**Test scenarios:**
- `setScriptCommand` on a blank line stores the lowered command; reading it back via the U1 printer round-trips.
- Insert at line N shifts lines down, bumps `length` (capped at `TT2_COMMANDS_PER_SCRIPT`); delete shifts up, drops `length`.
- Invalid text → returns parse error, leaves the script unchanged.
- Edit at/over the command cap is a safe no-op (no overflow).

### U3. Rebind TeletypeScriptViewPage — TrackMode-aware (orig | TT2)

**Goal:** The script view/edit/live page drives a TT2 track when selected, orig otherwise.
**Dependencies:** U2 (U1 shipped via plan 007).
**Files:** `src/apps/sequencer/ui/pages/TeletypeScriptViewPage.cpp` / `.h`.
**Approach:** At each data touch, branch on `selectedTrack().trackMode()`: orig path unchanged (`scene_state_t`); TT2 path uses `tt2Track().program()` + the **`TT2Printer`** (`tt2PrintScript`/`tt2PrintCommand`, display) + U2 edit API (commit). Live mode: TT2 branch parses → lowers → runs one command via the native evaluator against the track's `TT2Runtime`/`TT2OutputState` (vs orig `process_command`). I/O grid: TT2 branch hides/omits it (deferred). Keep all navigation/edit-buffer/key-handling UI logic shared. **Stale-page guard:** the TrackMode branch must resolve before any type-specific accessor runs — a TT2 track must never reach `scene_state_t` calls and vice versa (mirrors the PROJECT.md stale-page protection).
**Execution note:** Characterize first — capture the orig page's current behavior (it has no unit tests) before threading branches, so the orig path is provably unchanged.
**Patterns to follow:** the page's own existing structure; the TT2 engine's `runScript`/`evaluateCommand` for live exec.
**Test scenarios:** `Test expectation: none (UI page)` — verified via sim render + the parity harness (U6) + on-device. The testable logic lives in U1/U2. Manual: select a TT2 track → script lines render via the native printer; edit a line → parses, lowers, persists, re-renders; live-fire a line → native runner drives CV/gate; orig Teletype track still edits exactly as before.

### U4. Rebind TeletypePatternViewPage — TrackMode-aware

**Goal:** Pattern view/edit drives `program.patterns[]` for TT2, `scene_state_t` for orig.
**Dependencies:** U2.
**Files:** `src/apps/sequencer/ui/pages/TeletypePatternViewPage.cpp` / `.h`.
**Approach:** Same TrackMode branch on pattern get/set/len; TT2 path reads/writes `program.patterns[col]` directly.
**Execution note:** Characterize the orig pattern page behavior before branching.
**Test scenarios:** `Test expectation: none (UI page)` — verified via sim + on-device. Manual: TT2 track pattern values display and edit against `program.patterns[]`; orig unchanged.

### U5. TopPage routing + Pages registry for TeletypeV2

**Goal:** Selecting/navigating a TeletypeV2 track opens the (now dual) pages.
**Dependencies:** U3, U4.
**Files:** `src/apps/sequencer/ui/pages/TopPage.cpp`, `src/apps/sequencer/ui/pages/TrackPage.cpp` (the `switch (track.trackMode())` that today lacks a TeletypeV2 case — see the build warning at `TrackPage.cpp:167`), `src/apps/sequencer/ui/Pages.h` (only if a new page instance is needed — likely not, pages are reused).
**Approach:** Add `case Track::TrackMode::TeletypeV2:` to `TopPage`'s `setTrackView`/`setSequenceView`/`setSequenceEditPage` **and** to `TrackPage`'s trackMode switch, mirroring the `Teletype` cases (route to the same `teletypeScriptView`/track page). No new page objects if existing instances are reused. This is the enum-handling gap that currently leaves a TeletypeV2 track with no editor route.
**Test scenarios:** `Test expectation: none (UI routing)` — verified on sim/device: a TeletypeV2 track reaches the script + pattern pages via the same keys as orig Teletype; no fall-through to a blank page.

### U6. Behavioral parity harness (orig C VM vs TT2 native runner) — ✅ largely shipped

**Status: op-level done; tick-level still owed (it's the bridge-deletion gate).** The
existing `TestTeletypeV2Parity` runs the deterministic op surface through both the legacy
C VM and the native runner, asserting equal **return values** (it caught reversed bitwise
operands + a TT1 `QT` bug). But it **excludes** RNG/stateful/**engine** ops — so it proves
*computation* parity, **not** that the native engine reproduces the legacy **output over
time** (CV slew/offset, TR pulse/polarity, metro/delay timing, trigger firing — the actual
voltages and gate edges).

Tick-level CV/gate parity covers exactly that uncovered layer, and it is the
**confidence gate for bridge deletion** (Phase 5: tear out the legacy engine "once parity
is confirmed" — confirmed *by this*). It is **not needed for the editor to function**
(display/edit/route/run don't depend on it), so it does not block U2–U5. But it is **not
optional** for retiring the bridge — that step needs the tick-level harness (or an
explicit on-device audition sign-off) first. Deferred to the bridge-deletion effort, not
dismissed.

**Dependencies:** none (runtime-only; can land independently).
**Files:** `src/tests/unit/sequencer/TestTeletypeV2Parity.cpp` (new), `src/tests/unit/sequencer/CMakeLists.txt`.
**Approach:** For a set of representative scripts, feed identical text to both: orig path (parse → run via the orig Teletype engine/`process_command`) and TT2 path (`loadScriptText` → `runScript`). Step N ticks; compare CV (raw) + gate output per tick. Report first divergence. Resolve at execution time how to construct the orig engine in a test (it may need an `Engine` context the TT2 runner doesn't) — if the orig engine is too heavy to host in a unit test, drive both at the script/op level instead and compare the output state.
**Execution note:** Implement test-first — the harness IS the test.
**Test scenarios:**
- Identical output for: arithmetic/logic (`A ADD 1 2; CV 1 A`), pitch (`CV 1 N 12`), a metro/every script, a pattern read (`CV 1 P 0`), an IF/PROB line.
- Divergence is reported with the tick + channel, not a silent pass.
- Seeded RNG ops (TOSS/P.RND) — compare only deterministic scripts, or seed both identically; document which scripts are excluded.

---

## System-Wide Impact

- The two edit pages gain a TrackMode branch (temporary). Orig Teletype editing must stay byte-for-byte unchanged — the characterization notes (U3/U4) guard that.
- No change to the TT2 runtime/ops/data model (all landed). No serialization change.
- Flash: the names-only printer table is small; explicitly avoiding `tele_ops[]` keeps the C op implementations unlinked. Confirm STM32 release still links (post-`-Os` there's ~350 KB headroom).

---

## Risks & Mitigations

- **Accidentally relinking the C op table** via `tele_ops[]` for names → flash bloat + foreign dep. Mitigation: names-only table (U1 Key Decision); check the STM32 map for `tele_ops`/C-op symbols after U1.
- **Orig editing regression** from threading branches through a 1200-LOC page. Mitigation: characterize-first (U3/U4), orig path left structurally intact.
- **Parity harness construction** — orig engine may be hard to host in a unit test. Mitigation: U6 fallback to script/op-level comparison; noted as execution-time.
- **Coexist scaffold outliving its purpose.** Mitigation: teardown is an explicit deferred follow-up, not "someday."

---

## Verification

- U1/U2/U6 unit tests green (printer round-trip, edit API, parity).
- Full `TestTeletypeV2*` suite + STM32 release green.
- On device / sim: a TeletypeV2 track is selectable, its scripts render + edit + live-fire via the native path; a Teletype (orig) track in the same project still edits and runs unchanged → the two are directly comparable.
- Parity harness reports zero divergence on the representative script set.
- Execution posture: U1/U2/U6 test-first; U3/U4 characterize-first then rebind.
