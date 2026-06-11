# PhaseFlux Pipeline Reorder (v0.2) Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Reorder the PhaseFlux per-axis transform pipeline to the v0.2 order, after extracting the duplicated chain into shared functions so the reorder lives in one place.

**Architecture:** Extract the verbatim-duplicated pitch chain (2 sites) into one pure function `phaseFluxPitchValue`, and the temporal value-tail into `phaseFluxTemporalValue`. Phase 1 extracts with zero behavior change (existing tests stay green). Phase 2 reorders inside the functions. Temporal Repeat is restructured (decision A) so warp applies to the whole-cell position before sub-section allocation.

**Tech Stack:** C++ (STM32 firmware + sim host tests), CMake/CTest, `Curve::eval`, `PhaseFluxMath::{powerBend,holdPitchWindowBoundary,isInWindow}`.

**Companion:** see `2026-06-11-phaseflux-pipeline-reorder-design.md` for rationale and the locked-order analysis.

---

## Build & test commands (run from worktree root)

One-time: `make setup_sim`
Build a test: `cmake --build build/sim/debug --target TestPhaseFluxTrackEngine`
Run one: `ctest --test-dir build/sim/debug -R TestPhaseFluxTrackEngine --output-on-failure`
Run all PhaseFlux: `ctest --test-dir build/sim/debug -R PhaseFlux --output-on-failure`

Target engine file: `src/apps/sequencer/engine/PhaseFluxTrackEngine.cpp`
New helpers go in an anonymous-namespace block near the existing `applyPowerBend` (line ~68) / `isInWindow` (line ~77) helpers.

---

## Phase 1 — Extract (behavior-preserving)

### Task 1: Baseline green

**Step 1:** `make setup_sim`
**Step 2:** `cmake --build build/sim/debug --target TestPhaseFluxTrackEngine TestPhaseFluxRepeatWindow TestPhaseFluxPowerBend`
**Step 3:** `ctest --test-dir build/sim/debug -R PhaseFlux --output-on-failure`
Expected: all PASS. If not, STOP and report.
**Step 4:** No commit (baseline).

### Task 2: Extract `phaseFluxPitchValue`

**Files:** Modify `src/apps/sequencer/engine/PhaseFluxTrackEngine.cpp` (add helper near :68; replace `:400-407` and `:704-715`).

**Step 1:** Add the helper, body = the CURRENT order verbatim (no reorder yet):

```cpp
inline float phaseFluxPitchValue(float phi, int pitchR, int warpKnob,
                                 bool flipH, Curve::Type curveType,
                                 bool flipV, int respKnob,
                                 PhaseFluxSequence::WindowType windowType) {
    phi = PhaseFluxMath::holdPitchWindowBoundary(phi, windowType);
    float phi_repeated = (pitchR > 1) ? std::fmod(phi * float(pitchR), 1.f) : phi;
    float phi_warped = applyPowerBend(phi_repeated, warpKnob);
    float phi_input = flipH ? (1.f - phi_warped) : phi_warped;
    float p_curved = Curve::eval(curveType, phi_input);
    float p_flipped = flipV ? (1.f - p_curved) : p_curved;
    return applyPowerBend(p_flipped, respKnob);
}
```

**Step 2:** Replace the gate-path block `:400-407` with:
```cpp
float p_final = phaseFluxPitchValue(phi, pitchR, pitchWarpKnob, pFlipH,
                                    pitchCurveType, pFlipV, pitchRespKnob, pitchWindowType);
```
(Delete the now-inlined `phi = holdPitchWindowBoundary...` at :400 — it moved into the helper. Pass the raw `phi` from :391-397.)

**Step 3:** Replace the continuous-path block `:704-715` the same way, using `pWarpEff`/`pRespEff` and `pitchStage.pitchFlipH()/pitchFlipV()`.

**Step 4:** `cmake --build build/sim/debug --target TestPhaseFluxTrackEngine TestPhaseFluxRepeatWindow && ctest --test-dir build/sim/debug -R PhaseFlux --output-on-failure`
Expected: all PASS (proves extraction is faithful).

**Step 5:** Commit: `refactor(phaseflux): extract pitch pipeline into shared phaseFluxPitchValue`

### Task 3: Extract `phaseFluxTemporalValue`

**Step 1:** Add helper (current order):
```cpp
inline float phaseFluxTemporalValue(float t_local, int warpKnob,
                                    Curve::Type curveType, bool flipV, int respKnob) {
    float t_warped = applyPowerBend(t_local, warpKnob);
    float t_curved = Curve::eval(curveType, t_warped);
    float t_flipped = flipV ? (1.f - t_curved) : t_curved;
    return applyPowerBend(t_flipped, respKnob);
}
```
**Step 2:** Replace `:379-382` with:
```cpp
float t_final_local = phaseFluxTemporalValue(t_raw_local, tempWarpKnob, tempCurveType, tFlipV, tempRespKnob);
```
**Step 3:** `ctest --test-dir build/sim/debug -R PhaseFlux --output-on-failure` → all PASS.
**Step 4:** Commit: `refactor(phaseflux): extract temporal value chain into shared helper`

