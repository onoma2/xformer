# XFORMER engine refactor — toward Sequoa-style shared internals

Status: discussion / proposal. Not started. Captures a design conversation comparing
XFORMER (the `performer-phazer` fork of Phazerville PER|FORMER) against Sequoa's
"one primitive, many configs" model.

Subject codebase: `/Users/foronoma/Work/Code/Eurorack/performer-phazer/`
(engine at `src/apps/sequencer/engine/`; architecture ref at `docs/performer-architecture.md`).
Reference model: Sequoa (`drafts/sequoa/sequoa.html` in this repo) — the partition-crosser.
File/line claims below should be re-verified against code; last looked at 2026-06-08.

---

## 0. Framing shift: Sequoa as substrate, not just helper code

Sequoa was designed as a general all-type sequencer builder, independent of XFORMER. That
matters. XFORMER/PER|FORMER began with an x0x-shaped assumption: a track is a step sequence
with specialized data, originally enough for Note and Curve. Most later TrackModes were then
built vertically from their own musical idea, so timing, traversal, modulation, gate, and
output logic were repeatedly created inside each engine.

Sequoa starts from the opposite assumption:

> sequencer = composition of temporal and musical primitives

Use Sequoa here as a candidate substrate model, not as a blueprint to copy and not as a reason
to collapse XFORMER's UI into a config editor. The useful question is not "which helper can we
extract first?" It is:

- Which concepts recur because they are fundamental sequencer primitives?
- Which concepts recur only because XFORMER lacks a representation for them?
- Which concepts are genuinely inseparable track identities?

Likely primitive columns:

- phase or event source
- partition / traversal
- stage window and order
- parent dwell with child events
- value source
- degree-to-voltage / scale projection
- accumulator and mutation point
- gate policy
- slew / output policy
- reset and synchronization policy

Viewed this way, existing tracks become conceptual compositions while remaining concrete C++
classes, model structs, storage formats, and edit pages:

- **Note:** clock mover + uniform partition + rich event/gate policy + degree readout.
- **Indexed:** duration mover + ordered stages + simple gate/readout.
- **PhaseFlux:** weighted partition + traversal + nested pulse scheduler + curved value generation.
- **DiscreteMap:** external continuous mover + directional crossing policy + table readout.
- **Curve:** clock phase + function-valued stage source + continuous signal processing.
- **Stochastic / Tuesday / Teletype:** generators or VMs that may share output/utility edges, but
  do not reduce cleanly to the same stage pipeline.

Therefore the first useful artifact should be a **track decomposition matrix**, not immediate
common code. Rows are XFORMER track types; columns are Sequoa-derived primitives and lifecycle
semantics. Implement only the cells that prove identical under current behaviour. `StageCrosser`
or a smaller `StagePartition` may still be a good extraction, but it is a second-order result of
that matrix, not the starting premise.

---

## 1. Vision, Goals, and Hard Constraints

**The Vision:** We are explicitly abandoning the pursuit of a grand, unified "one-engine-to-rule-them-all" like Sequoa. XFORMER's strength is its diverse, bespoke track behaviors. The new vision is a surgical, low-risk cleanup: extracting localized, identical math into shared stateless helpers so the complex generative logic of each track is no longer cluttered by boilerplate.

If this refactoring strategy succeeds, we will achieve:
- **Codebase Maintainability:** Elimination of hand-written, copy-pasted boilerplate (like the 5 identical implementations of pitch projection) into a Single Point of Truth.
- **Semantic Consistency:** Guaranteed parity across tracks. If we ever tweak transposition edge-cases or add microtonal support, we do it in one place, ensuring all tracks handle pitch identically.
- **Resource Efficiency:** Zero RAM penalty. Using stateless, inlineable namespace helpers avoids vtables and union bloat, respecting the strict STM32 constraints and potentially saving a small amount of flash memory.
- **Architectural Momentum:** Establishing a safe, proven workflow (extract → verify parity → deploy) that paves the way for evaluating larger structural patterns (like `SortedQueue` convergence) without breaking live DSP code.

**Constraints:**
- **No config UI / no "Custom slot".** Track types stay the user-facing **presets** they are
  today (Note Circus, Curve Studio, DiscreteMap, Indexed, PhaseFlux, Tuesday/Algo, T9type,
  Stochastic, …). Their distinct identities and edit pages are the product.
- **Internals only.** No `*Track` model changes, no serialization/project-format change, no
  `ProjectVersion` bump (PROJECT.md forbids mid-dev bumps), no UI page rewrites.
- **Zero Regressions.** Every refactor must be **behaviour-preserving**, verified by focused parity unit tests or full output diffs (§6).

---

## 2. Current shape (what we're refactoring)

- One `TrackEngine` base: virtual `tick(uint32_t)` / `update(dt)` / `reset` / `restart` /
  `gateOutput` / `cvOutput` / `sequenceProgress`. Shared output surface.
- `Track` is a **tagged union** of per-mode model structs (`noteTrack()`, `curveTrack()`,
  `discreteMapTrack()`, `phaseFluxTrack()`, …), each guarded by `SANITIZE_TRACK_MODE`.
- One `…TrackEngine` subclass per `TrackMode`. The host is a **PPQN tick clock** (1 kHz update,
  192 master PPQN); engines step on tick counts. This is the inverse of Sequoa's clockless
  lookahead-phase scheduler.

