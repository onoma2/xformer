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

Give `TrackMode::TeletypeV2` an on-device editor by repointing the existing Teletype
edit pages (`TeletypeScriptViewPage`, `TeletypePatternViewPage`) to the **native TT2
data layer** — **single backend, native-only**. The pages bind `scene_state_t` today;
this plan replaces that editing path with the native one and **retires legacy Teletype
as an editable/selectable track mode**, ending the dual-track-type situation. The bridge
engine + C VM are kept *only* as the parity harness's reference, then deleted once parity
is locked.

**Decided architecture (2026-06-15 — native-only, parity-gated teardown):**
- **Option B lexer** — keep the Teletype **Ragel lexer** (`parse()`) for text input; display via the **native `TT2Command → text` printer** (`TT2Printer` + generated `TT2OpNames`, plan 007 — shipped). No `tele_command_t` round-trip, no `print_command`, no `tele_ops[]`.
- **Native-only, not coexist** — the editor pages drive TT2 **only** (single path, no `if(TT2){…}else{orig}` branches). `TrackMode::Teletype` is removed as a selectable/editable mode. One live Teletype track type.
- **Parity-gated teardown** — `TeletypeTrackEngine` + `scene_state_t` C VM stay compiled **solely** as the `TestTeletypeV2Parity` reference until **tick-level parity is locked**, then the whole bridge is deleted. (You can't diff against an engine you've already removed.)
- **Fallback:** tag `tt1-working-breakpoint` (`b2b64191`) marks the last commit with TT1 fully working/selectable — the pre-teardown reference.

---

## Scope Boundaries

**In scope:** native per-line script edit API (shipped); **TT2-only** repoint of the two pages; TopPage/TrackPage routing for TeletypeV2 (shipped); remove `Teletype` from selectable modes; **tick-level parity gate**; **bridge deletion** once parity locked.

### Deferred to Follow-Up Work
- **I/O routing grid** — TT2 has no I/O-routing data model yet; the editor omits the I/O grid until that model exists.
- **File save/load** — `FileManager` Teletype script/track overloads for TT2.

### Outside scope
- No dual-backend coexist path (decision reversed 2026-06-15).

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

### U3. Repoint TeletypeScriptViewPage to TT2 (native-only, single path)

**Goal:** The script view/edit/live page is the **native TT2 script editor** — one
backend, no dual branch.
**Dependencies:** U2 (U1 shipped via plan 007).
**Files:** `src/apps/sequencer/ui/pages/TeletypeScriptViewPage.cpp` / `.h`.
**Approach:** **Replace** the `scene_state_t` editing/display/live sites (~10: `ss_get_script_command`/`_len`, `print_command`, `ss_overwrite/insert/delete`, `ss_*_comment`, `process_command`) with the native path — no `if(TT2){…}else{orig}` branch, since TT2 is the only Teletype type: display = `tt2PrintScript`/`tt2PrintCommand` over `tt2Track().program()`; commit = U2 `set/insert/deleteScriptCommand`; live = parse → lower → `evaluateCommand` against the track's `TT2Runtime`/`TT2OutputState`. Comments have no TT2 equivalent (stripped at parse) → drop the comment affordance. I/O grid omitted (deferred). Keep navigation/edit-buffer/key-handling UI logic.
**Execution note:** **sim/on-device verification.** This is interactive OLED UI with no unit-testable surface (the testable logic is U1/U2, shipped). Each repointed site is verified by rendering/editing/live-firing a TT2 track on the simulator; a headless build only proves it compiles/links.
**Patterns to follow:** the page's own structure; `runScript`/`evaluateCommand` for live exec.
**Test scenarios:** `Test expectation: none (UI page)` — sim render + on-device. Manual: select a TT2 track → lines render via the native printer; edit → parse/lower/persist/re-render; live-fire → native runner drives CV/gate.

### U4. Repoint TeletypePatternViewPage to TT2 (native-only, single path)

**Goal:** Pattern view/edit drives `program.patterns[]` natively — single backend.
**Dependencies:** U2.
**Files:** `src/apps/sequencer/ui/pages/TeletypePatternViewPage.cpp` / `.h`.
**Approach:** Replace the `scene_state_t` pattern get/set/len sites with direct
`program.patterns[col]` access (add any model accessors needed). No TrackMode branch.
**Execution note:** sim/on-device verification (interactive UI).
**Test scenarios:** `Test expectation: none (UI page)` — sim + on-device. Manual: TT2 pattern values display and edit against `program.patterns[]`.

### U5. TopPage routing + Pages registry for TeletypeV2

