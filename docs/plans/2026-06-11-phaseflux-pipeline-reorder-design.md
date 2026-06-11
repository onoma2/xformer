# PhaseFlux Pipeline Reorder (v0.2) — Design

**Status:** design, awaiting validation of the one temporal decision (§4).

**Goal:** Reorder the PhaseFlux per-axis transform pipeline to wake dead controls, and extract the duplicated chain into shared functions so the reorder lives in one place and on-screen preview can never diverge from playback.

**Scope:** engine compute only. Window/Repeat are already fully wired (model `_data3`, serialization, engine, UI P2). No model layout change → no ProjectVersion bump. No UI change.

---

## 1. Why

The locked MVP order is `Window → Repeat → Warp → FlipH → Curve → FlipV → Response`. Analysis (this session) showed it leaves controls dead:

- **flipH ≡ flipV on Ramp/Bounce** — nothing nonlinear sits between the two flips, so they collapse to one control. Fix: put **Response between the flips** (response before flipV).
- **Repeat = N identical copies** — Warp hits the post-repeat sawtooth, bending every tile the same. Fix: **Warp before Repeat**, so the whole-cell warp makes the tiles uneven/progressive.
- **Window applied once** — moving it after Repeat lets it act per cycle.

Two dead spots are shape-intrinsic and **not** fixed here (no order can): flipH on symmetric curves (Bell/Triangle), and warp≈response on the linear curve.

## 2. New order (v0.2), both axes

```
Warp → Repeat → Window → [FlipH] → Curve → Response → FlipV → [FlipH] → direction/quantize
              pitch: FlipH pre-curve            time: FlipH = schedule reversal, last
```

Three swaps vs locked order: warp↔window ends, response↔flipV. FlipH placement is unchanged per axis (the existing asymmetry stays): pitch mirrors phase pre-curve; time reverses the built schedule last.

## 3. Extraction architecture

The pitch chain is duplicated **verbatim** at two sites — gate path `PhaseFluxTrackEngine.cpp:400-407` and continuous/CV path `:704-715` (differ only in knob-var names). Extract:

```cpp
// pure, header-static; both pitch paths call it
float phaseFluxPitchValue(float phi, int pitchR, int warpKnob,
                          bool flipH, Curve::Type curveType,
                          bool flipV, int respKnob,
                          PhaseFluxSequence::WindowType windowType);
// returns p_final in [0,1]; window+repeat+warp+flipH+curve+response+flipV inside
```

Temporal value-chain is one site `:379-382`. Extract the value part:

```cpp
float phaseFluxTemporalValue(float t_local, int warpKnob,
                            Curve::Type curveType, bool flipV, int respKnob);
// returns t_final_local in [0,1]
```

Temporal Window-drop (`:375`) and FlipH schedule-reversal (`:534-554`) are control-flow, stay at the call site.

**The reorder then lives inside these two functions only.** Preview/UI-scope rendering calls the same functions → cannot diverge.

## 4. The one temporal decision (NEEDS NOD)

Pitch Repeat is `fmod(phi*R)` — a phase multiply. "Warp before Repeat" is a clean swap there.

Temporal Repeat (`:340-372`) is **integer pulse-allocation**: `pulseCount` split into R equal-duration sub-sections, each re-traversing the curve over local `[0,1]`. There is no single phase for warp to fold before the split. "Mirror pitch fully" therefore means one of:

- **(A) Literal restructure** — warp the whole-cell pulse position `i/pulseCount` first, then derive `subIdx` and local position from the *warped* global. Tiles become uneven, matching pitch. Real change to `:340-372`. Matches "mirror fully" honestly.
- **(B) No-op** — keep warp on `t_raw_local` (current). The swap is textual only; sub-section allocation unchanged. Simpler, but warp-before-repeat does nothing on time.

Design recommends **(A)** to honor "mirror pitch fully." This is the only behavioral risk and the only nontrivial temporal task. **Confirm A or B before implementation.**

`response↔flipV` on temporal is a literal swap (`:381`↔`:382`), unaffected by this decision.

## 5. Test strategy

Guarding tests already exist: `TestPhaseFluxTrackEngine.cpp`, `TestPhaseFluxRepeatWindow.cpp`, `TestPhaseFluxPowerBend.cpp`.

- **Phase 1 (extract):** behavior-preserving. All existing tests stay green — that *is* the proof the extraction is faithful. No test edits.
- **Phase 2 (reorder):** new/updated tests pin the new behavior:
  - `response before flipV` makes flipH≠flipV on Ramp (assert the four `{none,H,V,HV}` orientations are now distinct with response≠0).
  - `warp before repeat` (pitch) gives uneven tiles (assert tile boundary positions shift under warp).
  - decision-A only: temporal sub-section allocation shifts under warp (assert pulse→subIdx mapping changes).
  - Window-after-repeat per-cycle clamp (pitch) — assert per-tile clamp.

## 6. Tasks (phased)

1. **Extract pitch function** — move `:400-407` chain into `phaseFluxPitchValue`, call from both pitch sites. Tests green. Commit.
2. **Extract temporal function** — move `:379-382` into `phaseFluxTemporalValue`, call from temporal site. Tests green. Commit.
3. **Reorder inside pitch function** — warp↔window, response↔flipV. Update tests. Commit.
4. **Reorder inside temporal function** — response↔flipV. Update tests. Commit.
5. **Temporal warp-before-repeat (decision A)** — restructure `:340-372`. New test. Commit. *(skip if B)*
6. **Spec delta** — update `docs/phaseflux-spec.md` §6.1/§6.2/§14.2 to v0.2 order + extraction note + FlipH asymmetry callout. Bump spec header to v0.2. Commit.

## 7. Out of scope

Model storage, serialization, UI rows, ProjectVersion, new params, the global/macro warp at `:601`.
