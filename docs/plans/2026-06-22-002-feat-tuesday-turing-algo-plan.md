---
id: tuesday-turing-algo
schema: plan
title: "feat: Turing-machine algorithm for the Tuesday track"
type: feat
status: completed
date: 2026-06-22
depth: standard
---

# Tuesday Turing-machine algorithm

Add a Music-Thing-style Turing machine (looping shift register with per-step bit-flip
probability) as a new Tuesday algorithm — deterministic on the preview/apply generator,
authentically live-evolving on the live track. No new TrackMode, no new model fields, no new
pages: it rides the existing Tuesday controls and per-step generate plumbing.

Origin: `.tasks/performer-improvements/TASK.md` → "Actionable: Turing as a Tuesday algo" (the
performer-nx fork survey flagged a shift-register sequencer as a genuine gap — none exists in src).

---

## Summary

A Turing machine is a circular shift register whose LSB flips with probability `p` before each
rotation: `p=0` repeats a fixed loop forever, `p=max` is random, in between the loop slowly
mutates. The register value maps to pitch. We add it as Tuesday algorithm index 15 (the 16th
algo), reusing Tuesday's control vocabulary and per-step generate shape.

Two behaviors, one core:
- **Generator path** (`TuesdayAlgoCore`, used by `AlgoGenerator` for preview/apply): clock the
  register from a *seeded* RNG → a reproducible loop. Deterministic; fits preview/apply.
- **Live-track path** (`TuesdayTrackEngine`): the register is persistent per-algo state clocked by
  the *running* RNG each playback step → authentic continuous drift; `power=0` freezes the current
  loop. This is the non-standard behavior, and it's inherent to the live path — no toggle, no model
  field.

---

## Problem Frame

Tuesday's 15 algos are implemented **twice**: in `TuesdayTrackEngine` (`generateTest`…`generateGanz`,
dispatched by `generateStep`) for live playback, and in `TuesdayAlgoCore::generate` (per-algo State
structs) for the `AlgoGenerator` preview/apply path. Both share `AlgoParams` (seed, flow, ornament,
glide, power, stepTrill, gateLength) and a per-step generate→result shape. So a new algo has **two
integration points** — the shift-register DSP is written once as a shared helper and called from
both. (The origin note's "16th algo in TuesdayAlgoCore" covers only the generator half.)

Reference study:
- **o_c `util_turing.h`** (`temp-ref/o_c/O_C-Phazerville/software/src/util/util_turing.h`) is the
  authentic mechanism: 32-bit register, length 3–32, probability 0–255, circular shift with
  probabilistic LSB flip, anti-all-zero guard. This is what we port (native, ~40 lines).
- **performer-nx `TuringAlgorithm`**
  (`temp-ref/performer-nx/src/apps/sequencer/model/algorithm-track/TuringAlgorithm.cpp`) is
  mislabeled — it is an 8-bit cellular automaton (`newBit = a ^ (b|c)`) populate-once generator with
  ~10 params, not a Turing machine. Not ported; a couple of its param ideas (note range, octave
  range) inform the pitch-window mapping only.

---

## Key Technical Decisions

- **Port the o_c shift-register mechanism, not performer-nx's CA.** o_c is the real Turing machine and is compact. Write it native as a small `TuringRegister` struct (no o_c dependency, no `random()` global — takes an RNG).
- **One shared DSP core, two call sites.** `TuringRegister` is the single source of truth; `TuesdayAlgoCore` and `TuesdayTrackEngine` both use it, matching the existing two-implementation structure without duplicating the shift logic.
- **Deterministic-vs-live is RNG sourcing, not a toggle.** The generator clocks the register with a seeded RNG (reproducible); the live track clocks a persistent register with the running RNG (drifts). `power=0` yields a pure rotation (no flip) → a clean repeating loop on both paths. No `AlgoParams` field, no model-union growth, no UI toggle.
- **Standard-control mapping (no new params):**
  - `seed` → initial 32-bit register fill
  - `flow` (0–16) → register **length** / loop length (map to ~2–32)
  - `power` (0–16) → flip **probability** (→ 0–255); `0` = frozen loop, `16` = random
  - `ornament` (0–16) → pitch **range/window**: how wide the register value spreads across scale degrees, then quantized to the project scale (same quantize path as other algos)
  - `glide` → slide, `gateLength` → gate ratio, `stepTrill` → trill (reused unchanged)
  - gate on/off derived from a register tap (classic Turing "pulses")
- **Live-track register is engine state, not model state.** A `TuringState { uint32_t reg; }` in the engine's `_algoState` (and `TuesdayAlgoCore`'s State) — same place the other algos keep per-algo state. Reset/seed on `reseed()` and transport reset.

