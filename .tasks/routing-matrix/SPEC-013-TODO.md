# spec 013 implementation TODO (followable)

Canonical contract: `docs/plans/2026-06-04-013-modulation-ui-spec.md` (FROZEN).
Phase-1 plan: `docs/plans/2026-06-04-014-modulation-phase1-plan.md`.
Provenance: `docs/routing-legacy-vs-matrix.md`.

## Working rules (apply to EVERY step — do not skip)

- **Hold the frozen spec.** A request that CONTRADICTS spec 013 → disagree + cite the section,
  don't amend/build. Only a genuine GAP gets a §15 amendment. (See memory `hold-the-frozen-spec`.)
- **Build gate = STM32 release.** `make -C build/stm32/release sequencer` must be green, no new
  warnings. Sim is for the host unit tests only. Never trust sim for the compile gate.
- **Render before UI.** Any OLED change → `ui-preview/` render + read back BEFORE firmware.
- **Per-phase gate (owner 2026-06-04) = STM32 release build green + Codex gate.** Every phase: run
  `make -C build/stm32/release sequencer` (green, no new warnings) AND a Codex review (ALLOW) before
  commit. These two are the gate that stands in for hardware flashing until phase 7.
- **Flash cadence (owner override 2026-06-04): owner flashes ONCE after phase 7.** Do NOT ask the
  owner to flash per phase. Commit each phase when STM32-green + reviewed; momentum over slivers.
- **One reviewable unit at a time** for review/commit, but keep building forward to phase 7.
  The routing-page tab editor already sets depth live — it stays usable throughout.

## Status legend
[x] done & committed · [~] built/uncommitted (awaiting flash+commit) · [ ] not started

---

## Phase 1 — retire legacy + MOD+/MOD- slot + draft/commit core

- [x] **Model: RouteDraft draft/commit core** — `create`/`begin`/`canCommit`/`commit`/`cancel`,
      bounds-guarded. Committed `8197ea0d`, Codex-gated. (`model/RouteDraft.h`, `TestRouteDraft`.)
- [x] **Model: membership ops** — `isTrackModulated` + `removeTrack` (per-track clear+free-if-last;
      global free). Committed `e2c00d16`, Codex-gated. 16 cases green.
- [x] **UI: MOD+/MOD- context slot** — state-dependent slot (per-track membership) on the 4
      migrated pages (Note/PhaseFlux/Project/Track); MOD+ = create→source→setSource, MOD- =
      removeTrack; migrated-only gate; non-migrated pages drop the ROUTE item; legacy
      `editRoute`/`beginModulate` hook retired (`showRoute`/`RouteListModel` kept, phase-9 delete).
      Committed `c9aefb52`. STM32-green, internal + Codex ALLOW, hardware-verified by owner.
      Renders `modulation-ctx-*.png`.
      - NOTE (carry to phase 3): phase-1 MOD+ writes source live immediately — this does NOT yet
        honor spec §3 "live untouched until COMMIT". Phase 3 replaces it with real draft/commit.

## Phase 2 — draft/commit core
- [x] Covered by the RouteDraft model above. UI consumption is phase 3 (param door) + phase 5
      (matrix). No separate work item.

## Phase 3 — param door, inline editing (spec §4, §6, §11)
- [x] Render-check inline row states (`mod-param-row.png`, `mod-param-depth.png` confirmed).
- [x] **Unit 1 — inline modulated-row DISPLAY** (draw-only): selected migrated+modulated row draws
      `<src› [horizontal bipolar bar]` in the middle band (`ListPage::draw` + base `routingTarget`
      hook), strictly gated. STM32-green, Codex ALLOW. (Editing is units 2-3 below.)
