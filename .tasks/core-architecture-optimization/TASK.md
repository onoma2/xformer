# core-architecture-optimization

## Goal

Research whether XFORMER needs core architecture changes beyond local Teletype and RAM cleanup. Original Performer was a compact X0X-style sequencer with Note/Curve tracks, simple routing, and small resident state; XFORMER added heavyweight track types and subsystems that may need resource-aware lifecycle, storage, routing, output, and UI mechanics.

**Current role:** reference/research only. Active RAM implementation lives in `.tasks/resource-optimization/TASK.md`; this task now holds source-grounded contracts, decision tables, and no-go/yes-go architecture evidence.

## Key files

- **`architecture-research-directions.md`** — Broad architecture research: assumptions, mismatches, directions, ranking
- **`architecture-change-decision-gates.md`** — Policy for deciding whether a mismatch justifies major architecture change
- **`ram-recovery-experiments.md`** — Narrow RAM recovery experiments with baselines, hypotheses, and verification gates
- **`model-pool-decision-table.md`** — Go/no-go table for model/engine pool architecture; current decision is no under "any track can be any type" semantics
- **`routingengine-refactor-phased-plan.md`** — Concrete phased plan for RoutingEngine state compaction (RAM only); implementation belongs under `resource-optimization`
- **`routing-five-sources-map.md`** — Map of the routing fan-out: a single target's definition is smeared across 5 sources of truth (enum / targetInfos / dispatch switch / picker / model writeRouted) that drift and produce silent dead routes. Inconsistency inventory + per-trackMode applicability matrix. Direction deferred.
- **`engine-update-contention-map.md`** — Map of `Engine::update()` execution: pass order, run-counts (routing + track-output + overrides recompute per-firing-track inside the tick loop = T× redundant global work), the CV-router-stale ordering bug, modulator→routing 1-tick staleness. Optimization candidates (hoist global passes out of per-track loop, fix ordering) deferred. Hoisting gated on the track-linking coupling — but see io-layout-linking-map: linking reads `sequenceState`, not composed output, so the gate likely clears.
- **`io-layout-linking-map.md`** — Map of track linking, output layout, input. Linking is a Curve-follows-stepped-source clone (`TrackLinkData` = sequenceState/divisor/relativeTick): only Curve consumes, only Note/Curve produce; 6 of 8 track types accept inert links. Output layout (channel→track/modulator + rotation) is type-agnostic but entangled with the per-tick rotation recompute. Input is fine (downstream complexity is in routing). Direction deferred.
- **`midi-subsystem-map.md`** — MIDI engine: `MidiOutputEngine` (per-output state, send Gate/Cv/CC/Modulator, CC rate-limit) + the layered input dispatch in `Engine::receiveMidi` (controllers → program-change → learn → routing → tracks → recording, first-consumer-wins) + MidiLearn + CvGateToMidiConverter. Direction deferred.
- **`recording-map.md`** — Live record-into-sequence (`RecordHistory` / `StepRecorder` / `CurveRecorder`). Second Note/Curve-only subsystem after linking — procedural types return the base no-ops, the parity gap in `finalize.md`. Direction deferred.
- **`clock-subsystem-map.md`** — Clock structural half (master gen, slave in, MIDI sync out + `Groove` swing, TapTempo/NudgeTempo, sub-tick scheduling). The time-source/µs half is owned by `wallclock-time-architecture`. Direction deferred.
- **`source-probe-investigation.md`** — Source trace for mutation boundaries, suspend/lock behavior, container lifetime gaps, and pool feasibility
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
- 2026-05-14: Full rewrite of all three documents from fresh source inspection. Key findings: (1) RoutingEngine evolved from sidecar to modulation engine — 5 stateful shapers, baseline ~7.4 KB CCMRAM; recoverable depends on design; (2) Model Container maxes at NoteTrack ~9.5 KB after P15 because Note/Curve genuinely need 17 full sequences; (3) Engine Container IS Teletype-driven at TeletypeTrackEngine=912 B, with conditional direct gap `(912 - 588) * 8 = 2,592 B`; (4) Pattern semantics diverged; (5) Output ownership is dual-path but undocumented; (6) Routing baseline now phrased as "~7.4 KB baseline; recoverable amount depends on design" not casual "~7 KB may recover"; (7) Experiment A1 explicitly states storage layout change is required (runtime branching alone saves nothing); A2 specifies per-TrackState union+discriminant; (8) Agent entry point guardrail restored; phase labels removed from architecture doc; unverified claims softened to "verify whether."
- 2026-05-14: Reclassified this task as research/reference, not active implementation. Model/container pool architecture is no-go under current "any track can be any type" semantics because static per-type pools are zero/negative RAM unless simultaneous type counts are product-capped. RoutingEngine state compaction remains the only current architecture-adjacent implementation candidate and is tracked under resource-optimization.
- 2026-05-15: Corrected post-P15 model-container sizing: ARM MonitorPage probes show `Track=9560`, `NoteTrack=9544`, `CurveTrack=9480`, and `TeletypeTrack=7104`; TeletypeTrack model cleanup is not a current top-level model RAM win. Engine probes show `TeletypeTrackEngine=912`, `Engine::TrackEngineContainer=912`, and `NoteTrackEngine=588`, making the direct conditional engine-container gap 2,592 B.

## Open questions

