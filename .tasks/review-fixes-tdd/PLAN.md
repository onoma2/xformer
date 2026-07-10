# TDD plan — adversarial review fixes & refactors

Companion to `0207refactoring.md`. Red-green-refactor per item: failing test first, minimum
fix, then cleanup. Ordered engine-first, cosmetics-last. Reviewed by adversarial-document
and feasibility passes; harness facts below are what the reviewers confirmed against the
build, not against file contents alone.

## Harness facts (build-verified)

- Test style: `#include "UnitTest.h"`, then `UNIT_TEST("Name") { CASE("...") { expectEqual(a, b, "msg"); } }`. See `src/tests/unit/sequencer/TestGateRotation.cpp`.
- Registration: one line per test in `src/tests/unit/sequencer/CMakeLists.txt` — `register_sequencer_test(TestX TestX.cpp)`, linked against `sequencer_shared`. `TuesdaySequence.cpp` is force-compiled into every test by the register function.
- **No registered test constructs a real `Engine`.** The ctor takes nine driver references (`Engine(Model&, ClockTimer&, Adc&, Dac&, Dio&, GateOutput&, Midi&, UsbMidi&, UsbH&)`, `engine/Engine.h:75`). The engine-instantiating test files on disk (`TestDiscreteMapTrackEngine`, `TestNoteTrackEngine`, `TestAccumulatorModulation`, `TestRTrigSpreadAccumTiming`) are all unregistered/disabled — `CMakeLists.txt:110,119,163` ("stale API", "requires complex mock dependencies"). Registered `TestFractalTrackEngine` takes an `Engine&` from raw storage without constructing one (`TestFractalTrackEngine.cpp:19`) — a fake-Engine precedent, not a real fixture. Two previous attempts at real engine tests died on this wall; Phase 0 exists because of it.
- Sim drivers register with the `sim::Simulator` singleton in their constructors; `Simulator::instance()` derefs a null global unless a Simulator was constructed first (`src/platform/sim/sim/Simulator.cpp:176`, set in the ctor at `:21`). **Construct the `sim::Simulator` before any driver** — `ClockTimer`, `Adc`, `Dac`, `GateOutput`, `Dio`, `Midi`, `UsbMidi` all touch `Simulator::instance()` in their ctors/setup. The Simulator ctor is lightweight — `sim::Target` is three `std::function`s (`src/platform/sim/sim/Target.h:13`) — and `Simulator.cpp` links into every test via the platform sources (`src/platform/sim/CMakeLists.txt:23` → `src/CMakeLists.txt:64`). A headless fixture is therefore buildable, not hypothetical.
- **Ticks advance via `Simulator::step()`, not `slaveTick()` alone.** `Clock::slaveTick(int)` only queues slave subticks; the real `_tick` advance happens in `Clock::onClockTimerTick()` (`Clock.cpp:129,321`), which the sim `ClockTimer` fires only from `Simulator::step()` callbacks (`ClockTimer.h:49` → `Simulator.cpp:180`). The fixture must drive `Simulator::step()` deterministically after posting clock events. `Clock::slaveTick`/`slaveStart`/`slaveStop`/`slaveContinue` are public (`Clock.h:91`), reachable via `Engine::clock()` (`Engine.h:188`). This step-drive gap is the seam the previous two attempts almost certainly tripped on.
- `Track::setTrackMode` is private; fixtures must use `project.setTrackMode(i, mode)` (idiom in `TestTT2UiAccess.cpp:6-7`).
- Model-level serialization tests use a `VersionedSerializedReader` over an in-memory buffer (`TestCurveTrackSerialization.cpp:26`).
- Header-only seams testable without the Engine: `TT2Evaluator.h` (free functions over `TT2Runtime`+`TT2OutputState`, precedent `TestTeletypeV2Evaluator.cpp`), `RouteResolve.h`, `RouteDraft.h`, `Routing.cpp` override table (`TestRoute*` precedent), pure clock helpers (`Clock::acceptSlavePeriod`, precedent `TestSlaveClockGuard`).
- Build/run: local machine only. Unit tests build under the top-level `build/` tree; the playable sim is `build/sim/debug`.

## Discipline

- A `CASE` that **reproduces a defect** must FAIL on current `dev` before its fix lands. That is the gate.
- Seam construction (fixtures, extractions that make a defect reachable) is production work that legitimately precedes the failing test — it must be behavior-neutral, and where practical is pinned by a characterization test of its own.
- Characterization tests (Phase 5) pass immediately by definition; their gate is that they stay green across the refactor.