**Already shared utils** (the cheap stuff is done): `SequenceState` (play order
fwd/rev/ping/random — Note, Curve, Indexed, Stochastic), `Slide`/`Slew` (glide — nearly all),
`Groove` (swing), `model/Scale` (quantize), `AccumulatorOps` (currently only PhaseFlux).

**Duplicated, per-engine** (the extraction targets): the placement→threshold→stage logic and
the readout pipeline ordering, each hand-written per engine.

---

## 3. The factoring: TIME × VALUE × GATE

Every stage-based track decomposes into three sublayers. They refactor very differently.

```
            TIME (when/which stage)        VALUE (what CV)            GATE (whether/how a trigger)
            ── shareable core ──           ── pipeline + sources ──   ── partially shareable ──
  driver →  StageCrosser ───────────────→  ValueSource → Readout ───→ GateSource
 (per-eng)  placement·stageOf·stagePhase   table|interp  quantize/     gate%·prob·ratchet
                                                         transpose/     (+ Note conditions: bespoke)
                                                         accum/slew
```

- **TIME** is Sequoa's crosser. Highly shareable.
- **VALUE** is Sequoa's readout. The pipeline is shareable; the per-type *data* stays in the
  existing models.
- **GATE** is where XFORMER diverges most from Sequoa (Sequoa has only gate%/rest/tie). Only the
  simple base is shareable.

---

## 4. Substrate Reality: Four Incompatible Time Primitives

Research against the actual XFORMER codebase (June 2026) reveals that Sequoa's central assumption—a unified `StageCrosser` handling fractions over weights—is **fundamentally incompatible** with XFORMER's stage tracks. 

XFORMER does not use one time model; it uses four distinct, irreducible time progression primitives:
1. **Grid/Tick Modulo:** Note and Curve tracks evaluate `tick % divisor` or `clock().tickPosition()`.
2. **Duration Sums:** Indexed and Stochastic tracks accumulate wallclock ticks (`_stepTimer += scaledDelta`) and evaluate sequentially against stored durations.
3. **Voltage Thresholds:** DiscreteMap uses external CV or an internal triangle to cross absolute voltage boundaries (Rise/Fall logic).
4. **Cumulative Fraction:** PhaseFlux does a linear scan over a pre-calculated `_cumulativeTicks` array to find its place within a cycle.

**Verdict on `StageCrosser` (TIME): Rejected.**
Attempting to merge these four mechanisms into a single time substrate would require massive union states, polymorphism in the 1kHz loop, and severe RAM bloat. The clean, localized logic of each track type would be obliterated. "Stage" and "Tick Position" mean radically different things depending on the track.

## 5. The True Commonalities: Readout and Queuing

While time is fractured, the **VALUE** and **GATE** layers show significant structural duplication.

### P1 — `PitchReadout` Pipeline (VALUE output). Immediate Extraction.
The most identical, duplicated block of code in XFORMER is the chromatic/scale projection logic:
`scale.noteToVolts(note + shift) + (scale.isChromatic() ? rootNote / 12.f : 0.f)`
This exact math is hand-written in Note, Indexed, PhaseFlux, Stochastic, and Tuesday.

**Extraction Strategy:** Extract this strictly into a stateless `PitchReadout` helper namespace. It holds no state, affects 5 files, prevents accumulator/slew creep, and poses zero risk of breaking 1kHz timing or RAM limits. 

### P2 — `SortedQueue` Event Queuing (GATE). Deferred.
Note, Stochastic, Tuesday, and Curve all share the `SortedQueue` structural pattern to push `{tick, cv/gate}` pairs outside the main state machine (handling overlapping ratchets/bursts). 
However, their payloads differ completely (e.g., Note carries accumulator/CV metadata, Tuesday carries CV per micro-gate). Moving all tracks to a unified queue wastes memory for simple timer tracks (Indexed), and forcing queue tracks back to timers breaks their overlapping ratchets. This is a shared pattern, but not yet a shared primitive.

### P3 — Slew Application
`Slide::applySlide()` is already universally used for time-based exponential smoothing across Note, Curve, Indexed, and Teletype. This is a solved primitive.

## 6. Verification Strategy

Because the first target (`PitchReadout`) is a tiny, purely mathematical extraction, the verification strategy shifts:

1. **Focused Parity Tests:** Skip the full "golden-output trace" harness for the first extraction. Instead, write focused parity unit tests around each caller to prove exact math equivalence before and after.
2. **Deferred Golden Traces:** Reserve the heavy "golden output trace" harness (logging `_cvOutputTarget` in the desktop simulator) for future extractions that touch timing, `SortedQueue` operations, accumulators, or scheduler order.

## 7. Open Questions

1. **NoteTrack Follower:** Can NoteTrack's harmony follower logic be integrated into the shared `PitchReadout` pipeline without creating circular dependencies between track engines?
2. **Accumulators:** NoteTrack's `Accumulator` object (global/local UI) is entirely different from PhaseFlux's array of `AccumulatorOps` (Wrap/Pendulum bounds logic) and Stochastic's simple cycle-end `_jumpRegister`. Is there any value in unifying these, or should mutation mechanics remain explicitly per-track?

## 8. Executive Verdict

XFORMER's time models are not variants of one simple fraction model. The seductive "all tracks are StageCrosser configs" path is blocked.

The near-term extraction must stay very small and stateless. Extract `PitchReadout` to deduplicate pitch projection math, verify with unit tests, and leave time, queues, and accumulators alone until the math baseline is solidified.
