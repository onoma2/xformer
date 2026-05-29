# Routing — five-sources-of-truth map

_Surveyed 2026-05-29. Research artifact only — no direction chosen yet. Documents why the routing system is "impossible to navigate and think about": the definition of a single routable target is smeared across five places that must be edited in lockstep, and drift between them produces silent dead routes._

## Scale

- `Routing::Target` enum: **103 entries** (`Routing.h`), grouped by `XxxFirst`/`XxxLast` sentinel pairs.
- `Routing::Source` enum: **37 entries** (`Routing.h`), `CvFirst..CvLast`, `Mod`, `Bus`, etc.
- ~2,380 lines total: `Routing.h` 872 + `Routing.cpp` 606 + `RoutingEngine.{h,cpp}` 770 + `CvRoute.h` 134.
- Built for 2 homogeneous track types (Note/Curve, shared param set); now spans 8 heterogeneous types, each bolting a param block onto one flat global enum.

## The five sources of truth

A single routable target is defined across all five. Adding one param = five coordinated edits; miss one = **silent no-op**.

1. **`Target` enum** — `model/Routing.h`, `enum class Target`. The flat namespace + sentinel groups (Engine/Project/PlayState/Track/Sequence/Tuesday/Chaos/Wavefolder/DiscreteMap/Indexed/Bus/Stochastic).
2. **`targetInfos[int(Target::Last)]`** — `model/Routing.cpp:375`, `struct TargetInfo` array (name, range, flags), designated-initialized per target. Metadata source.
3. **`writeTarget` dispatch switch** — `model/Routing.cpp:208-319`. Per-`trackMode` switch mapping target groups → model setter (track-level vs all-patterns sequence-level).
4. **UI picker applicability** — `ui/pages/RoutingPage.cpp` + `Routing::isPerTrackTarget` (`Routing.h:410`). Decides which targets the user can pick / whether per-track.
5. **Per-model `writeRouted`** — each `*Sequence`/`*Track` implements the actual write. The leaf that can silently lack a `case`.

Group membership itself lives in 13 `isXxxTarget()` range predicates (`Routing.h:362-413`), each `>= First && <= Last`. Sources have a **parallel** fan-out: `RoutingEngine::updateSources()` (`RoutingEngine.cpp:349`) reads engine state into `_sourceValues[]` per source — the same smearing on the source side.

## Per-trackMode applicability matrix (from the dispatch switch)

| TrackMode | Target groups accepted | Notes |
|-----------|------------------------|-------|
| Note | Track | sequence targets not wired |
| Curve | Track + `CurveRate`; Sequence + Chaos + Wavefolder | Curve hosts wavefolder/chaos modes |
| MidiCv | Track | |
| Tuesday | Track + Sequence + Tuesday | |
| DiscreteMap | Track + DiscreteMap; Sequence | |
| Indexed | Track + **DiscreteMap** (borrowed); Sequence + Indexed | borrows DiscreteMap targets AND has own IndexedA/B — dual-sourced |
| Teletype | **none** | `Routing.cpp:284` — `break`, no targets |
| Stochastic | Track; Sequence + Stochastic; Divisor/Scale/RootNote **special-cased individually** | special-cased because `StochasticSequence._divisor` is plain `int`, not `Routable` |
| PhaseFlux | Track + Sequence | **no PhaseFlux-specific targets exist** |

## Inconsistency / drift inventory

- **`StochasticFeel` silent no-op** — in `isStochasticTarget` range + picker, but `StochasticSequence::writeRouted` has no case. Routed value assigned, never written. (Also in `performer-improvements` Phase 0 / `stochastic-track-port/finalize.md`.) Canonical proof of the five-source drift failure.
- **PhaseFlux has zero routable params** — rich per-stage nudges/warp shipped, none added to enum/dispatch/picker. Only universal Track targets route.
- **Indexed borrows DiscreteMap targets** — track-level writes gated on `isDiscreteMapTarget`; its own `IndexedA`/`IndexedB` handled separately at sequence level. Two target groups for one track type.
- **Teletype: no routing targets** at all.
- **Stochastic Divisor/Scale/RootNote special-cased** — because its divisor diverges from the `Routable` pattern (see also the shared-`Sequence`-base item: Stochastic's non-`Routable` divisor).
- **Missing Stochastic targets** (per finalize.md): PatienceMelody (rhythm twin routable, melody not), Range, MaskMelody, TiltMelody.
- **Naming drift** — Stochastic-prefixed (`StochasticMask`) vs Tuesday bare (`Flow`/`Ornament`/`Power`) vs DiscreteMap. No convention.

## Why it resists thinking

The target's identity has no single home. Adding `FooParam` to track type X means: enum entry (1) + targetInfos row (2) + a `case`/group in the dispatch switch (3) + picker applicability (4) + the model `writeRouted` case (5) — plus possibly a new `isXxxTarget` predicate and First/Last sentinels. Any omission compiles fine and fails silently at runtime. The drift bugs above are each a different one of these five being skipped.

## Candidate directions (deferred — not chosen)

- **Collapse the five into one** — a single per-target descriptor (name, range, applicable track modes, model setter ref) that dispatch + picker + metadata derive from. Adding a param = one edit; silent no-op becomes impossible.
- **Per-track-type target ownership** — each track type registers its own routable params; the flat enum stops being the dumping ground.
- **Consistency pass only** — keep the architecture, fix the listed drift (PhaseFlux targets, Indexed dual-sourcing, StochasticFeel, naming). Doesn't stop future drift.

## Relation to existing slivers

- `route-reordering` (`reorder-spec.md`) — only signal-flow **ordering** of the enum, and stale (62-target snapshot, pre-Stochastic/PhaseFlux). Cosmetic; orthogonal to the fan-out.
- `routingengine-refactor-phased-plan.md` — only **RAM** (TrackState union compaction). Orthogonal to the fan-out.
- Neither addresses the five-sources problem.