---

## Phase 0 — EngineTestFixture (DONE 2026-07-06)

Landed: `src/tests/unit/sequencer/EngineTestFixture.h` + `TestEngineFixture.cpp`
(registered), green. No production changes. Confirmed findings for anyone building on it:
- Drive time with `Simulator::wait(ms)` (public; `step()` is private). `_tick` is a fake
  counter, so it is deterministic and instant — no real sleeping.
- Construct `sim::Simulator` with a **non-empty** `Target` (create/destroy/update lambdas);
  `step()` calls `_target.create()`/`update()` unconditionally, so empty `std::function`s
  throw `bad_function_call`.
- **The Clock boots with a pending Reset** (`Clock.h`: `_requestedEvents = Reset`). On
  hardware the engine runs `update()` from boot so it drains before play. A fixture that
  calls `clockStart()` immediately queues Start alongside the boot-Reset; `checkEvent()`
  processes Start (running=true) then Reset (running=false) and the transport looks dead.
  The fixture drains it with one `update()` in its ctor before exposing `start()`.
- Drive `engine.update()` each step via `Simulator::addUpdateCallback`; the ClockTimer's
  own callback (registered in its ctor) runs first, so ticks are advanced before the
  engine consumes them within the same step.

### Original Phase 0 brief

A shared headless fixture, constructed in strict order: no-op-`Target` `sim::Simulator`
**first**, then sim drivers, then `Model`, then `Engine`. Deterministic tick delivery is
post clock event (`Clock::slaveTick`/`slaveStart`/`slaveStop`/`slaveContinue` via
`Engine::clock()`) **then drive `Simulator::step()`** to actually advance `_tick` — calling
`slaveTick()` alone only queues subticks and advances nothing. Prove it with one smoke test
— construct, set a track mode via `project.setTrackMode`, step N ticks, read
`gateOutput()`/`busCv()` — registered and green before any bug test is written against it.

- Unblocks: 1.2, 2.1, 2.2, 2.3, 3.2-integration, 3.3, 4.2, and the ClockMult case in Phase 6.
- Known risk: deterministic clock injection is exactly where the previous two engine-test attempts died. If the fixture stalls, the fallback for each blocked item is its named pure-function seam (listed per item), not skipping the test.
- Effort: 1-2 days. Nothing in Phases 1-4 that needs it starts before the smoke test is green.

---

## Phase 1 — Critical

### 1.1 UserScale out-of-bounds write (`model/UserScale.cpp:38-55`)
Constructible today; no fixture needed. Highest severity, cheapest test.

- **Seam:** none — `UserScale::read(VersionedSerializedReader&)` over an in-memory buffer.
- **RED** — `TestUserScaleSerialization.cpp`:
  - `CASE("read clamps oversized size byte")`: buffer with size byte 255 → after `read()`, assert `size() <= CONFIG_USER_SCALE_SIZE`. Note the failure mode on current code is a real ~450-byte out-of-bounds write inside the test process — under ASan that is a crash, which IS the red signal; without ASan the size assertion carries it. No canary tricks (UB-reliant).
  - `CASE("read with correct hash but oversized size still rejected")`: a well-hashed buffer with size 40 → `read()` must not return true (today it does — hash passes after the stomp).
  - `CASE("valid round-trip")`: `write()` a 12-note scale, `read()` back → equal.
- **GREEN:** clamp `_size` immediately after `reader.read(_size)` — route through `setSize()` (`UserScale.h:77-78`) so there is one clamp authority; bail before the loop if the file byte exceeds the array. Read-side only; `write()` and the format untouched; no version gating (no-migration policy).
- **REFACTOR:** make `FileManager::readUserScale` (`model/FileManager.cpp:213`) check `header.valid()`/`type` the way `readProject` does (manual sim verification — no harness seam for FileManager tasks). Audit other `read()`s that loop on a file-supplied count (`Serialize.h` readArray callers).

### 1.2 Routed-bus double-count (`Engine.cpp:125-127,172,~290`; `Engine.h:155-169`)
Riskiest item in the plan. Needs Phase 0. Two writer classes double, not one.

