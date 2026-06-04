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
- [ ] Encoder: **press toggles value↔depth**, turn edits the active one (depth turn = staged draft).
- [ ] Footer F-keys while a modulated row is focused: **SRC=F2, COMBINE=F3, COMMIT=F5** (F5 lit only
      when draft dirty/committable), **CANCEL = back key**. Draft/commit: live untouched until COMMIT;
      CANCEL reverts (removes if newly created); **source required to commit**. Replaces phase-1's
      immediate source write.
- [ ] Creation defaults: source None, depth 0, combine Modulate, current track only.

## Phase 4 — param-door spread (spec §4, §6)
- [ ] **Shift+S5** → per-track vertical bipolar bars for this param × tracks (render `mod-spread.png`).
      Engine already reads per-track `depthPct[]`; UI-only.

## Phase 5 — matrix door (spec §5, §6, §11)
- [ ] param×track **numeric** grid; column headers number+engine letter (`1N 2C 3N…`); ineligible `-`,
      eligible-unrouted `.`; tabs (Left/Right) switch bands. Renders `mod-matrix*.png`.
- [ ] Cursor = brightness (cell + col header + row name), not a box; by-type = thin underline.
- [ ] One source per row (SRC F2); COMBINE F3; per-row COMMIT F5; CANCEL back; REMOVE clears row.
- [ ] Scope: T1–T8 toggle columns into the focused row; Shift+Tn = by-type.
- [ ] F1 VIEW cycles depth↔source (shaper view deferred until shaper port).

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
