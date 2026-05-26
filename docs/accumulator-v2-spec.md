# Adversarial Review — Implementation Readiness

The current spec is not implementable as written.

Critical issue: it describes a general accumulator framework, not a bounded `performer-phazer` feature plan. It mixes current behavior, Csound-style abstraction, future target types, and unbudgeted storage changes.

## Findings

1. **State ownership is unresolved.**
   The spec says each instance owns `ACC`, `INC`, limits, direction, mode, polarity, and scope.
   Current code stores one `Accumulator` inside each `NoteSequence`, while the engine mutates it through `const_cast`.
   Relevant paths: `src/apps/sequencer/model/NoteSequence.h`, `src/apps/sequencer/model/Accumulator.h`, `src/apps/sequencer/engine/NoteTrackEngine.cpp`.

2. **`LOCAL` scope explodes RAM unless redefined.**
   The spec says each step maintains its own accumulator state.
   Current model only has one accumulator plus a 4-bit per-step command.
   Literal per-step accumulator state means 64 states per sequence and needs a hard rejection or compressed design first.

3. **Trigger semantics conflict with the engine.**
   The spec says "boolean rising edge evaluated at step clock rate."
   Current behavior supports `Step`, `Gate`, and `Retrigger` modes in separate timing paths.
   The spec must define ordering relative to condition, gate probability, retrigger scheduling, fill sequence, and note evaluation.

4. **Boundary math is under-specified for large overshoot.**
   `WRAP` and `PENDULUM` examples handle one breach, not repeated wrap or reflection when `INC` exceeds the full range.

5. **The spec adds modes the UI/model cannot hold.**
   Current `Order` is `Wrap`, `Pendulum`, `Random`, `Hold`.
   The spec adds `RTZ` and `ONE-SHOT`.
   That requires enum storage, UI rows, serialization, and tests.

6. **Per-step commands do not fit current step storage.**
   The spec wants `DEFEAT`, `ABS_SET`, `REL_NUDGE`, `RESET`, `REVERSE`, limit changes, and increment changes.
   Current per-step storage is a 4-bit `OFF / S / -7..+7` trigger or override value.
   This is a schema redesign, not an accumulator tweak.

7. **Continuous domain is premature.**
   The spec allows float timing, CV, cents, and ms.
   Current implementation is integer pitch offset only.
   Float accumulators across timing and CV targets need target-specific clamps, units, display, serialization, and RAM accounting.

## Proposed Action Plan

1. Rewrite the spec around current topology first:
   model owns accumulator config; engine owns runtime tick application; UI edits config and reads current value only if that ownership is deliberate.

2. Split v2 into phases:
   - Phase 1: integer pitch accumulator only.
   - Phase 2: cleanup current semantics and tests.
   - Phase 3: optional new commands.
   - Phase 4: optional continuous targets.

3. Define `LOCAL` without per-step full state.
   Either reject it for v2, or define it as "apply only on marked steps using one shared accumulator."
   Do not allow literal 64 accumulator states without RAM proof.

4. Replace abstract trigger text with the actual three engine paths:
   `Step`, `Gate`, and `Retrigger`.
   Include exact ordering relative to condition, gate probability, retrigger scheduling, fill sequence, and note evaluation.

5. Fix boundary algorithms with modulo/reflection tests.
   Cover `INC > range`, negative `INC`, invalid `min > max`, `min == max`, `Freeze`, and random range.

6. Keep per-step v2 storage compatible unless a schema redesign is explicitly accepted.
   Current 4-bit `OFF / S / -7..+7` override can be formalized.
   The command-table idea should move to "future schema."

7. Add a verification checklist:
   unit tests for accumulator math, note-engine trigger timing tests, serialization round-trip, UI render if OLED pages change, and STM32 release RAM check.

Do not implement from this doc yet. First edit should be a spec tightening pass, not code.

---

# Accumulator — Revised Specification

## 1. Definition
A stateful register that increments or decrements by a fixed step value on each valid trigger, stores the result, and makes that result available as an offset or threshold. Boundary logic resolves overflow against configurable limits.

## 2. Data Model

The accumulator exists in two type domains. The implementation must instantiate the correct domain for its target parameter.

| Domain | Storage | Use Case | Typical Unit |
|--------|---------|----------|--------------|
| **Discrete** | Signed 32-bit integer (`int32`) | Pitch (semitones, scale degrees), velocity tiers, probability thresholds, stage indices | Semitones, scale steps, MIDI ticks |
| **Continuous** | 32-bit float (`float32`) | Timing (delay, length), modulation voltage, cents, ms | ms, V, cents |