- **Mechanism:** `_busCvWritten` resets once per frame; the frame-level `_routingEngine.update()` seeds each lane; the in-tick recompute calls `update()` again with the seed flag already true → additive double. The CV-router path doubles the same way: `updateOverrides()` → `updateCvRouteOutputs()` → `setBusCv(..., BusWriterCvRouter)` (`Engine.cpp:857`) runs both inside the recompute (`:291`) and at frame tail (`:300`).
- **Hard constraint:** the second compose exists on purpose — it breaks the routing↔CV-route one-frame-staleness cycle (comment block `Engine.cpp:277-283`). The fix dedupes *accumulation per writer*, it does not remove the second compose. Teletype's legitimate accumulation from track ticks (`TT2TrackEngine.cpp:281`) must keep summing.
- **RED** — `TestRoutingBusDoubleCount.cpp` (fixture):
  - `CASE("routing writer holds depth on ticking and non-ticking frames")`: Modulator→BUS1 route at depth → lane reads V via public `busCv(int)` on a non-ticking frame AND on a frame where a track ticks past the reducer (fails today at 2V).
  - `CASE("cv-router writer not doubled on ticking frames")`: same shape for the `BusWriterCvRouter` path.
  - `CASE("two distinct writers sum once each")`.
  - `CASE("teletype tick accumulation still sums")`: regression for the intentional multi-write case.
- **GREEN:** make each writer's per-frame contribution idempotent across routing recomputes (the per-writer tagging on the lane already exists — extend seed-vs-add to be per-writer-per-frame), leaving the second compose in place.
- **REFACTOR:** none beyond the green — the "unify the call sites" idea is off the table per the hard constraint. Update `docs/performer-architecture.md` §3/§10 to describe the real reducer/compose loop as part of this item (the doc drift is what hid the bug).

---

## Phase 2 — Major, audible (all need Phase 0)

### 2.1 Gates stick HIGH on MIDI/slave stop (`TrackEngine.h:62`; `Engine.cpp:135-139`)
- **RED** — `TestTransportStopGates.cpp` (fixture):
  - `CASE("Clock::Stop drops pending gates")`: Note track, gate-on scheduled with later gate-off, deliver Stop between them → `gateOutput()` low (fails today).
  - One `CASE` per engine with a `stop()` override — Stochastic, Fractal, PhaseFlux, **Indexed** (`IndexedTrackEngine.h:25`) — hook fires, queue cleared.
  - `CASE("reset path still clean")`: regression.
- **GREEN:** call `trackEngine.stop()` for every engine from the `Clock::Stop` handler; add a `NoteTrackEngine::stop()` that drains `_gateQueue` and forces gate low.
- **Owner note (mild):** dropping gates on Stop changes Stop→Continue behavior for slaved playback (pending gate-offs no longer straddle the stop). Flag at implementation, not a blocker.
- **REFACTOR:** share the gate-drain between `stop()` and the reset path.

### 2.2 Harmony follower vs master track order (`NoteTrackEngine.cpp:967-985`) — DEFERRED
**Owner-deferred to the harmony-track implementation.** The follower's order-dependent read of
the master's `currentStep()` is a real defect, but the fix (index-order constraint vs
frame-start snapshot) is a design choice that belongs with the dedicated harmony-track work,
not this review-fixes pass. Not tested or changed here. Carry the finding forward: today the
result depends on whether `masterTrackIndex < followerIndex` (master-below = current step,
master-above = one tick stale), and routing's frame-start `_sourceValues` snapshot is the
mechanism a placement-free fix would reuse.

### 2.3 Pulse-hold reads the unrotated step (`NoteTrackEngine.cpp:193-198` vs `:556`)
- **RED** — `TestNoteTrackPulseHold.cpp` (fixture; `NoteTrackEngine::tick` derefs `_engine.measureDivisor()`/`midiOutputEngine()`/`state()`, so no placeholder-Engine shortcut):
  - `CASE("pulse count read from the audible rotated step")`: rotate=1, pulses=4 on the sounding step → hold lasts 4 pulses (fails today).
  - `CASE("fill sequence pulses honoured")`, `CASE("rotate=0 unchanged")`.
- **GREEN:** read `pulseCount()` from the same rotated/eval `(sequence, index)` pair `_currentStep` derives from.
- **REFACTOR:** compute the audible pair once, use it for both trigger and hold lookup.

---

## Phase 3 — Major, routing / model

