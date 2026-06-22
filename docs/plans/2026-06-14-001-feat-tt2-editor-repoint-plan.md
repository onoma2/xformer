---
id: feat-tt2-editor-repoint
schema: plan
title: "feat: TT2 on-device editor (native-only strip+rebind, parity-gated teardown)"
type: feat
status: active
date: 2026-06-14
depth: standard
---

# feat: TT2 on-device editor (native-only strip+rebind, parity-gated teardown)

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
- **Parity-gated teardown** — `TeletypeTrackEngine` + `scene_state_t` C VM stay compiled **solely** as the parity reference until **the U6 gate is satisfied** (op-value parity + output-value parity green, TT2 shaping tests green, on-device audition signed off — **not** tick-level identity, which U6 rejected as the wrong bar), then the whole bridge is deleted. (You can't diff against an engine you've already removed.)
- **Fallback:** tag `tt1-working-breakpoint` (now at HEAD) marks the last commit with TT1 fully working/selectable — the pre-teardown reference.

---

## Scope Boundaries

**In scope:** native per-line script edit API (shipped); **TT2-only strip + rebind** of the
two pages; host-scoped live exec/trigger entry points on the engine; TopPage/TrackPage
routing for TeletypeV2 (shipped); remove `Teletype` from selectable modes; the parity gate;
**bridge deletion** once parity locked.

**"Repoint" is a strip + rebind, not a data-call swap (discovered in execution).** Both
pages are *legacy Teletype-track* editors wired to five subsystems with **no TT2 model**.
Native-only means **removing** those subsystems and rebinding the core to native — a partial
rewrite of each page, not ~10 line edits. The track-agnostic UI (edit buffer, cursor, step
char-entry, history, keyboard) is kept; the Teletype-track plumbing is cut.

| Subsystem | Legacy binding | Fate |
|-----------|----------------|------|
| Slot/clip store | `activePatternSlot`, `captureActiveClip`, `SlotScriptIndex`, `setTeletypePattern`, `P%d` labels | **Strip** (TT2 has no slot/clip store) |
| SD file I/O | `writeTeletypeScript/Track` + the SAVE/LOAD context menu | **Strip** (deferred; no TT2 `FileManager` overloads) |
| I/O routing grid | `drawIoGrid`/`drawBipolarBar`, `busCv`/`cvRaw`/`gateOutput`/`triggerInputSource`/`cvInSource` | **Strip** (deferred; TT2 I/O model not wired to engine) |
| Hardware dashboard | `dashboardScreen`/`get_dashboard_value` | **Strip** (no TT2 equivalent) |
| Comments | `ss_*_comment`, `commentLine`, comment toggle | **Strip** (lowering drops comments) |

**Rebound, not stripped:** the live indicator row (metro `m_act` / per-CV slew / delays /
stack), the turtle (pattern page), and the vars panel all map onto existing `TT2Runtime`
fields (`variables`, `turtle`, `stack`, `delay`, `cv_slew`) — kept, rebound to the runtime.

### Deferred to Follow-Up Work
- **I/O routing-*assignment* editing** — choosing which input feeds each trigger and which physical output each slot owns needs the TT2 I/O data model (not yet wired). Only the *editing/ownership* half of the legacy grid is deferred. (Correction 2026-06-15: U3 originally stripped the **whole** right-side overlay, including the live-value readout. That over-stripped — the readout is restorable now (U3b). Only assignment editing waits on the model.)
- **File save/load** — `FileManager` Teletype script/track overloads for TT2 + a TT2 context menu.

### Restored to scope (correction)
- **U3b — live-value HUD.** CV/TR/IN/PARAM/BUS *activity* (read from `output()`, `runtime.variables`, `_engine.busCv`) is the readout you watch while auditioning; it has TT2 data today and must be present for the U3/U4 audition. Layout for TT2's 8 outputs (vs legacy 4) designed via `ui-preview` first.

### Outside scope
- No dual-backend coexist path (decision reversed 2026-06-15).

---

## Key Technical Decisions