**Rule**: Discrete targets must never use float accumulation to avoid drift. Continuous targets must use float to preserve sub-step resolution.

## 3. State Registers (per instance)

| Register | Discrete | Continuous | Description |
|----------|----------|------------|-------------|
| `ACC` | `int32` | `float32` | Current accumulated value |
| `INC` | `int32` | `float32` | Step increment (added per trigger) |
| `LIM_HI` | `int32` | `float32` | Upper boundary (inclusive) |
| `LIM_LO` | `int32` | `float32` | Lower boundary (inclusive) |
| `DIR` | `int8` | `int8` | Direction flag: `+1`, `-1`, `0` |
| `MODE` | `uint8` enum | `uint8` enum | Boundary resolution mode |
| `POLARITY` | `uint8` enum | `uint8` enum | `UNI` or `BI` (affects initialization only) |
| `SCOPE` | `uint8` enum | `uint8` enum | `LOCAL` (step-only) or `TRACK` (global) |

## 4. Trigger & Update

A trigger is a boolean rising edge evaluated at the step clock rate. Valid sources: step advance, pulse/ratchet, external gate, manual trigger, or a conditional boolean.

On trigger, if `DIR != 0`:
```
ACC ← ACC + (INC × DIR)
```

If `DIR == 0`, the register holds its value. `INC` may be negative; `DIR` is multiplicative.

## 5. Boundary Resolution

Evaluated immediately after the update. Modes are identical for both type domains.

| Mode | Discrete Logic | Continuous Logic |
|------|----------------|------------------|
| **WRAP** | `ACC = LIM_LO + (ACC - LIM_HI - 1)` if `ACC > LIM_HI`; mirror for underflow. Wraps to opposite limit. | `ACC = LIM_LO + (ACC - LIM_HI)` if `ACC > LIM_HI`; mirror for underflow. |
| **PENDULUM** | `ACC = LIM_HI - (ACC - LIM_HI)`; `DIR ← -DIR`. Bounces off limit and reverses direction. | Identical, float-safe. |
| **CLIP** / **HOLD** | `ACC = LIM_HI` (or `LIM_LO`). Further triggers in the same direction have no effect until `DIR` reverses. | Identical. |
| **RTZ** | `ACC = 0`. Accumulation continues from zero with original `DIR`. | Identical. |
| **RANDOM** | `ACC = rand(LIM_LO, LIM_HI)`. `DIR` unchanged. | Identical. |
| **ONE-SHOT** | `ACC = breached_limit`; register ignores all further triggers until explicit `RESET`. | Identical. |

**Polarity initialization**:
- `UNI`: `ACC` initializes to `0` (or `LIM_LO` if configured). Accumulates toward `LIM_HI` or `LIM_LO` depending on `DIR`.
- `BI`: `ACC` initializes to `0`; `LIM_HI > 0`, `LIM_LO < 0`. Zero is the center.

## 6. Application to Targets

The register value is never output directly. It is applied through one of four roles:

**A. Offset**
```
TARGET = BASE + ACC
```
- Discrete: `BASE` and `ACC` are both integers; sum is integer. Used for chromatic or diatonic pitch offsets.
- Continuous: `BASE` and `ACC` are floats. Used for timing delay, length, or voltage.

**B. Threshold / Mask**
```
IF ACC > THRESHOLD THEN EVENT_ENABLED = TRUE
```
`ACC` acts as a comparison register. The threshold may be a constant or another accumulator.

**C. Timing Modifier**
```
DELAY_MS = BASE_DELAY_MS + ACC
LENGTH_MS = BASE_LENGTH_MS + ACC
```
Continuous domain only.

**D. Scaling Factor**
```
TARGET = BASE × (1 + ACC)
```
Continuous domain only. `ACC` interpreted as ratio (e.g., `0.5` = 150%).

## 7. Per-Step Override Events

Any step may issue one command to the accumulator without altering other step data:

| Command | Effect |
|---------|--------|
| **DEFEAT** | This step ignores `ACC` for its own target parameter. `ACC` continues to evolve and affects other steps. |
| **ABS_SET** | `ACC ← VALUE`. Overwrites current value. |
| **REL_NUDGE** | `ACC ← ACC + VALUE`. One-time offset; does not alter `INC`. |
| **RESET** | `ACC ← 0` (or configured reset value). |
| **REVERSE** | `DIR ← -DIR`. Flips direction immediately. |
| **SET_LIM_HI** | `LIM_HI ← VALUE` for subsequent steps. |
| **SET_LIM_LO** | `LIM_LO ← VALUE` for subsequent steps. |
| **SET_INC** | `INC ← VALUE` for subsequent steps. |