- [x] **Unit 2 — inline edit draft/commit** (`ListPage` state machine): press toggles Value↔Depth;
      turn edits base value (live) or stages depth into a file-scope `RouteDraft` (live untouched);
      **F2 SRC** (picker→draft source), **F3 COMBINE** (draft combine), **F4 CANCEL** (discard+exit),
      **F5 COMMIT** (write draft→live + exit). Footer `SRC·COMBINE·CANCEL·COMMIT`. CANCEL bound to
      **F4** (spec §15 amend — these pages have no back key; LEARN reservation → F1). Owner-scoped
      teardown (`cancelModEditIfOwner`, `_manager.top()==this`) so the source-picker round-trip
      survives. STM32-green, Codex ALLOW (4 review rounds on picker/global-draft interaction).
- [x] **Unit 3 — draft-based MOD+ create**: `ListPage::beginNewModulation` reserves an inert slot
      (`RouteDraft::create`, isNew) → enters inline edit + source picker → F5 commits / F4 frees the
      slot. Pages' MODULATE action = `isTrackModulated ? removeTrack : beginNewModulation`;
      `TopPage::toggleModulation` removed. Honors §3 (silent until COMMIT). STM32-green, Codex ALLOW.

**Phase 3 DONE** — param-door inline editor complete (display + draft/commit depth/source/combine
+ draft MOD+). Commits: unit1, unit2, unit3.

## Phase 4 — param-door spread (spec §4, §6) — DONE
- [x] **Shift+S5** → per-track vertical bipolar bars (`ListPage` spread mode on the owner-guarded
      draft): 8 bars from `depthPct[t]`, number+engine labels, eligible (migrated) tracks editable,
      Left/Right nav skips ineligible, encoder edits depth + adds track to mask, F2/F3/F4/F5 shared
      with inline editor. Reached on a modulated row; editing other eligible tracks builds the
      multi-track spread. STM32-green, Codex ALLOW.

> **FLASH IS NOW THE HARD CONSTRAINT (not RAM).** App partition = 960 KB (`sequencer.ld`,
> 0x08010000). Phase-4 hit the ceiling. Reclaimed: removed the superseded bias/depth/shaper
> overlay (~5 KB) + gated the Asteroids easter-egg off (`CONFIG_ENABLE_ASTEROIDS`, ~14.5 KB).
> Now ~16.7 KB free. Flash culprits + measurement commands documented in PROJECT.md
> ("Flash ceiling snapshot"). Phases 5-7 must watch flash; gate more non-core (Intro) if needed.

## Phase 5 — matrix door (spec §5, §6, §11)
**ENTRY (spec §15): the ROUTING menu page BECOMES the matrix** — remove its legacy overview +
per-route editor + `RouteListModel`; the tab editor evolves into the grid. Renders `mod-matrix*.png`.
Per-row draft/commit via `RouteDraft`; footer F1 VIEW · F2 SRC · F3 COMBINE · F4 CANCEL · F5 COMMIT.
- [x] **5a** — ROUTING page = matrix: removed legacy overview/per-route editor/`RouteListModel`
      (EmptyListModel for the ctor); ROUTING now opens straight into the tab editor (self-inits via
      `enterTabEditor`/`tabRefocus`); BACK pops the page. Freed ~9.7 KB. STM32-green, Codex ALLOW.
- [x] **5b** — grid draw: 8-col numeric cells (`-`/`.`/number via `tabCellEligible`+`findRoute`),
      number+engine column headers (real `RouteFork::migrated` eligibility dim), cursor=brightness
      (cell+header+rowname, fillRect + left tab bar removed), band header + DEPTH + MODULATE.
      `_tabCol` column cursor (default=current track, no nav yet). Track-letter helper unified into
      `Track::trackModeLetter` (RoutingPage + ListPage). +468 B. STM32-green, Codex ALLOW.
