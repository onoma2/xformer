# review-fixes-tdd

## Goal
Land the adversarial-review fixes and refactors under a strict red-green-refactor
discipline: each defect gets a `CASE` that fails on current `dev` before its fix, engine
bugs first, cosmetics last. Full plan in `PLAN.md` (moved here from
`docs/plans/2026-07-02-review-fixes-tdd-plan.md`). Companion prose lives in
`0207refactoring.md` at the repo root.

## Key files
- `PLAN.md` — the phased TDD plan (harness facts, Phases 0-6, owner design calls, sequencing)
- `../../0207refactoring.md` — companion refactoring writeup the plan tracks
- `src/tests/unit/sequencer/CMakeLists.txt` — test registration; engine-instantiating tests currently disabled
- `src/apps/sequencer/engine/Engine.h` — 9-driver ctor that blocks naive engine tests (Phase 0 exists for this)

## Landed (TDD, rescue-verified)
- **Phase 0** — `EngineTestFixture` (`e...`, see decisions). Drive via `Simulator::wait()`; drains the boot Reset.
- **1.1** UserScale OOB — reject oversized size byte (`e2ba3472`). Codex GO.
- **4.1** TT2 op budget — shared per-line budget over L/W/P.MAP/PN.MAP + DEL.X (`98348609`, `724062d5`). 3 rescue gaps fixed.
- **1.2** routed-bus double-count — per-writer contribution slots; override/cross-frame/suspend edges (`84f5ea03`→`b4e21d32`→`7689f8ea`→`7ec4d37d`). 3 rescue rounds, final GO.
- **2.1** stuck gates on stop — call `stop()` on all engines; 8 sequencer-clock engines drop gates (Note/Stochastic/Fractal/PhaseFlux/Indexed/Tuesday/DiscreteMap/Curve) (`eaaddda0`→`c62f869c`→`9e16ac8b`). MidiCv/TT2/TT2Mini out of scope (transport-independent). Final GO.
- **2.2** harmony order — DEFERRED to harmony-track work (owner call).
- **2.3** pulse-hold reads the rotated audible step — pure `rotatedStepPulseCount` helper (`a6f3704a`). Codex GO.