### 3.1 Scale/RootNote route crushes the Default (-1) sentinel (`Routing.cpp:326-332`; 7 sequence types)
Constructible today (model-level, `TestRoute*` idiom). **Owner-resolved:** nonzero modulation
on a Default-follow track modulates **relative to the resolved project scale**, not to an
absolute index. Rationale: a track set to "follow project scale" that gets modulated must
keep following project-scale changes; the mod is an offset from the resolved scale, so it
never silently detaches. Delta-0 preserving `-1` is settled.
**Seam (corrected):** the resolution must live where the project scale is available. The
zero-arg getter `scale()` (`NoteSequence.h:392`) has only `_trackIndex`/`_scaleGroup` — no
`Project&`, so it **cannot** compute `project + delta`. The project scale reaches the sequence
only as the argument to `selectedScale(projectScale, projectRotate)` (`NoteSequence.h:411-412`),
which the engine supplies from `_model.project().scale()`. So the fix splits by case:
  - Delta-0 / at-rest sentinel preservation belongs in the routed getter (return `-1` unchanged when no override).
  - Default + nonzero-delta resolution belongs in `selectedScale()` (and the rootNote analog), which has `projectScale` in hand.
- **RED** — `TestRoutedScaleSentinel.cpp`:
  - `CASE("scale route at rest preserves Default")`: delta 0 on a `-1` track → `scale()` returns `-1` (fails today — clamps to 0). Same for rootNote.
  - `CASE("non-Default base still modulates")`: ordinary track (base ≥ 0) modulates as before — regression guard.
  - `CASE("nonzero delta on Default modulates around the resolved project scale")`: `selectedScale(P, …)` on a `-1` track with override delta d resolves to `clamp(P + d, 0, Scale::Count-1)`, and a project-scale change moves it (fails today — the getter clamps `-1`→0 before `selectedScale` ever sees the sentinel).
- **GREEN:** getter returns the sentinel untouched at rest; `selectedScale`/`selectedRootNote`
  apply the override delta to the resolved project value when the raw base is the sentinel. The
  seven consumers share the identical `_scaleGroup` BitField and `selectedScale(projectScale, …)`
  shape (NoteSequence, PhaseFluxSequence, TuesdaySequence `:321-322`, StochasticSequence `:29-30`),
  so one resolution helper serves all — but it lives at the `selected*` layer, not the getter.
  **Caveat:** `selectedRootNote` exists for Note/Stochastic/PhaseFlux but **Tuesday resolves
  rootNote by manual inline fallback** (`TuesdayTrackEngine.cpp:1658`, no helper) — the rootNote
  half needs a `TuesdaySequence::selectedRootNote` added or Tuesday's inline resolution updated.

### 3.2 Mute/Fill/FillAmount/Pattern routing — PORT the legacy capability (`RouteParamKey.h:40`; `RoutingEngine.cpp:283-286`)
**Owner-resolved: build it, don't drop it.** These are per-track PlayState targets that our
fork left as no-ops. The legacy PER|FORMER mechanism (`temp-ref/original-performer`) is the
port basis; our fork still retains the write machinery (`PlayState::writeRouted`
`PlayState.h:317` + mutators `muteTrack`/`fillTrack`/`setFillAmount`/`selectTrackPattern`
`PlayState.h:265-283`), so this is wiring, not new subsystems.

**Model (legacy semantics to match):** all four are per-track, **level-driven, no rising
edge**, re-evaluated each frame, idempotent via equality guards.
- Mute (range 0..1): `round(value) != 0` → mute/unmute (request flag, applied in `updatePlayState`).
- Fill (0..1): threshold 0.5 → flips the fill bit directly, immediately.
- FillAmount (0..100): denormalize → `setFillAmount`, direct.
- Pattern (0..15): `round(value·15)` = pattern index → `selectTrackPattern` (bails if a snapshot is active).

**Fit to our fork:** these are level/trigger consumers like Run/Reset (already migrated off
the legacy branch), NOT base+delta override params. Wire them the Run/Reset way — read the
raw routed source (`_sourceValues[routeIndex]`, consumed for Run/Reset three lines up at
`RoutingEngine.cpp:275,281`) at the per-track branch, threshold/scale per target, call the
PlayState mutator — not through the override table. `PlayState::writeRouted` is **live** in
our fork (`PlayState.cpp:254-288`), not dead — call it, don't "revive" it. (`setFillAmount`
is a `TrackState` method — `PlayState.h:36` — reached via `trackState(t).setFillAmount()`.)

