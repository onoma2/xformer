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

---

## 12. Reference implementations — Cirklon and Metropolix

Two well-established hardware sequencers ship accumulator features. Their UI vocabulary differs from this spec but the underlying mechanics map cleanly. Used as design reference for what musicians actually want.

### 12.1 Cirklon (Sequentix)

- **Three accumulators per track**: note / velocity / aux D. Each is a constantly-applied offset to its target value. Hidden state.
- **Scope is always TRACK** (track-level state, not per-step state).
- **Per-step event hooks**: any step may carry an aux event in the "Accumulator" group:
  - `offset note abs` — ABS_SET (replaces accumulator with the event's value)
  - `offset note rel` — REL_NUDGE (adds the event's value to the accumulator)
  - same pair for velocity and aux D
- **Order modes** (`accum conf` page): `rtz`, `clip`, `rvtz`, `rvbp`.
  - `rtz` — return to zero on overflow
  - `clip` — stick at limit
  - `rvtz` — reverse on limit, accumulator returns toward zero
  - `rvbp` — reverse on limit, accumulator continues bi-polar (sign flips, keeps moving)
- **Reverse switch** per accumulator — toggled automatically by `rvtz`/`rvbp` on limit hit. Subsequent REL events subtract instead of add. Switch resets when accumulator is cleared.
- **Range limit** is absolute (`limit = N` means `−N..+N`).
- **Reset** at pattern start / new pattern selection. Otherwise the accumulator state persists across pattern loops indefinitely.

### 12.2 Metropolix (Intellijel)

- **Per-stage accumulator state** with sequence-wide configuration on a `LIM` screen.
- **Per-stage fields**:
  - `TRANSPOSE AMOUNT` — step value in scale degrees, ±7 default
  - `TRANSPOSE TRIGGER` — `Stage` / `Pulse` / `Ratch` (when this stage's step value fires into the counter)
- **Sequence-level LIM screen**:
  - **Mode** — `LOCAL` (counter is per-stage; today's PhaseFlux behavior) vs `TRACK` (one shared counter across all stages)
  - **Range limits** — explicit ± bounds
  - **Order / Polarity / Reset** — analogous to §5 modes
- **Bulk operations** — ALT-modifier shortcuts for "set all stages at once", "reset all", "set all triggers to same value".
- **Reset** on sequence RESET (manual or auto) returns all counters to their starting pitches.
- **Interactions** documented across `GATE TYPE` (Hold/Multi/Single/Rest) × `TRANSPOSE TRIGGER` (Stage/Pulse/Ratchet). Worth reading before implementing — there are non-obvious cases (e.g., `Rest` gate type doesn't trigger accumulation; ratchets only contribute under `Ratch` trigger).

### 12.3 Mapping across the three models

| Concept | This spec (§1-§11) | Cirklon | Metropolix |
|---|---|---|---|
| Per-step step value | `INC` | event value (abs/rel) | `TRANSPOSE AMOUNT` |
| Trigger granularity | "rising edge from step clock / pulse / ratchet / external" | every pattern step | per-stage `TRANSPOSE TRIGGER` |
| Scope | `LOCAL` / `TRACK` (§8) | always TRACK | LIM screen `Mode` LOCAL / TRACK |
| Range limit | `LIM_HI` / `LIM_LO` | `limit` (absolute, ±N) | LIM screen ± bounds |
| Overflow modes | `WRAP` / `PENDULUM` / `CLIP` / `RTZ` / `RANDOM` / `ONE-SHOT` | `rtz` / `clip` / `rvtz` / `rvbp` | LIM screen Order |
| Polarity | `UNI` / `BI` (§5) | sign of stored value | LIM screen Polarity |
| Per-step overrides | `DEFEAT` / `ABS_SET` / `REL_NUDGE` / `RESET` / `REVERSE` / `SET_LIM_*` / `SET_INC` (§7) | aux events (abs / rel only) | not exposed |
| Direction reverse | `DIR` register (§3) | reverse switch | derived from sign of `TRANSPOSE AMOUNT` |

Cirklon's strength is the per-step event vocabulary (rich set of one-shot modifiers). Metropolix's strength is the per-stage trigger granularity (Stage/Pulse/Ratchet) and the explicit LOCAL/TRACK scope toggle. This spec generalises both into the §5 / §7 / §8 model.

---

## 13. Application to PhaseFlux

### 13.1 Current state (pre-spec)

PhaseFlux today carries a single, minimal accumulator — per-cell counter array with everything else hardcoded:

| §3 register | PhaseFlux (current) | Source |
|---|---|---|
| `ACC` | `_accumulatorCounter[16]` (int per cell, runtime only) | `engine/PhaseFluxTrackEngine.h` |
| `INC` | per-cell `accumulatorStep` (±15 scale degrees, `SignedValue<5>`) | `model/PhaseFluxSequence.h` |
| `LIM_HI` / `LIM_LO` | derived: `[0, accumulatorLength − 1]` | per-cell `accumulatorLength` 1..16 |
| `DIR` | always `+1` | not configurable |
| `MODE` | always `WRAP` | `(counter + 1) % length` |
| `POLARITY` | always `UNI` | counter ∈ 0..length−1 |
| `SCOPE` | always `LOCAL` | per-cell counter array |
| trigger | cell-completion only | advances in slot-change block |

Application: §6.A `Offset` — `accOffset = counter × stepValue` added into the per-cell degree before `Scale::noteToVolts`.

Reset: cleared on pattern switch; preserved across measure reset (`docs/phaseflux-spec.md` §7.1).

### 13.2 Locked design — dual-accumulator MVP

PhaseFlux gains **two independent accumulators per sequence**: a **Note** accumulator (`N`) targeting scale-degree offset, and a **Pulse** accumulator (`P`) targeting `pulseCount`. They share architecture (config + ops) but have separate state (counters, direction, INC) so their drift periods are independent — Cirklon's "3 accumulators per track" model.

Shared architecture (cross-track):

- `model/AccumulatorConfig.h` — settings struct, per-sequence. Fields: `Scope` / `Order` / `Polarity`. Serialised as one byte per instance.
- `engine/AccumulatorOps.h/.cpp` — stateless free functions for order resolution and §7 override commands. Identical code path for any track that opts in.
- Per-track runtime state (counter or counter array, direction state) stays local to each track's engine. Storage shape varies by scope (single counter for `Track`, 16-counter array for `Local`).

NoteTrack's existing `model/Accumulator.h` class remains untouched in v1. Migration to the shared engine is a follow-up after PhaseFlux is hardware-audible.

### 13.3 Storage layout

**Per-sequence (`PhaseFluxSequence`)** — one `AccumulatorConfig` per accumulator:

| Field | Type | Values | Default |
|---|---|---|---|
| `noteAccumScope` | 1 bit | `Local` / `Track` | `Local` |
| `noteAccumOrder` | 2 bits | `Wrap` / `Pendulum` / `Hold` / `RTZ` | `Wrap` |
| `noteAccumPolarity` | 1 bit | `Uni` / `Bi` | `Uni` |
| `noteAccumReset` | 4 bits | integer 0..15; `0` = manual, `N>0` = auto every N cycles | `0` (manual) |
| `noteAccumPosLim` | 5 bits | 1..28 scale degrees | 7 (1 octave) |
| `noteAccumNegLim` | 5 bits | 1..28 scale degrees | 7 (1 octave) |
| `pulseAccumScope` | 1 bit | `Local` / `Track` | `Local` |
| `pulseAccumOrder` | 2 bits | (same set as note) | `Wrap` |
| `pulseAccumPolarity` | 1 bit | `Uni` / `Bi` | `Uni` |
| `pulseAccumReset` | 4 bits | integer 0..15; `0` = manual, `N>0` = auto every N cycles | `0` (manual) |
| `pulseAccumPosLim` | 4 bits | 1..8 pulses | 8 |
| `pulseAccumNegLim` | 4 bits | 1..8 pulses | 8 |

Total: ~34 bits ≈ 5 bytes for both `AccumulatorConfig`s. Sequence has ~5 KB headroom; trivial.

**Order vocabulary aligned to Metropolix + NoteTrack**: `Hold` replaces the earlier draft's `Clip`. Same semantics (sticks at limit until external reset); naming matches the wider ecosystem.

**Limits are explicit and decoupled from `accumulatorLength`** — Metropolix LIM screen pattern (Positive Limit and Negative Limit, 1..28 each). Earlier draft derived range from `length × step` which forced a coupling; explicit limits give independent control of step magnitude vs reachable range.

**`*AccumReset` semantics in PhaseFlux** — integer field; the "auto" mode is implicit in the value:

- `0` — **Manual.** Counters survive `resetMeasure` loops (Cirklon-style; matches current §7.1 of `docs/phaseflux-spec.md`). Cleared only on **transport restart** or **pattern switch** (hard resets, not configurable).
- `N > 0` — **Auto every N cycles.** Counters reset every N full sequence cycles (one cycle = one complete 16-cell traversal). `N=1` resets every cycle (each loop starts fresh); `N=4` resets every 4 cycles, etc.

Both modes share the hard-reset clear on transport + pattern switch. The integer-instead-of-enum design removes the need for a separate `Manual` vs `Auto` toggle — the value itself carries the policy, and `N` is naturally encoder-editable on the hero page (F5 slot).

`External` (Cirklon's `Aux/Mod` — reset only via a routed input) is deferred to v2 alongside `Routing::Target::AccumReset` infrastructure.

**Per-cell (`Stage`)** — note accumulator keeps its existing step field; pulse accumulator gets a parallel step field. `accumulatorLength` (existing) is **removed** — per-sequence `*AccumPosLim` / `*AccumNegLim` replace it. The 4 bits previously holding `accumulatorLength` (`_data2[14..17]`) are reclaimed.

| Field | Type | Values | Default |
|---|---|---|---|
| `accumulatorStep` (existing) | 5 bits signed | ±15 degrees per trigger | 0 (no drift) |
| `accumulatorTrigger` (new) | 1 bit | `Stage` / `Pulse` | `Stage` |
| `pulseAccumStep` (new) | 4 bits signed | ±7 pulses per trigger | 0 |
| `pulseAccumTrigger` (new) | 1 bit | `Stage` / `Pulse` | `Stage` |

**Bit-pack layout** (locked). `_data2` after the changes:

```
bits  0..2   phaseShift (3)
bits  3..5   mask (3)
bits  6..8   maskShift (3)
bits  9..13  accumulatorStep (5)         [unchanged]
bits 14..17  pulseAccumStep (4)          [NEW — reclaims accumulatorLength]
bits 18..24  gateLength (7)
bits 25..27  stageDivisor (3)
bit  28      skip (1)
bit  29      accumulatorTrigger (1)      [NEW — was spare]
bit  30      pulseAccumTrigger (1)       [NEW — was spare]
bit  31      spare (1)
```

`_data3` is unchanged (stageLen at bits 0..6; bits 7..31 spare). No `Stage` size increase — still 4 × `uint32_t`. 1 spare bit remains in `_data2` for future per-cell additions; 25 spare bits in `_data3`.

Added per cell: 10 bits × 16 stages = 160 bits ≈ 20 bytes. Fits in spare `_data2` / `_data3` without repack — verified against current bit usage in `PhaseFluxSequence::Stage`.

**Engine state** (runtime, not serialised):

```cpp
int   _noteAccumCounter[16];     // per-cell or [0]-shared depending on scope
int8  _noteAccumDir[16];         // direction-flip state for Pendulum (per-counter)
int   _pulseAccumCounter[16];
int8  _pulseAccumDir[16];
```

Cleared on pattern switch (`changePattern()`); preserved across measure reset.

### 13.4 Engine semantics

**Trigger sources:**

- `Stage` trigger: counter advances when the engine moves out of the cell (existing slot-change block).
- `Pulse` trigger: counter advances on each pulse fire inside the per-pulse loop of `rebuildSchedule`. Cells with `pulseCount > 1` produce ramps within a single visit.

**Application:**

- Note accumulator: `noteAccOffset = noteCounter × stage.accumulatorStep`, added into the degree before `Scale::noteToVolts` (existing code path).
- Pulse accumulator: `effectivePulseCount = clamp(stage.pulseCount + pulseCounter × stage.pulseAccumStep, 1, 8)`. Applied at `rebuildSchedule`'s pulse-loop start; clamping is hard (no overflow into next cell).

**Limits** (set per-sequence): `posLim` and `negLim`, each 1..28 scale degrees (note accumulator) or 1..8 pulses (pulse accumulator). Effective counter range depends on Polarity (below).

**Order resolution** (one `AccumulatorOps::applyOrder(counter, dir, polarity, posLim, negLim, order)` call per advance) — behavior depends on **Order × Polarity** (Metropolix LIM screen pattern):

| Order | Polarity | Behavior |
|---|---|---|
| `Wrap` | `Uni` | Counter rises from 0 to `posLim` (or falls from 0 to `−negLim` if step is negative). On overflow, wraps back to **0** and continues in same direction. |
| `Wrap` | `Bi` | Counter ranges `−negLim..+posLim`. On overflow at `+posLim`, wraps to `−negLim` (opposite limit). Same in reverse. |
| `Pendulum` | `Uni` | Counter bounces between **0** and the active-direction limit. On hitting `+posLim`, reverses; counter descends back to 0 and reverses again. (Mirror for negative step.) |
| `Pendulum` | `Bi` | Counter bounces between `+posLim` and `−negLim`. Reverses direction at each limit. |
| `Hold` | either | Counter rises to the active-direction limit and sticks there until reset. Direction never auto-reverses. (Metropolix calls this Hold; NoteTrack also uses Hold; earlier draft of this spec called it Clip — renamed for cross-track alignment.) |
| `RTZ` | either | On overflow at either limit, counter snaps back to 0. Direction unchanged; accumulation continues from 0. |

**Polarity meaning recap:**

- `Uni`: counter is one-sided. Sign matches the active step's direction. Counter ∈ `[0, +posLim]` if stepping positive, `[−negLim, 0]` if stepping negative. The "zero" end is always the stage's reference pitch (no offset applied).
- `Bi`: counter is two-sided. Counter ∈ `[−negLim, +posLim]`. Zero is centred; positive and negative offsets are both reachable regardless of step sign.

**Scope:**

- `Local`: each cell carries its own counter. Cell A's INC + length only affect cell A's counter. 16 counters in play.
- `Track`: single counter (`_*Counter[0]`); each cell's trigger contributes its own INC to that shared counter. All cells read the same counter value when applying the offset.

**Independence of N and P:** the two accumulators run in parallel with no coupling. Note accumulator's counter doesn't influence pulse counter's advance or vice versa. Different drift periods → polymetric pitch-vs-rhythm evolution.

### 13.5 UI surface — hero edit page extension

PhaseFlux's existing `PhaseFluxEditPage` carousel (`TEMP` / `PTCH`) extends to **four sets**: `TEMP` / `PTCH` / `ACCUM.N` / `ACCUM.P`. Left/Right paging cycles. Each ACCUM set has its own slot map and its own strip visualisation; both ACCUM sets share the same dual-strip visual (one is bright, the other dim, depending on which set is currently active).

**Header** (`drawActiveFunction`):

- `ACCUM.N` — Note accumulator set, Local scope
- `ACCUM.NT` — Note accumulator set, Track scope
- `ACCUM.P` — Pulse accumulator set, Local scope
- `ACCUM.PT` — Pulse accumulator set, Track scope

**Slot map (both ACCUM.N and ACCUM.P):**

| Slot | Plain label | Param | Scope | Shift+Fn |
|---|---|---|---|---|
| F1 | `Amount` | active-set's per-cell `*AccumStep` (signed: note ±15 / pulse ±7) | per-cell | **`Trig`** — toggle `*AccumTrigger` Stage ↔ Pulse (per-cell) |
| F2 | `+Lim`   | active-set's `*AccumPosLim` (1..28 / 1..8) | per-sequence | — |
| F3 | `-Lim`   | active-set's `*AccumNegLim` (1..28 / 1..8) | per-sequence | — |
| F4 | `Order`  | active-set's `*AccumOrder` — encoder cycles `Wrap` / `Pend` / `Hold` / `RTZ` | per-sequence | **`Polar`** — toggle `*AccumPolarity` Uni ↔ Bi (per-sequence) |
| F5 | `Reset`  | active-set's `*AccumReset` — integer 0..15; `0` = manual, `N>0` = auto every N cycles | per-sequence | **`Scope`** — toggle `*AccumScope` Local ↔ Track (per-sequence) |

**Footer rows:**
```
plain: AMOUNT | +LIM | -LIM | ORDER | RESET
shift: TRIG   |  —   |  —   | POLAR | SCOPE
```

**Right pane row label swap on Shift held**: F1 row label/value swaps to `Trig` / Stage|Pulse, F4 to `Polar` / Uni|Bi, F5 to `Scope` / Local|Track. F2 and F3 rows unchanged (no shift action). Matches the PTCH set's NOTE→SPAN swap pattern.

**Interaction model**: plain Fn press selects the slot (highlight on the right-pane value column); encoder then edits (continuous slots: Amount, +Lim, -Lim) or cycles (enum slots: Order, Reset). Shift+Fn fires the discrete toggle / cycle for the slot's shift-assigned param (Trig / Polar / Scope) without disturbing the encoder's bound slot.

**`Reset` encoder semantics**: encoder turns step through 0..N. `0` displays as `Manual` (preserve across `resetMeasure`, today's PhaseFlux default). `N>0` displays as `Ncyc` (e.g., `4cyc` = auto-reset every 4 full sequence cycles). Integer storage; the "auto" mode is implicit in `N>0` — no separate enum needed.

Per-cell vs per-sequence write targets: F1 (Amount) and Shift+F1 (Trig) write to the selected cell's stage record. F2 / F3 / F4 / F5 and their shifts write to the sequence's `AccumulatorConfig` for the active set.

**Middle pane — dual-strip visualisation:**

- **Two stacked strips**: top = Note accumulator amounts (`accumulatorStep` per cell, ±15 range); bottom = Pulse accumulator amounts (`pulseAccumStep` per cell, ±7 range).
- **No outer rectangle, no midlines**. Bars float around an invisible vertical centre in each strip.
- **Active layer bright** (Color.MediumBright / Bright). **Inactive layer dim** (Color.Low). The active layer is determined by which ACCUM set is currently displayed (N or P).
- **Stage badge** at top-left of the plot area: shows selected stage index `0..15`, or `T` when the active set is in Track scope. Tiny font, MediumBright.
- **Bar layout**: 16 cells across, grouped 4-4-4-4. Within each group of 4: bar_w = 3 px, 1-px gap. Between groups: 5-px inter-group span with a 1-px **dotted vertical line** (Color.Low) at the centre. Pattern reads as `[][][][] · [][][][] · [][][][] · [][][][]`.
- **Sign**: positive amounts grow upward from the invisible midline; negative grow downward. Zero values render nothing.
- **Selected cell** highlighted via brighter bar color (Color.MediumBright vs Color.Medium for non-selected on the active layer).
- **Active cell** (engine playhead) highlighted via Color.Bright on the active layer's bar.

**Right pane — 5-row param list:**

Mirrors TEMP/PTCH right-pane style. Each row shows `Label` (left, Color.Medium) + value (right, Color.MediumBright; Color.Bright when selected). Selected row gets a 1/3-width value-column outline rect (matching the C++ highlight pattern).

Per-cell rows (F1/F2/F4) display the **active stage's** value. Per-sequence rows (F3/F5/Shift+F5) display the sequence-level value.

**Render reference**: `ui-preview/phaseflux-accum/` — `dual-stacked-N-active.png`, `dual-stacked-P-active.png`.

### 13.6 Cross-track architecture notes

PhaseFlux's `AccumulatorConfig` + `AccumulatorOps` are designed for cross-track reuse. NoteTrack's existing `model/Accumulator.h` can migrate to the shared engine in a follow-up:

- NoteTrack's existing per-step "trigger flag" → `*AccumTrigger = Pulse` (per-step) in the shared model.
- NoteTrack always operates at `Scope = Track` (single counter); the shared engine's `Track` branch covers it.
- NoteTrack's existing `Order = Random` and `Direction = Down/Freeze` are out of MVP scope for PhaseFlux but planned features for the shared `AccumulatorOps` later. They can be ported when NoteTrack migrates.

Until that migration, NoteTrack continues to use its existing class unchanged; PhaseFlux uses the new shared engine alone.

### 13.7 Deferred to a later pass

- **Per-step override commands** (§7): `DEFEAT` / `ABS_SET` / `REL_NUDGE` / `RESET` / `REVERSE` / `SET_LIM_HI` / `SET_LIM_LO` / `SET_INC`. UI cost is significant — no spare slots on the hero page.
- **Order mode `Random`** — Metropolix drunken-walk algorithm. Specified here for precision when added; not implemented in v1.

  ```
  on trigger (Random order):
      r = rand_uniform(0, 100)
      if r < 50:           // 50% — move in step direction
          distance = rand_int(1, |step|)
          counter += distance × sign(step)
      elif r < 80:         // 30% — move opposite direction
          distance = rand_int(1, |step|)
          counter -= distance × sign(step)
      else:                // 20% — don't accumulate this trigger
          (no change)
      apply boundary using current polarity rules
  ```

  Bias toward step direction (50%) means the counter "generally trends" in the configured direction while still wandering. Counter constrained by `posLim` / `negLim` per polarity rules. The "drunken walk" character emerges from the weighted random choice + variable distance.

- **Order mode `OneShot`** — fires once, then locks counter until external reset. Niche for a grid sequencer; defer.
- **Cirklon `rvtz` / `rvbp` reverse-on-limit variants** distinct from Pendulum. Distinct only in post-flip target (zero vs opposite limit); already covered by `Pendulum × Uni` vs `Pendulum × Bi` in the table above — so likely no new Order value needed. Verify in audition.
- **`accumulatorReset = External`** — Cirklon's `Aux/Mod`-only reset. Requires a new `Routing::Target::AccumReset` destination; defer until per-track routing surface lands.
- **CTRL knob routing for accumulator config fields** — Metropolix exposes `Accum Mode`, `Accum Polar`, `Accum PosLim`, `Accum NegLim` as routable destinations. Reserve slots in `Routing::Target` planning; not implemented in v1.
- **NoteTrack migration** to the shared `AccumulatorConfig` + `AccumulatorOps`. After PhaseFlux is verified.
- **Per-step Abs vs Rel mode** (Cirklon's `offset note abs` / `offset note rel` events). Useful as branch points but adds 1 bit per cell and requires a UI surface decision. Defer.

### 13.8 Locked rules — counter decoupled from output

A single universal rule resolves the three earlier open questions:

> **The counter is decoupled from output.** Counter ticks on its configured trigger event (Stage completion or Pulse fire) regardless of whether the downstream application produces audible output. Output saturation, clamping, or other post-counter handling does not feed back into counter state.

Applied to the three questions:

- **Reset on `pitchMode` switch** — counters **preserve**. The pitch curve source is orthogonal to the accumulator's drift state; flipping Cell ↔ Global does not reset counters.
- **`Pulse` trigger with `skip = true`** — counter follows trigger events. Skip is implemented by removing the cell from the cumulative tick table → no Stage or Pulse trigger events fire for that cell → counter doesn't tick. The cell is *absent*, not *muted*. Same outcome as if it were never configured to drift.
- **Pulse clamp at `pulseCount` boundaries (1 or 8)** — counter ticks. `clamp(stage.pulseCount + offset, 1, 8)` clips the *applied value*; the counter has already incremented by the trigger event and doesn't see whether the result was clipped. Symmetrically: Note accumulator's counter ticks even when the resulting degree saturates `scale.noteToVolts()`.

The "decoupled" rule simplifies `AccumulatorOps`: the engine never has to look at downstream application outcomes to decide whether to advance. Counter advance is a pure function of (current counter, direction, config, trigger event).

### 13.9 Boundary resolution — reuse the NoteTrack implementation

The four Order resolutions (Wrap / Pendulum / Hold / Random) are **already implemented and shipping** in `src/apps/sequencer/model/Accumulator.cpp`. Extract these as free functions for `AccumulatorOps`; don't re-derive.

| Order | Existing function | Lines | Notes |
|---|---|---|---|
| Wrap | `Accumulator::tickWithWrap()` | 58-70 | Direction-aware (Up/Down). Uses `_minValue` / `_maxValue` directly. |
| Pendulum | `Accumulator::tickWithPendulum()` | 72-86 | Tracks `_pendulumDirection` (runtime ±1), reverses at min/max. Clamps to exactly min/max on hit. |
| Hold | `Accumulator::tickWithHold()` | 104-118 | Direction-aware, clips at limit. |
| Random | `Accumulator::tickWithRandom()` | 88-102 | Uniform random in `[min, max]`. NB: this is NOT Metropolix's weighted drunken walk (§13.7); if drunken walk is wanted, that's a separate decision. For v1, NoteTrack's uniform Random is the shipped algorithm — keep it consistent. |

**Critical architecture detail:** the existing class operates on **absolute `_minValue` / `_maxValue`**, not polarity-derived bounds. `_polarity` is set at config time and used in `reset()` for the initial value — it never appears in the tick functions themselves. The polarity-vs-step-sign mapping happens at the **caller** (which sets min and max explicitly).

This means `AccumulatorOps` should take explicit `min` and `max` as parameters:

```cpp
// Free functions in engine/AccumulatorOps.h, extracted from Accumulator.cpp.
// Each takes the counter and runtime direction by reference, mutates in place.
// `min` and `max` are absolute bounds — caller computes them per polarity.
void tickWrap     (int& counter, int direction, int min, int max, int step);
void tickPendulum (int& counter, int& pendulumDir, int min, int max, int step);
void tickHold     (int& counter, int direction, int min, int max, int step);
void tickRandom   (int& counter, int min, int max);  // direction ignored
```

Engine-side polarity-to-bounds mapping (called by `PhaseFluxTrackEngine` before each tick):

```cpp
int absMin, absMax;
if (cfg.polarity == Uni) {
    // One-sided: bounds depend on configured step sign.
    if (stage.step > 0) { absMin = 0;           absMax = +cfg.posLim; }
    else                { absMin = -cfg.negLim; absMax = 0;            }
} else {
    // Two-sided: bounds span both limits regardless of step sign.
    absMin = -cfg.negLim;
    absMax = +cfg.posLim;
}
// Then call the appropriate tick function.
```

This caller-side decomposition resolves the Pendulum+Uni zero-crossing concern automatically: when `step > 0` in Uni mode, `absMin = 0`, and the existing `tickWithPendulum` clamps to `_minValue` (= 0) on the downward bounce. Pendulum reflects at 0 going back up. No edge case to fix in the ops layer.

**Direction state:** the existing class uses `_direction` (config: Up/Down/Freeze) and `_pendulumDirection` (runtime: ±1). For PhaseFlux, we don't need a separate Direction config — the sign of `stage.step` carries the direction. `Freeze` is implicit when `step == 0` (no advance happens). So the engine passes `direction = step > 0 ? +1 : -1` to the tick functions, and tracks `pendulumDir` per-counter as runtime state.

**Pendulum signed-step semantics — divergence from NoteTrack:** PhaseFlux passes the *signed* `step` (not its magnitude) to `tickPendulum`, so `counter += step * pendulumDir` honors the cell's configured drift sign on the first move. NoteTrack uses unsigned step × a separate Direction enum; PhaseFlux folds the direction into the step sign. Without this, a Uni cell with negative step (bounds `[-negLim, 0]`, initial `pendulumDir = +1`) would advance up out of range on tick 1, clamp back to 0, then flip — producing a degenerate one-tick stall. A Bi cell with negative step would ascend first instead of descending. With signed step, both cases drift in the configured direction immediately.

**RTZ Order (the one NoteTrack doesn't have):** add a new free function:

```cpp
// On overflow at either bound, snap to zero. Direction unchanged.
// Used only when counter is already out of range.
void tickRTZ(int& counter, int min, int max) {
    if (counter > max || counter < min) {
        counter = 0;
    }
}
```

Engine integration: after advancing the counter by `step × direction`, the engine dispatches on `cfg.order` to call the right tick function (or for RTZ, the range check above).

For implementer guidance: extract the four NoteTrack tick functions VERBATIM into `engine/AccumulatorOps.cpp`, parameterising on `min` / `max` / `step` / `direction` instead of class members. Same algorithm, same boundary math. Add `tickRTZ` as a new fifth function. Unit tests (§13.11) compare new free functions' outputs against equivalent direct-class outputs — a passing test means the extraction was faithful.

### 13.10 Implementation tasks — for subagent-driven dispatch

Each task is independently dispatchable. Order matters where dependencies are listed; otherwise tasks can fan out. Each task is sized so a single subagent can implement-and-test it in one pass.

**Task 1: `AccumulatorConfig` struct + roundtrip test**
- *Files*: new `src/apps/sequencer/model/AccumulatorConfig.h` + `.cpp`
- *Deliverable*: struct with fields `scope` (1 bit), `order` (2 bits), `polarity` (1 bit), `reset` (4 bits), `posLim` (u8), `negLim` (u8). Enums: `Scope { Local, Track }`, `Order { Wrap, Pendulum, Hold, RTZ }`, `Polarity { Uni, Bi }`. Default ctor initializes to `Local` / `Wrap` / `Uni` / 0 / 7 / 7. Read/write serialization methods.
- *Tests*: `TestAccumulatorConfig.cpp` — roundtrip of every field value, default-state roundtrip, edge values (max posLim, max reset, etc.).
- *Depends on*: nothing.

**Task 2: `AccumulatorOps` free functions extracted from NoteTrack + per-case unit tests**
- *Files*: new `src/apps/sequencer/engine/AccumulatorOps.h` + `.cpp`
- *Deliverable*: extract `tickWithWrap` / `tickWithPendulum` / `tickWithHold` / `tickWithRandom` (currently `Accumulator.cpp:58-118`) as free functions `tickWrap` / `tickPendulum` / `tickHold` / `tickRandom`, parameterised on `(counter&, direction, min, max, step)` instead of class members. Add new function `tickRTZ(counter&, min, max)` for the order absent from NoteTrack. Pure functions, no class state.
- *Tests*: `TestAccumulatorOps.cpp` — at minimum one test per (Order × overflow-direction × polarity-bounds) combination per §13.4. Faithfulness tests: run each new free function and the equivalent NoteTrack class method on identical inputs and verify equal output.
- *Depends on*: Task 1 (uses the enum types).

**Task 3: `PhaseFluxSequence` per-sequence config fields**
- *Files*: `src/apps/sequencer/model/PhaseFluxSequence.h/.cpp`
- *Deliverable*: two `AccumulatorConfig` members on `PhaseFluxSequence`: `_noteAccumConfig` and `_pulseAccumConfig`. Accessors + serialization extension. `clear()` resets both to defaults.
- *Tests*: extend `TestPhaseFluxSequenceSerialization.cpp` — non-default values for both configs survive write→read.
- *Depends on*: Task 1.

**Task 4: `PhaseFluxSequence::Stage` per-cell field changes (bit-pack rework)**
- *Files*: `src/apps/sequencer/model/PhaseFluxSequence.h/.cpp`
- *Deliverable*: Remove `accumulatorLength` field (reclaim `_data2[14..17]`). Add `pulseAccumStep` (4 bits signed, `_data2[14..17]`), `accumulatorTrigger` (1 bit, `_data2[29]`), `pulseAccumTrigger` (1 bit, `_data2[30]`). Update `Stage::clear()` defaults. Layout must match §13.3.
- *Tests*: extend `TestPhaseFluxSequenceSerialization.cpp` — non-default values for new fields survive roundtrip. `TestPhaseFluxSize.cpp` confirms `Stage` is still 16 bytes. Bit-pack non-overlap test (each field reads back the value it was written, with no collision).
- *Depends on*: nothing (parallel to Task 3).

**Task 5: Engine state arrays + cycle counter**
- *Files*: `src/apps/sequencer/engine/PhaseFluxTrackEngine.h/.cpp`
- *Deliverable*: add `_noteAccumCounter[16]`, `_noteAccumDir[16]`, `_pulseAccumCounter[16]`, `_pulseAccumDir[16]`, `_cycleCount` (uint32_t — counts completed full sequence cycles since last reset). Initialise in `reset()`. Detect cycle wrap in `tick()` (when `relativeTick` wraps `_cycleTicks`). Update `changePattern()` and the resetMeasure block per §13.4 reset semantics. **No application yet** — counters just maintain state.
- *Tests*: extend `TestPhaseFluxTrackEngine.cpp` — counter starts at 0, advances on cell-completion when `accumulatorTrigger = Stage` and step ≠ 0, respects Order (use `applyOrder`), respects per-sequence reset policy (`reset = 0` preserves across `resetMeasure`; `reset = N>0` clears every N cycles).
- *Depends on*: Tasks 2, 3, 4.

**Task 6: Wire Note accumulator into pitch pipeline**
- *Files*: `src/apps/sequencer/engine/PhaseFluxTrackEngine.cpp`
- *Deliverable*: In `rebuildSchedule()` and the Always-mode CV branch, compute `noteAccOffset = noteCounter × stage.accumulatorStep` (Local scope) or `noteAccOffset = sharedCounter × stage.accumulatorStep` (Track scope). Add to `baseDegree` before scale-quantization. Same code path for both Cell and Global pitchMode (per §13.8 decoupling).
- *Tests*: extend `TestPhaseFluxTrackEngine.cpp` — set up a stage with `accumulatorStep = +1`, scope=Local, order=Wrap, posLim=7. Advance through several cycles. Verify produced CV reflects degree drift `0, +1, +2, ..., +7, 0, +1, ...`.
- *Depends on*: Task 5.

**Task 7: Wire Pulse accumulator into pulse-count pipeline**
- *Files*: `src/apps/sequencer/engine/PhaseFluxTrackEngine.cpp`
- *Deliverable*: In `rebuildSchedule()`, compute `effectivePulseCount = clamp(stage.pulseCount + pulseAccumOffset, 1, 8)`. Use `effectivePulseCount` in the per-pulse loop instead of `stage.pulseCount()`. Trigger advance per `pulseAccumTrigger`: at cell start (Stage) or per pulse fire (Pulse).
- *Tests*: extend `TestPhaseFluxTrackEngine.cpp` — set up a stage with `pulseCount = 1`, `pulseAccumStep = +1`, `pulseAccumTrigger = Stage`. Across cycles, verify cell fires 1, 2, 3, ..., 8 pulses then wraps per Order setting.
- *Depends on*: Task 5.

**Task 8: ACCUM.N + ACCUM.P sets in `PhaseFluxEditPage`**
- *Files*: `src/apps/sequencer/ui/pages/PhaseFluxEditPage.h/.cpp`
- *Deliverable*: extend `_currentSet` carousel from 2 (TEMP / PTCH) to 4 (TEMP / PTCH / ACCUM.N / ACCUM.P). Header label switching (`ACCUM.N` / `ACCUM.NT` / `ACCUM.P` / `ACCUM.PT`). Footer labels per §13.5 slot map. F1-F5 plain + Shift+F1/F4/F5 actions wired per §13.5. Encoder edits selected slot's value.
- *Tests*: none directly (UI). Manual verification via simulator.
- *Depends on*: Tasks 3, 4.

**Task 9: Dual-strip visualisation in middle pane**
- *Files*: `src/apps/sequencer/ui/pages/PhaseFluxEditPage.cpp`
- *Deliverable*: draw `drawAccumDualStrip()` matching `ui-preview/pages_phaseflux_hero_accum.py` — two stacked strips, grouped 4-4-4-4 layout (bar_w=3, gap_within=1, inter_group=5 with dotted line at center, Color.Low), no midlines, active layer bright / inactive dim, stage badge top-left (`5` or `T` for Track scope).
- *Tests*: none directly (UI). Manual verification.
- *Depends on*: Task 8.

**Task 10: Default project demo + spec compliance audition**
- *Files*: `src/apps/sequencer/model/Project.cpp`
- *Deliverable*: track 8 PhaseFlux sequence gets non-zero `accumulatorStep` on a few stages (e.g., stage 0 = +2, stage 4 = -1) and demonstrative `pulseAccumStep` values. Order = Wrap, Polarity = Uni, posLim = 7, Reset = 4 (auto every 4 cycles). Audible drift verified by ear in simulator.
- *Tests*: none — audition is the test.
- *Depends on*: Tasks 6, 7, 8, 9.

### 13.11 Test harness — concrete test names

For each task, the unit tests to write before the implementation (TDD per CLAUDE.md). Names are deliberately verbose so failures are self-explanatory.

**`TestAccumulatorConfig.cpp`** (Task 1):
- `defaults_match_spec` — `Scope=Local, Order=Wrap, Polarity=Uni, reset=0, posLim=7, negLim=7`
- `every_field_roundtrips` — set each field to a non-default value, write+read, verify
- `reset_field_handles_max_15` — `reset=15` roundtrips
- `pos_lim_handles_max_28` — note variant; same for pulse variant up to 8

**`TestAccumulatorOps.cpp`** (Task 2):

Per-function tests against bounds (caller-supplied `min` and `max`, matching how the engine drives the ops):

- `tick_wrap_overflow_returns_to_min_when_direction_up`
- `tick_wrap_underflow_returns_to_max_when_direction_down`
- `tick_pendulum_overflow_reflects_at_max_and_flips_dir`
- `tick_pendulum_underflow_reflects_at_min_and_flips_dir`
- `tick_pendulum_at_zero_min_descends_then_reflects` — Pendulum+Uni positive-step case (caller sets `min=0`)
- `tick_hold_overflow_pins_at_max`
- `tick_hold_underflow_pins_at_min`
- `tick_random_stays_in_range`
- `tick_rtz_overflow_snaps_to_zero`
- `tick_rtz_underflow_snaps_to_zero`

Faithfulness tests — verify extracted free functions produce identical output to the existing NoteTrack `Accumulator` class methods on equivalent inputs:
- `extracted_wrap_matches_notetrack_wrap`
- `extracted_pendulum_matches_notetrack_pendulum`
- `extracted_hold_matches_notetrack_hold`

Edge cases:
- `pendulum_after_flip_descends_correctly_until_next_limit`
- `wrap_step_of_3_handles_overshoot_correctly`
- `pendulum_clamp_to_exact_min_max_on_hit`  — existing `tickWithPendulum` clamps to exactly `_minValue`/`_maxValue`; verify the extracted version preserves this

**`TestPhaseFluxSequenceSerialization.cpp`** (Tasks 3, 4 — extends existing file):
- `note_accum_config_roundtrips_with_non_defaults`
- `pulse_accum_config_roundtrips_with_non_defaults`
- `pulse_accum_step_per_cell_roundtrips`
- `accumulator_trigger_per_cell_roundtrips`
- `pulse_accum_trigger_per_cell_roundtrips`
- `bit_pack_no_collision` — write each per-cell field to a distinct value, read all back, none was corrupted by another

**`TestPhaseFluxSize.cpp`** (Task 4 — extends existing file):
- `stage_record_remains_16_bytes_after_accumulator_v2`

**`TestPhaseFluxTrackEngine.cpp`** (Tasks 5, 6, 7 — extends existing file):
- `note_counter_advances_on_cell_completion_when_step_nonzero`
- `note_counter_holds_when_step_is_zero`
- `note_counter_respects_wrap_uni_order`
- `note_counter_respects_pendulum_bi_order`
- `note_counter_resets_to_zero_when_reset_equals_1_per_cycle`
- `note_counter_preserves_through_reset_measure_when_reset_equals_0`
- `note_counter_clears_on_pattern_switch`
- `note_counter_track_scope_shares_single_counter_across_cells`
- `pulse_counter_clamps_to_1_at_lower_bound_but_counter_continues`
- `pulse_counter_clamps_to_8_at_upper_bound_but_counter_continues`
- `skip_cell_does_not_advance_counter_under_either_trigger`
- `pitch_pipeline_adds_note_accumulator_offset_to_degree`
- `pulse_pipeline_uses_effective_pulse_count_from_clamp`

**Audible / UI verification (Tasks 8, 9, 10)**: no unit tests. Manual checklist:
- ACCUM.N selected → F1 highlight, encoder edits selected stage's `accumulatorStep`
- Shift+F4 toggles Order display through Wrap → Pend → Hold → RTZ
- Shift+F5 toggles SCOPE (Local ↔ Track), header marker swaps `.N` → `.NT`
- Strip shows 16 bars in 4-grouped layout, dotted lines at group seams, active layer bright
- Project demo on track 8 produces audible degree drift over a few cycles
- Setting Reset=4 in the demo: drift resets every 4 cycles, audibly verifiable