- **Native names registry — generated, NOT `tele_ops[]`, NOT a hand-written merge (SHIPPED, decision revised).** `print_command` reads `tele_ops[id]->name`, but `tele_ops[]` carries function pointers into the C op implementations — referencing it relinks the foreign C op code TT2 dropped. 001 originally proposed merging into `tele2_ops[]{name,handler}` (hand-written names, replacing `tt2NativeOpTable`). **Revised on the way to shipping (plan 007):** the name side is **generated from `match_token.rl`** (`TT2OpNames`: `tt2OpName`/`tt2ModName`), the handler side stays `tt2NativeOpTable` (untouched). This avoids both the C-op relink *and* hand-sync drift — names are derived from the same grammar Ragel compiles, so they can't disagree with the lexer. No merge, no dispatch restructure. The hand-written `{name,handler}` array is **not** built. (Arity stays implicit / per-handler `stackSize` checks; a future cleanup if ever needed.)
- **Per-line edit is cheap and native.** Edit a line = `parse(text)` (Ragel) → `lowerCommand` → store at `program.scripts[s].commands[line]`. TT2 already has `parse` + `lowerCommand`; only thin set/insert/delete helpers on `TeletypeProgram` are new. No `tele_command_t` persists past the lower step.
- **Live mode reroutes to the native runner.** The legacy live exec was `process_command(scene_state_t)`; the TT2 path parses → lowers → runs one command via the native evaluator (`evaluateCommand`) / `runScript`, against the track's `TT2Runtime`/`TT2OutputState`.
- **Single path, no TrackMode branch.** Each page drives TT2 **only** — the `scene_state_t` calls are *replaced*, not branched. There is no `if(TT2){…}else{legacy}`: the page no longer edits legacy Teletype at all (legacy is removed from selection in U7, and the editor pages stop touching it here). The legacy includes (`TeletypeBridge`/`TeletypeTrackEngine`/`state.h`) are dropped from the pages. The C VM survives only as the U6 parity reference, reachable from the harness, not from the UI.

---

## High-Level Technical Design

This illustrates the intended approach and is directional guidance for review, not implementation specification.