- **Seam (before RED):** mint the reserved `ParamKey`s 22..25 (Mute/Fill/FillAmount/Pattern)
  in `RouteParamKey.h:40`; add 4 `targetToParamKey` cases (`RouteResolve.h:40`) — `paramKeyToTarget`
  inverts by scanning it (`RouteBrowse.h:91`), no edit needed. **UI placement RESOLVED (owner):
  fold the four into the existing Clock band** — it already carries Run/Reset, the same per-track
  level/trigger transport category, and Clock is entirely per-track. Append `Mute, Fill,
  FillAmount, Pattern` to `clock[]` and bump its count 4→8 (`RouteBrowse.h:22-29`); add
  `shortLabel` entries as needed ("Fill Amt", "Pattern"). No new band: `kBandCount` stays 3,
  tab ring/labels untouched. Row buffers are `keys[16]` and rows render in a scroll window
  (`RoutingPage.cpp:531`), so 8 rows scroll without truncation. Render the 8-row band through
  ui-preview before shipping (label-gutter fit). Behavior-neutral until wired.
- **RED** — `TestRoutedPlayState.cpp` (model/header-level where possible; the apply path uses the Phase 0 fixture):
  - `CASE("every PlayState target round-trips through a ParamKey")`: `targetToParamKey` then `paramKeyToTarget` is identity for Mute/Fill/FillAmount/Pattern (fails today — keys unminted, resolve to `None`).
  - `CASE("mute route at value 1 mutes the masked track")` (fixture): route→Mute on track 3, source drives ≥0.5 → after `updatePlayState`, track 3 muted; source <0.5 → unmuted. Fails today (no-op branch).
  - `CASE("fill route flips fill immediately")`, `CASE("fillAmount route sets 0..100")`, `CASE("pattern route selects the rounded index")`.
  - `CASE("pattern route inert while a snapshot is active")`: matches legacy `selectTrackPattern` guard.
  - `CASE("track mask scopes the effect")`: only masked tracks change.
- **GREEN:** replace the `isPlayStateTarget` no-op at `RoutingEngine.cpp:284-285` with a
  consumer that denormalizes the raw source per target and calls `PlayState::writeRouted`.
  Level-driven + equality-guarded, matching legacy — no edge detection. **Watch the mask:**
  `writeRouted(target, tracks, …)` iterates the whole track mask itself (`PlayState.cpp:257`),
  but the GREEN site is *inside* the per-track loop (`RoutingEngine.cpp:258`) — call it once
  outside the loop, or add a single-track variant, else it applies N² (the "mask scopes"
  CASE can pass while the apply runs redundantly).
- **REFACTOR:** confirm the request flags land in the same `Engine::updatePlayState` drain
  (`Engine.cpp:160,180`) the fixture exercises.

### 3.3 New MIDI route applies a stale source value (`RoutingEngine.cpp:190-194,208-215`)
- **Seam:** `RoutingEngine(Engine&, Model&)` and `resolveSourceValue` deref the Engine → fixture (or a narrow friend seam if the fixture stalls).
- **RED** — `TestRoutingMidiStaleSource.cpp`: populate a slot's source value via a CV route, remove it, create a MIDI route in the same slot → applied value neutral before any MIDI arrives (fails today — inherits the CV value).
- **GREEN:** clear `_sourceValues[slot]` (and gate/arm state) on route (re)assignment, not only on MIDI events.
- **REFACTOR:** one "slot reinitialized" routine covering source value, gate mask, routedSet bit.

### 3.4 MOD+ extend mutates the live route; CANCEL doesn't revert (`ListPage.cpp:396-411`; `RouteDraft.h:65-69`)
- **Seam first (behavior-neutral move, before the RED):** the extend/cancel staging logic moves out of `ListPage` into `RouteDraft` (header-only model — trivially testable). "A small test hook" on the page statics is not viable: no registered test instantiates any page, and `PageContext` carries `Engine&`.
- **RED** — `TestRouteDraftExtend.cpp`: committed route on track 2; begin extend onto track 5 (inlet target); cancel → track 5 depth 0, membership bit clear (fails against the current live-mutation logic once it lives in RouteDraft). `CASE("extend then commit persists")`.
- **GREEN:** stage the extend in the draft copy; the live route is untouched until commit.

---

## Phase 4 — TT2 execution safety

### 4.1 No shared execution budget on the iterating mods `L` / `W` / `P.MAP` / `PN.MAP` (`TT2Evaluator.h`)
Constructible today — evaluator is free functions over `TT2Runtime`+`TT2OutputState`
(`TestTeletypeV2Evaluator.cpp` idiom). Best-grounded item.

