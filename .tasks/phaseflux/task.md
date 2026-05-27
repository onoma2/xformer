# phaseflux

## Goal

Implement PhaseFlux as a new Performer `TrackMode::PhaseFlux` — a 4×4 grid sequencer with stateless-ramp engine (DiscreteMap pattern), scale-degree pitch with Bell+vflip-style curve flips (NoteTrack chassis), per-stage MaskMelody/TiltMelody centrality filter (Stochastic borrow), and per-stage transforms (Flux PHAS + MASK borrows). Single CV + single Gate output; multi-jack routing mirrors. Engine designed around `_resetTickOffset` + integer `relativeTick` math; no heap, no RNG, all per-stage params bit-packed into 3 × uint32_t (93 bits, 3 spare).

Full design locked in `docs/phaseflux-spec.md` — single source of truth. Lessons from the design process extracted to `docs/spec-template.md`.

## Key files

**Spec & reference**
- `docs/phaseflux-spec.md` — locked MVP spec (18 sections, all 66 audit questions resolved)
- `docs/spec-template.md` — extracted process + architecture lessons for next track spec
- `ui-preview/pages_phaseflux.py` — rendered UI proposal (4×4 grid + dual scopes + 8-box edit-hold strip)
- `ui-preview/phaseflux-preview.png` — side-by-side default + edit-hold render

**Borrowed patterns (cite when implementing)**
- `src/apps/sequencer/engine/DiscreteMapTrackEngine.cpp:128-198` — stateless-ramp + `_resetTickOffset` pattern (spec §3)
- `src/apps/sequencer/engine/NoteTrackEngine.cpp:69-101` — `evalStepNote` pitch chassis + `Scale::noteToVolts` epilogue (spec §6.2)
- `src/apps/sequencer/engine/NoteTrackEngine.cpp:104, 144` — `reset()` vs `restart()` two-tier transport pattern (spec §11.1)
- `src/apps/sequencer/engine/StochasticTrackEngine.cpp:454-466` — MaskMelody/TiltMelody centrality + tilt math (spec §6.2.1)
- `src/apps/sequencer/engine/StochasticGenerator.cpp:261-311` — `evaluateBurst` clamp law (spec §6.4 pulse collision)
- `src/apps/sequencer/model/StochasticTrack.h:185-195` — Track-level chassis to borrow (spec §12.1)
- `src/apps/sequencer/model/NoteSequence.h:349-374` — BitField packing pattern (spec §16)
- `src/apps/sequencer/model/Bitfield.h` — `UnsignedValue<N>` / `SignedValue<N>` / `BitField<T, Index, Bits>` template
- `src/apps/sequencer/model/Curve.cpp:18-108` — LUT shapes (`RampUp`, `Bell`, `Triangle` reused; `Bounce` LUT TBD at impl time)

**Files created (Phase A — model layer)**
- `src/apps/sequencer/model/PhaseFluxTrack.h/.cpp` — track chassis (StochasticTrack borrow), serializes everything except `_lock`
- `src/apps/sequencer/model/PhaseFluxSequence.h/.cpp` — sequence chassis + 16-stage array; `Stage` is 3 × `uint32_t` bit-packed (24 fields, 93 bits, 3 spare in `_data2[29..31]`)
- `src/tests/unit/sequencer/TestPhaseFluxSequenceSerialization.cpp` — 7 round-trip cases (default + chassis + every stage field + min/max edges + bit-pack non-overlap + lock-not-persisted + Track-level mode round-trip)

**Files still to create (Phase B / C)**
- `src/apps/sequencer/engine/PhaseFluxTrackEngine.h/.cpp`
- `src/apps/sequencer/ui/pages/PhaseFluxEditPage.h/.cpp`
- `src/apps/sequencer/ui/pages/PhaseFluxSequencePage.h/.cpp`
- `src/apps/sequencer/ui/model/PhaseFluxSequenceListModel.h`
- `src/apps/sequencer/ui/model/PhaseFluxTrackListModel.h`

**Files modified (Phase A — Track infrastructure wiring)**
- `src/apps/sequencer/model/Track.h` — `TrackMode::PhaseFlux` (serialize ID 8), `phaseFluxTrack()` accessor pair, Container template parameter, union member, `setTrackIndex` + `initContainer` switch cases
- `src/apps/sequencer/model/Track.cpp` — `clearPattern` / `copyPattern` / `gateOutputName` / `cvOutputName` / `write` / `read` switch cases
- `src/apps/sequencer/model/ClipBoard.h/.cpp` — Pattern union member + copy/paste switch cases
- `src/apps/sequencer/model/Routing.cpp` — `writeTarget` PhaseFlux dispatch (track + sequence Routables via generic `isTrackTarget` / `isSequenceTarget` checks)
- `src/apps/sequencer/model/ProjectVersion.h` — `Version_PhaseFlux_Pending = 36` placeholder constant outside the enum chain (Latest stays at Version35, no per-save version churn)
- `src/apps/sequencer/ui/model/TrackModeListModel.h` — temporary gate skipping PhaseFlux in the forward direction (removed when engine + UI land)
- `src/apps/sequencer/CMakeLists.txt` + `src/tests/unit/sequencer/CMakeLists.txt` — register new sources + test target