- [x] **5c** — matrix editing (spec §15 nav: **S1-S8 = column, encoder = row/depth, press = toggle
      nav↔edit**): per-row `RouteDraft` member on RoutingPage; routed row→begin (any-track lookup
      `0xFF`/global `0`), unrouted→create; cursor-cell depth + mask merge; F2 SRC / F3 COMBINE / F4
      CANCEL / F5 COMMIT; draft-aware draw. Removed old depth-modal + tabCreateRoute +
      gRouteDepthQuickEditModel + _tabScopeMask. Global band = single slot-0 value. STM32-green,
      Codex ALLOW (1 BLOCK fixed: per-track routed-cell duplicate). Residual: multiple disjoint
      per-track routes for one target → lookup returns first (pre-existing param-door artifact).
- [x] **5d** — T1-T8 toggle membership into the focused row (edit mode, eligible only), Shift+Tn =
      by-type (same-engine group), by-type underline (cursor-column engine cells of cursor row).
- [x] **5e** — F1 VIEW depth↔source (`_matrixView`; source view shows route source abbrev per cell).
- [x] **5f** — REMOVE = **Shift+F4** on a routed focused row (clears the route).
- **OWNER-REVIEW FLAGS (5d/5f, agent-chosen defaults, can override):** by-type underline = persistent
  cue keyed off the cursor column's engine; REMOVE gesture = Shift+CANCEL (F1-F5 were full).
- **DRY note:** `printSourceAbbrev` now duplicated in RoutingPage + ListPage (unify later like `trackModeLetter`).

**PHASE 5 (matrix door) DONE** — ROUTING page is the matrix: grid, nav, per-row draft/commit,
T-toggle/by-type, F1 VIEW, REMOVE. Commits 5a 89cf0fe5 / 5b c941a429 / 5c d5d787b6 / 5d-f 8f20b958.

## Hardware-tuning round (post-phase-5, owner flashing + iterating) — DONE
Owner flashed and tuned phases 1-5 live. Model + UX decisions locked (spec §15 amendments) and
many UI fixes landed. **All on `refactor/routing-matrix`, STM32 + sim clean, Codex-gated where
behavioral. Branch tip ≈ `51661240`.**
- **MODEL: ONE source per row (spec §15, commit 67b63ef0).** Resolved the §2/§5 contradiction:
  a param's modulation is ONE Route (one source, one combine, per-track depthPct[8] spread, track
  mask). Fits the 16-route budget (`CONFIG_ROUTE_COUNT`=16; 1 route per fully-spread param). The
  param door's **MOD+ now EXTENDS** the param's single route (live membership add of the current
  track, inherits the shared source, inert depth 0 → depth edit) instead of minting per-track
  routes. New `RouteDraft::findRouteForTarget`. Combine is also per-route (shared by member tracks).
- **MATRIX NAV remapped (spec §15, commit bc14d3d7):** **T1-T8 = move cursor to that track's column**
  (was S1-S8); **membership is implicit** (dial depth → member, 0 → drop); **Shift+Tn = by-type
  spread** (copy cursor depth to all same-engine tracks); **step buttons freed**. Nav/edit is the
  **F2 EDIT** toggle (lit in footer), not encoder-press (commit 63e28736).
- **MATRIX source editing is inline (spec §15, commit 72bc5640):** SOURCE view (F1) → encoder
  cycles the row's source, **Shift+encoder group-jumps** (None/IN/O/B/G/M). Matrix picker + F2 SRC
  dropped. Source abbrev unified `Routing::printSourceAbbrev`, **CvIn renamed CV→IN1-4** (fixed the
  "CV15" BusCv mislabel). SOURCE view paints the row source on **all eligible cells** (d7ac68bd).
- **Other matrix polish:** header **X/16 slot counter** (f424cd63); per-row **M/A** combine marker
  + F3 footer label **MOD/ABS** (2c47b55a, 51661240, d5883e35); short matrix row names + truncation
  (6ecea98a, 8f3c7b67); **Page+key passes through to TopPage** so you can exit ROUTING + reach the
  Modulator page (2e9399e0 — was a 5a regression).
