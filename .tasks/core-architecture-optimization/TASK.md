# core-architecture-optimization

## Goal

Research whether XFORMER needs core architecture changes beyond local Teletype and RAM cleanup. Original Performer was a compact X0X-style sequencer with Note/Curve tracks, simple routing, and small resident state; XFORMER added heavyweight track types and subsystems that may need resource-aware lifecycle, storage, routing, output, and UI mechanics.

## Key files

- **`architecture-research-directions.md`** — Broad architecture research: assumptions, mismatches, directions, ranking
- **`ram-recovery-experiments.md`** — Narrow RAM recovery experiments with baselines, hypotheses, and verification gates
- **`research-spec.md`** — Agent handoff brief (points to the two derived docs above)
- `.tasks/teletype-performer-ecosystem-redesign/savings-plan.md` — current RAM optimization catalog
- `.tasks/resource-optimization/TASK.md` — active measured RAM/flash recovery work
- `src/apps/sequencer/engine/RoutingEngine.h:30-65` — RouteState with unconditional TrackState[8]
- `src/apps/sequencer/engine/Engine.h:41-42` — TrackEngineContainer array of 8
- `src/apps/sequencer/model/Track.h:273` — model Track::_container
- `src/apps/sequencer/model/TeletypeTrack.h:682-684` — scene_state + PatternSlot redundancy
- `src/apps/sequencer/model/Routing.h:819-822` — per-track shaping config arrays

## Decisions log

- 2026-05-14: Created as a research-only task.
- 2026-05-14: Completed initial 7-question research pass. Produced combined findings document.
- 2026-05-14: Restructured into two documents per user direction: `architecture-research-directions.md` (broad architecture, not implementation) and `ram-recovery-experiments.md` (narrow RAM experiments only). Key corrections: model Container is Note/Curve-driven not Teletype-driven; engine Container IS Teletype-driven; RoutingEngine is the #1 CCMRAM mismatch AND an architecture mismatch (evolved from sidecar to modulation engine); output ownership is deliberately dual-path; Teletype pattern redundancy is designed swap mechanism.
- 2026-05-14: Full rewrite of all three documents from fresh source inspection. Key findings: (1) RoutingEngine evolved from sidecar to modulation engine — 5 stateful shapers, baseline ~7.4 KB CCMRAM; recoverable depends on design; (2) Model Container maxes at NoteTrack ~9.5 KB because Note/Curve genuinely need 17 full sequences; (3) Engine Container IS Teletype-driven at ~904 B, 1.8 KB direct gap; (4) Pattern semantics diverged; (5) Output ownership is dual-path but undocumented; (6) Routing baseline now phrased as "~7.4 KB baseline; recoverable amount depends on design" not casual "~7 KB may recover"; (7) Experiment A1 explicitly states storage layout change is required (runtime branching alone saves nothing); A2 specifies per-TrackState union+discriminant; (8) Agent entry point guardrail restored; phase labels removed from architecture doc; unverified claims softened to "verify whether."

## Open questions

- [ ] Should RoutingEngine modulation state have an explicit lifecycle (create/destroy on shaper change) vs unconditional fixed array?
- [ ] Should TeletypeTrackEngine use a pool with configurable max count vs always-resident Container?
- [ ] Should Note/Curve sequence storage become lazy (active-pattern-only) to reduce model inflation?
- [ ] Should shaper state persist across project save/load, or is "always resets on load" acceptable?
- [ ] Should track type be split into persistent project data + runtime generator?

## Completed steps

- [x] Captured research spec
- [x] Completed full research across all 7 axes (A-H)
- [x] Produced combined findings document (later split)
- [x] Restructured into architecture-research-directions.md + ram-recovery-experiments.md per user direction
- [x] Corrected model vs engine Container confusion
- [x] Added cross-references to resource-optimization task for verified size figures

## Notes

This task is research-only. `architecture-research-directions.md` contains the architectural analysis. `ram-recovery-experiments.md` contains measurable RAM experiments. Do not collapse them — they serve different purposes.