**Now four iterating constructs, not one (updated 2026-07-06):** since the core-mods port,
`W`, `P.MAP`, `PN.MAP` are implemented alongside `L`, and each runs its body synchronously in
one frame:
- `L x y` — up to ~65535 body evals (`int16_t` bounds).
- `W cond` — up to **10000** body evals (its own hardcoded cap, matching original TT).
- `P.MAP` / `PN.MAP` — up to `PatternLength` (64) body evals.
`L` bounds and `W`'s 10000 cap stay (program compatibility / original fidelity) — do not narrow
the language. Our TT2 also already has the SCRIPT-nesting guard (`TT2_EXEC_DEPTH`) and the metro
`guard < 64`.

**Why a shared budget:** original TT runs on a dedicated MCU where a blocking line just delays
the next line. Our TT2 shares the ~1 ms engine frame with 8 tracks + routing, so `L 1 32000` —
or `W`'s 10000, or a nest of any of these — starves audio on every track. `W`'s per-loop 10000
cap is far too coarse for the frame budget; it needs the shared op budget like the rest. The fix
is one per-script-invocation **op budget** covering all four iterating mods (and `DEL.X`'s
enqueue loop), independent of each construct's own bound.
- **Owner call — the op-budget value.** User-visible ceiling; needs a hardware per-op cost
  measurement against the frame budget. Ship the mechanism first with a conservative default,
  tune after measuring. (Not 1024-by-guess — measured.)
- **RED** — `TestTT2ExecutionBudget.cpp` (evaluator is a free `template<Cfg>` over `TT2Runtime`
  + `TT2OutputState`, `TestTeletypeV2Evaluator.cpp` idiom — no Engine):
  - `CASE("L honors the op budget")`, `CASE("W honors the op budget")` — a loop exceeding the budget returns an error result rather than running to completion (both fail today).
  - `CASE("P.MAP over a large window honors the budget")`.
  - `CASE("nested loops compound within one budget")`, `CASE("short loop unaffected")`.
  - `CASE("L bounds remain int16, W keeps its 10000 cap")`: bounds/caps unchanged — the op budget is the *additional* frame-safety ceiling.
- **GREEN:** a per-invocation op counter — ride it on `TT2Runtime` (already threaded to every
  `evaluateCommand` call → zero signature churn) rather than a new parameter; increment per body
  eval in the `L`/`W`/`P.MAP`/`PN.MAP` loops; mirror the metro `guard < 64` idiom
  (`TT2Runner.h:203-208`).
- **REFACTOR:** same op budget covers `DEL.X`'s enqueue loop (`:490-506`) and unifies with the
  4.2 recursion budget — one budget concept.

### 4.2 Combined recursion depth vs 4KB engine stack (`TT2TrackEngine.cpp:240-246`; `TeletypeNativeOps.cpp:2754`)
**Ordered after 5.1** — the guard lives inside the duplicated host block; fixing before the
dedup means hand-editing both engine files or creating the first real Full/Mini drift.
- **Seam:** new test file (fixture — cross-track triggering goes through `_engine.trackEngine(t)`; `TestTT2HostCrossTrack.cpp` covers only the pattern-cell helper and has nothing to extend).
- **RED** — `TestTT2RecursionBudget.cpp`: A→B→C→D→A cross-trigger cycle, each nested; assert combined live `runScript` depth stays under one unified cap (fails today — the 8-deep exec cap and 4-deep cross cap multiply).
- **GREEN:** one shared depth/op budget decremented by both same-track and cross-track re-entry, sized for the 4KB stack — in the deduped `TT2HostImpl` (see 5.1), so it lands once.

---

## Phase 5 — Refactors (characterization-first, with two inversions)

### 5.1 TT2 host dedup → `TT2HostImpl<Cfg>` mixin
Extraction-first (the mixin IS the testable seam; a pre-extraction parity test would need
the Phase 0 fixture for zero gain):
1. Extract the 31 shared `host*` bodies into `TT2HostImpl<Cfg>` templated on `TT2ConfigFull`/`TT2ConfigMini`; Full-only `runLiveCommand` stays out.
2. **Pin after extraction** — `TestTT2HostParity.cpp`: drive the mixin against a fake host context; assert Full and Mini instantiations produce identical results for a representative method sample (locks in the no-drift state the review confirmed).
3. Both engines derive; sim build + existing `TestTeletypeV2*` suite green = the behavior gate.