```
   ┌─────────────────────────────────────────────┐
   │  TeletypeScriptViewPage / PatternViewPage     │  (kept UI: draw / keys /
   │  TT2-only — legacy subsystems stripped         │   editBuffer / live mode)
   └───────────────┬───────────────────────────────┘
                   │  single path (no branch)
       ┌───────────▼──────────────────┐
       │ NATIVE TT2 data layer         │
       │ • printer: TT2Command→text    │  (TT2OpNames + TT2Printer, shipped)
       │ • per-line set/ins/del        │  (parse→lowerCommand, shipped)
       │ • pattern access program[]    │
       │ • live: runLiveCommand/       │  (U2b host-scoped on TT2TrackEngine)
       │         triggerScript         │
       └───────────────────────────────┘

   Parity harness (separate, no UI; the C VM's ONLY remaining caller until U8):
       script text ─┬─► legacy C VM ──► CV/gate ─┐
                    └─► TT2 runner  ──► CV/gate ─┴─► diff
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

### U2. Native per-line script edit API + pattern accessors on TeletypeProgram — ✅ SHIPPED

**Status: done** — `setScriptCommand`/`insertScriptCommand`/`deleteScriptCommand` +
`tt2detail::isBlank`/`parseLine` live in `TT2ScriptLoader.h`; `patternVal` in
`TeletypeProgram.h`. Round-trip, insert/delete shift+length, parse-error-leaves-unchanged,
and cap no-op covered by `TestTeletypeV2ScriptEdit`. U3 consumes these.

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

### U2b. Host-scoped live exec / trigger entry points on TT2TrackEngine

**Goal:** A UI-callable way to run a single live command and to fire a whole script
**with the scoped host active**, so host ops (`W*`/`BUS`/`MO`/`G`) work when fired from the
editor. This is the hinge U3's live-fire verification gate depends on.
**Dependencies:** none (engine-only).
**Files:** `src/apps/sequencer/engine/TT2TrackEngine.h` / `.cpp`;
`src/tests/unit/sequencer/TestTeletypeV2LiveExec.cpp` (new).
**Why it's needed:** the existing public `runScript(idx)` does **not** wrap `ScopedHost`
(only `tick()` does). A live command containing a host op fired from the UI thread would
have no active host → the op fails. Legacy `triggerScript` was a bare synchronous
`runScript` from the UI thread (no queue, no lock) — the native path mirrors that, plus the
host scope.
**Approach:** add two public methods that set `ScopedHost(this)` then run:
`TT2EvalResult runLiveCommand(const TT2Command &cmd)` (one command against this track's
program/runtime/output) and `void triggerScript(int scriptIndex)` (host-scoped wrapper over
`runScript`, name-parity with the legacy engine). Synchronous, UI-thread, matching legacy.
**Execution note:** test-first for the headless slice.
**Test scenarios:**
- `runLiveCommand` of a non-host op (e.g. `CV 1 V 1`) sets the expected `output.cv` target.
- `runLiveCommand` returns the top-of-stack `value` for an expression line.
- `triggerScript` runs all lines of a stored script against the track output.
- (Host ops `W*`/`BUS`/`MO`/`G` need a live `Engine` → sim/on-device, not headless.)

### U3. Strip + rebind TeletypeScriptViewPage to TT2 (native-only, single path)

**Goal:** The page becomes the **native TT2 script editor** — one backend, the
Teletype-track subsystems removed.
**Dependencies:** U2, U2b (U1 shipped via plan 007).
**Files:** `src/apps/sequencer/ui/pages/TeletypeScriptViewPage.cpp` / `.h`.
**Approach — rebind the core, strip the rest (see the Scope Boundaries table):**
- **Page guards (required)** — `draw()` opens with two `close()` guards keyed on
  `TrackMode::Teletype` (`TeletypeScriptViewPage.cpp` model + engine checks) plus
  `as<TeletypeTrackEngine>()` casts and engine-readiness checks scattered through
  `keyPress`/`commitLine`/`keyboard`. **Repoint every one to `TrackMode::TeletypeV2`** and
  `as<TT2TrackEngine>()`. Without this, the page opens and immediately closes/blanks for a
  TT2 track (TopPage already routes here). This is step one of the rebind.
- **Display** — `ss_get_script_len`/`ss_get_script_command` + `print_command` →
  `program().scripts[s].length` + `tt2PrintCommand` over `tt2Track().program()`.
- **Edit** — `ss_overwrite/insert/delete_script_command` → U2 `set/insert/deleteScriptCommand`;
  `duplicateLine` → `insertScriptCommand`; `loadEditBuffer` print → `tt2PrintCommand`.
- **Live** — `process_command` → U2b `runLiveCommand` (carries `_liveResult`/`has_value`);
  script triggering (shift+F keys, kbd F1–F5) → U2b `triggerScript`.
- **Script nav** — remap to TT2's six scripts (trigger 0–3, metro `TT2_METRO_SCRIPT`=4,
  init `TT2_INIT_SCRIPT`=5); init becomes editable. Drop `EditableScriptCount`/`SlotScriptIndex`/
  `METRO_SCRIPT` (legacy) for the TT2 constants.
- **Indicator row** — rebind metro/slew/delays/stack to `TT2Runtime` (`variables.m_act`,
  `cv_slew`, `delay`, `stack`); **drop** the dashboard cell (no TT2 equivalent).
- **Strip** — slot/clip (`activePatternSlot`/`captureActiveClip`/`P%d`), file I/O
  (`saveScript/loadScript/saveTrack/loadTrack` + the context menu), `drawIoGrid`/
  `drawBipolarBar`, comments (`commentLine`/`ss_*_comment`). Remove the now-dead
  `TeletypeBridge`/`TeletypeTrackEngine`/`state.h` includes.
- **Keep** — edit buffer, cursor, step char-entry maps, history, keyboard handling (all
  track-agnostic).
**Execution note:** **characterize-first then rebind; sim/on-device verification.** No
unit-testable surface in the page itself (testable logic is U1/U2/U2b). A headless build
only proves it compiles/links; render/edit/live-fire is auditioned on the simulator.
**Patterns to follow:** the page's own structure; U2b `runLiveCommand`/`triggerScript`.
**Test scenarios:** `Test expectation: none (UI page)` — sim render + on-device. Manual:
select a TT2 track → lines render via the native printer; edit → parse/lower/persist/
re-render; live-fire → native runner drives CV/gate; F-keys fire scripts (host ops work).

### U4. Strip + rebind TeletypePatternViewPage to TT2 (native-only, single path)

**Goal:** Pattern view/edit drives `program().patterns[]` natively, Teletype-track plumbing
removed.
**Dependencies:** U2.
**Files:** `src/apps/sequencer/ui/pages/TeletypePatternViewPage.cpp` / `.h`.
**Approach:**
- **Page guard (required)** — `draw()` `close()`s unless the track is `TrackMode::Teletype`
  (`TeletypePatternViewPage.cpp` model guard). Repoint to `TrackMode::TeletypeV2` (+ any
  `as<…>()` engine cast) first, or the page blanks for TT2.
- **Cells** — `ss_get/set_pattern_val/len/start/end/idx` → direct `program().patterns[col]`
  fields (`.val[]`, `.len`, `.start`, `.end`, `.idx`); add bounds-checked model accessors if
  cleaner than raw field access. `PATTERN_LENGTH` → `TT2_PATTERN_LENGTH` (both 64).
- **Vars panel + turtle** — rebind to `TT2Runtime` (`variables.a..t`, `turtle`); the turtle
  toggle/draw read `runtime.turtle`.
- **Strip** — `syncPattern` collapses to a plain `program().patterns[]` write (no
  `setTeletypePattern`/`captureActiveClip`); drop `activePatternSlot`/`P%d`.
- Edits wrap `_engine.lock()/unlock()` as today (concurrent with engine update).
**Execution note:** sim/on-device verification (interactive UI).
**Test scenarios:** `Test expectation: none (UI page)` — sim + on-device. Manual: TT2
pattern values + loop range + playhead + turtle + vars display and edit against
`program().patterns[]`/runtime.

### U3b. Restore the live-value HUD on the script page (correction)

**Goal:** The script page shows live output activity again so it's auditionable —
the readout U3 over-stripped, rebuilt on TT2 state.
**Dependencies:** U3; `ui-preview` layout sign-off.
**Files:** `src/apps/sequencer/ui/pages/TeletypeScriptViewPage.cpp`,
`src/apps/sequencer/engine/TT2TrackEngine.h` (expose `inputState()`),
`ui-preview/pages_tt2_script.py` (new, proposed layout).
**Approach:** Render a right-side HUD from TT2 state — CV bars (`output().cv[i]`,
8), TR gate (`output().tr[i].level`, 8), IN/PARAM (`runtime.variables.in/param`),
BUS (`_engine.busCv(i)`), TI activity (new public `inputState()`). **No** routing
source/dest assignment or ownership coloring (deferred — no model). 8-output
layout differs from legacy's 4; design + render in `ui-preview`, get sign-off,
then write page code (OLED-layout rule).
**Execution note:** ui-preview render → sign-off → implement; sim/on-device verify.
**Test scenarios:** `Test expectation: none (UI)` — ui-preview render + sim audition.

### U5. TopPage routing + Pages registry for TeletypeV2 — ✅ SHIPPED

**Status: done.** `TopPage`'s `setTrackView`/`setSequenceView`/`setSequenceEditPage` and
`TrackPage`'s trackMode switch already carry `case Track::TrackMode::TeletypeV2:` (routes to
`teletypeScriptView`; TrackPage no-op covers the enum and cleared the `TrackPage.cpp:167`
warning). No new page objects.
**Remaining (folds into U3/U4 verification):** routing reaches the pages, but the pages
themselves still `close()` for TT2 until U3/U4 repoint their guards — so the end-to-end
"select TT2 → editor opens and stays" check belongs to U3/U4's sim audition, not here.

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
**Files:** `src/apps/sequencer/ui/model/TrackModeListModel.h`; any other UI that cycles or
lists `TrackMode`. **Not** the engine, **not** the `Track.h` enum (order preserved).
**Approach — skip logic, not a label drop (the enum can't be reordered).** The mode picker
cycles the *raw contiguous* enum: `TrackModeListModel::edit()` calls
`ModelUtils::adjustedEnum`, which clamps over `[0, Track::TrackMode::Last - 1]`. `Teletype`
sits immediately **before** `TeletypeV2` in `Track.h`, so it is reachable on every cycle —
removing its `trackModeName` label would just show a blank but still selectable slot. The
fix: after `adjustedEnum`, if the result is `TrackMode::Teletype`, step once more in the
same direction (sign of `value`/offset, clamped) so the picker jumps Teletype→TeletypeV2 (or
TeletypeV2→Teletype-1 going down) and never lands on it. (Enum stays put for the engine +
U6 parity reference; only the *selectable* set excludes it.)
Keep the `TrackMode::Teletype` enum value, `TeletypeTrackEngine`, and the `scene_state_t`
C VM **compiled** — they remain solely as the `TestTeletypeV2Parity` reference until U8.
No-migration: existing projects with a Teletype track are not converted (dev files break);
such a track still *renders* its name but can't be cycled back to once changed away.
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
**Execution note:** **Do not start U8 until the U6 gate is satisfied** — op-value parity +
output-value parity green, TT2 shaping tests green, and on-device audition signed off. (Not
tick-level identity — U6 rejected that as the wrong bar.) This is the irreversible step; the
breakpoint tag is the safety net.
**Test scenarios:** full `TestTeletypeV2*` + STM32 release green after removal; no
`scene_state_t`/`TeletypeTrackEngine` symbols remain; flash drops by the removed bridge.

---

## System-Wide Impact

- The two edit pages stop editing legacy Teletype entirely (single TT2 path). Legacy editing is **removed**, not preserved — once U7 drops `Teletype` from selection there is no legacy track to edit. No byte-for-byte-unchanged constraint; the guard instead is that the C VM still *builds* for the U6 harness.
- No change to the TT2 runtime/ops/data model (all landed). No serialization change.
- Flash: the names-only printer table is small; explicitly avoiding `tele_ops[]` keeps the C op implementations unlinked. Confirm STM32 release still links (post-`-Os` there's ~350 KB headroom).

---

## Risks & Mitigations

- **Accidentally relinking the C op table** via `tele_ops[]` for names → flash bloat + foreign dep. Mitigation: names-only table (U1 Key Decision); check the STM32 map for `tele_ops`/C-op symbols after U1.
- **Editing regression** from rewiring a 1200-LOC page across a strip + rebind. Mitigation: characterize the kept UI logic (edit buffer/cursor/step-entry/history) before touching it; rebind data sites one subsystem at a time; sim-audition each.
- **Stale page guards** — both pages `close()` on entry unless the selected track is `TrackMode::Teletype` (`TeletypeScriptViewPage.cpp` draw guard, `TeletypePatternViewPage.cpp` draw guard). TopPage already routes TeletypeV2 here, so TT2 currently opens a page that immediately closes/blanks. Mitigation: U3/U4 replace these guards with `TeletypeV2` (required step, called out per unit).
- **Parity harness construction** — the C VM may be hard to host in a unit test. Mitigation: U6 shipped at script/op + output-value level (no Engine needed).

---

## Verification

- U1/U2/U2b/U6 unit tests green (printer round-trip, edit API, live exec, parity).
- Full `TestTeletypeV2*` suite + STM32 release green.
- On device / sim: a TeletypeV2 track is selectable and opens the editor (no close/blank-out); its scripts render + edit + live-fire via the native path; F-keys fire scripts with host ops working; the pattern page edits values/loop/turtle/vars.
- Parity harness reports zero divergence on the representative script set.
- Execution posture: U1/U2/U2b/U6 test-first; U3/U4 characterize-first then rebind.