---

## Scope Boundaries

In scope:
- `TuringRegister` shared core (port of o_c mechanism).
- Turing as Tuesday algo index 15 in **both** `TuesdayAlgoCore` (deterministic, preview/apply) and `TuesdayTrackEngine` (live, drifting).
- Algo count 15→16, "Turing" name/label in the selector and both paths.
- The standard-control mapping above; live drift via `power` on the live track.

Out of scope:
- No new `AlgoParams` fields, no new TrackMode, no new pages, no new model storage.
- performer-nx's CA algorithm and its extra params (range/octave beyond the `ornament` window).
- A separate "live/deterministic" toggle — the path determines it.

### Deferred to Follow-Up Work
- Exposing register length beyond the `flow` range, or a dedicated "lock" gesture distinct from `power=0`.
- A bit-pattern visualization of the register in the Tuesday/preview UI.

---

## Implementation Units

### U1. `TuringRegister` shared core

**Goal:** A native, self-contained shift register porting the o_c mechanism, taking an RNG so it serves both the seeded (deterministic) and running (live) callers.

**Dependencies:** none.

**Files:**
- `src/apps/sequencer/engine/generators/TuringRegister.h` (create — header-only struct)
- `src/tests/unit/sequencer/TestTuringRegister.cpp` (create)
- `src/tests/unit/sequencer/CMakeLists.txt` (register the test)

**Approach:** A `struct TuringRegister { uint32_t reg; uint8_t length; }` with `clock(rng, probability) -> uint32_t` reproducing o_c's logic: with probability `p` (out of 255) flip the LSB, circular-shift right with wrap into bit `length-1`, anti-all-zero guard, return the low `length` bits. A `seed(uint32_t)` fill and `set_length` with the o_c grow-guard. Take the project RNG type (`Random` from `core/utils/Random.h`) by reference rather than a global `random()`.

**Patterns to follow:** `temp-ref/o_c/O_C-Phazerville/software/src/util/util_turing.h` for the exact shift/flip/guard; `src/apps/sequencer/engine/generators/TuesdayAlgoCore.h` for the project's struct/RNG style; `src/tests/unit/sequencer/TestTT2Transpose.cpp` for the unit-test shape.

**Execution note:** Test-first — this is pure logic and the one cleanly unit-testable unit.

