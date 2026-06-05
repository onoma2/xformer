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

**PHASE 5 (matrix door) DONE** — ROUTING page is the matrix: grid, S1-S8/encoder nav, per-row
draft/commit, T-toggle/by-type, F1 VIEW, REMOVE. Commits 5a 89cf0fe5 / 5b c941a429 / 5c d5d787b6 / 5d-f.

## Phase 6 — per-engine migration (spec §8)
- [ ] Wire each remaining engine's `ParamTable*` live + give its page `currentRouteTarget()`, one
      engine at a time, closing its dark gap: Curve, Tuesday, Stochastic, DiscreteMap, Indexed.
      (Tables already staged; this lights them on the override path + new UI.)

## Phase 7 — MIDI source + shaper (spec §7, §13)
- [ ] MIDI source via **F4 LEARN** (MidiLearn → source=Midi + midiSource on the live route).
- [ ] Shaper UI (engine-gated; only None/TriangleFold live today), surfaced in spread.

## Deferred / out of scope (spec §13)
- `scaleSource` cross-source resolution; flat "every modulation" audit list; serialized-format
  cleanup (U7/§8 wire format); deleting old `writeTarget`/`Routable` routed half (phase 9);
  deleting `RouteListModel`/legacy RoutingPage editor (phase 9).