## Phase 3 landed (TDD, rescue-verified)
- **3.1** scale/rootNote Default sentinel — getters preserve -1; ScaleResolve modulates around the resolved project value. Scale: all 7 types. RootNote: Note/Stochastic/PhaseFlux/Tuesday, then Indexed/Fractal added after rescue (`69445471`, `49942eb1`, `fdb5920a`). Rescue GO + gap-closed.
- **3.3** stale MIDI source — RouteState tracks source; clear `_sourceValues` when a slot switches to MIDI (`e3003137`).
- **3.4** MOD+ extend — `RouteDraft::extend` stages on the draft, live untouched until commit; cancel reverts (`661231d7`).
- **3.2** PlayState routing PORT — Mute/Fill/FillAmount/Pattern wired the Run/Reset way via `PlayState::writeRouted` (one call, full mask, level-driven); ParamKeys 22-25; folded into the Clock band (8 rows, ui-preview'd) (`208f208f`).

## Open follow-ups (flagged, not regressions)
- 2.3 fill-sequence: pulse-hold reads base `_sequence`, but `triggerStep` uses the fill sequence under `FillMode::NextPattern`. Needs a design call — the per-trigger fill roll doesn't exist yet at the pulse-hold point.
- 2.1 panic: whether transport-stop should hard-cut MidiCv / TT2 TR outputs (currently no — they're transport-independent). Owner call if a panic-stop is wanted.
- 3.3 MIDI-reconfig staleness: changing a CC/note on an EXISTING MIDI route (source stays Midi) doesn't clear the old value — routeChanged only tracks target/tracks/source, not midiSource config. Out of the plan's CV→MIDI scope; needs midiSource-config tracking in RouteState if wanted.
- 3.3 same-frame race: a matching MIDI message arriving in the exact transition frame (receiveMidi runs before routing update) is cleared for one frame, then self-corrects. Non-regression (pre-fix held stale CV forever). A per-slot midi-dirty flag would close it.
- 3.2 FillAmount writes each frame (setFillAmount is a bare clamp-assign, not equality-guarded like Mute/Fill/Pattern) — idempotent, no thrash, just not guarded.
- Remaining plan items unstarted: 3.1, 3.2, 3.3, 3.4, 4.2, Phase 5 refactors, Phase 6 minors (ClockMult per-track de-rounding, ConfigLock delete, etc.).

## Decisions log
- 2026-07-07: **3.2 UI placement resolved — fold the four PlayState targets into the Clock band.** It already holds Run/Reset (same per-track level/trigger transport category) and is entirely per-track. Append Mute/Fill/FillAmount/Pattern to `clock[]`, count 4→8; no new band (`kBandCount` stays 3, tab ring untouched). Buffers are `keys[16]` and rows scroll (`RoutingPage.cpp:531`), so 8 rows fit. The plan's "add a 4th band" hazard is dropped. ui-preview the 8-row band before shipping.
- 2026-07-06: **Phases 1-2 fixes landed TDD + codex-rescued.** 1.1 UserScale OOB guard. 4.1 op budget (rescue found 3 gaps: live-eval reset, DEL loops, S.ALL propagation — all fixed). 1.2 routed-bus double-count via per-writer slots — 3 rescue rounds surfaced the cv-router-override stale-hold, the first-compose cross-frame stale read, and the suspend hold; all fixed, final GO. 2.1 stuck-gates-on-stop: `Clock::Stop` now calls `stop()` on every engine; rescue found Tuesday/DiscreteMap then Curve missing, and Tuesday's retrigger latches incomplete — all fixed; MidiCv/TT2/TT2Mini scoped out (transport-independent), owner-accepted, final GO. 2.3 pulse-hold: reverted a flaky gate-timing fixture test, re-did it as a pure `rotatedStepPulseCount` helper unit test — clean GO. The codex rescue loop earned its keep: nearly every fix had a real edge case the first pass missed.
- 2026-07-06: **Review gate (adversarial + feasibility) corrected three premises before handoff.** ClockMult: "shared-reference" is wrong — `clockMultiplier` is per-sequence/per-track and per-track routable, so no shared reference exists; the real bug is per-track integer rounding (`lround(divisor/clockMult)`), fixed by rational-boundary comparison. Blast radius is 6 idiom engines, not 7 — PhaseFlux (cumulative-tick) and Indexed (float delta-scale) differ. 4.1: `W` is a parsed token with no execution branch — nothing to cap; dropped. 3.1: the zero-arg `scale()` getter has no `Project&`, so resolution moves to `selectedScale(projectScale, …)`, not the getter. 3.2 holds (writeRouted is live; call once outside the per-track loop or it applies N²; no PlayState UI band exists yet). ConfigLock delete safe (dead no-op, no lock was ever there).
- 2026-07-06: **Six owner calls resolved + plan rewritten TDD-style.** 3.2 build the PlayState routing (legacy `temp-ref/original-performer` mechanism: per-track, level-driven, no edge; our fork retains `PlayState::writeRouted` + mutators, reserved keys 22-25 — wire like Run/Reset, not the override table). 3.1 relative-to-resolved. ClockMult shared-reference (per-track rounding breaks 1:2 → 3:5 at 1.5×). ConfigLock delete. 2.2 deferred to harmony-track. 4.1: original TT `L` is int16, uncapped, synchronous; original guards are `W`≤10000 + `SCRIPT`≤8 (we have the SCRIPT analog `TT2_EXEC_DEPTH`); keep `L` bounds, add a shared op budget for our shared-thread frame — numeric value pending a hardware measurement.
- 2026-07-06: **Phase 0 DONE.** `EngineTestFixture.h` + `TestEngineFixture.cpp` landed green, no production changes. TDD: smoke test asserts `engine.tick()` advances under `Simulator::wait()`. Root-caused the historical "clock injection" failure — not clock injection at all: the Clock boots with a pending Reset (`_requestedEvents = Reset`) that clobbers a Start queued in the same frame; the fixture drains it with one `update()` in the ctor. Real drive seam is `Simulator::wait()` (public), not `slaveTick()`. 3 unrelated tests fail on HEAD (TestModel, TestPhaseFluxTrackEngine, TestLinkingRemovalSerialization) — pre-existing, production diff empty.
- 2026-07-06: Codex readiness review (review-only) → **GO** on Phase 0. Corrected the plan: `slaveTick()` only queues subticks — ticks advance in `Clock::onClockTimerTick()`, fired only from `Simulator::step()`, so the fixture must drive `Simulator::step()` after clock events (the likely cause of the prior two failures). Also: construct `sim::Simulator` before any driver; real path is `src/platform/sim/...`; disabled-test CMakeLists lines are 110/119/163.
- 2026-07-06: Moved the plan out of `docs/plans/` into this task folder; tracked in the wiki as ⚪ Ready (defined, Phase 0 not started).
- 2026-07-02: Plan revised per adversarial-document + feasibility review; harness facts are build-verified, not file-read (commits `feec5b9b` → `fd645529`).

## Owner design calls — RESOLVED 2026-07-06
- [x] 3.2 — **build** the Mute/Fill/FillAmount/Pattern routing (port legacy PER|FORMER mechanism); not drop
- [x] 2.2 — **deferred** to the harmony-track implementation
- [x] 3.1 — **relative-to-resolved-project-scale**
- [x] 4.1 — keep `L` bounds int16 + `W`'s 10000 cap; add ONE shared op budget over all four iterating mods `L`/`W`/`P.MAP`/`PN.MAP` (+`DEL.X`). W is now implemented (core-mods work) so it's in scope, not dropped. Numeric budget value pending a hardware per-op measurement (mechanism-first)
- [x] 6 ClockMult — **per-track de-rounding** (corrected from shared-reference: multiplier is per-track, no shared reference exists; bug is per-track integer rounding)
- [x] 6 ConfigLock — **delete**
- [ ] 2.1 Stop/Continue gate-drop — mild behavior change, flagged not gating (still an implementation-time note)

## Completed steps
- [x] Plan written, adversarially reviewed, revised
- [x] Moved into `.tasks/review-fixes-tdd/` and entered on the board

## Notes
Nothing implemented yet. Phase 0 fixture is the gate for anything engine-facing; 1.1,
3.1-at-rest, 3.2-mapping, 4.1, and the Phase-6 arithmetic items can proceed without it.