---

## Phase 2 — Reorder (v0.2 behavior)

### Task 4: Pin flipH/flipV independence (failing test first)

**Files:** Modify `src/tests/unit/sequencer/TestPhaseFluxTrackEngine.cpp` (or a new `TestPhaseFluxPipelineOrder.cpp` — register in `src/tests/unit/sequencer/CMakeLists.txt`).

**Step 1:** Write a failing test: with curve=Ramp, response≠0, assert `phaseFluxPitchValue` gives DISTINCT results for {flipH only} vs {flipV only}. Under the current order they're equal; assert they differ.
**Step 2:** Build + run → FAIL (current order: H==V).
**Step 3:** (no impl yet — Task 5 makes it pass.)

### Task 5: Reorder pitch inside `phaseFluxPitchValue`

**Step 1:** Change the helper body to v0.2 order (warp→repeat→window→flipH→curve→response→flipV):
```cpp
inline float phaseFluxPitchValue(float phi, int pitchR, int warpKnob,
                                 bool flipH, Curve::Type curveType,
                                 bool flipV, int respKnob,
                                 PhaseFluxSequence::WindowType windowType) {
    float phi_warped = applyPowerBend(phi, warpKnob);
    float phi_repeated = (pitchR > 1) ? std::fmod(phi_warped * float(pitchR), 1.f) : phi_warped;
    float phi_windowed = PhaseFluxMath::holdPitchWindowBoundary(phi_repeated, windowType);
    float phi_input = flipH ? (1.f - phi_windowed) : phi_windowed;
    float p_curved = Curve::eval(curveType, phi_input);
    float p_resp = applyPowerBend(p_curved, respKnob);
    return flipV ? (1.f - p_resp) : p_resp;
}
```
**Step 2:** Build + run Task 4 test → PASS. Run all PhaseFlux tests; update any that asserted the OLD order (expect `TestPhaseFluxRepeatWindow` to need new expected values — recompute by hand, document in the test).
**Step 3:** Add tests: warp-before-repeat gives uneven pitch tiles (assert tile-edge positions shift with warp≠0); window-after-repeat clamps per cycle (repeat=2 + Focus → two clamped tiles).
**Step 4:** Commit: `feat(phaseflux): reorder pitch pipeline to v0.2 (warp→repeat→window, response→flipV)`

### Task 6: Reorder temporal value chain

**Step 1:** Swap response↔flipV in `phaseFluxTemporalValue` (curve→response→flipV):
```cpp
float t_curved = Curve::eval(curveType, t_warped);   // t_warped still passed in for now
float t_resp = applyPowerBend(t_curved, respKnob);
return flipV ? (1.f - t_resp) : t_resp;
```
(Warp moves out in Task 7; until then it stays as a param.)
**Step 2:** Update temporal-order tests; run → PASS.
**Step 3:** Commit: `feat(phaseflux): reorder temporal value chain (response→flipV)`

### Task 7: Temporal warp-before-repeat (decision A — restructure)

**Files:** Modify `:340-372` (sub-section allocation) and the temporal call site.

**Step 1:** Write a failing test: with tempWarp≠0, assert a pulse's `subIdx`/local-position differs from the warp=0 allocation (warp now skews sub-section membership). Currently allocation is warp-independent → FAIL.
**Step 2:** Restructure: compute whole-cell raw position per pulse, warp it, derive `subIdx` and local position from the WARPED position, map back via `(subIdx + value)/R`. Remove warp from `phaseFluxTemporalValue` call (warp already applied pre-allocation). Keep distribution policy (earlier subs get remainder) defined on the warped axis. **This is the high-risk task — keep the commit isolated.**
**Step 3:** Run all PhaseFlux tests → PASS; confirm preview path (UI scope) uses the same allocation so preview==playback.
**Step 4:** Commit: `feat(phaseflux): temporal warp precedes repeat — warped sub-section allocation`

---

## Phase 3 — Spec

### Task 8: Spec delta to v0.2

**Files:** Modify `docs/phaseflux-spec.md`.
**Step 1:** Update §6.1.2/§6.2/§14.2 pipeline-order text to `Warp → Repeat → Window → [FlipH] → Curve → Response → FlipV`. Note the FlipH per-axis asymmetry explicitly. Note the temporal warped-allocation (decision A). Add a short "extracted to `phaseFluxPitchValue`/`phaseFluxTemporalValue`" implementation note.
**Step 2:** Change header `# PhaseFlux Track — Locked MVP Spec` → `v0.2` status; record the reorder in the changelog/retro section.
**Step 3:** Commit: `docs(phaseflux): pipeline reorder spec delta v0.2`

---

## Done criteria

- All `ctest -R PhaseFlux` pass.
- flipH≠flipV on Ramp+response; warp-before-repeat uneven on both axes; response-before-flipV on both.
- Preview path and playback path call the same two helper functions.
- No model/serialization/UI/ProjectVersion change in the diff.