**Goal:** Selecting/navigating a TeletypeV2 track opens the (now dual) pages.
**Dependencies:** U3, U4.
**Files:** `src/apps/sequencer/ui/pages/TopPage.cpp`, `src/apps/sequencer/ui/pages/TrackPage.cpp` (the `switch (track.trackMode())` that today lacks a TeletypeV2 case — see the build warning at `TrackPage.cpp:167`), `src/apps/sequencer/ui/Pages.h` (only if a new page instance is needed — likely not, pages are reused).
**Approach:** Add `case Track::TrackMode::TeletypeV2:` to `TopPage`'s `setTrackView`/`setSequenceView`/`setSequenceEditPage` **and** to `TrackPage`'s trackMode switch, mirroring the `Teletype` cases (route to the same `teletypeScriptView`/track page). No new page objects if existing instances are reused. This is the enum-handling gap that currently leaves a TeletypeV2 track with no editor route.
**Test scenarios:** `Test expectation: none (UI routing)` — verified on sim/device: a TeletypeV2 track reaches the script + pattern pages via the same keys as orig Teletype; no fall-through to a blank page.

### U6. Parity harness (orig C VM vs TT2 native runner) — ✅ shipped (the gate)

**Status: done — two layers, the correct equivalence bar.**
- **Op-value parity** (`TestTeletypeV2Parity`, prior) — deterministic op surface through
  both VMs, equal return values (caught reversed bitwise operands + a TT1 `QT` bug).
- **Output-value parity** (`TestTeletypeV2OutputParity`, this unit) — identical command
  text through both VMs, asserting **CV/gate target values match** over the shared 1–4
  output range: `ss.variables.cv[i]/tr[i]` vs `out.cv[i].targetRaw / out.tr[i].level`.

**Tick-shaped identity was rejected as the wrong bar.** The original ambition was a
tick-by-tick CV/gate diff, but TT2's output shaping (slew curve, TR pulse timing) is a
**native reimplementation, intentionally not legacy-identical** — an engine-level tick
diff would (a) need a full `Engine` for both backends and (b) **fail on those intended
shaping differences**. So the gate is: op-value + output-value parity (both engines
*compute* the same outputs) **plus** TT2's shaping verified against its own spec
(`TestTeletypeV2OutputShaping`, existing) **plus** on-device audition. That set is the
**bridge-deletion confidence gate** for U8 — it does not require a legacy tick clone.

Harness boundary found in execution: legacy = **4 CV / 4 TR** (`CV_COUNT`/`TR_COUNT`);
TT2 extends to **8** (Performer) — a deliberate feature, so parity is asserted only over
the shared 1–4 range. The full-Engine construction the tick approach needed is feasible
in tests (Harmony tests do it) but was not the right tool here.

**Files:** `src/tests/unit/sequencer/TestTeletypeV2OutputParity.cpp`,
`src/tests/unit/sequencer/TestTeletypeV2Parity.cpp`, `…/CMakeLists.txt`.
**Verification:** both harnesses green; they run headless against the C VM (no Engine).

### U7. Remove `Teletype` as a selectable / editable track mode

**Goal:** One live Teletype track type (TT2). Legacy `Teletype` is no longer offered.
**Dependencies:** U3, U4 (TT2 editor must work before removing the old mode).
**Files:** `TrackModeListModel` / wherever the selectable `TrackMode` list is built; any
UI that lists/labels `TrackMode::Teletype`. **Not** the engine yet.
**Approach:** Drop `Teletype` from the selectable-mode list/labels so it can't be chosen.
Keep the `TrackMode::Teletype` enum value, `TeletypeTrackEngine`, and the `scene_state_t`
C VM **compiled** — they remain solely as the `TestTeletypeV2Parity` reference until U8.
No-migration: existing projects with a Teletype track are not converted (dev files break).
**Test scenarios:** `Test expectation: none (UI)` — sim: `Teletype` absent from mode
select; `TeletypeV2` present and editable. `TestTeletypeV2Parity` still builds + passes
(the C VM is still linked).

### U8. Delete the bridge — parity-gated (the teardown)

**Goal:** Remove the legacy engine entirely once the native runner is proven equivalent.
**Dependencies:** **U6 parity gate satisfied** (op-value + output-value parity green +
TT2 shaping spec + on-device audition sign-off), U7. Note: deleting the bridge also
removes the C VM the parity harnesses diff against, so they retire with it — run them
green one last time immediately before deletion.
**Files:** delete `TeletypeTrackEngine.*`, `TeletypeBridge.*`, the `scene_state_t` VM
usage, `TrackMode::Teletype` enum + all its `switch` cases, `TeletypeTrackListModel`, the
orig page bindings; drop the now-unused C-VM sources from the build. The Ragel
lexer/`parse()` + op tables stay (the native dialect uses them).
**Approach:** Mechanical removal after parity is locked. The parity harness is retired in
the same step (its reference engine is gone) — its job is done once parity is confirmed.
Re-tag/branch from `tt1-working-breakpoint` is the fallback if anything regresses.
**Execution note:** **Do not start U8 until U6 tick-level parity passes.** This is the
irreversible step; the breakpoint tag is the safety net.
**Test scenarios:** full `TestTeletypeV2*` + STM32 release green after removal; no
`scene_state_t`/`TeletypeTrackEngine` symbols remain; flash drops by the removed bridge.

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