- [ ] Should RoutingEngine modulation state have an explicit lifecycle (create/destroy on shaper change) vs unconditional fixed array? Implementation candidate; see `routingengine-refactor-phased-plan.md`.
- [x] Should TeletypeTrackEngine or model Track storage use pools? No under current semantics; only saves RAM with explicit caps on simultaneous track types/Teletype engines.
- [x] Should Note/Curve sequence storage become lazy (active-pattern-only) to reduce model inflation? No for current RAM work; it implies save-format/value-copy semantics changes.
- [ ] Should shaper state persist across project save/load, or is "always resets on load" acceptable?
- [ ] Should track type be split into persistent project data + runtime generator?
- [ ] **Shared `Sequence` base for the universal property trio.** All 7 sequence types (Note/Curve/Stochastic/Indexed/DiscreteMap/Tuesday/PhaseFlux) duplicate `_trackIndex` + `_divisor` + `_resetMeasure` + the `isRouted`/`printRouted`/`writeRouted` plumbing. `temp-ref/phazer-dev-performer/src/apps/sequencer/model/Sequence.h` extracts these but does so wrong for XFORMER: it makes `writeRouted` **pure virtual** (current classes are non-polymorphic — adds a 4-byte vtable ptr × ~136 live sequences = **~544 B RAM regression** on a model at ~92.9%), bundles `firstStep`/`lastStep`/`runMode` that only Note/Curve (and partly Indexed) use, and assumes a `Routable` divisor that Stochastic lacks. Right shape: a **non-virtual** base (or CRTP) holding only the universal trio + inline routing helpers, `writeRouted` kept concrete per class; `firstStep`/`lastStep`/`runMode` go in a separate `StepSequence` intermediate for Note/Curve/Indexed. Dedup with zero vtable cost; verify sizeof on ARM before committing.
- [ ] **Engine update contention: dedup + reorder the per-tick passes.** `updateTrackOutputs`/`updateOverrides`/`routingEngine.update` are global recomputes invoked per-firing-track inside the tick loop (`Engine.cpp:179-181`) — T× redundant. Loop ordering also makes CV-route outputs one-update-stale. See `engine-update-contention-map.md`. Hoisting the global passes out of the loop is gated on confirming the track-linking intra-loop dependency (does a linked track read its source's freshly-composed output mid-loop?).
- [ ] **Linking: remove it** (lean, 2026-05-30 — never used). Curve-follows-Note synced modulation, now served by global modulators + routing + Curve clock-mult/global-phase. Drop `linkTrack` UI + `linkedTrackEngine` wiring + `TrackLinkData`/`linkData()`; forward-compatible read of the serialized `linkTrack` (no version bump). Also unblocks the engine-update hoist. See `io-layout-linking-map.md`.
- [ ] **Recording: needs more thought** — Note/Curve-only live capture; not a remove candidate. Scope-the-UI vs generalise-a-capture-contract, revisit after deciding if external-MIDI-into-procedural-tracks matters (Fractal/Stochastic capture may set precedent). See `recording-map.md`.
- [ ] **IO output layout** — channel→track/modulator + rotation recompute; rationalise together with the engine-update hoist (shared per-tick recompute).
- [ ] **Routing fan-out: collapse the five sources of truth.** A single routable target is defined across enum + `targetInfos` + `writeTarget` dispatch switch + picker applicability + model `writeRouted` (see `routing-five-sources-map.md`); drift produces silent dead routes (StochasticFeel no-op, PhaseFlux zero targets, Indexed borrowing DiscreteMap). Direction deferred — candidates: single per-target descriptor that all five derive from, vs per-track-type target ownership, vs consistency-pass-only. The `route-reordering` (ordering) and `routingengine-refactor-phased-plan` (RAM) tasks are orthogonal slivers, not this.
- [ ] **Generic `RoutableListModel` base.** UI list models for routable params repeat the same routed-getter / `printRouted` boilerplate per page. `temp-ref/performer-nx/src/apps/sequencer/ui/model/RoutableListModel.h` is a generic base for this. Pairs with the shared-`Sequence`-base dedup above; verify the boilerplate is actually repeated across list models before adopting.

## Completed steps

- [x] Captured research spec
- [x] Completed full research across all 7 axes (A-H)
- [x] Produced combined findings document (later split)
- [x] Restructured into architecture-research-directions.md + ram-recovery-experiments.md per user direction
- [x] Corrected model vs engine Container confusion
- [x] Added cross-references to resource-optimization task for verified size figures
- [x] Added source-probe and model-pool decision artifacts
- [x] Reclassified model-pool architecture as no-go under current product semantics

## Notes

This task is research/reference only. `architecture-research-directions.md` contains the architectural analysis. `ram-recovery-experiments.md` contains measurable RAM experiments. Do not collapse them — they serve different purposes.

If asked for the next step of `architecture-research-directions.md`, answer in architecture scope: refine the source-grounded assumption/mismatch map, including musical/workflow identity. Do not route to RAM experiments unless the user explicitly asks to switch to RAM recovery or `resource-optimization`.

Before proposing a broad refactor, use `architecture-change-decision-gates.md`. Most mismatches should become documentation, measurement, or local optimization unless they pass the gates.

Current restructuring decision: do not create a model-pool implementation task. The pool decision belongs here as a closed architecture decision unless the product semantics change from "any track can be any type" to explicit simultaneous type caps.