**Test scenarios:**
- `probability=0`, length L: clocking L times returns the register to its start → the output sequence has period exactly L (a frozen loop). Verify for a couple of lengths.
- `probability=255`: LSB flips every clock (output diverges from the `p=0` baseline within a few clocks).
- Seed reproducibility: two registers with the same seed + same RNG seed + same probability produce identical clock sequences.
- Length bounds: `length` clamped to [3,32]; output masked to the low `length` bits (never exceeds `(1<<length)-1`).
- Anti-all-zero: a register driven toward 0 never sticks at all-zero (matches o_c's guard).

**Verification:** `TestTuringRegister` passes; sim + STM32 build clean with the header added.

---

### U2. Turing in `TuesdayAlgoCore` (deterministic generator path)

**Goal:** Algo index 15 in the generator: produce a reproducible Turing loop for preview/apply.

**Dependencies:** U1.

**Files:**
- `src/apps/sequencer/engine/generators/TuesdayAlgoCore.h` (add `TuringState`, bump algo count)
- `src/apps/sequencer/engine/generators/TuesdayAlgoCore.cpp` (the algo body + dispatch)

**Approach:** Add a `TuringState { TuringRegister reg; }` to the State set; in `init()` seed the register from `params.seed` and set length from `flow`. In `generate(15, ctx)`: clock the register once per step using the core's seeded RNG and `power`→probability; map the returned value through the `ornament` pitch window, quantize to the project scale (reuse the same quantize path the other algos use), derive gate from a register tap, and fill `AlgoResult` (note/octave/gate/slide from `glide`, gateRatio from `gateLength`). Deterministic because the RNG is seeded.

**Patterns to follow:** an existing `TuesdayAlgoCore` algo (e.g. the Markov case) for the State + generate + scale-quantize idiom; the control names from `AlgoParams`.

**Test scenarios:**
- Same seed + same params → identical generated sequence on two runs (determinism — exercised via `AlgoGenerator` preview if a generator-level test exists; otherwise build + manual preview).
- `power=0` → the previewed loop repeats with period = mapped length.
- Test expectation: covered at the core level by U1; this unit's mapping is verified by build + manual preview (no generator unit-test harness).

**Verification:** "Turing" selectable in the AlgoGenerator/preview; preview shows a stable loop; reseed reproduces; sim + STM32 clean.

---

### U3. Turing in `TuesdayTrackEngine` (live, drifting)

**Goal:** Algo index 15 on the live track, with authentic continuous drift.

**Dependencies:** U1.

**Files:**
- `src/apps/sequencer/engine/TuesdayTrackEngine.h` (add `TuringState` to `_algoState`)
- `src/apps/sequencer/engine/TuesdayTrackEngine.cpp` (`generateTuring`, `generateStep` switch case 15, count, `reseed`/reset handling)

**Approach:** Add `generateTuring(const GenerationContext &ctx)` mirroring the other `generateX` methods; keep a persistent `TuringRegister` in `_algoState`. Each playback step clock it with the **running** `_rng` and `power`→probability → live drift; `power=0` rotates without flipping → frozen loop. Map value → scale-quantized note + gate exactly as U2 (shared mapping helper preferred). Wire `reseed()` and transport reset to re-fill the register from the seed. Add `case 15: return generateTuring(ctx);` and bump the algo count.

**Patterns to follow:** `generateStomper`/`generateMarkov` in `TuesdayTrackEngine.cpp` for the persistent-state + per-step shape; `reseed()` for state re-init.

**Test scenarios:**
- `power=0`: the live output repeats every mapped-length steps (frozen loop) — manual on the sim.
- `power>0`: the loop audibly drifts over several cycles (not identical loop to loop) — manual on the sim.
- `reseed` / transport reset returns the register to the seeded loop — manual on the sim.
- Test expectation: shift math is covered by U1; live integration is build + manual (no live-engine unit-test harness, consistent with the rest of the Tuesday engine).

**Verification:** "Turing" plays on a Tuesday track; `power` sweeps frozen→drifting; reseed resets; sim + STM32 clean.

---

### U4. Registration, name, and labels

**Goal:** Surface "Turing" as the 16th algo everywhere the algo list is presented, with sensible knob labels.

**Dependencies:** U2, U3.

**Files:**
- The Tuesday algo name/label table(s) — locate during implementation (the selector source feeding `TuesdayEditPage` and `AlgoGenerator::paramName`/printParam). Likely `TuesdayAlgoCore` and/or a shared algo-name array.

**Approach:** Add "Turing" to the algo name list; raise any `0-14`/`numAlgos == 15` bounds to 16 (the `AlgoParams.algorithm` comment, selector clamps, both dispatch switches). Confirm the three character knobs read sensibly for Turing in the UI — if Tuesday relabels `flow`/`ornament`/`power` per algo, set them to length/range/probability; if labels are generic, leave them. No new UI page.

**Test scenarios:**
- The algo selector lists "Turing" and selecting it routes to the new code on both paths.
- Bounds: selecting algo 15 never clamps back to 14; no off-by-one in either dispatch.
- Test expectation: none beyond build + manual selector check — labels/registration are not behavioral logic.

**Verification:** "Turing" appears in the Tuesday algo selector and the generator; both paths dispatch to it; sim + STM32 clean.

---

## System-Wide Impact

- **Surfaces touched:** new `TuringRegister` header + test; `TuesdayAlgoCore` (generator algo); `TuesdayTrackEngine` (live algo); algo name/count table. No model, file-format, routing, or IO changes — `AlgoParams` is unchanged, so projects and save files are unaffected.
- **The one behavioral subtlety:** the live track's Turing drifts (non-reproducible across playthroughs at `power>0`) while every other Tuesday live algo is seed-deterministic. This is intended (it's the Turing identity) and isolated to algo 15; document it where the algos are listed.

---

## Verification (whole plan)

- `TestTuringRegister` green; sim (`make -C build/sim/debug sequencer`) and STM32 release build clean.
- On the sim: "Turing" selectable on a Tuesday track and in preview/apply; `power=0` frozen loop, `power>0` live drift on the track, deterministic reproducible loop in preview; `flow` changes loop length, `ornament` changes pitch spread; reseed/transport reset re-seeds the register.
- No change to any saved project (AlgoParams unchanged) — load an existing Tuesday project and confirm its algos still resolve (the count bump must not shift existing indices).