## 8. Scope

- **LOCAL**: `ACC` modifies only the step instance that owns it. Each step maintains its own accumulator state.
- **TRACK** / **GLOBAL**: A single `ACC` is shared across all steps in the track. A trigger on any step updates the global register; all steps read the same value.

## 9. Reset Behavior

The accumulator may be zeroed or re-initialized by:

- **MANUAL**: Dedicated trigger input or UI command.
- **AUTO**: Reset after `N` triggers or `N` pattern loops.
- **LOOP_BOUNDARY**: Reset at phrase end.
- **SYNC**: External reset signal (e.g., transport start).

## 10. Algorithmic Pseudocode (k-rate, per-step)

```csound
; --- STATE (persists across steps) ---
k_acc     init 0       ; int32 or float32 depending on domain
k_dir     init 1       ; int8: +1, -1, 0
k_inc     init 1       ; int32 or float32
k_lim_hi  init 12      ; int32 or float32
k_lim_lo  init -12     ; int32 or float32
k_mode    init 0       ; 0=WRAP, 1=PENDULUM, 2=CLIP, 3=RTZ, 4=RANDOM, 5=ONE-SHOT

; --- PER-STEP INPUTS ---
k_trigger     = step_clock OR gate_in
k_defeat      = step_defeat_flag
k_reset_cmd   = step_reset_flag
k_abs_val     = step_abs_value      ; or -1 for NULL
k_reverse     = step_reverse_flag
k_nudge       = step_nudge_value    ; 0 if inactive

; --- UPDATE PHASE ---
if k_reset_cmd == 1 then
    k_acc = 0
elseif k_abs_val != -1 then
    k_acc = k_abs_val
elseif k_reverse == 1 then
    k_dir = -k_dir
elseif k_nudge != 0 then
    k_acc = k_acc + k_nudge
elseif k_trigger == 1 && k_dir != 0 then
    k_acc = k_acc + (k_inc * k_dir)
endif

; --- BOUNDARY PHASE ---
if k_acc > k_lim_hi then
    if k_mode == 0 then           ; WRAP
        k_acc = k_lim_lo + (k_acc - k_lim_hi - 1)
    elseif k_mode == 1 then       ; PENDULUM
        k_acc = k_lim_hi - (k_acc - k_lim_hi)
        k_dir = -k_dir
    elseif k_mode == 2 then       ; CLIP
        k_acc = k_lim_hi
    elseif k_mode == 3 then     ; RTZ
        k_acc = 0
    elseif k_mode == 4 then     ; RANDOM
        k_acc = rand(k_lim_lo, k_lim_hi)
    elseif k_mode == 5 then     ; ONE-SHOT
        k_acc = k_lim_hi
        k_dir = 0                ; lock
    endif
elseif k_acc < k_lim_lo then
    ; mirror logic for lower limit
    if k_mode == 0 then
        k_acc = k_lim_hi - (k_lim_lo - k_acc - 1)
    elseif k_mode == 1 then
        k_acc = k_lim_lo + (k_lim_lo - k_acc)
        k_dir = -k_dir
    elseif k_mode == 2 then
        k_acc = k_lim_lo
    elseif k_mode == 3 then
        k_acc = 0
    elseif k_mode == 4 then
        k_acc = rand(k_lim_lo, k_lim_hi)
    elseif k_mode == 5 then
        k_acc = k_lim_lo
        k_dir = 0
    endif
endif

; --- APPLICATION PHASE ---
if k_defeat == 0 then
    if i_domain == 0 then         ; DISCRETE (pitch, velocity index)
        k_out = k_base + k_acc
    else                          ; CONTINUOUS (timing, CV)
        k_out = k_base + k_acc
    endif
endif
```

## 11. Implementation Notes

- **Drift**: Discrete domains must use integer math exclusively. Do not cast semitone counts to float for accumulation and back to int for output.
- **Initialization**: Default `ACC = 0`, `DIR = +1`, `MODE = WRAP`, `POLARITY = BIPOLAR`.
- **Disconnection**: If `ACC` is used purely for threshold logic, do not auto-sum it into the primary output. Maintain two registers (`ACC_LOGIC`, `ACC_OFFSET`) or a disconnect flag.
- **Polyphony**: Each voice may host an independent accumulator instance, or a single global accumulator may modulate all voices. Scope is per-instance.