- **Param-door polish:** row layout `name | VALUE(its column) | narrow bar | SOURCE(right)`
  (b1ca68a0); **bar+source on EVERY modulated row** not just selected (cb9eed53); source-picker
  selection highlight restored (owner-gated, 2abbbed5); unused `[this]` capture warnings fixed
  (97a2c16a — sim/clang catches what arm-gcc doesn't, so build sim too).
- **VERIFIED wired:** Note + PhaseFlux per-track + global (Tempo/Swing/CVR). The 6 dark engines
  show `-` (ineligible) until phase 6.

## Phase 6 — per-engine migration (spec §8, §17)
Read-side migration (slice-4 pattern) per engine: `RouteFork::migrated` case + getter→`routedValueInt`
+ gate-free base edits + bridge each field in `targetToParamKey`. Tables already staged (§17 diff).
- [x] **Curve** (2026-06-06) — plan `docs/plans/2026-06-05-014-phase6-curve-migration.md`,
      `43c968b8`→`978d3dbc`. 18 getters, 9 bridges added, centi handling for curveRate/wavefolder/
      djFilter; 4 Codex ALLOW + final ALLOW; suite green; STM32+sim clean. `Phase` row orphan (no Target).
- [x] **Tuesday** (2026-06-06) — `cbb0d78c` + `079d3057` + `3de7d0c3`. One file (TuesdaySequence — no
      track getters); 16 getters incl. plain-int Scale/RootNote (−1 Default preserved) + Algorithm/Flow/
      Ornament/Power/Glide/Trill/StepTrill/GateLength/GateOffset bridges. **Two field<table clamp fixes**
      Codex caught: transpose ±60→±11 (getter+table), rotate dynamic clamp ±(loopLen−1) in the getter;
      divisor shortcut keys base-edit (divisorBase) to stop base-lurch. 2 Codex BLOCK→ALLOW; suite green.
- [x] **Stochastic** (2026-06-06) — `de345ad0` + `e2e55f24`. 23 params across StochasticSequence (17 +
      plain-int Scale/RootNote) + StochasticTrack (3); 16 Stochastic*/Feel bridges (name divergence:
      StochasticMask→MaskRhythm, StochasticTilt→TiltRhythm, StochasticFeel→Feel…). Codex caught:
      transpose field ±100 (not table ±60), variation abs-before-clamp, and **hero-grid base-lurch**
      (editLiveStep/editLoopStep edited 16 migrated params from the effective getter → added xBase()
      accessors). 1 BLOCK→ALLOW; suite green. Param-door already wired (Config+Performance ListPages).
- [x] **DiscreteMap** (2026-06-06) — `32c1fbde` + `cbce2717`. Bespoke: float fields rangeHigh/rangeLow
      (**first use of `routedValue` float**) + base-less Input/Scanner inlets modeled as **base-0 overrides**
      (owner choice, matrix-only); 4 bridges (Input/Scanner/RangeHigh/RangeLow); plain-int scale/rootNote;
      plain-field octave/transpose/offset/divisor. Sync retired (no row, no bridge). Codex caught
      base-lurch in DiscreteMapSequenceListModel + **base-0 inlet silent at depth 0** → added
      `Routing::isInletTarget`, inlet routes default depth 100 (create + MOD+ extend). 1 BLOCK→ALLOW; suite green.
- [ ] Indexed (IndexedA/B + sequence; Sync retired).
- [ ] MidiCv (optional — SlideTime/Transpose only).

## Phase 7 — MIDI source + shaper (spec §7, §13)
- [ ] MIDI source via **F4 LEARN** (MidiLearn → source=Midi + midiSource on the live route).
- [ ] Shaper UI (engine-gated; only None/TriangleFold live today), surfaced in spread.

## Deferred / out of scope (spec §13)
- `scaleSource` cross-source resolution; flat "every modulation" audit list; serialized-format
  cleanup (U7/§8 wire format); deleting old `writeTarget`/`Routable` routed half (phase 9);
  deleting `RouteListModel`/legacy RoutingPage editor (phase 9).
