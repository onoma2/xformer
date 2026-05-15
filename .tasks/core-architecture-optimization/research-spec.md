# Research Spec: XFORMER Core Architecture Optimization

## Objective

Research whether XFORMER's current architecture still fits its expanded scope.

Original Performer was designed as a compact X0X-style sequencer with mainly Note and Curve tracks, simple routing, and small resident state. XFORMER added multiple heavyweight track types and subsystems: Teletype, Tuesday, DiscreteMap, Indexed, Harmony, advanced RoutingEngine state, USB keyboard, Launchpad expansion, and fork-derived features.

The goal is to identify where original Performer core mechanics are now structurally mismatched, and propose research-backed optimization directions for RAM, lifecycle, state ownership, UI complexity, and musical behavior preservation.

This is a research task only. Do not implement code.

## Derived Documents

This spec has produced the following derived documents:

1. **`architecture-research-directions.md`** — Broad architecture research: original assumptions, mismatches, research directions, ranking. Not an implementation plan. Covers structural incoherence, lifecycle, pattern semantics, routing evolution, output ownership, state taxonomy, and UI friction.

2. **`architecture-change-decision-gates.md`** — Policy for deciding whether a mismatch justifies major architecture change. Use before proposing broad refactors.

3. **`ram-recovery-experiments.md`** — Narrow RAM recovery experiments only. Baseline measurements, specific experiments with hypotheses, verification methods, exit criteria, and rollback plans. Does not pretend to be the architecture answer.

4. **`source-probe-investigation.md`** — Source trace for allocation timing, realtime reference stability, project load suspend behavior, and container lifetime gaps.

5. **`model-pool-decision-table.md`** — Go/no-go decision artifact for Track/Engine pool architecture. Current decision: no model-pool implementation under "any track can be any type" semantics.

6. **`routingengine-refactor-phased-plan.md`** — Concrete plan for RoutingEngine state compaction. Implementation belongs under the active `resource-optimization` task.

This spec serves as the agent handoff brief for future research passes. Read the derived documents for findings.

## Repository Context

Working directory:

`/Users/foronoma/Work/Code/Eurorack/performer-phazer`

Primary reference docs:

- `.tasks/teletype-performer-ecosystem-redesign/OVERVIEW.md`
- `.tasks/teletype-performer-ecosystem-redesign/savings-plan.md`
- `.tasks/resource-optimization/TASK.md`
- `.tasks/STATUS.md`

Primary source areas:

- `src/apps/sequencer/model/` — Track types, Project, Routing, FileManager, ClipBoard
- `src/apps/sequencer/engine/` — Engine, RoutingEngine, TrackEngine, TeletypeTrackEngine, TeletypeBridge
- `src/apps/sequencer/ui/` — Pages, controllers, Ui singleton
- `teletype/src/` — VM, ops, state, parser, bridge stubs
- `src/apps/sequencer/CMakeLists.txt` — Build configuration, included/excluded Teletype files

## Central Thesis

XFORMER may need resource-aware core architecture changes, not only local RAM optimizations.

Original Performer could afford always-resident variant containers because Note/Curve had similar footprints. XFORMER cannot assume every track type, engine, pattern object, route shaper, and UI surface should be resident at maximum size all the time.

Test whether heavy features should move toward explicit lifecycle, pools, caps, capability-based interfaces, or shared native services.

Before recommending a major architectural change, apply `architecture-change-decision-gates.md`. Do not turn every mismatch into a rewrite.

## Key Findings Summary

### Architecture Problems (not just RAM)

1. **Routing is now a modulation engine**, not a sidecar. It owns time-accumulated state for 5 shapers across 16 routes × 8 tracks, allocated unconditionally. It needs lifecycle semantics.
2. **Model Container pays NoteTrack's 9,544 B tax** because all track types share one max-sized variant. This is Note/Curve-driven, not Teletype-driven; post-P15 ARM probes show `Track=9560`, `NoteTrack=9544`, `CurveTrack=9480`, `TeletypeTrack=7104`.
3. **Engine Container pays TeletypeTrackEngine's 912 B tax** because all engine types share one max-sized variant. This IS Teletype-driven.
4. **Pattern storage semantics diverged:** Note/Curve have 17 full sequences; Teletype has 2 swap-buffer slots; Tuesday/Indexed have param-only sequences. One model doesn't fit all.
5. **Output ownership is deliberate dual-path** (track engines + Teletype bypass + routing + bus CV + rotation), but undocumented.
6. **State lifecycle is scattered**: persistent project state, runtime/rebuildable state, transaction/scratch state, compatibility state, and dual-path mirror state have no formal classification.
7. **UI branching multiplied across 7 track types** (~15-20 call sites), creating maintenance friction for future track type additions.

### RAM Recovery Opportunities (measurable)

See `ram-recovery-experiments.md` for full details:

1. **Experiment A1:** Skip TrackState for non-per-track routes (~2,688 B CCMRAM)
2. **Experiment A2:** Union-based TrackState by shaper type (~4,096 B CCMRAM)
3. **Experiment B:** Consolidate ttSlot backup buffers (~1,226 B BSS)
4. **Experiment C:** Container size measurement (info only)
5. **Experiment D:** Symbol and section measurement (info only)
6. **Experiment E:** Task stack watermark measurement (~1,536 B CCMRAM potential)

## How To Answer "Next Step"

If asked for the next step of `architecture-research-directions.md`, stay in architecture scope:

1. Build or refine the source-grounded Original Performer assumption/mismatch map.
2. Include musical/workflow assumptions, not just storage/RAM assumptions.
3. Separate each finding into architecture problem, user-visible quirk, future-feature friction, RAM side benefit, and verification needed.

Do not answer with a RAM experiment unless the user explicitly asks to switch to `ram-recovery-experiments.md` or the active `resource-optimization` task.

## Recommended Document Use

1. Use `architecture-research-directions.md` for broad architecture research and assumption/mismatch mapping.
2. Use `architecture-change-decision-gates.md` to decide whether a mismatch justifies a major refactor.
3. Use `ram-recovery-experiments.md` only for concrete, measurable RAM recovery experiments.
4. Use this spec as the stable handoff point for future research agents.

Current restructuring decision: keep core architecture as research/reference, not an implementation queue. The next implementation work is resource optimization: ARM size probes, RoutingEngine state compaction, then lower-risk Teletype backup and sequence-header experiments.

## Evidence Requirements

Every finding cites source: file path, line number where practical, symbol/struct/function name, and whether evidence is from source, build artifact, or task doc.

Use current source as authority. Task docs are useful but can be stale.

Do not run destructive git commands. Do not commit.
