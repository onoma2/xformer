---
id: modulation-ui-spec
schema: spec
title: "spec: modulation UI — two doors, draft/commit, one model (CONTRACT)"
type: spec
status: draft
scope: next
date: 2026-06-04
supersedes:
  - routing-front-door-design        # 011
  - routing-tab-editor-complete-design # 010
  - routing-ui-nav-spec              # 009 (for migrated/global modulation UI)
related:
  - routing-legacy-vs-matrix
---

# Modulation UI — contract spec

> **For Claude:** This is the frozen contract from the 2026-06-04 grillme session. Every
> decision below is locked. Do NOT improvise unspecified behavior — if implementation hits a
> case not covered here, STOP and amend this spec. Renders precede code (see §10). Detailed
> bite-sized TDD task-plans are written per-phase **after** the renders are approved.

**Goal:** Let the musician modulate a param — "I set up a param, I want to modulate it" —
through two deliberate doors into one modulation model, with draft→commit editing.

**Architecture:** One model (`Routing::Route` = invisible slot). Two UI doors: a **param door**
(inline on the param's own row) and a **matrix door** (param×track grid for bulk sessions). All
editing is staged and applied on COMMIT. The legacy `RouteListModel` editor is removed.

**Vocabulary:** the user thinks in **modulation**, never "routes/slots." See
`docs/routing-legacy-vs-matrix.md` (a Route is storage plumbing). The legacy context word
"ROUTE" is renamed **MODULATE**.

---

## 1. Supersession

This supersedes 009 (lens nav), 010 (tab-editor), and 011 (front-door) for the modulation UI of
migrated/global params. The **Page+S6 tab editor, the source overlay/depth-modal auto-chain, and
the legacy `RouteListModel` per-route editor are all retired** by this spec. Keep
`docs/routing-legacy-vs-matrix.md` (provenance).

## 2. Domain model & data (LOCKED)

- A **modulation** = `(param, track-set, source, per-track depth, combine, shaper)`. The
  `Routing::Route` slot that stores it is invisible to the user — never surfaced as "slot N".
- **Depth = one `Route::depthPct[8]` array.** Two views of the same array:
  - **unified** — all member tracks equal (the route's single amount),
  - **spread** — per-track values differ.
  There is no second depth field. Legacy `_min`/`_max` exist but are **inert** under the
  override path and are never shown.
- **One source per param, per track.** A param has at most one modulation source on a track.
  No source stacking; the grid's one-row-per-param holds.
- `combine` = Modulate (centred, wiggle ±d around base) or Absolute (sweep from base).
- Base value (the param's set value) is the Hermod-style offset; modulation is `clamp(base + delta)`.

## 3. Editing model — draft → COMMIT (LOCKED, both doors)

- Editing builds a **draft**; the **live modulation is untouched until COMMIT**. Nothing is
  audible/active before COMMIT. This **replaces** the tab editor's live-edit model.
- **COMMIT = F5.** **CANCEL = the page back/exit key** (not an F-slot).
- **CANCEL** reverts a draft to its last committed state; **CANCEL on a newly-created draft
  removes it** (frees the slot).
- **Source is required to COMMIT** — COMMIT is unavailable until a real source is set.
- A modulated row is **always editable**: turning the encoder stages a change; the moment a
  draft is dirty, COMMIT (F5) is offered; CANCEL (back) reverts.

## 4. Param door — inline on the param's row (LOCKED)

For a single param on the current track, edited where the param already lives (no separate
editor, no page switch).

- **Invoke:** the context-menu modulation slot is **state-dependent on the current track's
  membership** (single slot — the legacy ROUTE slot; the 5-slot footer is full on most pages and
  per-track removal is track-scoped, not modulation-scoped — see §15 amendment):
  - **MOD+** (this track not modulated for the param): if no modulation exists for the param,
    create a draft (source None, **depth 0**, combine **Modulate**, tracks = **current track
    only**) → source picker → COMMIT. If a modulation already exists on other tracks, add this
    track to it (depth 0; inherits the modulation's source).
  - **MOD-** (this track modulated): remove this track from the modulation (clear its track bit,
    zero its depth). When it was the **last** track, the slot frees (whole modulation gone).
  - Global params (Tempo/Swing/CVR) have no track-set: MOD+ = create, MOD- = delete the one
    modulation.
- **Row display (modulated):** `name  source › [horizontal bipolar depth bar]  value`.
- **Encoder:** **press toggles edit target value ↔ depth**; turn edits the active one (turning
  depth stages it).
- **SRC** (F-key) → source picker → COMMIT sets the source, returns to the row.
- **COMBINE** (F-key) → toggles the draft's Modulate/Absolute.
- **Shift+S5** → the param door's **own per-track spread sub-view** (vertical bipolar bars for
  this param across tracks). Same bipolar idiom as the matrix cells.
- **REMOVE:** the same slot shows **MOD-** when this track is modulated → removes this track (and
  frees the slot only when it was the last track). See §15.

## 5. Matrix door — param×track grid (LOCKED)

For setting up several params across several tracks in one session.

- **Layout:** rows = the current band's params; columns = the 8 tracks; **each cell = a depth
  number** (numeric grid, à la the Numerical Audio Matrix). Column headers = **number+engine
  letter** (`1N 2C 3N…`). Ineligible cells (engine doesn't own the param) show `-`; eligible-
  unrouted show `.`. Tabs (Left/Right) switch bands.
- **Cursor = brightness** (the cursor cell + its column header + the row name brighten); **not a
  box**. **By-type = a thin underline** on the selected group's cells (distinct from the cursor
  brightness cue).
- **One source per row** — a row = one source modulating that param across its tracks; cells
  differ only in **depth** (= spread). **SRC** F-key sets the row's source.
- **Scope:** **T1–T8 toggle columns** into the focused row; **Shift+Tn = by-type** (all tracks
  of track n's engine).
- **Depth:** cursor lands on a cell; **encoder stages that cell's depth**.
- **COMBINE** F-key per row. **COMMIT (F5) is per-modulation (per-row).** CANCEL (back) reverts
  the row.
- **REMOVE:** clears the focused row's modulation.
- Cells for tracks whose engine doesn't own the param render **ineligible** (dim/dash).
- **Cell views, cycled by F1 (VIEW):** **depth** (numbers per cell) → **source** (each routed
  cell's source abbreviation — `CV1`/`O1`/`B1`/`G1`/`M1`) → **shaper** (per-cell shaper name —
  deferred until the shaper port). Depth + shaper are per-cell (per-track arrays); source is
  per-row (one-source-per-row, so a row's routed cells share their source). Header tag shows the
  current view. Until shaper lands, F1 cycles two (depth↔source).

## 6. Depth language (LOCKED)

- **Horizontal** bipolar bar = the param-door unified amount (one bar in the row's dead space,
  centre tick = base/neutral, fill right = +, left = −).
- **Vertical** bipolar bars = the spread sub-view (per-track, full height).
- **Numbers** = the matrix grid cells (depth value per cell). The bipolar-bar idiom is used only
  where it has room (param-door row, spread); the dense grid is numeric.
- Hermod model: depth = attenuverter, combine = polarity, base = offset.
- **Track/engine labels:** spread labels and matrix column headers are **number+engine letter**
  (`1N 2C 3N…`), so the engine — and thus eligibility — is visible per track.

## 7. Source picker (LOCKED)

- Reuse `RouteSourceSelectPage`. Footer button is **COMMIT** (not OK).
- List = CV-domain sources (CV In/Out, Bus, Gate, Mod) **plus None** at top. **None cannot be
  committed** (source required). The self-route bus is excluded for the target.
- MIDI source is **deferred** (would need F4 LEARN) — and is dark anyway under §8.

## 8. Legacy boundary (LOCKED — accepted regression)

- The legacy `RouteListModel` / per-route editor is **hidden/removed now**.
- The new modulation UI covers **migrated engines only: Note, PhaseFlux, and global (Tempo/
  Swing/CVR)**.
- The six non-migrated engines (Curve, Tuesday, Stochastic, DiscreteMap, Indexed) **and MIDI
  source have NO routing UI until each migrates.** This dark gap is accepted, in exchange for a
  single clean UI. Migration brings each engine onto the new UI by wiring its `ParamTable*` to
  the override path + giving its page a `currentRouteTarget()`.

## 9. Naming (LOCKED)

- Gesture/menu: a **single state-dependent slot** replacing the legacy ROUTE slot —
  **MOD+** (this track not modulated → add this track / create) / **MOD-** (this track modulated
  → remove this track; free slot when last). Supersedes the earlier MODULATE / REMOVE MOD
  two-item naming (see §15 amendment).
- Commit button: **COMMIT** (was OK). Cancel: the back/exit key.

## 10. Render plan — BEFORE any code

Render each screen with `ui-preview` and read it back; only then implement. Screens:

1. **Param door — modulated row** (clean): `name  src › [h-bar]  value`, value-edit focus.
2. **Param door — depth focus + dirty**: depth-edit active, COMMIT (F5) shown, CANCEL=back.
3. **Param door — spread sub-view** (Shift+S5): vertical bipolar bars, this param × tracks.
4. **Source picker**: CV-domain + None, footer CANCEL(back)/COMMIT.
5. **Matrix grid — depth view** (a band): param rows × 8 track columns, **numeric** cells,
   number+engine column headers, footer SRC/COMBINE/COMMIT.
6. **Matrix grid — by-type**: Shift+T underlines a homogeneous engine set across a row.
7. **Matrix grid — source view**: same grid, cells show the **source** per routed cell
   (`CV1 B1 M1…`) instead of depth — toggled from the depth view.

Renders done — `ui-preview/modulation/mod-*.png`, renderer `ui-preview/pages_modulation.py`.

## 11. Render outcomes (SETTLED 2026-06-04)

- **F-slots:** **SRC = F2, COMBINE = F3, COMMIT = F5** (lit only when the draft is dirty/
  committable); **CANCEL = the page back/exit key** (frees an F-slot). The param-row footer swaps
  to these modulation controls while a modulated row is focused.
- **Labels:** spread labels and matrix column headers are **number+engine letter** (`1N 2C 3N…`).
  Engine letter reveals eligibility (a param's non-owning engines show ineligible).
- **Matrix cells are numeric** (depth value); ineligible `-`, eligible-unrouted `.`.
- **Cursor = brightness** (cursor cell + its column header + row name brighten), **not a box**.
- **By-type = a thin underline** on the group's cells (distinct from the cursor brightness).
- **Matrix cell views cycled by F1 (VIEW):** depth → source → shaper (shaper deferred until the
  port; F1 cycles two until then). Source abbreviations: `CV1` CV In, `O1` CV Out, `B1` Bus,
  `G1` Gate, `M1` Mod. Matrix footer = **F1 VIEW · F2 SRC · F3 COMBINE · F5 COMMIT**.
- Spread sub-view and a matrix row are the **same vertical-bar idiom** reached two ways (no
  separate widget); the matrix's own cells are numeric, not bars.

## 12. Implementation phases (detailed TDD plans written per phase, after renders)

1. **Retire legacy + MODULATE/REMOVE rename** — hide `RouteListModel`/old editor; rename the
   context action ROUTE→MODULATE (+ REMOVE MOD); migrated-only gating (`RouteFork::migrated`/
   `migratedGlobal`); non-migrated context entry hidden.
2. **Draft/commit core** — staging buffer per modulation; COMMIT(F5)/CANCEL(back); source
   required; live route written only on COMMIT.
3. **Param door** — inline modulated row (horizontal bar), press value↔depth, SRC picker
   (COMMIT), COMBINE, creation defaults (depth 0).
4. **Param-door spread** — Shift+S5 vertical bipolar bars.
5. **Matrix grid** — param×track grid, cells, T-toggle + Shift+T by-type, per-row source/COMMIT.
6. **Per-engine migration** — wire each remaining engine's `ParamTable*` live + `currentRouteTarget()`; it joins the new UI, its dark gap closes.
7. **MIDI source (F4 LEARN)**; **shaper** (engine-gated, in spread).

## 13. Deferred / out of scope

Shaper UI (until the shaper port; only None/TriangleFold live); MIDI-learn; `scaleSource` cross-
source; the flat "every modulation" overview list (the grid is the editor; an audit list can come
later); serialized-format cleanup; deleting old `writeTarget`/`Routable` routed half.

## 14. MVP code already on branch — reconcile

The redirect MVP (TopPage::editRoute fork → source overlay → depth modal, depth-0 create,
findRoute isPerTrackTarget fix, cancel-cleanup) was an interim that this spec **replaces**: the
auto-chain source→depth-modal is dropped (§4 uses inline row + draft/commit), but the
`findRoute` fix and depth-0-on-create remain correct and carry forward.

## 15. Amendments (post-freeze)

**2026-06-04 — context-menu modulation slot: MOD+/MOD- (single, per-track).** During phase-1
implementation, two facts collided with the frozen §4/§9 "MODULATE + REMOVE MOD as two items":
(1) the context-menu footer has exactly **5 slots** (F1–F5; a 6th item never draws and has no
key), and 3 of the 4 migrated pages are already full (NoteSequence INIT/COPY/PASTE/DUPL/ROUTE,
Project INIT/LOAD/SAVE/SAVE AS/ROUTE, Track INIT/COPY/PASTE/ROUTE/RESEED); (2) "REMOVE MOD"
(57px) overflows one 51px slot. More fundamentally, removal on a per-track modulation is
**track-scoped**, not whole-modulation: removing one track of a multi-track modulation must keep
the others. Resolution (owner decision): **one state-dependent slot** (the legacy ROUTE slot)
showing the current track's membership — **MOD+** add this track / create, **MOD-** remove this
track / free slot when last. Labels rendered to fit (MOD+ / MOD- well under 51px). Verified
against `ui-preview/modulation/modulation-ctx-*.png`. This supersedes §4 invoke/REMOVE and §9
naming above (edited inline to match).

**2026-06-04 — param-door CANCEL = F4 (these pages have no back key).** Spec §3 set "CANCEL =
the page back/exit key (not an F-slot)", but the migrated param pages (Note/PhaseFlux/Project/
Track sequence + track pages) are top-level Page-modifier pages with **no in-page back key** — the
only exit is navigating away. So CANCEL is bound to a function slot. Owner chose **F4** (sits next
to COMMIT on F5). Edit-footer = **F2 SRC · F3 COMBINE · F4 CANCEL · F5 COMMIT** (F1 free). On a
modulated row in edit: encoder **press toggles Value↔Depth** (§4); **F4 CANCEL** discards the
uncommitted draft + exits to navigate; **F5 COMMIT** writes draft→live + exits; leaving the page
also cancels. This displaces the **MIDI LEARN** reservation (was F4, §12 phase 7) to the free
**F1** slot. Supersedes §3 ("CANCEL = back key") and §11/§12's F4=LEARN for the param door.

**2026-06-05 — matrix-door entry = the ROUTING menu page.** §5 never said how the matrix door is
entered, and §1 retired the Page+S6 tab editor. Owner decision: the **ROUTING top-level menu page
becomes the matrix door** — its legacy overview + per-route editor + `RouteListModel` are removed,
and the page shows the param×track matrix grid directly (the tab-editor band/route machinery
evolves into it). No new gesture, no new page, no menu slot; reuses the existing ROUTING entry.
This finally makes §8 true on hardware (legacy RouteListModel gone) and reclaims ~4-5 KB flash.
The matrix grid uses the param-door's draft/commit + F4=CANCEL convention (§15 above): footer
**F1 VIEW · F2 SRC · F3 COMBINE · F4 CANCEL · F5 COMMIT**, per-row draft via `RouteDraft`.

**2026-06-05 — matrix cursor navigation (§5 didn't specify it).** §5 spent the controls on bands
(Left/Right), membership (T1-T8 / Shift+Tn), depth (encoder), and the F-keys — leaving no cursor
nav. Owner decision: **Steps S1–S8 = cursor column** (the 8 step buttons map to the 8 track
columns; direct jump); **encoder = cursor row** in nav mode; **encoder press = toggle nav↔edit**;
in edit mode **encoder = the cursor cell's depth** (staged into the row's `RouteDraft`), S1–S8
still move the cursor cell within the row. **F5 COMMIT** writes the row draft→live + exits to nav;
**F4 CANCEL** (or press-out) reverts + exits. Editing is **per-row** (one draft per param-row =
one modulation across its tracks); a cell edit sets `depthPct[col]` + adds `col` to the row's mask.
Entering edit on an unrouted row creates a draft (source None, depth 0) à la the param door; F2 SRC
sets the source, source required to COMMIT. This replaces the tab editor's live depth-modal editing.

**2026-06-05 — matrix source editing is inline (no picker); supersedes §7 for the matrix.** Owner
decision: in the matrix, source is edited inline in **SOURCE view** (F1 VIEW), exactly like depth in
depth view — the encoder cycles the focused row's single source; **Shift+encoder jumps by group**
(None → CV → O → B → G → M → wrap). The modal `RouteSourceSelectPage` picker and the **F2 SRC**
F-slot are **dropped from the matrix** (matrix footer = **VIEW · — · COMBINE · CANCEL · COMMIT**).
The param door still uses the picker (its `src ›` field). **Source abbreviations:** `IN1-4` CV inputs
(renamed from CV1-4), `O1-8` CV outputs, `B1-4` bus, `G1-8` gate outs, `M1-8` modulators — unified
in one shared abbreviator used by both the matrix and the param-door row.