### 5.2 Voicing table dedup (`DiscreteMapSequencePage.cpp:97-142` / `IndexedSequenceEditPage.cpp:79-124`)
Move-first (the tables are file-static — unreadable by any test until the move):
1. Move `Voicing` + `kPianoVoicings`/`kGuitarVoicings` + counts to a shared header; both pages include it. `applyVoicing()` stays per-file (correctly divergent).
2. **Pin after move** — `TestVoicingTables.cpp`: assert values/lengths against the review-recorded state.

### 5.3 `TeletypeScriptViewPage` cleanups
- **deleteLine (bug):** loader behavior (`deleteScriptCommand` bounds/return) is already covered by `TestTeletypeV2ScriptEdit.cpp`. The page-level defect — unconditional "Line deleted" on a no-op (`TeletypeScriptViewPage.cpp:879-901`) — has no harness seam (no page instantiation). Fix is two lines (honour the bool, guard like `duplicateLine`); verification is loader tests green + manual sim check. Exception to the red-first gate, recorded here.
- **commitLine double-parse (optimization):** pin correctness via loader-level round-trip (commit stores the right command), then reuse the validation parse's `tele_command_t` in `setScriptCommand` instead of re-parsing.
- **`isMini()` triplication, `keyboard()` split, magic step indices, Home/End dup, inlined KeyDelete:** pure structure, page-level, no harness seam — behavior pinning is manual/sim. Extract the `activeProgram()` helper, split `keyboard()` into named sub-handlers, name the step-index constants.

---

## Phase 6 — Minors & wrap-safety (batch; seam extractions are the pattern)

- **ClockMult ratio drift (`NoteTrackEngine.cpp:167-173` + engines) — RESOLVED: per-track de-rounding (NOT shared reference — see correction below).** `clockMultiplier` is a **per-sequence** field (0.5×..1.5×, `NoteSequence.h:808`), itself per-track routable (`ParamKey::ClockMultiplier`), so tracks can hold *different* multipliers — there is no single shared multiplier to "apply once," and cross-track ratio preservation is neither possible nor intended when multipliers differ. The actual defect is **per-track integer rounding**: `divisor = max(1, lround(baseDivisor / clockMult))` rounds each track's tick period independently, so sub-tick precision is lost per track and intended step-timing ratios break under any fractional multiplier. One demonstration: two tracks at 1.5× with base divisors 1 and 2 become `round(4/1.5)=3` and `round(8/1.5)=5` ticks/step — 5:3 instead of 2:1. (Note: "same multiplier" is not the failure boundary — different multipliers can also round to colliding/diverging integer divisors; the rounding is the fault, not a multiplier match.) Fix: stop pre-rounding the divisor — compare the running tick against a rational boundary (`relativeTick * clockMultNum >= step * baseDivisor * clockMultDen`, integer math) so no precision is lost per track.
  - **RED** — `TestClockDivisorRatio.cpp` (pure arithmetic): `CASE("equal 1.5x mult keeps 1:2 base divisors at 2:1 step timing")` (fails today — 5:3), `CASE("integer mult unchanged")` (regression), `CASE("unequal multipliers are independent")` (documents the non-goal).
  - **GREEN:** a pure step-boundary predicate replacing the per-engine `lround`. **Blast radius (corrected):** six engines share the `lround` idiom (Note 3 sites, Curve, DiscreteMap, Fractal, Stochastic, Tuesday 2 sites); **PhaseFlux** uses a cumulative-tick layout (`computeCumulativeTicks`) and **Indexed** a float delta-scale — both need per-engine adaptation or are out of scope, and leaving them on the old path re-introduces the drift against the fixed engines. Scope the GREEN to the six idiom engines first; PhaseFlux/Indexed are a follow-up. Side-note (Fractal): its `clockMultiplier` getter reads `Routing::routedValueInt` but there is no `ClockMultiplier` row in `ParamTableFractal.cpp`, so it isn't actually route-modulatable today — orthogonal gap, flag but don't fix here.
