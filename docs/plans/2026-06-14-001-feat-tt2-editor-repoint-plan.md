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
- **Option B** — keep the Teletype **Ragel lexer** (`parse()`) for text input (trusted dialect spec, already ships); display via a **native `TT2Command → text` printer** (walks `TT2Command` tags/values + a **names-only** op-id→name table; **no** `tele_command_t` round-trip, **no** `print_command`, and crucially **not** `tele_ops[]` — see Key Decisions).
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

- **Native `tele2_ops[]` op registry — NOT `tele_ops[]`, NOT a parallel names-only array.** `print_command` reads `tele_ops[id]->name`, but `tele_ops[]` carries function pointers into the C op implementations — referencing it would relink the foreign C op code (and `scene_state_t` deps) TT2 dropped, bloating flash. A separate names-only `const char*[]` would avoid that but duplicate the existing function-pointer `tt2NativeOpTable` (two arrays per id, hand-synced). Instead, build **`tele2_ops[]`** — TT2's own native registry, one entry per `E_OP` id holding `{ name, handler }`, replacing the bare `tt2NativeOpTable`. Single source of truth: printer reads `.name`, evaluator dispatches `.handler`. Native, no C-op relink. Names are hand-written alongside each registration (`{ "ADD", opAdd }`), self-documenting; one canonical name per id (aliases like `+` are distinct ids). Arity stays implicit for now (per-handler `stackSize` checks) — a `tele2_ops[].arity` field is a noted **future** cleanup, out of scope here.
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

### U1. Native `tele2_ops[]` op registry + TT2Command→text printer

**Goal:** Consolidate the op table into a native `{ name, handler }` registry, and render a `TT2Command` to its source-text line from it — the display keystone.
**Dependencies:** none.
**Files:** `src/apps/sequencer/engine/TeletypeNativeOps.cpp` (restructure the op table → `tele2_ops[]`), `src/apps/sequencer/engine/TT2Evaluator.h` (dispatch via `tele2_ops[value].handler` instead of `tt2NativeOpTable[value]`), a printer in `TT2ScriptLoader.h` or new `TT2CommandPrinter.h`, `src/tests/unit/sequencer/TestTeletypeV2Printer.cpp` (new), `src/tests/unit/sequencer/CMakeLists.txt`.
**Approach:** Define a `Tele2Op { const char *name; TT2OpFunc handler; }` array sized `E_OP__LENGTH`; migrate the existing `table[E_OP_X] = opX;` registrations to `{ "X", opX }` entries (canonical name per id; nullptr name/handler for unimplemented ids). Repoint the evaluator's dispatch to `tele2_ops[value].handler`. The printer walks `tag[]`/`value[]`: NUMBER/XNUMBER/BNUMBER/RNUMBER → int/hex/bin/rbin; OP → `tele2_ops[value].name`; SEP/PRE_SEP → `;`/`:`. Must NOT reference `tele_ops[]` (Key Decisions). May split into "registry migration" + "printer" commits during execution if cleaner.
**Patterns to follow:** the current `OpTableBuilder` in `TeletypeNativeOps.cpp` (what's being restructured); `print_command` in `teletype/src/command.c` for token-walk + number-formatting *shape only* (do not call it); `lowerCommand` in `src/apps/sequencer/model/TeletypeProgram.h` for the tag/value layout.
**Test scenarios:**
- Round-trip: `text → parse → lowerCommand → print` equals the normalized text, for: a bare op (`CV 1 5`), nested ops (`CV 1 ADD 100 5`), a mod-prefixed line (`IF X: TR 1 1`), symbols (`+ 1 2`, `>= 4 5`), negative numbers (`N -12`), a multi-segment line (`A 1; B 2`).
- Pitch/pattern ops render with correct names (`P 0`, `P.NEXT`, `N 0`).
- Empty command (`length==0`) → empty string.
- Op id with no name entry → does not crash (renders a placeholder), proving the table is bounds-safe.
- **Registry-migration regression guard:** the full existing `TestTeletypeV2*` suite stays green after the `tt2NativeOpTable` → `tele2_ops[]` dispatch restructure — every wired op behaves identically (the migration changes the table's shape, not any handler).

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
**Dependencies:** U1, U2.
**Files:** `src/apps/sequencer/ui/pages/TeletypeScriptViewPage.cpp` / `.h`.
**Approach:** At each data touch, branch on `selectedTrack().trackMode()`: orig path unchanged (`scene_state_t`); TT2 path uses `tt2Track().program()` + U1 printer (display) + U2 edit API (commit). Live mode: TT2 branch parses → lowers → runs one command via the native evaluator against the track's `TT2Runtime`/`TT2OutputState` (vs orig `process_command`). I/O grid: TT2 branch hides/omits it (deferred). Keep all navigation/edit-buffer/key-handling UI logic shared.
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
**Files:** `src/apps/sequencer/ui/pages/TopPage.cpp`, `src/apps/sequencer/ui/Pages.h` (only if a new page instance is needed — likely not, since pages are reused).
**Approach:** Add `case Track::TrackMode::TeletypeV2:` to `setTrackView`, `setSequenceView`, `setSequenceEditPage` mirroring the `Teletype` cases (route to the same `teletypeScriptView` / track page). No new page objects if the existing instances are reused.
**Test scenarios:** `Test expectation: none (UI routing)` — verified on sim/device: a TeletypeV2 track reaches the script + pattern pages via the same keys as orig Teletype; no fall-through to a blank page.

### U6. Behavioral parity harness (orig C VM vs TT2 native runner)

**Goal:** Exact, repeatable proof that a script produces the same CV/gate under both engines.
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