**Files to modify in Phase B / C (still pending)**
- `src/apps/sequencer/engine/Engine.cpp:527-554` — add `case TrackMode::PhaseFlux:` that instantiates `PhaseFluxTrackEngine`
- `src/apps/sequencer/ui/pages/TopPage.cpp:97, 296-348, 354-380` — PhaseFlux dispatch cases
- `src/apps/sequencer/ui/pages/TrackPage.cpp:152, 188-190` — PhaseFlux case in `setTrack()` + remove the `Last` assert hole
- `src/apps/sequencer/ui/pages/Pages.h:63` — register `phaseFluxEdit` + `phaseFluxSequence` page members
- `src/apps/sequencer/ui/model/TrackModeListModel.h` — drop the temporary skip once PhaseFlux is selectable
- `src/apps/sequencer/model/Project.h` — add `selectedPhaseFluxSequence()` accessor (first UI consumer triggers this)

## Decisions log

Reverse-chronological — append-only.

- 2026-05-27: **TEMP scope pulse marks redesigned — 3×3 ring per base pulse, 3×2 U per accumulator-added extra, dim → bright when playhead crosses, vertically centred + engine-aligned `i/N` spacing.** Was: 1 px × 3 px vertical stems at the scope baseline using `i/(N-1)` spacing — produced last pulse exactly on the slot boundary (visually clipped + diverged from the engine's §6.1 `i/N` formula by one pulse-interval). Now: each pulse mark is a 3×3 hollow ring (`canvas.drawRect`) centred at `ScopeY + ScopeH/2 = y=31`, 2 px inset from each border so a 3-wide ring stays 1 px clear of the outline at both extremes. Spacing uses `t_raw = i/N` matching the engine — last pulse lands at `(N-1)/N`, the gap to the next pulse falls in the next stage's first ring (the "empty space" on the right of a single-stage scope is the spacing slot the next stage fills, so multi-stage playback reads as straight subdivisions). Dim/bright logic: `Color::Medium` by default; the ring flips to `Color::Bright` once `te->sequenceProgress() >= t_shifted` in the active cell (rings light up left-to-right as the playhead crosses each one, reset each stage cycle). Muted pulses collapse to a single `Color::Low` centre dot at the same Y. **Accumulator-extras carried into the scope**: loop now iterates `effective = clamp(stage.pulseCount() + te->pulseAccumCounter(idx), 1, 8)` (matches engine), the first `min(base, effective)` pulses render as rings, the remaining `max(0, effective - base)` render as a 3 wide × 2 tall U-shape (open top, closed bottom — `point + point + hline`) sharing the dim/bright + muted behaviour. When `effective < base`, the loop's upper bound is `effective`, so the "removed" base pulses simply aren't drawn — hidden, not phantomed. Spec §17.2 bullet for the temporal scope rewritten end-to-end with the new glyph vocabulary + dim/bright rules + decreased-pulse hide rule.
- 2026-05-27: **ACCUM hero page redesigned — 1×16 row + per-cell bipolar `Ac.St` pancakes + paged footer with F5=Next + limits moved to list page.** Layout: dropped the 4×4 grid + dual-strip middle pane + 5-row right-pane param list for the ACCUM topic. Replaced with a 1×16 cell row centred at y=28..36 (cells 9 px, gap 6 px, row x_start=17), per-cell bipolar `Ac.St` pancakes on the right half of each cell column (1 px lit + 1 px dark gap, positive grows up from square top, negative grows down from square bottom, magnitude UI-clamped to 7 for both N and P), per-cell S/P trig chip on the left half below each square (tiny5x5 single char), and three hand-drawn 5×5 glyphs in the left margin at x=4 (Order: Wrap/Pend/Hold/RTZ; Polar: Uni/Bi; Reset uses tiny5x5 font `M`/digit/`+`). Footer paged: page 0 = `Ac.St / — / — / Order / Next`, page 1 = `Reset / Polar / Trig / — / Next`; F5 cycles `_accumPage` 0↔1. `Amount` renamed to **`Ac.St`** throughout the UI. `+Lim` / `-Lim` editing moved off the hero page entirely — defaults to max span (note 28/28, pulse 8/8 — set explicitly in `PhaseFluxSequence::clear()`) so the accumulator is unbounded by default; limit edits will land on the sequence list page later. Scope binding removed from the UI; `AccumulatorConfig::Scope` field stays in the data model (header still renders `ACCUM.NT` / `ACCUM.PT` if Track is set by some other path, but no UI reaches it). Shift bindings dropped entirely for ACCUM (Trig / Polar / Reset are first-class on the two pages now). Track 8 default-project demo simplified: 4 active stages (0..3), 12 skipped (4..15), all `pulseCount=1` + `pitchCurve=Ramp`, no per-stage temporal/pitch curve variation, no accumulator demo overrides — pure testbed for the redesigned ACCUM page. Pancake redundancy lesson captured as memory rule `feedback_data_scope_vs_visual_repetition.md`: never paint N identical copies of a sequence-wide value just because the layout was described per-cell. Spec `§13.5` rewritten end-to-end. Render reference: `ui-preview/pages_phaseflux_accum_redesign.py` → `ui-preview/accum-n/accum-n-row.png`. **Not done this round (deferred):** wiring `+Lim` / `-Lim` editing on `PhaseFluxListPage`; UI cleanup of the unreachable `ACCUM.NT` / `ACCUM.PT` header variants; the `Sleep` slot (`§14.2` drafted) on page-1 F4.
- 2026-05-27: **Accumulator v2 wired end-to-end + 5 post-audit bug fixes + Repeat/Window drafted in spec.** All 10 implementation tasks from `docs/accumulator-v2-spec.md §13.10` landed via subagent-driven-development workflow (Tasks 1-10 each with implementer + spec-compliance review + code-quality review, ~30 subagent dispatches total). Post-audition bug fixes: (1) `changePattern()` was zeroing counters every measure — moved counter-clear into the lazy seq-pointer change branch in `tick()` so only actual pattern switches clear; (2) pulse accum defaults 4/4 → 8/8 + cycle-wrap branch resets `_pulseFired` for the 1-active-stage silence case; (3) Pendulum first-move direction now respects signed step; (4) **three critical math bugs surfaced by adversarial review** — double-multiplication on application (`counter × step` where counter already held accumulated value gave 2× drift + sign inversion), Pendulum stuck at boundary with negative step (hardcoded ±1 dir-set vs flip-dir), Wrap runaway with step > range (single-subtract vs proper positive-remainder modulo); all fixed via `AccumulatorOps` updates + dropping the multiplication in `rebuildSchedule` and the Always-mode CV branch. Size measured: Stage=16 B (locked), Sequence=280 B, Track=4772 B (vs 9544 ceiling), Engine=480 B (vs TeletypeTrackEngine 944 real container limit). Engine static_assert tightened 1024→900 for early-warn. **Repeat + Window drafted** in spec §6.1.2/§6.1.3/§6.2.4/§6.2.5 + §14.2: per-cell `temporalRepeat`/`pitchRepeat` (1..8) for multi-cycle curve traversal; per-cell `temporalWindowStart`/`End`/`pitchWindowStart`/`End` (3-bit each) for input-axis cropping. Both ride along inside existing Stage 16 B — 18 new bits packed into `_data3`'s spare, 7 spare remain. Pipeline order: Repeat → Window → Warp → Curve eval → FlipV → Response → FlipH. UI uses Shift+F3/F4/F5 in TEMP and PTCH-Cell sets. Hardware audition is the gate before considering accumulator v2 complete; Repeat/Window implementation can follow once audition passes.
- 2026-05-26: **Accumulator v2 spec locked — dual accumulator (Note + Pulse) hero UI + shared AccumulatorConfig/Ops engine.** docs/accumulator-v2-spec.md §13 now contains the full implementation spec for PhaseFlux: dual accumulators (Note targets degree offset, Pulse targets pulseCount), shared `AccumulatorConfig` (per-sequence: scope/order/polarity/reset/posLim/negLim) + `AccumulatorOps::applyOrder` engine (stateless, 8 Order×Polarity cells documented in pseudo-code). UI extends `PhaseFluxEditPage` carousel with `ACCUM.N` / `ACCUM.P` sets — dual-strip visualization in middle pane (top=Note, bottom=Pulse, active bright/inactive dim, no midlines, 4-cell grouped layout with Color.Low dotted gridlines, stage badge top-left). Slot map: F1=Amount, F2=+Lim, F3=-Lim, F4=Order, F5=Reset (integer 0..15: 0=manual, N>0=auto every N cycles). Shift variants: Shift+F1=Trig, Shift+F4=Polar, Shift+F5=Scope. Bit-pack: removes `accumulatorLength`, reclaims 4 bits; adds 6 new per-cell bits (pulseAccumStep, accumulatorTrigger, pulseAccumTrigger). No Stage record size growth. Per-sequence: ~6 bytes for both AccumulatorConfigs. Universal "counter decoupled from output" rule resolves all open questions (pitchMode flip preserves counters; skip prevents trigger events; clamp doesn't feed back to counter). Random drunken-walk algorithm spec'd but deferred. NoteTrack migration to shared engine deferred. Render reference: `ui-preview/phaseflux-accum/`. Spec ready for subagent-driven-development dispatch — §13.10 has 10 numbered implementation tasks with dependencies + deliverables, §13.11 has concrete unit-test names per task (TDD-ready).
- 2026-05-26: **PitchMode (Cell/Global) + pitchRate landed — sequence-owned pitch source switch.** New `PhaseFluxSequence::PitchMode { Cell, Global }` enum (1 byte) + `_pitchRate` index into a 17-entry P:T ratio table (1:1 default). Cell mode = today's behavior (per-stage pitch curve/warp/response read from active stage, sampled at `_stagePhase`). Global mode = `stage[0]`'s pitch fields ARE the globals (no new per-stage storage); engine reads `stage[0]`'s curve at a free-running `_pitchPhase` accumulator that advances every tick at `rate × tempo` regardless of mode (mode flips are seamless). basePitch / pitchRange / accumulator stay per-cell in both modes. Pulse-fire CV in Global mode projects pitchPhase forward to each pulse's `triggerTime`. Always-mode (continuous CV) branches the same way. Roundtrip lossless — stages 1..15 keep their pitch fields when in Global mode, restored on flip back to Cell. **First wire put pitchMode on PhaseFluxTrack, then re-scoped to PhaseFluxSequence so different sequences on the same track can run different modes; track no longer carries the byte.** Hero edit page PTCH set: header `PTCH` / `PTCH.G`, stage badge `n` / `G`, footer Curve/Warp/Resp/Rate/Note in Global; F1-F3 edit stage[0], F4 edits sequence pitchRate, F5 shows active stage's note name (NoteSequenceEditPage scale.noteName Short1 form) with range dots beneath. Shift+F5 in Global cycles active stage's pitchRange (range demoted from F5 encoder edit); F5 panel label swaps `NOTE` → `SPAN` while Shift is held. Footer `Rng` renamed to `Span` everywhere. PitchMode + PitchRate rows added to PhaseFluxSequenceListModel (between GlobalPhase and Scale). Serialization roundtrip test extended to cover both fields on the sequence. Spec docs updated: new §6.2.3 (pitch source — Cell vs Global), §12.2 sequence-borrow list, §17.2.1 (PTCH set Cell vs Global visuals), §17.3 (sequence list rows). UI-preview variants at `ui-preview/phaseflux-pitchmode/` (cell / global / global-shift). No ProjectVersion bump (HARD RULE).
- 2026-05-26: **Continuous pitch CV (cvUpdateMode = Always) + mute decoupled.** §6.2.2 added: pitch CV evaluation timing now controlled by `cvUpdateMode`. `Gate` (default, preserved) = sample CV at pulse-fire ticks, hold between pulses (current point-sample). `Always` = evaluate pitch pipeline every engine tick from `_stagePhase` → continuous degree-quantized envelope across each stage; pulses still fire gates from the schedule independently. Result is "continuous-timing, discrete-value" — output stairsteps between scale notes as the curve crosses degree boundaries (Q12 added for hysteresis decision). §9 mute behavior updated: was force-CV-to-0V on mute+Gate (`PhaseFluxTrackEngine.h:41`), now decoupled — `cvOutput()` returns `_cvOutput` unconditionally, mute handled at the CvUpdate-emit layer (DAC holds previous voltage, matches NoteTrack precedent). Bit-pack: zero new fields — the existing `_cvUpdateMode` byte was repurposed from mute-coupling to timing-mode. Tests green.
- 2026-05-26: **Pulse-position formula changed to `i/N` + collision clamp removed + pitch Bounce curve added + Page+S15 Random + playhead LEDs.** Position formula: previously `t_raw = (N==1) ? 0.5 : i/(N-1)` — single-pulse special-cased to midpoint, multi-pulse spanned 0..1 inclusive (caused Bell/Triangle single-pulse to hit `fmod(1.0)→0` and get clamped silent). Now `t_raw = i/N` uniformly — first pulse at slot start (0), last at `(N-1)/N`, never on slot boundary. Single-pulse is the natural N=1 case. §6.4 collision clamp removed: NoteTrack precedent (no per-pulse gap guard exists on any other Performer track). Trigger overlap is allowed; runtime `_gateState=true` retrigger merges gates. gateLength rules retained: 0 → drop, 1 → floor at 6, ≥2 → percent (drop if computed < 6). Pitch curve enum extended Ramp/Bell/Triangle → also Bounce (= `Curve::ExpDown3x`, 3 cascading exp drops), filling the spare 2-bit slot. Page+S15 quick-edit on EditPage = set-aware randomize (TEMP randomizes curve/warp/resp/pulseCount, PTCH randomizes curve/warp/resp/pitchRange; stageLen + basePitch deliberately preserved). Step matrix LED port from NoteSequenceEditPage: red = playhead OR selected cell, green = stage will fire (not skipped AND stageLen > 0). Empty quick-edit overlay placeholder dims bottom 8 LEDs when Page is held.
- 2026-05-26: **Per-stage `stageLen` continuous length multiplier + per-sequence `globalPhase` + gateLength rule set landed.** `stageLen` final shape after spec iteration: `UnsignedValue<7>` storing 0..127 with default 64 = ×1 transparent (Phaseque STEP_LEN pattern — `ZZC/src/Phaseque/Phaseque.hpp:393-394`). 0 = silent stage, 127 ≈ ×2 stretch. Math: `effectiveTicks = enumTicks × stageLen / 64`. Stored in a new `_data3` word on `Stage` (`PhaseFluxSequence::Stage` is now 4 × uint32_t — 16 bytes — and `PhaseFluxTrack` grew 3480 → 4568 B, well under the 9544 envelope). Display in TEMP set F4 as `"1.00x"`. **`globalPhase`** (per-sequence, CurveTrack precedent — `model/CurveTrack.h:201`) wired as plain `float [0,1]` on `PhaseFluxSequence`, applied in engine `tick()` as `relativeTick = (relativeTick + uint32_t(phase × cycleTicks)) % cycleTicks`. Exposed via PhaseFluxSequenceListModel as "Global Phase" row. **gateLength engine rules (no UI exposure for now)**: `len == 0` → drop (explicit silence), `len == 1` → floor at `kMinPulseGateTicks` (= 6) sentinel for always-audible minimum, `len >= 2` → scale by percent, drop if computed < 6. **temporal flipH** re-spec'd at §6.1.1 as a post-clamp time-axis mirror (input-domain mirror was a no-op due to §6.4 sort). Shift+Sx (Shift + step button) toggles skip on the pressed stage; Skip removed from TEMP set F5 shift slot. **Default project** track-8 demo no longer needs `setStageLen` override — transparent default handles it. **Tests**: every `computeCumulativeTicks` call carries the `stageLenArr` now; `clearLens` helper sets all 16 stages to 64 (transparent); dedicated `stage_len_scales_per_stage` case locks stored 32 → 0.5×, 96 → 1.5×, 0 → silent. Bit-pack budget headline: with `_data3` extension PhaseFluxTrack reached 4568 B — 4976 B of headroom remain, so future per-stage fields fit without repack.
- 2026-05-26: **Hero edit page landed (Phase C UI seed).** PhaseFluxEditPage now hosts two F1..F5 parameter sets — TEMP (Curve, Warp, Resp, Gate, Puls) and PTCH (Curve, Warp, Resp, Base, Rng) — with Left/Right paging between them and Shift+F toggles for the binary slots (FlipV/FlipH/Skip). Layout: 4×4 grid (left) + single waveform scope (middle, swaps between temporal/pitch based on active set) + 5-row param list (right). Stage badge ("n", Tiny font, no rect) rendered top-left inside the scope. Value-only highlight box (right ~1/3 of row) marks the active F-slot; label column stays static. Footer labels swap on Shift held. Page-modifier passthrough fixed (early `if (key.pageModifier()) return;`) and keyDown/keyUp no longer blindly consume — Page+S0/S1/S2 navigation now works. Spec sketches captured in §14.2 of `docs/phaseflux-spec.md` (stageLen / sequenceShift / firstStage-lastStage proposals) and §14.1 carries the open Q8..Q11 (curve squash both axes, cycleSpread, global phase warp, snap-to-grid utility).
- 2026-05-26: **Sequence divisor wired into engine timing.** Pre-fix `PhaseFluxSequence::divisor()` was a dead field — the cumulative-table math ignored it. Now `PhaseFluxMath::computeCumulativeTicks()` takes a `sequenceDivisor` parameter and scales each stage's tick budget by `divisor / kReferenceSequenceDivisor` (= 12, the 1/16 reference at PPQN-48). Default divisor=12 reproduces pre-fix behaviour; divisor=24 doubles the cycle, divisor=6 halves it. Test family extended with `sequence_divisor_scales_cycle_uniformly` locking the contract (per-stage delta proportional to divisor). Per-stage `stageDivisor` enum is now a relative ratio against the sequence base. Default simulator project gets a single PhaseFlux track on track 8 with each of the 16 stages showcasing a distinct curve/warp/response combination on either the temporal or pitch axis; sequence divisor = 96 (1/2 note) so `pulseCount=4` ratchets are audible. Track 7 set to empty NoteTrack as a quiet companion.
- 2026-05-25: **Fix: changePattern() was nuking engine state every measure.** `Engine::updatePlayState()` calls `trackEngine->changePattern()` on every track when `handleSongAdvance` fires (`_tick % measureDivisor == 0`) — that condition is true every measure regardless of song mode. NoteTrack's `changePattern()` is a light pointer rebind, but PhaseFlux's was `reset()`, which re-anchored `_resetTickOffset` and re-set `_firstTickAfterReset = true` at every measure boundary, killing the held cycle. Fixed by limiting `changePattern()` to clearing accumulator counters per §7.1; sequence pointer rebind is already handled lazily in `tick()` via the `seq != _sequence` check.
- 2026-05-25: 66-question external audit closed in single 1-by-1 pass. Critical math errors (§6.2 direction × curve table) and missing rules (§3.4 cumulative-recompute side effects) fixed. Remaining contradictions (§7 vs §11 accumulator-state) resolved via §7.1 asymmetry-by-design rationale. Spec now internally consistent.
- 2026-05-25: Adopted refined curve model — base shape (Linear/Bell/Bounce temporal, Ramp/Bell/Triangle pitch) + vflip + hflip bits. Exp/Log absorbed by `temporalWarp`. Mirror = Bell+vflip. Stage record 90 → 93 bits, still fits 3 × uint32_t.
- 2026-05-25: Pitch chassis rewritten scale-degree based (NoteTrack-aligned). `basePitch` = `SignedValue<7>` scale degrees. `pitchRange` = {0.5, 1, 2, 3} octaves of active scale. `pitchDirection` = Up/Down/Bipolar-Centered. `accumulatorStep` moved from ±1V to ±15 scale degrees. Output via `Scale::noteToVolts()`.
- 2026-05-25: Per-stage MaskMelody + TiltMelody added (Stochastic centrality-filter borrow, byte-for-byte math match). Per-cell bitmask alternative rejected. Caveat: function is 12-EDO-tuned; documented as known limitation in §6.2.1.
- 2026-05-25: Pulse-collision clamp law from Stochastic `evaluateBurst` — drops PowerBend collisions, enforces `kMinPulseGateTicks = 6` audible floor + 1-tick retrigger gap. `pulseCount` becomes max-cap, not strict.
- 2026-05-25: Transport behavior locked — NoteTrack two-tier (`reset()` full / `restart()` light). Stop preserves all state, Start full-resets, Continue resumes mid-tick.
- 2026-05-25: Engine = stateless-ramp DiscreteMap pattern. Cursor + intra-stage phase derived per tick from `relativeTick`. `cumulativeTicks[17]` table recomputed on stageDivisor/skip edit. Reset Measure preserves accumulator counters; pattern switch clears them (§7.1).
- 2026-05-25: PowerBend ±1 degeneracy guarded by encoding-side clamp — `SignedValue<7>` mapped via `/64` (not `/63`), max-knob lands at ±0.984.
- 2026-05-25: Per-cell Routables deferred to v2. All per-stage fields plain bitfields in MVP.
- 2026-05-25: Serialization version policy: dev iterates at `Version35` with placeholder constant; bump to `Version36` on ship.
- 2026-05-25: UI surfaces locked — three files modelled on NoteTrack pattern. EditPage rendered + verified in `ui-preview/phaseflux-preview.png`.
- 2026-05-25: Locked grid traversal as snake permutation (boustrophedon). All-cells-skipped → §3.5 idle state. `kMinCycleTicks = 1/32 × measureDivisor` floor with proportional scaling.
- 2026-05-25: **Phase A landed.** Stage bit-pack layout: `_data0` = basePitch / pitchRange / pitchDirection / pitchCurve / pitchFlipV/H / pitchWarp / pitchResponse / pulseCount (32 used); `_data1` = temporalCurve / temporalFlipV/H / temporalWarp / temporalResponse / maskMelody / tiltMelody (32 used); `_data2` = phaseShift / mask / maskShift / accumulatorStep / accumulatorLength / gateLength / stageDivisor / skip (29 used, 3 spare bits 29..31). `stageDivisor` enum mapping: `{Div1_32, Div1_16T, Div1_16, Div1_8T, Div1_8, Div1_4T, Div1_4, Div1_2}` (3 bits, 8 slots, default `Div1_16`). `_lock` field present on PhaseFluxTrack but intentionally not serialized (Stochastic parity). `_edited` declared as transient UI dirty bit only — not serialized (NoteSequence parity).
- 2026-05-25: PhaseFlux participates in Project-level scale/rootNote inheritance via `_scale` / `_rootNote` int8_t fields on PhaseFluxSequence, default −1 (inherit). Pattern follows StochasticSequence.
- 2026-05-25: No new `Routing::Target::` enum values for PhaseFlux. Existing `SlideTime` / `Octave` / `Transpose` / `Divisor` / `ClockMult` / `Scale` / `RootNote` targets dispatch through `phaseFluxTrack().writeRouted(...)` and `phaseFluxTrack().sequence(p).writeRouted(...)` like Stochastic. Per-cell routing remains deferred to v2.
- 2026-05-25: ProjectVersion stays at `Version35`. `Version_PhaseFlux_Pending = 36` lives as a standalone `constexpr int` after the enum's closing brace, NOT as a new enum entry — adding it inside the enum would auto-bump `Latest` and cause version-guard churn on every save. Rename + insert into the enum at ship time.
- 2026-05-25: Temporary gate in `TrackModeListModel` skips PhaseFlux during forward cycling. Without it, the user could land on PhaseFlux from the Track Setup page; Engine.cpp has no per-mode case yet, so the old TrackEngine instance would keep ticking with the wrong `_trackMode`, triggering `SANITIZE_TRACK_MODE` asserts or release-build UB. Gate gets removed when `PhaseFluxTrackEngine` + UI page land.
- 2026-05-25: `Project::selectedPhaseFluxSequence()` accessor deferred to Phase B (UI is the only consumer). No tests or model-layer code in this commit reference it; adding it now would be cruft.
- 2026-05-25: **Phase A math families landed.** `PhaseFluxMath` lives in `model/` alongside `Curve.{h,cpp}` (precedent for stateless math helpers used by engines). Snake order is a file-static `std::array<uint8_t, 16>` returned by reference. Floor-scaling uses int64 promotion + round-to-nearest with `+ cycleTicks/2` and re-derives final `cycleTicks` from the rounded `cum[16]` so the table is internally consistent. Linear scan over 16 slots in `deriveTickPosition` instead of binary search (16 elements, readability wins). Acceptance tests verify the §3.1 contract that the floor scales BOTH `cycleTicks` AND every `cumulativeTicks[i]` — regression where someone "fixes" the floor by setting cycleTicks alone now fails the test.

## Open questions

All 66 audit questions resolved. No open product decisions before implementation.

Implementer-resolved items (engineering punch list, §16):
- [x] Exact byte-pack offsets within the 3 × uint32_t stage record — see Phase A decision log entry above
- [x] Whether PhaseFlux participates in Project-level scale/rootNote inheritance — yes, via `_scale` / `_rootNote = -1 = inherit`
- [ ] Bounce temporal-curve LUT choice (ExpDown2x, ExpDown3x, or new `Bounce = 1 − (1−x)³` entry in `model/Curve.h`) — engine-side, deferred to Phase B
- [ ] Default `_resetMeasure` UI step granularity (powers-of-two from NoteSequence) — UI-side, deferred to Phase B/C
- [ ] Pre-build snake permutation lookup (`snakeOrder[16]`) — static const or computed once at engine init — engine-side, deferred to Phase B

## Completed steps

**Design**
- [x] Spec design via grilling — 8 rounds, ~25 product decisions locked
- [x] DiscreteMap stateless-ramp pattern adopted for engine
- [x] NoteTrack pitch chassis adopted (scale-degree integer storage)
- [x] Stochastic borrow pattern for Track-level chassis (§12.1)
- [x] Flux PHAS + MASK transforms borrowed (§6 transforms)
- [x] Stochastic MaskMelody + TiltMelody centrality filter borrowed (§6.2.1)
- [x] Refined curve model (base + vflip + hflip) replacing per-variant enums
- [x] UI surfaces designed and rendered (`ui-preview/pages_phaseflux.py`)
- [x] External 3-reviewer audit run (adversarial / coherence / feasibility) — all clean
- [x] 66-question external audit completed and resolved
- [x] Spec template extracted to `docs/spec-template.md`

**Phase A — math & storage foundations (done — all 4 test families per spec §18.1)**

Storage layer (commit `e8e69c0d` + cleanup `5fc124b5`):
- [x] `Version_PhaseFlux_Pending = 36` placeholder constant landed outside the enum chain
- [x] `PhaseFluxSequence` storage class — chassis (`_divisor`, `_clockMultiplier`, `_resetMeasure`, `_scale`, `_rootNote`, `_edited`) + 16-stage array
- [x] `PhaseFluxSequence::Stage` bit-packed record — 3 × `uint32_t`, 24 fields, 93 bits used, 3 spare
- [x] `PhaseFluxTrack` storage class — chassis (`_slideTime`, `_octave`, `_transpose`, `_playMode`, `_fillMode`, `_cvUpdateMode`, `_lock`) + pattern slot array, `static_assert(sizeof(...) <= 9544)`
- [x] All required `Routable<>` wrappers (`_slideTime`, `_octave`, `_transpose`, `_divisor`, `_clockMultiplier`) — day-1 contract per spec §12.3
- [x] **Test family 1: Serialization round-trip** — 7 cases (default, every chassis field, every stage field, every numeric min/max edge, bit-pack non-overlap, `_lock` non-persistence)

Track infrastructure wiring (commit `f5f5a9e3` + gating `8996ef3a`):
- [x] `TrackMode::PhaseFlux` enum value (serialize ID 8) wired through `Track.h` + `Track.cpp` (6 switch sites each)
- [x] `Track::phaseFluxTrack()` accessor pair, `Container<...>` template parameter, union member, `#include`
- [x] `ClipBoard` pattern copy/paste wired
- [x] `Routing` `writeTarget` PhaseFlux dispatch wired (no new `Routing::Target::` entries)
- [x] `TrackModeListModel` temporary gate so PhaseFlux is unreachable via Track Setup page until engine + UI land
- [x] Track-level round-trip test (8th case in `TestPhaseFluxSequenceSerialization`)

Math helpers (commit `c397e381` + cleanup `f8a1bc6d`):
- [x] `PhaseFluxMath` class (`src/apps/sequencer/model/PhaseFluxMath.h/.cpp`) — stateless helpers, no engine state
- [x] `snakeOrder()` — boustrophedon 4×4 permutation per spec §2: `{0,1,2,3, 7,6,5,4, 8,9,10,11, 15,14,13,12}`
- [x] `stageDivisorTicks(slot)` — 8-slot enum → ticks at CONFIG_SEQUENCE_PPQN=48: `{6, 8, 12, 16, 24, 32, 48, 96}` matching `KnownDivisor.h`
- [x] `powerBend(z, p)` — spec §6 formula `pow(z, (1-p)/(1+p))`, no defensive clamping (encoding boundary is the guard)
- [x] `powerBendKnobToParam(encoded)` — spec §16 `encoded/64.0f` (NOT `/63`), max-knob ±0.984375
- [x] `computeCumulativeTicks(...)` — spec §3.1 snake-walk + kMinCycleTicks floor (proportional stretch) + all-skipped idle marker
- [x] `deriveTickPosition(...)` — spec §3.2 modulo + slot search + active cell remap + stagePhase; idle return on `cycleTicks == 0`
- [x] **Test family 2: Cumulative duration table** — 9 cases including snake-order verification + floor-stretch correctness (cum[] scales proportionally, not just cycleTicks)
- [x] **Test family 3: Per-tick derivation** — 8 cases including cycle wraparound + large-tick overflow + idle return + snake remap at derive time
- [x] **Test family 4: PowerBend encoding** — 8 cases including identity at p=0 + endpoint preservation + monotonicity + max-knob abs < 1.0 degeneracy guard

Quality gates:
- [x] Spec-compliance + code-quality reviewer subagents passed on every Phase A commit
- [x] Sim release build (`make -C build/sim/release sequencer`) clean; full sequencer binary links
- [x] All 32 PhaseFlux test cases green (7 serialization + 9 cumulative + 8 per-tick + 8 powerbend)
- [x] Regression: Stochastic / Accumulator / NoteSequence serialization tests still green
- [x] Pre-existing failure `TestStochasticDurationDictionary::direct_history_event_is_compact_ui_truth` noted (unrelated to PhaseFlux)

## Notes

**Implementation tier order** (spec §18.1):
1. ~~**Phase A — math & storage foundations**~~ ✅ done. Storage round-trip is the only Phase A test that can land before engine code; cumulative duration table + per-tick derivation + PowerBend correctness all need engine code and live with Phase B/C.
2. **Phase B — first hardware guard (next)**: boot smoke + sub-stage curves audible + transforms audible (phaseShift, masks). Requires `PhaseFluxTrackEngine` + minimal UI (Track page dispatch + EditPage skeleton) so the user can actually select PhaseFlux and hear it.
3. **Phase C — remaining test families**: skip mask, `kMinCycleTicks` floor, idle state, mask × shift, melody mask byte-for-byte, pulse collision clamp, accumulator lifecycle, pitch quantization, curve LUTs at φ=0/0.25/0.5/0.75/1, curve flips, cumulative-recompute side effects, transport behavior.

**Phase B entry checklist** (when resuming):
1. Build `PhaseFluxTrackEngine.h/.cpp` modelled on `DiscreteMapTrackEngine` (stateless-ramp + `_resetTickOffset`).
2. Add `case TrackMode::PhaseFlux:` in `Engine.cpp:527-554` instantiating the new engine.
3. Wire UI: `PhaseFluxTrackListModel`, `PhaseFluxSequenceListModel`, `PhaseFluxEditPage`, `PhaseFluxSequencePage`, register in `Pages.h`, dispatch in `TopPage.cpp` and `TrackPage.cpp`.
4. Add `Project::selectedPhaseFluxSequence()` accessor (UI consumer triggers it).
5. **Remove the `TrackModeListModel` PhaseFlux skip** (search for `// PhaseFlux has no engine/UI yet`).
6. STM32 release build + container-size measurement (RAM gate) before flashing.
7. Boot smoke + sub-stage curves + phaseShift/mask audibility on hardware.

**MVP-done gate list** (spec §18.2): 10 comprehensive gates — engine functional + UI surfaces complete + all test phases pass + STM32 RAM container ceilings respected + boot-to-sound + Pattern A/B/C + Snapshot + Routable-via-global-Routing verified + retro lesson gates passed + adversarial review clean on running firmware.

**Estimated RAM footprint** (§13): `PhaseFluxTrack ≈ 3,422 B` (vs NoteTrack 9,544 B ceiling — ~6,100 B headroom). `PhaseFluxTrackEngine ≈ 200 B` (vs TeletypeTrackEngine 912 B — ~700 B headroom). Adding the variant is **free** under container-gate analysis.