- **`UpdateReducer` wrap (`UpdateReducer.h:10-17`):** `update()` reads `os::ticks()` internally — inject a time source first (seam-before-red, behavior-neutral), then `CASE` straddling the uint32 wrap; convert to the signed-difference `WallTimer` idiom.
- **Clock wrap + slaved pulse width:** extract the two compares (`Clock.cpp:334` sub-tick, `:438` pulse width) into pure static helpers (the `acceptSlavePeriod`/`TestSlaveClockGuard` pattern). Two separate defects, two cases: wrap-straddle for the sub-tick compare; **wrong-BPM** for pulse width (`_masterBpm` used while slaved — not a wrap bug; assert width derives from the effective slaved tempo).
- **`Clock::setMode` missing interrupt lock (`Clock.cpp:34-44`):** no practical unit test (ISR race); add the `os::InterruptLock` like every sibling method, note as reviewed-fix.
- **`Model::ConfigLock` no-op (`Model.h:19-26`) — RESOLVED: delete.** Empty no-op type, one unused RAII local (`ClipBoard.cpp:127`). It is simply dead — no lock was ever in effect at that site (not "superseded elsewhere"; whether `pasteTrack` *should* be locked is a separate pre-existing question, out of this pass). Remove the type and the dead local. No test (deleting a no-op); build + existing ClipBoard tests staying green is the gate.
- **Route conflict guard dead (`Routing.cpp:190-217`):** wire `checkRouteConflict` into the create path with a `CASE`, or delete it. Engineering call at implementation.
- **Cross-bus feedback (`Routing.h:556-559`):** if blocking is chosen, `CASE("bus1→bus2→bus1 detected")`; if kept as patchable feedback, document and skip.
- **`DEL.G` int32 overflow (`TT2Evaluator.h:560-572`):** header-only; `CASE("geometric interval saturates")` — clamp per iteration.

---

## Owner design calls — RESOLVED (2026-07-06)

1. **3.2 PlayState routing** → **build it** (port the legacy Mute/Fill/FillAmount/Pattern mechanism; §3.2). Not drop.
2. **2.2 harmony order** → **deferred** to the harmony-track implementation (§2.2). Not in this pass.
3. **3.1 Default-scale modulation** → **relative-to-resolved-project-scale** (§3.1).
4. **4.1 TT2 budget** → keep `L` bounds `int16_t` and `W`'s 10000 cap; add one shared per-invocation
   op budget covering all four iterating mods `L`/`W`/`P.MAP`/`PN.MAP` (+`DEL.X`). (`W` is now
   implemented — see the core-mods work — so it's in scope, not dropped.) **Numeric value owner-set
   after a hardware per-op measurement** — ship the mechanism, tune later (§4.1).
5. **6 ClockMult** → **per-track de-rounding** (corrected from "shared-reference" — the
   multiplier is per-track, so there is no shared reference; the bug is per-track integer
   rounding, fixed by rational-boundary comparison) (§Phase 6).
6. **6 ConfigLock** → **delete** (§Phase 6).

Remaining owner input: (a) the numeric op-budget value in 4.1 (needs a hardware measurement);
(b) the UI band placement for the four PlayState routing targets in 3.2. (2.1's Stop/Continue
gate-drop is a mild behavior change — flagged, not gating.)

**Post-review correction note (2026-07-06):** an adversarial + feasibility pass overturned two
premises still standing — the ClockMult "shared reference" (multiplier is per-track, not shared)
and the 3.1 getter seam (zero-arg getter has no project scale). A third — "4.1 `W` has no
execution branch, drop the W cap" — was true when written but is now **obsolete**: the core-mods
work (commit `301ef5a4`) implemented `W`, `P.MAP`, `PN.MAP`. See the ⚠ below.

## ⚠ Shipped-hazard: iterating mods run unbudgeted (2026-07-06)

The core-mods port added three synchronous in-frame loops — `W` (up to 10000 body evals),
`P.MAP`/`PN.MAP` (up to `PatternLength`) — with only each construct's own coarse cap, no shared
budget. On the ~1 ms shared engine frame a `W 1: …` or a nest starves audio on all 8 tracks. This
is a live regression introduced by that commit; **§4.1 is now the highest-priority TT2 item** —
it stops being a nice-to-have on `L` and becomes the mitigation for a hazard already on `dev`.

## Sequencing

1. **Phase 0 fixture** (smoke test green) — DONE — before anything engine-facing; 1.1, 3.1-at-rest, 3.2-mapping, 4.1, and the Phase 6 arithmetic items can proceed without it.
2. **4.1 op budget is elevated** — do it next, ahead of the other Phase 4/5 TT2 work: it fixes the unbudgeted-loop hazard the core-mods commit shipped. Needs only the evaluator (no fixture, no Engine).
3. Phases 1-4 bug-first; each defect-reproducing `CASE` fails on `dev` before its fix.
4. **5.1 before 4.2** (recursion budget lands once, in the mixin).
5. Seam moves precede their tests where listed (3.4 RouteDraft, 5.1/5.2 inversions, Phase 6 extractions) — behavior-neutral, pinned after.
6. `docs/performer-architecture.md` §3/§10 correction ships with 1.2.
