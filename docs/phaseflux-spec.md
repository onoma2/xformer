# PhaseFlux Track — Spec (v0.2)

Monophonic grid sequencer as a new Performer TrackMode. MVP was locked from a grilling session against `retro/stochastic-retro.md` lessons; v0.2 reopens the transform pipeline order (below). Mutation is post-MVP.

## v0.2 amendment — pipeline reorder (supersedes MVP order)

The per-axis transform pipeline is reordered to wake dead controls:

**New order:** `Warp → Repeat → Window → FlipH → Curve → Response → FlipV` (pitch). For time, FlipH stays the post-schedule reversal (last), as before.

- **Warp before Repeat** — warp bends the whole-cell position; Repeat then tiles the *warped* phase, so the N copies become uneven/progressive instead of identical. Temporal: `evalTemporalAlloc` seeds each pulse at `i/pulseCount`, warps it, then splits by R → warped sub-section allocation (bar-level rubato). `warp=0` is globally even and matches the old split for divisible pulseCount.
- **Window after Repeat** — window now acts per repeated cycle.
- **Response before FlipV** — puts a nonlinearity between FlipH and FlipV so they are independent on Ramp/Bounce (were redundant under the MVP order).

**Implementation:** the phase chain (`PhaseFluxMath::evalPitchPhase`), value tail (`evalResponseFlipV`), and temporal allocation (`evalTemporalAlloc`) are unit-tested `PhaseFluxMath` functions; the engine (gate, continuous, preview) delegates to them so preview can never diverge from playback. `PhaseFluxMath::evalWindowRepeat` is superseded and unused by the engine (cleanup candidate). UI footer slot order (§17) is unchanged — it no longer mirrors the compute order. Older prose in §6.1/§6.2/§14.2 describing `Window → Repeat → Warp` is superseded by this section.

---

## 1. Class shape

New TrackMode trio, peer to Note / Curve / Stochastic / Tuesday:

- `PhaseFluxTrack` (model)
- `PhaseFluxSequence` (model, one per pattern slot)
- `PhaseFluxTrackEngine` (engine)

No NoteSequence::Mode extension. No CurveTrack inheritance. Curve shape LUTs are *shared* from CurveTrack's existing infrastructure but the model/engine code is its own.

---

## 2. Grid & traversal

- **Grid**: 4×4 = 16 stages.
- **Visitation order**: **boustrophedon snake** — row 0 L→R, row 1 R→L, row 2 L→R, row 3 R→L, wrap to row 0. Encoded as a static 16-entry permutation `snakeOrder[]`. No "direction bit" state — the snake is the permutation.
- **Active cell**: derived from current tick (see §3). No accumulated cursor state.
- **Skip**: per-cell `skip` bool. Skipped cells contribute **0 ticks** to the cumulative duration table, making them transparent to the timing math — no separate seek-and-retry pass needed.
- **MVP Y axis**: not independent. Snake-walk handles Y implicitly (X wrap → next row). Independent Y clock deferred to v2.

---

## 3. Timing model — stateless phase, DiscreteMap-style

The engine borrows DiscreteMap's stateless-ramp pattern (`src/apps/sequencer/engine/DiscreteMapTrackEngine.cpp`, the `_rampPhase / _resetTickOffset` pair). Active cell index and intra-stage phase are computed as a pure function of `relativeTick` every tick — not advanced as accumulated state.

**Per-stage `stageDivisor`**: discrete musical divisions, Performer's existing divisor primitive (1/32, 1/16, 1/8, 1/4, 1/2, 1 bar). Reuses `ModelUtils::clampDivisor` and divisor-knob UI.

### 3.1 Cumulative duration table

Recomputed in the engine whenever any `stageDivisor` or `skip` flag changes (16 additions; nanoseconds on Cortex-M; not per-tick):

```
for i in 0..15:
  cell                 = snakeOrder[i]
  durationTicks[i]     = skip[cell] ? 0 : (stageDivisor[cell] × measureDivisor / clockMultiplier)
  cumulativeTicks[i+1] = cumulativeTicks[i] + durationTicks[i]
cycleTicks = cumulativeTicks[16]

// Degenerate-case guards (see §3.5)
kMinCycleTicks = (1/32) × measureDivisor       // ~24 ticks at standard PPQN
if cycleTicks == 0:
  enter_idle_state()                            // all 16 cells skipped — see §3.5
else if cycleTicks < kMinCycleTicks:
  scaleFactor          = kMinCycleTicks / cycleTicks
  for i in 0..16:
    cumulativeTicks[i] *= scaleFactor          // proportional stretch — preserves relative cell weights
    durationTicks[i]   *= scaleFactor          // (i in 0..15)
  cycleTicks           = cumulativeTicks[16]
```

### 3.2 Per-tick derivation

```
relativeTick   = tick − _resetTickOffset                     // integer uint32_t
cycleTick      = relativeTick mod cycleTicks
slotIdx        = binary_search(cumulativeTicks, cycleTick)   // 0..15 (snake slot)
activeCell     = snakeOrder[slotIdx]
posInStage     = cycleTick − cumulativeTicks[slotIdx]
stagePhase     = posInStage / durationTicks[slotIdx]         // 0..1, float for curve eval only
```

**Integer tick math throughout** — no `Clock::tickPosition()` (the `double` sub-tick API DiscreteMap uses for its Free-mode ramp). PhaseFlux events (pulse-fire, slot change, gate timer) are all discrete tick-aligned; sub-tick precision would buy nothing audible. `stagePhase` becomes a float only at the moment of curve evaluation per pulse, not as carried engine state. Pairs cleanly with the recipe-replay identity model (§4) — derivation is a pure integer function of `relativeTick`.

Pulses (sub-triggers) fire at temporal-curve-warped positions inside `stagePhase` (see §6). Stage duration tracks cursor by construction — **no mid-ramp leave is possible** — so spec Q1/Q2/Q4 dissolve.

### 3.3 Reset Measure — verbatim DiscreteMap

```
resetDivisor = resetMeasure × measureDivisor                 // 0 ⇒ off

if resetDivisor > 0 && relativeTick mod resetDivisor == 0:
  _resetTickOffset = tick
  // accumulator counters preserved across reset (locked decision §7)
  // pulse-fire edge memory cleared
  // gate timer reset
```

The math falls out — no separate "reset cursor to (0,0)" step, since the cursor is derived from `relativeTick`. After reset, `relativeTick = 0` → `cycleTick = 0` → `slotIdx = 0` → `activeCell = snakeOrder[0]` → stage (0,0).

Sequence-level `resetMeasure` (0..128, NoteSequence parity). Default = 0 (off); phase-lock is opt-in.

### 3.4 Knob-turn behavior — cumulative-table recompute

Editing `stageDivisor` or `skip` on a live cell triggers a cumulative-table recompute. Active cell and phase jump to whatever position the new layout dictates given the current `relativeTick`. Consistent with DiscreteMap's existing behavior — and means there's no "pending edit, applies next cycle" semantics to maintain.

**On every cumulative-table recompute, the engine must also clear:**
- **`gateTimer = 0`** — any held gate from the previous layout ends immediately. Prevents a stuck gate held by a cell the player just marked `skip=true`.
- **`pulseFired[8] = 0`** — pulse-fire edge memory cleared so the new layout is re-evaluated cleanly. Prevents the case where an edit lands the cursor back into the *same* slotIdx at a later `posInStage`, retaining stale `pulseFired` bits and silently swallowing pulses 0..k.
- `_resetTickOffset` is **NOT** touched — `relativeTick` continues, only the cumulative table changes.

Accumulator counters are **NOT** cleared on knob-turn recompute (they're persistent drift state — see §7).

### 3.5 Idle state — all cells skipped

When all 16 cells have `skip=true`, `cycleTicks` would compute to 0 and the §3.2 modulo would be undefined. The engine handles this as an explicit idle state, not an error:

- Gate held **low** (no pulses fire)
- CV **freezes** at last value (regardless of `_cvUpdateMode`)
- `accumulatorCounter[16]` **frozen** — counters do not advance while idle
- `_resetTickOffset` preserved — when an unskip edit re-establishes `cycleTicks > 0`, cumulative-table recompute runs (§3.4) and engine resumes; `relativeTick = tick − _resetTickOffset` continues from wherever it had drifted
- Idle state is **distinct from mute** — `_lock`/mute/solo are independent; idle is "no active cells exist in the layout"

The `cycleTicks < kMinCycleTicks` floor in §3.1 (clamped to one 1/32-note minimum, ~24 ticks at standard PPQN) is the second guard — it prevents the cycle from collapsing to audio-rate when most cells are skipped but at least one survives. The floor scales all cell durations proportionally so musical proportions are preserved at the minimum size.

### 3.6 Skip behavior — `cycleLength: Adaptive | Fixed`

Per-sequence `cycleLength` field on `PhaseFluxSequence` picks how `skip` participates in the cumulative table:

- **`Adaptive`** (legacy): `raw = skip[cell] ? 0 : stageDivisorTicksArr[cell]`. Skipped stages contribute zero ticks to the cycle. Toggling skip resizes the cycle and shifts the playback rate of surviving stages against the external clock. Useful for live "drop a stage to tighten the loop" gestures.
- **`Fixed`** (default for new sequences): `raw = stageDivisorTicksArr[cell]` unconditionally. Skipped stages keep their slot in the cycle but emit silence during it (`rebuildSchedule` early-returns with `scheduleCount = 0`; accumulators stay frozen for that slot — see §7.1). Toggling skip mutes a region without rearranging neighbor timing. Matches Note Track's gate-off-keeps-cycle-length precedent — only convention that exists elsewhere in the codebase.

Default policy: existing serialized sequences load as `Adaptive` (preserves prior playback exactly); `clear()` initializes new sequences to `Fixed`. Storage uses one spare bit in the `_pitchRate` serialization byte (bit 5) — old files have bit 5 == 0 → Adaptive; no `ProjectVersion` bump.

---

## 4. Identity / storage model

- **Stages**: 16 stored records on `PhaseFluxSequence`. Persisted.
- **Cursor + intra-stage phase**: **not stored**. Derived from `relativeTick` every tick (§3.2).
- **Pulse schedule + CV**: recipe-replayed live from curves on every engine tick. No event array materialised.

**Engine state** (transient):
- `_resetTickOffset` (uint32_t) — engine's local time-zero
- `cumulativeTicks[17]` (derived; recomputed on edit) — snake-walk durations
- `accumulatorCounter[16]` — per-cell drift state, indexed by cell ID (0..15), increments on slot change (§7)
- `gateTimer` — held-gate duration after pulse fire (cleared on cumulative-table recompute §3.4)
- `pulseFired[8]` bitmask — which pulses already fired in current stage visit (cleared on slot change AND on cumulative-table recompute §3.4)
- `prevSlotIdx` — last tick's slot, for slot-change detection

Knob turns reshape output instantly because the curve recipe and cumulative table are re-evaluated immediately. No cache invalidation footgun.

---

## 5. Per-stage record

PhaseFlux pitch is **scale-degree based, NoteTrack-aligned**. `basePitch` is an integer scale degree; voltage emerges only at output time via `Scale::noteToVolts()`. Spec previously had pitch in volts — that was pre-Performer-convention design, replaced here.

18 stored params + 1 bool = 19 fields per cell × 16 cells, packed into 3 × `uint32_t` (12 B/stage):

| Field | Range | Bits | Notes |
|---|---|---|---|
| `pulseCount` | 0..16 | 5 | `UnsignedValue<5>` in `_data3[17..21]`. **0 = silent stage (duration consumed, no events fired)**; 1..16 = N pulses per stage. Engine clamps `pulseCount + accumOffset` to `[0, kMaxPulses=16]`. |
| `basePitch` | ±63 scale degrees | 7 | `SignedValue<7>`. Integer scale degree, not volts. NoteTrack-uniform. |
| `pitchRange` | 0.5 / 1 / 2 / 3 octaves | 2 | enum of the active scale's octave width (`notesPerOctave` × value) |
| `pitchDirection` | Up / Down / Bipolar | 2 | direction of sweep relative to basePitch (1 spare value) |
| `temporalCurve` | enum 1 of 3 | 2 | Base shape: Linear / Bell / Bounce (1 spare slot). **Exp / Log dropped** — reachable via `temporalWarp` on Linear (warp ±0.5 ≈ Exp/Log). |
| `temporalFlipV` | bool | 1 | Vertical flip (output `1 − y`). Linear+vflip = reverse trigger order. |
| `temporalFlipH` | bool | 1 | Horizontal flip (input `1 − x`). For symmetric base shapes (Bell), no audible effect; for asymmetric (Bounce), reverses the bounce direction. |
| `temporalWarp` | −0.984 .. +0.984 | 7 | `SignedValue<7>` mapped via `/64` (see §6 PowerBend safety). PowerBend axis warp on input. |
| `temporalResponse` | −0.984 .. +0.984 | 7 | `SignedValue<7>`. PowerBend axis response on output. |
| `pitchCurve` | enum 1 of 3 | 2 | Base shape: Ramp / Bell / Triangle (1 spare slot). **Mirror dropped** — reachable as Bell+vflip (smooth V) or Triangle+vflip (sharp V). |
| `pitchFlipV` | bool | 1 | Vertical flip (output `1 − y`). Bell+vflip = Mirror. Ramp+vflip = RampDown. |
| `pitchFlipH` | bool | 1 | Horizontal flip (input `1 − x`). For Bell + Triangle (symmetric), no effect; for Ramp, same as vflip. |
| `pitchWarp` | −0.984 .. +0.984 | 7 | `SignedValue<7>`. PowerBend |
| `pitchResponse` | −0.984 .. +0.984 | 7 | `SignedValue<7>`. PowerBend |
| `maskMelody` | 0..100 | 7 | `UnsignedValue<7>`. Centrality threshold; 100 = bypass. Borrowed from Stochastic MaskM. |
| `tiltMelody` | 0..100 | 7 | `UnsignedValue<7>`. Tilt — 0 = tonal survives, 100 = anti-tonal survives. Borrowed from Stochastic TiltM. |
| `phaseShift` | 0..7 (×45°) | 3 | `UnsignedValue<3>`. Rotates pulse pattern within stage. Borrowed from Flux `PHAS`. |
| `mask` | enum 1 of 8 | 3 | Temporal mute pattern: Off / 1in2 / 1in3 / 1in4 / 2in3 / 2in4 / 3in4 / 1in8. Borrowed from Flux `MASK`. |
| `maskShift` | 0..7 | 3 | Starting offset for temporal mask pattern. Borrowed from Flux `MSK>`. |
| `accumulatorStep` | ±15 scale degrees | 5 | `SignedValue<5>`. Sign = direction. NoteTrack-uniform (scale degrees, not volts). |
| `accumulatorLength` | 1..16 | 4 | `UnsignedValue<4>`. mod-N wrap. |
| `gateLength` | 0..100 % | 7 | `UnsignedValue<7>`. Duty cycle per pulse. |
| `stageDivisor` | enum | 3 | Discrete musical divisions (Performer divisor primitive: 1/32 .. 1 bar) |
| `skip` | bool | 1 | Cursor walks past if true |

**Bit budget**: 3+7+2+2+2+1+1+7+7+2+1+1+7+7+7+7+3+3+3+5+4+7+3+1 = **93 bits/stage → 3 × uint32_t (96 bits, 12 B) with 3 spare**.

**Defaults on fresh sequence**: every cell audible at scale tonic.
`pulseCount=1, basePitch=0 (scale tonic), pitchRange=1 octave, pitchDirection=Up, temporalCurve=Linear, temporalFlipV=false, temporalFlipH=false, *Warp=0, *Response=0, pitchCurve=Ramp, pitchFlipV=false, pitchFlipH=false, phaseShift=0, mask=Off, maskShift=0, maskMelody=100 (bypass), tiltMelody=0, accumulatorStep=0, accumulatorLength=1, gateLength=50%, stageDivisor=1/16, skip=false.`
A fresh PhaseFlux track plays a clean 1/16 click train at the scale tonic. With Ramp + pitchRange=1 and pulseCount=1, each pulse fires at basePitch — sweep doesn't activate until `pulseCount > 1` (multiple curve-positions per stage).

---

## 6. Curve math

Both temporal and pitch curves use identical pipeline:

```
warped_x   = WarpAxis(raw_x, warp)
curve_out  = Curve(warped_x, curve_id)     // LUT lookup from CurveTrack
final_out  = ResponseAxis(curve_out, response)
```

PowerBend deformation (one function, two uses):

```
PowerBend(z, p) = z ^ ((1 − p) / (1 + p))     for z ∈ [0,1], p ∈ (−1, +1)
WarpAxis(x, w)    = PowerBend(x, w)
ResponseAxis(y, r) = PowerBend(y, r)
```

- p = 0 → identity
- p > 0 → curve compressed toward low end
- p < 0 → curve compressed toward high end

**Numerical safety — encoding-side clamp.** `p = ±1` is mathematically degenerate (at +1 the exponent collapses to 0 → all output → 1; at −1 the exponent diverges). The `SignedValue<7>` storage range −63..+63 maps to user-float via **`p_user = encoded / 64`** (not `/ 63`), so max-knob lands at **±0.984** — never at the degenerate boundary. The PowerBend math is never invoked with `p ∈ {−1, +1}`. Engine-side has no defensive clamp; the encoding boundary is the single source of truth.

**Interpolation**: borrow CurveTrack's LUT-per-shape directly. No new interp paths.

### 6.1 Temporal curves — base shape + flips

```
t_raw[i]        = i / N                                                    // N = pulseCount; first pulse at 0 (slot start), last at (N−1)/N
t_warped        = WarpAxis(t_raw[i], temporalWarp)
                                                                             // vflip after the base curve (output-axis only — see §6.1.1 for temporalFlipH)
t_curved        = TempCurve(t_warped, temporalCurve)
t_flipped       = temporalFlipV ? (1.0 − t_curved) : t_curved
t_final         = ResponseAxis(t_flipped, temporalResponse)
trigger_time[i] = ((t_final + phaseShift / 8) mod 1.0) * stage_duration
```

`phaseShift` rotates the trigger pattern by `phaseShift × 45°` inside the stage, modulo `stage_duration`. Applied **after** curve+flip evaluation, **before** mask. Deterministic.

**Note on `temporalFlipH`:** unlike `temporalFlipV` and the pitch flips, the temporal horizontal flip is **not** applied at the curve-input stage. The §6.4 collision clamp sorts pulses by trigger time, which cancels any input-domain mirror on Linear/Bell/Bounce curves (the same set of trigger times re-emerges in the same order). Instead `temporalFlipH` is applied to the **scheduled** trigger intervals after the clamp — see §6.1.1.

**Base shapes** (3 enum values, 1 spare):

| ID | Base | LUT | Behavior |
|---|---|---|---|
| 1 | Linear | `RampUp` (`x`) | Even spacing. With `temporalWarp = +0.5` ≈ Log (fast onset); with `temporalWarp = −0.5` ≈ Exp (accelerating). Warp absorbs Exp/Log shape duties. |
| 2 | Bell | `Bell` (`0.5 − 0.5 cos(2πx)`) | Single density peak in the middle. Symmetric — `temporalFlipH` has no audible effect. |
| 3 | Bounce | LUT TBD at impl time | Multi-cycle drum-roll feel. Candidate LUTs from CurveTrack: `ExpDown2x` (two cascading decays), `ExpDown3x` (three cascading decays), or a new `Bounce = 1 − (1 − x)³` added to `model/Curve.h`. Implementer picks at integration. |

**Flips** (vflip + hflip = 2 bits, 4 variants per base):

- `temporalFlipV = true` → output mirrored top-to-bottom (`1 − y`). Linear+vflip = reverse trigger order (last pulse fires first). Bell+vflip = inverted bell (concentrates pulses at endpoints, sparse in middle).
- `temporalFlipH = true` → see §6.1.1. Curve-input mirror was the original spec but was effectively a no-op because §6.4 sort re-orders the trigger times. Re-spec'd as a post-clamp time-axis reflection.

### 6.1.1 `temporalFlipH` — post-clamp time-axis mirror

Applied after the §6.4 collision clamp produces the final scheduled pulses `(triggerOffset[k], gateTicks[k], cv[k])`:

```
for each scheduled k:
    endTick[k]        = triggerOffset[k] + gateTicks[k]
    newOffset[k]      = stage_duration − endTick[k]      // mirror the gate interval around slot midpoint
    triggerOffset[k]  = max(0, newOffset[k])
re-sort by triggerOffset                                  // order flips
```

Gate duration is preserved. CV pairing is preserved per-pulse (no re-mapping of pitches), so the pitch profile mirrors in time along with the trigger positions — the pulse that previously fired last with its pitch now fires first.

**Audible effect:** with `temporalWarp = +50` (Linear), pulses cluster late (silence at start, busy end). Add `temporalFlipH = true` → cluster shifts to early (busy start, silence at end). On Bell curve with no warp, the cluster sits in the middle and the mirror is symmetric → still no audible change. Bounce is asymmetric in time, and the time-axis mirror flips the bounce direction.

### 6.1.2 `temporalRepeat` — N copies of the curve within one cell

**Status: draft, not yet wired.**

Per-cell 4-value enum (`UnsignedValue<2>`): **x1 / x2 / x3 / x5** (default x1 = no repeat). Divides the cell duration into R equal sub-sections; each sub-section traverses the (post-Window) temporal curve. Effect: R copies of the windowed pattern within one cell visit.

Storage: `temporalRepeat` as `UnsignedValue<2>` stored on `_data3` per §14.2 layout. Default value = 0 (x1, no repetition).

Engine math in `rebuildSchedule` (pipeline: **Warp → Repeat → Window → FlipH → Curve eval → Response → FlipV**):

```
R               = repeatValue(stage.temporalRepeat())          // 1, 2, 3, or 5
pulsesPerSub    = pulseCount / R                                // base allotment
remainder       = pulseCount % R                                // earlier subs get +1

for each pulse i in [0, pulseCount):
    subIdx          = (i × R) / pulseCount
    pulsesInThisSub = pulsesPerSub + (subIdx < remainder ? 1 : 0)
    localPulseIdx   = i − (start index of subIdx)
    t_raw_local     = localPulseIdx / pulsesInThisSub           // local position 0..1
    // Window applies INSIDE each sub-section (Window → Repeat order):
    if !isInWindow(t_raw_local, stage.temporalWindow()): drop pulse
    t_warped        = applyPowerBend(t_raw_local, temporalWarp)
    t_curved_local  = curve(t_warped)
    t_shifted_global = (subIdx + t_curved_local) / R
    // continue with §6.1 pipeline
```

**Distribution policy**: when `pulseCount` doesn't divide evenly by R, earlier sub-sections get the extra pulse(s).

**Edge case `R > pulseCount`**: some sub-sections have zero pulses → silence inside those sub-sections, bursts in others.

**Interaction with `phaseShift`, `mask`, `maskShift`**: applied AFTER Repeat math.

**Combined with Window**: per the pipeline order, Window applies *within* each repeated sub-section. Bell + x3 + Polarize 50 = three "rise-gap-fall" patterns. Bounce + x3 + Focus 50 = three middle-of-drop bursts.

**Coverage**: with Repeat × Window combos, the multi-bump / sawtooth-N / sine-N curve families (§14.1 Q8) become reachable from base shapes. No new curve enum slots needed.

### 6.1.3 `temporalWindow` — crop which portion of the curve the engine sees

**Status: draft, not yet wired.**

Per-cell 5-value enum (`UnsignedValue<3>`):

| Value | Mode | Visible input range | Hidden |
|---|---|---|---|
| 0 (default) | Off | `[0, 1]` (full) | — |
| 1 | Focus 70 | `[0.15, 0.85]` | outer 30% |
| 2 | Focus 50 | `[0.25, 0.75]` | outer 50% |
| 3 | Polarize 70 | `[0, 0.35] ∪ [0.65, 1.0]` | middle 30% |
| 4 | Polarize 50 | `[0, 0.25] ∪ [0.75, 1.0]` | middle 50% |

Engine math (Window applied INSIDE each Repeat sub-section, BEFORE warp):

```
// Inside the per-pulse loop in §6.1.2:
t_raw_local                = ... (per-pulse local position 0..1)
if !isInWindow(t_raw_local, stage.temporalWindow()):
    drop pulse                                                   // does not fire
t_warped = applyPowerBend(t_raw_local, temporalWarp)
// ... continues
```

Where `isInWindow(t, mode)` returns true if `t` falls in the visible band per the table above.

**Defaults**: Off → identity, current behavior.

**Semantic**:
- **Focus** keeps the center band — pulses cluster around the curve's middle. Bell + Focus 50 = pulses fire only near the peak (sustained high output). Bounce + Focus 50 = middle of the curve only.
- **Polarize** keeps the outer bands — pulses cluster at the cell's start AND end with a silent middle. Bell + Polarize 50 = small rise on left, gap, small fall on right.

**What this does that warp/response can't**: warp/response BEND the axes; Window REMOVES segments. Especially Polarize — produces a hole in the middle that no warp can imitate.

**Combined with Repeat**: per pipeline order **Window → Repeat → ...**, Window applies INSIDE each repeated sub-section. `Bell + Repeat 3 + Polarize 50` = three rise-gap-fall patterns. `Bounce + Repeat 3 + Focus 50` = three middle-drop pulses.

**Audible character per base curve** (with default warp=0, response=0, Repeat x1):

| Curve | Window | Effect on temporal pulse positioning |
|---|---|---|
| Linear | Focus 50 | Pulses cluster in middle half of slot. |
| Linear | Polarize 50 | Pulses cluster at start AND end, silent middle. |
| Bell | Focus 50 | Pulses cluster very tightly near slot midpoint (peak region). |
| Bell | Polarize 50 | Pulses cluster early AND late with quiet middle. |
| Bounce | Focus 50 | Pulses fire only in the middle drop region. |
| Bounce | Polarize 50 | Pulses fire only in the first and last drops, middle drop silent. |

### 6.2 Pitch curves — scale-degree quantized, NoteTrack-aligned

PhaseFlux pitch is an integer scale degree, voltage emerges via `Scale::noteToVolts()` exactly like NoteTrack (`engine/NoteTrackEngine.cpp::evalStepNote`). No raw-voltage math anywhere in the pipeline.

```
φ_i           = trigger_time[i] / stage_duration
φ_warped      = WarpAxis(φ_i, pitchWarp)                                      // v0.2: warp first
φ_repeated    = pitchRepeat>1 ? frac(φ_warped × R) : φ_warped                 // repeat the warped phase
φ_windowed    = HoldWindow(φ_repeated, pitchWindow)                          // window after repeat
φ_input       = pitchFlipH ? (1.0 − φ_windowed) : φ_windowed                 // flipH, then curve
p_curved      = PitchCurve(φ_input, pitchCurve)                              // 0..1
p_resp        = ResponseAxis(p_curved, pitchResponse)                        // response before flipV
p_final       = pitchFlipV ? (1.0 − p_resp) : p_resp                          // 0..1

rangeDegrees  = max(1, round(pitchRange × scale.notesPerOctave()))           // 0.5..3 octaves of active scale, rounded to nearest integer scale degree, minimum 1
                                                                             // e.g. pentatonic (N=5) + 0.5oct → round(2.5) = 3 degrees; chromatic (N=12) + 0.5oct → 6 degrees

offset_degrees = match pitchDirection:
                   Up      →  p_final × rangeDegrees                         // 0 → basePitch, 1 → basePitch + range
                   Down    → -p_final × rangeDegrees                         // 0 → basePitch, 1 → basePitch − range
                   Bipolar → (p_final − 0.5) × rangeDegrees                  // Centered: 0 → basePitch−range/2, 1 → basePitch+range/2

degree[i]     = basePitch + round(offset_degrees) + acc_offset
              + octave × scale.notesPerOctave() + transpose                  // track-level (§12.1 NoteTrack borrow)

cv[i]         = scale.noteToVolts(degree[i])
              + (scale.isChromatic() ? rootNote × (1/12) : 0)                // same epilogue as NoteTrack
```

**Base pitch shapes** (3 enum values, 1 spare):

| ID | Base | LUT | Behavior over φ ∈ [0,1] |
|---|---|---|---|
| 1 | Ramp | `RampUp` (`x`) | Monotonic 0 → 1. With `pitchFlipV` = monotonic 1 → 0 (RampDown). |
| 2 | Bell | `Bell` (`0.5 − 0.5 cos(2πx)`) | Rises smoothly to peak at φ=0.5, returns to 0 — `sin(πφ)` curve family. With `pitchFlipV` = **smooth V (Mirror)** — starts at 1, dips smoothly to 0 at midstage, back to 1. |
| 3 | Triangle | `Triangle` (linear Λ) | Linear rise to sharp peak at φ=0.5, linear descent to 0. With `pitchFlipV` = **sharp V** — starts at 1, linear descent to 0 at midstage, linear rise back to 1. The "harder" alternative to Bell. |

(Mirror dropped as separate enum — reachable as Bell+vflip or Triangle+vflip. Flat dropped — `pitchRange = 0.5oct` with all pulses near basePitch covers the "constant" use case. Walk and S/H both dropped to keep engine deterministic.)

**Pitch flips** (vflip + hflip = 2 bits):

- `pitchFlipV = true` → output mirrored (`1 − p_curved`). Bell+vflip = Mirror (smooth V crossing the curve's mid-axis). Ramp+vflip = RampDown.
- `pitchFlipH = true` → input mirrored (`1 − φ_warped`). For Bell + Triangle (symmetric around φ=0.5), no effect. For Ramp, same audible result as vflip.

Practical means: per-stage flips compose with `pitchDirection` to give 12 distinct sweep behaviors per base shape. Example: Ramp + vflip + Down + range=1 = "pitch starts 1 oct below basePitch, rises to basePitch over the stage" — a different musical motion from any non-flipped variant.

**Direction × Curve = musical shape** (`r` = `rangeDegrees`, endpoint trajectory shown as `start → midstage → end`). `Mirror` from earlier spec passes is now reachable as `Bell + vflip` (smooth) or `Triangle + vflip` (sharp):

| Direction | + Ramp | + Bell | + Bell + vflip (= smooth Mirror) | + Triangle | + Triangle + vflip (= sharp Mirror) |
|---|---|---|---|---|---|
| Up | `basePitch → basePitch+r` | `basePitch → basePitch+r → basePitch` (hump above) | `basePitch+r → basePitch → basePitch+r` (smooth V dip through basePitch at midstage) | `basePitch → basePitch+r → basePitch` (sharp peak) | `basePitch+r → basePitch → basePitch+r` (sharp V dip through basePitch) |
| Down | `basePitch → basePitch−r` | `basePitch → basePitch−r → basePitch` (dip below) | `basePitch−r → basePitch → basePitch−r` (smooth Λ peaking at basePitch) | `basePitch → basePitch−r → basePitch` (sharp dip) | `basePitch−r → basePitch → basePitch−r` (sharp Λ at basePitch) |
| Bipolar | `basePitch−r/2 → basePitch+r/2` (crosses basePitch once) | `basePitch−r/2 → basePitch+r/2 → basePitch−r/2` (smooth symmetric swing; crosses basePitch at φ=0.25 and 0.75) | `basePitch+r/2 → basePitch−r/2 → basePitch+r/2` (smooth V; endpoints `+r/2` above, bottom `−r/2` below; crosses basePitch at φ=0.25 and 0.75) | (sharp linear version of Bipolar+Bell) | (sharp linear version of Bipolar+Bell+vflip) |

Adding `vflip` to any cell of the **Up** column produces the equivalent of the same shape played in reverse (`basePitch+r → ...`). Adding `hflip` to symmetric base shapes (Bell, Triangle) has no audible effect. Adding `hflip` to Ramp is equivalent to vflip.

Key reading rule: under **Bipolar**, neither endpoint sits at basePitch — the start and end always sit `±r/2` from basePitch, never at it. Crossings of basePitch occur where the curve evaluates `p_final = 0.5`, which is at midstage for Ramp and at φ=0.25 / 0.75 for Bell/Triangle (whether vflipped or not).

**Scale extends infinitely** — out-of-range `degree[i]` values land outside the scale's nominal min/max; `Scale::noteToVolts()` handles arbitrary integers by repeating the interval table across octaves. No clamping, no wrap — NoteTrack-faithful.

**No RNG.** Walk and S/H both dropped — engine is fully deterministic.

### 6.2.2 Pitch CV evaluation timing — `cvUpdateMode`

The pitch curve can either be **sampled at pulse-fire moments only** (discrete point-samples coupled to gate events) or **evaluated continuously every engine tick** (a smooth envelope across the stage, independent of pulse activity). This is the "CurveTrack-style continuous CV" pattern carried into PhaseFlux's per-stage curve world.

Selected via `PhaseFluxTrack::cvUpdateMode`:

| Mode | Pitch CV behavior | When CV target changes |
|---|---|---|
| `Gate` (default) | Sampled at each pulse-fire tick using the same pitch pipeline that built `_schedule[k].cv`. Between pulses, CV holds the last sampled value. | only when a scheduled pulse fires |
| `Always` | Evaluated every tick from the current `_stagePhase` through the full pitch pipeline (warp / curve / flip / response / direction / range / basePitch / scale-quantize). | every engine tick (PPQN-192) |

In `Always` mode, the gate path (pulses fire at scheduled offsets) is **unaffected** — pulses still trigger gate events from `_schedule`. Only the pitch CV stream becomes continuous. Pulses become pure gate-trigger events; pitch is an independent stream that follows the curve continuously across the stage.

**Per-tick formula in Always mode** (mirrors the pitch pipeline used in `rebuildSchedule` for `Gate` mode, just sampled at `_stagePhase` instead of pulse `t_shifted`):

```
phi              = _stagePhase                              // 0..1 across the active stage
phi_warped       = WarpAxis(phi, pitchWarp)
phi_input        = pitchFlipH ? (1 − phi_warped) : phi_warped
p_curved         = PitchCurve(phi_input, pitchCurve)
p_flipped        = pitchFlipV ? (1 − p_curved) : p_curved
p_final          = ResponseAxis(p_flipped, pitchResponse)
offsetDegrees    = direction-signed × p_final × rangeDegrees + accumulatorOffset
degree           = basePitch + offsetDegrees + octave × notesPerOctave + transpose
cv               = scale.noteToVolts(degree) + (chromatic ? rootNote/12 : 0)
_cvOutput        = cv
```

**Scale-degree hysteresis is open** — continuous evaluation crosses degree boundaries inside the curve and re-quantizes each tick. Without hysteresis the output can jitter on near-boundary phase positions. Marbles-style hysteresis (`_lastDegree` member; only re-quantize when phase has moved past half a degree-width) is the obvious follow-up — see §14.1 Q12.

**Engine cost**: one extra branch + the existing pitch pipeline call per tick. The pipeline is already in `rebuildSchedule` per pulse; `Always` mode runs it ~PPQN-192 times per beat instead of `pulseCount` times per stage. Cheap (single Curve::eval LUT + a few floats).

**Default**: `cvUpdateMode = Gate` (current behavior preserved). Track-level setter via `PhaseFluxTrack::setCvUpdateMode()`. Editable in the Track list page (`PhaseFluxTrackListModel`).

### 6.2.3 Pitch source — `pitchMode` (Cell vs Global)

The pitch curve+warp+response triple can either live **per-cell** (today's behavior — each of the 16 stages defines its own contour) or **globally** on the sequence (one shape, drifted across all cells against the temporal walker). Selected via `PhaseFluxSequence::pitchMode`:

| Mode | Pitch curve source | Phase input | Range / basePitch / accumulator |
|---|---|---|---|
| `Cell` (default) | Active stage's `pitchCurve` / `pitchWarp` / `pitchResponse` / `pitchFlipV` / `pitchFlipH`. | `_stagePhase` (0..1 within the active cell, resets at each cell entry). Same as today. | Per-cell, unchanged. |
| `Global` | `stage[0]`'s pitch curve+warp+response+flips ARE the global ones — no separate storage. Stages 1..15 keep their pitch fields in storage but the engine ignores them. | `_pitchPhase` (free-running accumulator, never resets at cell entry). Advances every tick by `rate × tempo` where one cell = one unit. | Per-cell, unchanged. `basePitch` and `pitchRange` still come from the active stage. |

**Storage layout** — no new per-stage fields; `stage[0]`'s existing pitch fields are reused as the globals. Roundtrip is lossless: switching `Cell → Global → Cell` returns the original per-cell data intact because stages 1..15 are never overwritten by the Global path.

**Rate ratio** (`PhaseFluxSequence::pitchRate`) — index into a static 17-entry P:T ratio table. `1:1` is the locked baseline (Global mode at rate 1:1 visually matches Cell mode in that each cell sees one full pitch curve). Non-integer ratios make `_pitchPhase` drift against the temporal walker so repeated visits to the same cell sample the curve at different positions, producing different notes on each revisit.

**`_pitchPhase` lifetime** — always advancing, regardless of mode. In Cell mode the accumulator ticks but the value is unused. This keeps mode flips instant and seamless (Global mode picks up wherever the phase happens to be).

**Per-tick advance** (every engine tick, both modes):

```
ratePerSlot     = pitchRateNum / pitchRateDen
slotTicks       = _cumulativeTicks[_slotIdx+1] − _cumulativeTicks[_slotIdx]
_pitchPhase    += ratePerSlot / slotTicks
_pitchPhase    -= floor(_pitchPhase)                  // mod 1
```

**Pulse-fire pitch in Global mode** (in `rebuildSchedule`, replacing the per-pulse `phi = t_shifted` of Cell mode):

```
phi = _pitchPhase + triggerTime × (ratePerSlot / slotDurationTicks)
phi -= floor(phi)
```

i.e. the pitch head's position is projected forward from slot start to each scheduled pulse's fire time. `stage[0]`'s curve+warp+response then run the rest of the pitch pipeline at that `phi`.

**Always-mode (continuous CV) in Global pitch mode** — same pipeline as §6.2.2 but with curve source = `stage[0]` and `phi = _pitchPhase` instead of `_stagePhase`.

**Default**: `pitchMode = Cell`. Sequence-level setter via `PhaseFluxSequence::setPitchMode()`. Editable in the Sequence list page (`PhaseFluxSequenceListModel`). Each sequence on a track can choose its own mode independently.

### 6.2.4 `pitchRepeat` — N copies of the pitch curve within one cell

**Status: draft, not yet wired.**

Per-cell 4-value enum (`UnsignedValue<2>`): **x1 / x2 / x3 / x5** (default x1 = no repeat). Stored on `_data3` per §14.2 layout.

Engine math (pipeline: **Warp → Repeat → Window → FlipH → Curve eval → Response → FlipV**). The pitch curve is sampled at the windowed-then-repeated position:

```
R              = repeatValue(stage.pitchRepeat())                // 1, 2, 3, or 5
// Window applies to phi BEFORE repeat:
if !isInWindow(phi, stage.pitchWindow()):
    pulse contributes no pitch motion (held at boundary)
phi_repeated   = fmod(phi × R, 1.0)
phi_warped     = applyPowerBend(phi_repeated, pitchWarp)
phi_input      = pitchFlipH ? (1 − phi_warped) : phi_warped
p_curved       = PitchCurve(phi_input, pitchCurve)
// ... rest of §6.2 pipeline unchanged
```

With `pitchCurve = Ramp` and `pitchRepeat = x3`, the pitch traversal looks like three sawtooth ramps within one cell visit — the "stacked ramps" pattern.

**Cell mode only.** In Global pitch mode, `pitchRate` already owns the multi-cycle frequency; pitchRepeat is ignored.

**Coverage**: with Repeat × Window combos on Ramp/Bell/Triangle/Bounce, multi-bump / sawtooth-N / sine-N pitch families become reachable from base shapes.

**Interaction with `pitchDirection`, `pitchRange`, `pitchFlipV/H`, `pitchWarp`, `pitchResponse`**: all applied AFTER the Repeat fmod.

**Audible effect example**: `pitchCurve = Ramp`, `pitchRange = 2 oct`, `pitchRepeat = x5`, `pulseCount = 8` → 8 pulses sample 5 stacked ramps spanning the 2-octave range. 16th-note-arpeggio-inside-the-cell feel instead of one slow sweep.

### 6.2.5 `pitchWindow` — crop which portion of the pitch curve the engine sees

**Status: draft, not yet wired.**

Per-cell 5-value enum (`UnsignedValue<3>`):

| Value | Mode | Visible input range | Hidden |
|---|---|---|---|
| 0 (default) | Off | `[0, 1]` (full) | — |
| 1 | Focus 70 | `[0.15, 0.85]` | outer 30% |
| 2 | Focus 50 | `[0.25, 0.75]` | outer 50% |
| 3 | Polarize 70 | `[0, 0.35] ∪ [0.65, 1.0]` | middle 30% |
| 4 | Polarize 50 | `[0, 0.25] ∪ [0.75, 1.0]` | middle 50% |

Same math as §6.1.3 but on the pitch axis. Engine math (Window applies BEFORE Repeat, BEFORE warp):

```
phi          = (per-pulse phi or _stagePhase per §6.2.2)
if !isInWindow(phi, stage.pitchWindow()):
    // hidden — pulse contributes no pitch motion; CV held at boundary
phi_repeated = fmod(phi × pitchRepeat, 1.0)                   // §6.2.4
phi_warped   = applyPowerBend(phi_repeated, pitchWarp)
// ... rest of §6.2 pipeline unchanged
```

**Defaults**: Off → identity, current behavior.

**Audible character examples** (with default warp=0, response=0, Repeat x1):

| Curve | Window | Effect on pitch CV |
|---|---|---|
| Ramp | Focus 50 | Pitch covers only the middle 50% of range — narrower sweep. |
| Ramp | Polarize 50 | Pitch fires at the lowest AND highest quarters; mid-range silent. |
| Bell | Focus 50 | Pitch stuck near peak — sustained high note. |
| Bell | Polarize 50 | Low pitches at cell start AND end, silent in between (the peak is hidden). |
| Triangle | Focus 50 | Sharp peak region only. |
| Triangle | Polarize 50 | Up-slope start + down-slope end with silent middle. |
| Bounce | Focus 50 | Middle drop only — single pluck mid-cell. |
| Bounce | Polarize 50 | First and last drops; middle drop silent. |

**Cell mode only**; ignored in Global pitch mode (paired with Repeat constraint).

**Combined with Repeat**: per pipeline order Window→Repeat, the windowed curve gets repeated. `Ramp + x3 + Polarize 50` = three patterns each with a low-pitch start, gap, low-pitch end (the polarize bands of the ramp = its bottom and top edges). Each repeat carries the windowed character.

**Coverage gained**: Window unlocks "Bell-half = Ramp", "Triangle-half = monotonic", "Bounce-front = sharp pluck", "Bell-peak = sustained", and the mirror cases via FlipH/FlipV. Many shape characters reachable from a single base curve.

### 6.2.1 Melody mask (centrality filter, per-stage) — orthogonal union

Each stage has `maskMelody` (0..100, tonal-filter strength) and `tiltMelody` (0..100, anti-tonal-filter strength). The pair filters pulses by the **innate scale-degree centrality** — tonic + fifth = high centrality, tritone-ish = low. Two independent monotonic threshold sweeps on opposite ends of the centrality axis; pulse passes if EITHER filter admits it.

```
N            = scale.notesPerOctave()
degInOct     = ((degree[i] mod N) + N) mod N
centrality   = stochasticPitchCentrality(degInOct, N)              // raw 0..centralityMax
maskThresh   = centralityMax × (100 − maskMelody) / 100
tiltThresh   = centralityMax × (100 − tiltMelody) / 100
maskPass     = centrality ≥ maskThresh
tiltPass     = tiltMelody > 0 ∧ (centralityMax − centrality) ≥ tiltThresh
passes       = maskPass ∨ tiltPass
```

- `maskMelody = 100` → mask filter bypass (every position passes via mask, tilt becomes inert)
- `maskMelody = 0` → only top centrality (tonic) passes via mask
- `tiltMelody = 0` → tilt filter disabled (no anti-tonal positions added)
- `tiltMelody = 100` → tilt filter bypass (every position passes via tilt)
- defaults `maskMelody = 100, tiltMelody = 0` → fully transparent (mask lets everything through, tilt adds nothing)
- `maskMelody = 20, tiltMelody = 20` → root + most-anti-tonal pair (maximum-tension set)

**Why this shape replaces the prior `lerp(centrality, complement, tilt/100)` formula**: any linear interpolation between a function and its complement has a fixed point at midpoint where every position collapses to the same effective value (the "dead zone"). The orthogonal-union design has each knob act on its own pool with no interaction in the formula — no dead zone, mask remains monotonic, tilt's polarity-flip semantic preserved as "add wrong notes" rather than "invert weights".

**Fail action** (when `passes == false`): pulse becomes a rest — gate is suppressed. CV behavior is delegated to track-level `_cvUpdateMode` (§12.1, NoteTrack-uniform):
- `CvUpdateMode::Gate` (default) → CV holds previous value through the silenced pulse
- `CvUpdateMode::Always` → CV still updates to the would-be `degree[i]` silently (useful for slewing pitch under VCA control or feeding external modules)

**Scale-bias caveat**: `stochasticPitchCentrality` (`model/StochasticTypes.h:38`) is a triangular-kernel function with `halfWidth = N/6` and integer weights anchored to chromatic-scale assumptions (root 30, fifth 20, thirds 10, fourths 5). Tuned for 12-EDO; produces musically intuitive results on Chromatic, Major, Minor, and other 7+-degree diatonic scales. On scales with fewer degrees the kernel collapses: whole-tone (N=6) gives sharp integer hits with no overlap; pentatonic (N=5) double-counts two positions; microtonal scales below N≈5 produce degenerate distributions. UserScale precedent: weights MUST be algorithm-derived from position (not stored per-degree) because UserScale lets users define any N up to `CONFIG_USER_SCALE_SIZE`. Marbles-style per-degree weight authoring would require new UserScale UI; out of scope for MVP.

The Stochastic engine carries the same orthogonal-union formula (`StochasticTrackEngine.cpp:454-471`) — semantics aligned across both engines.

### 6.3 Pulse mask

After timing is determined for each pulse `i ∈ 0..pulseCount−1`, apply the boolean mute pattern:

```
muted[i] = MASK_TABLE[mask][(i + maskShift) mod 8]
fires[i] = !muted[i] && !skip
```

Mask patterns (8 fixed Boolean ring patterns, length 8):

| `mask` | Pattern (8 bits, LSB = pulse 0) | Behavior |
|---|---|---|
| Off | `00000000` | No mask — every pulse fires |
| 1in2 | `10101010` | Mute every 2nd pulse |
| 1in3 | `01001001` | Mute every 3rd pulse |
| 1in4 | `00010001` | Mute every 4th pulse |
| 2in3 | `10110110` | Mute 2 of every 3 |
| 2in4 | `01010101` | Mute 2 of every 4 |
| 3in4 | `01110111` | Mute 3 of every 4 |
| 1in8 | `00000001` | Mute every 8th pulse |

`maskShift` rotates the starting index 0..7. Borrowed from Flux `MASK` / `MSK>`. Deterministic.

The mask gates pulse output only — pitch CV still evaluates at the masked pulse's curve position (sustains last value via `_cvUpdateMode`).

### 6.4 Pulse-fire schedule — overlap-allowed

After §6.1 produces `trigger_time[i]` (with `phaseShift` applied) and §6.3 mask + §6.2.1 melody mask determine which pulses survive, the engine schedules gate events. **Trigger overlap is allowed** — NoteTrack precedent: the runtime tick loop merges overlapping gates via `_gateState=true` retrigger, so no "gap between pulses" or "previous gate must end first" guards are needed. Each surviving candidate fires at its computed time regardless of previous gate state.

```
const uint32_t kMinPulseGateTicks = 6;     // ~30ms @ 120 BPM, PPQN=192 — audible floor

if stage_duration < kMinPulseGateTicks:
  return                                     // stage too short to fit any audible gate

// Surviving pulses sorted by trigger_time ascending (PowerBend + phaseShift can scramble order)
survivors = [(i, trigger_time[i]) for i in 0..pulseCount-1 if !muted[i] && melodyMaskPass[i]]
sort survivors by trigger_time

for (i, t) in survivors:
  next_t       = next survivor's trigger_time, OR stage_duration
  pulse_period = next_t - t
  if pulse_period <= 0: continue              // defensive (i/N positions never produce this)

  // gateLength rules:
  //   len == 0  → explicit silence, pulse dropped
  //   len == 1  → always-audible-minimum sentinel; floor at kMinPulseGateTicks
  //   len >= 2  → scale by percent, drop if computed gate < kMinPulseGateTicks
  if gateLength == 0: continue
  gate_ticks = (gateLength × pulse_period) / 100
  if gateLength == 1 && gate_ticks < kMinPulseGateTicks:
    gate_ticks = kMinPulseGateTicks
  if gate_ticks < kMinPulseGateTicks: continue

  schedule gate-on at tick t
  schedule gate-off at tick t + gate_ticks
```

**Locked semantics**:

- **`pulseCount` is a max-cap, not a strict count.** A pulse can still be dropped via `gateLength` rules above (explicit 0 or computed < 6 ticks). Otherwise all surviving candidates fire.
- **Overlap is allowed.** A pulse firing while the previous gate is still high re-asserts `_gateState=true` and resets `_gateTimer`. Downstream sees one extended gate — same as NoteTrack HOLD mode.
- **Gate length not capped by pulse period.** `gate_ticks = (gateLength × pulse_period) / 100` directly; can extend past the next pulse's trigger or even past slot end. The runtime tick loop handles cross-pulse and cross-stage overlap via retrigger.
- **Same-tick collisions** (sort puts them adjacent, pulse_period = 0 → defensive continue drops the duplicate). Mask indices preserved against the original pulseCount.
- **`kMinPulseGateTicks = 6`** matches Stochastic's `minChildGate` constant — single source of truth for "audible floor" across track types.
- **Pulse position formula** (§6.1): `t_raw[i] = i / N` (uniform spacing). First pulse at 0 (slot start); last at `(N−1)/N` (never lands on the boundary). Single-pulse is the natural N=1 case: one pulse at slot start.

Order of application:
1. §6.1 produce `trigger_time[i]` for i in 0..pulseCount−1 (with phaseShift mod)
2. §6.3 temporal mask gates each pulse on its original index
3. §6.2 pitch evaluation per non-muted pulse; §6.2.1 melody mask filters by centrality
4. §6.4 (this section) collision clamp on survivors, drops overflow, schedules gate events

Mask indices in §6.3 and §6.2.1 always refer to the **original pulse index** (0..pulseCount−1), never the post-drop index — masks stay musically predictable regardless of warp setting.

---

## 7. Accumulator

Per-cell drift counter, **scale-degree quantized (NoteTrack-uniform)**. `accumulatorCounter[16]` is the transient engine state declared in §4 (one slot per cell ID, not per snake position):

```
// Stage completion detected as slot-index change between adjacent ticks
on slotIdx change (prevSlotIdx → slotIdx):
  completedCell                       = snakeOrder[prevSlotIdx]
  accumulatorCounter[completedCell]   = (accumulatorCounter[completedCell] + 1) mod accumulatorLength[completedCell]

acc_offset = accumulatorCounter[activeCell] * accumulatorStep[activeCell]    // in scale degrees
```

- **Unit**: `accumulatorStep` is signed scale degrees (`SignedValue<5>` = ±15). Drift is added to `degree[i]` in the §6.2 pipeline as an integer scale-degree offset — NoteTrack uses the same approach (`engine/NoteTrackEngine.cpp:84-88`).
- **Advance mode**: hardcoded to per-stage-completion (original spec mode B). Modes A (per-pulse) and C (per-grid-cycle) deferred to v2.
- **Direction**: implied by sign of `accumulatorStep`. No separate direction bit.
- **Wrap**: mod-N. Ping-pong deferred.
- **Per-stage vs global**: per-stage independent. Each cell owns its own `accumulatorStep` + `accumulatorLength`. No sequence-level global (diverges from NoteTrack's `Accumulator` class which has a global step + per-step override).

### 7.1 Counter lifecycle across reset events

Different reset events have different semantics — accumulator state is **preserved across some, cleared by others**. The asymmetry is deliberate:

| Event | Counter behavior | Rationale |
|---|---|---|
| Reset Measure boundary (§3.3) | **Preserved** | Same pattern, same stages — drift is the musical point. Long-form accumulation across multiple bars is the feature. |
| Cumulative-table recompute (§3.4) | **Preserved** | Same stages, just re-laid out. Drift state belongs to the cells, not the layout. |
| Pattern switch (§11) | **Cleared (all → 0)** | Different musical material loading. The new pattern's stage record may have different `accumulatorStep` / `accumulatorLength` per cell — carrying counter state across would apply old drift to a new musical context. |
| Transport restart / stop / continue | (see §11, transport behavior section) | |

Reset Measure is a **rhythmic re-anchor within one pattern**. Pattern switch is a **musical context change**. They sound similar in name but are not the same kind of event.

`acc_offset` is added in the §6.2 pitch pipeline as `basePitch + round(offset_degrees) + acc_offset + octave×notesPerOctave + transpose`.

---

## 8. Transforms

**All four deferred to v2.** No Reverse, Octave Shift, Humanize, or Ratchet in MVP. Stage outputs the curve+pulse spec directly.

---

## 9. Outputs

- **Gate Out** — single gate, per pulse, held for `gateLength × pulse_period` (see §6.4 for the per-pulse gate rules; overlapping gates merge via the runtime's `_gateState=true` retrigger).
- **CV Out** — quantized to scale degree via `Scale::noteToVolts()` (§6.2), 1V/Oct. External quantizer is unnecessary because output is already scale-locked. Evaluation timing controlled by `cvUpdateMode` (§6.2.2): `Gate` = sampled at pulse fires; `Always` = continuous envelope across each stage.

PhaseFlux produces exactly **one logical CV value + one logical gate value** per tick. Two jacks (Performer standard pitch+gate pair). No per-cell jack routing.

**Mute behavior** (matches NoteTrack): when the track is muted, the engine **stops pushing CV updates** to the output. The DAC continues to hold its previous voltage in hardware. CV does NOT snap to 0V — silence in CV terms is "no further updates", not "force a specific voltage". This is decoupled from `cvUpdateMode` — that enum controls evaluation timing only. (Historical: pre-MVP code force-zeroed CV on mute; that was wrong and removed when the §6.2.2 continuous-CV mode landed.)

**Multi-jack mirroring**: when the player routes PhaseFlux to multiple physical CV/gate jacks via Performer's `CvRoutePage`, all routed jacks output the **same** single logical value. No per-jack divergence — PhaseFlux is single-voice monophonic by design, mirroring follows Performer's standard track-to-jack mapping.

---

## 10. Modulation inputs

**Deferred to v2.** No per-track mod-input subsystem. Future modulation will route through Performer's existing global Routing engine. Every routable field must be wrapped `Routable<>` from day 1 to avoid a future serialization break — see §12.

---

## 11. Performer integration

| Feature | MVP |
|---|---|
| Mute / Solo | Yes |
| Pattern A/B/C... (multi-pattern per track) | Yes |
| Snapshot (per-pattern saved state) | Yes |
| Pattern switch mid-cycle | **Full reset**: `_resetTickOffset = tick` (cursor/phase derive to (0,0)/0), all `accumulatorCounter[16]` → 0 (see §7.1 — pattern switch is the one event that clears counters), `pulseFired[8]` cleared, `gateTimer` cleared, cumulative duration table recomputed against new sequence |
| Fill (alt sequence) | Deferred to v2 |
| Routing system | Deferred to v2 (fields declared Routable<> for forward compat) |
| Harmony follower | Deferred to v2 |

### 11.1 Transport behavior — NoteTrack two-tier pattern

Mirrors NoteTrack's `reset()` / `restart()` split (`NoteTrackEngine.cpp:104, 144`). PhaseFlux maps the engine's `reset()`/`restart()` methods onto transport commands:

| Transport event | Action | Counters | Notes |
|---|---|---|---|
| **Stop** | Drain `_gateQueue` and `_cvQueue`. Emit gate-off for any active gate (`gateTimer → 0`, gate jack → low immediately). | **Preserved** | `_resetTickOffset`, cursor, phase, counters all preserved. Engine simply stops being ticked. |
| **Start** (from stopped) | Full reset — `_resetTickOffset = tick`, `accumulatorCounter[16]` → 0, `pulseFired[8]` cleared, `gateTimer` cleared. | **Cleared** | Equivalent to pattern switch full-reset (§11). Engine begins ticking from cycle position (0,0). |
| **Continue** (from stopped, resume) | No-op. Engine resumes ticking. | **Preserved** | Cursor derives from current `tick − _resetTickOffset`, which has advanced during the stop period — may land on a different cell than where it stopped. By design: PhaseFlux is recipe-replay, so resuming = "where it would have been if not stopped". |
| **External Reset trigger** (light reset) | `_resetTickOffset = tick`, `pulseFired[8]` cleared, `gateTimer` cleared. | **Preserved** | Same as Reset Measure (§3.3). Counters preserved per §7.1 reset-event-asymmetry rule. |

**Continue caveat**: because PhaseFlux is recipe-replay, Continue doesn't "freeze and resume at the same beat" — it resumes at whatever tick the clock has reached. If the player stops, waits 5 seconds, then Continues, the cursor will have "virtually advanced" through those 5 seconds. To pause-and-resume at the exact beat, use Stop + Start (which resets) and use Pattern restart to anchor.

---

## 12. Performer alignment — Track / Sequence borrowed fields

Mandatory for parity with the rest of the firmware. Follow the Stochastic borrow pattern: copy NoteTrack's pitch/playback chassis verbatim (with Routable wrappers), keep clockMultiplier + resetMeasure on the sequence, drop probability-bias knobs that don't apply, drop runMode/firstStep/lastStep if traversal is self-managed.

### 12.1 `PhaseFluxTrack` — borrow from NoteTrack (Stochastic borrow pattern)

Direct port, same types, same Routable wrappers:

| Field | Type | Notes |
|---|---|---|
| `_trackIndex` | `int8_t` | Standard back-reference, default −1 |
| `_slideTime` | `Routable<uint8_t>` | 0..100. Mono pitch slew. Required by category. |
| `_octave` | `Routable<int8_t>` | ±10. Track-wide octave shift. |
| `_transpose` | `Routable<int8_t>` | ±100 **scale degrees** (not strict semitones — added as integer scale-degree offset via `evalTransposition` chassis from `NoteTrackEngine.cpp:69`. Becomes semitones only when the active scale is Chromatic, where `notesPerOctave()=12`). |
| `_playMode` | `Types::PlayMode` | Aligned / Free / Last. PhaseFlux engine logic lives under Free, but field exists for parity. |
| `_fillMode` | `FillMode` | Stored but Fill is deferred. Default `None`. |
| `_cvUpdateMode` | `CvUpdateMode` | Continuous vs on-gate. Default `Gate`. |

Omitted (intentional, match Stochastic):
- `_fillMuted` (Fill deferred)
- `_rotate` (move to Sequence if added)
- `_gateProbabilityBias`, `_retriggerProbabilityBias`, `_lengthBias`, `_noteProbabilityBias` — no probability fields to bias

PhaseFlux-specific additions:
- `_lock` (bool) — TBD if locking semantics make sense for grid track
- `PhaseFluxSequenceArray _sequences` — pattern slots

### 12.2 `PhaseFluxSequence` — borrow from NoteSequence

Direct port:

| Field | Type | Notes |
|---|---|---|
| `_trackIndex` | `int8_t` | Engine reads `sequence.trackIndex()` constantly |
| `_divisor` | **`Routable<uint16_t>`** | X-axis clock divisor. **Wrap Routable from day 1** — diverges from Stochastic, matches NoteSequence. |
| `_clockMultiplier` | `Routable<uint8_t>` | 50..150. Free-mode math reads it. |
| `_resetMeasure` | `uint8_t` | 0..128. Phase-lock guarantee (see §3). |
| `_edited` | `uint8_t` | Dirty bit for UI. |

Borrowed routing infrastructure: `isRouted` / `printRouted` / `writeRouted` dispatch table pattern from NoteSequence/StochasticSequence.

Omitted:
- `_mode` (only one PhaseFlux variant, no mode enum needed)
- `_runMode` (traversal is self-managed: boustrophedon snake)
- `_firstStep` / `_lastStep` (replaced by per-cell `skip`)
- `_divisorY` / `_divisorYSource` / `_divisorYTrack` (dual-clock deferred to v2)
- `_noteFirstStep` / `_noteLastStep` (Ikra-specific)
- `_masterTrackIndex` / `_harmonyScale` / `_harmonyInversion` / `_harmonyVoicing` / `_harmonyTranspose` (harmony deferred)

PhaseFlux-specific additions:
- 16 × stage record (§5)
- Optional: `_scale`, `_rootNote` (−1 = inherit from project default) if PhaseFlux participates in scale routing later. Default −1.
- `_pitchMode` (enum Cell/Global, 1 byte) — selects per-cell vs `stage[0]`-as-globals pitch source; see §6.2.3.
- `_pitchRate` (1 byte, index into 17-entry P:T ratio table) — drift rate of the free-running `_pitchPhase` accumulator in Global mode. Default index = `1:1`.
- `_globalPhase` (float ∈ [0, 1]) — fraction-of-cycle phase shift, CurveTrack precedent. Covers the cycle-shift role (was previously also drafted as a separate `sequenceShift` bipolar field in §14.2; that proposal is now retired in favor of this). Making it `Routable<float>` for CV is the only remaining surface decision; field already lives at the right place in the engine pipeline (`PhaseFluxTrackEngine.cpp:558-561`).

### 12.3 Routable-from-day-1 contract

Mod inputs are deferred but Routing alignment is not. Fields that may ever be CV-routed must be `Routable<T>` on first commit. Changing a plain int to `Routable<int>` later is a serialization break requiring migration. Mandatory Routable wrap:

- Track: `_slideTime`, `_octave`, `_transpose`
- Sequence: `_divisor`, `_clockMultiplier`

**Per-cell routing: deferred to v2.** MVP keeps all per-stage fields as plain bitfields in the 3 × uint32_t record (no per-cell Routable wrappers). Architecture decision intentionally reserved — adding per-cell Routables later requires extracting affected fields out of the bit-packed record into a sidecar Routable array (storage hit) but does not break any existing serialization since dev iterates at `Version35` (see §16). Live-tweak granularity in MVP: track-level (`_slideTime`, `_octave`, `_transpose`) and sequence-level (`_divisor`, `_clockMultiplier`, `_scale`, `_rootNote`) only.

---

## 13. Retro lesson gates (from `retro/stochastic-retro.md`)

| # | Lesson | How MVP satisfies |
|---|---|---|
| 1 | State ownership locked before code | §4, §12 — Track / Sequence / Engine table is complete |
| 2 | Embedded-incompatible audit | Zero RNG (Walk + S/H both dropped). No `std::vector` / `std::mt19937` / heap alloc. LUT + `pow()` only. |
| 3 | RAM verified on STM32 | **Container-gate check** per `PROJECT.md` §244-272. Projection: `PhaseFluxTrack ≈ 3,422 B` (16 sequences × ~212 B + ~30 B chassis) vs **NoteTrack 9,544 B model ceiling** → ~6,100 B headroom, **adding the variant is free** (slot waste already exists). `PhaseFluxTrackEngine ≈ 200 B` vs **TeletypeTrackEngine 912 B engine ceiling** → ~700 B headroom. Measure with `make -C build/stm32/release sequencer` and verify `Model`, `Engine`, `Track::_container`, `TrackEngineContainer` sizes before Phase 1. |
| 4 | Serialization round-trip test first | Mandatory before any feature work — every field write→clear→read→assert. |
| 5 | Integer-division cliffs | Continuous warp/response knobs use float math; no `KNOB/N` integer division in engine path. |
| 6 | Single frozen vocabulary | `stage` (new), `pulse` (Performer existing, replaces "sub-trigger"), `cursor` / `phase` (Performer existing), `skip` (new). Frozen here. |
| 7 | Identity model validated | Recipe replay for pulses, cursor, and intra-stage phase (all derived from `relativeTick`, DiscreteMap-style); storage for stages. Only counters + gate timer + pulse-fire memory are stateful. |
| 8 | Cache invalidation owner | N/A in MVP — no cache to invalidate, curves evaluated live. |
| 9 | Adversarial review early | Schedule after `PhaseFluxTrack` + `PhaseFluxSequence` model lands, again after engine output pipeline. |
| 10 | Consolidated design docs | This file is the single source of truth. Prior spec in chat is superseded. |

---

## 14. Spec open questions — resolutions

| Spec Q | Resolution |
|---|---|
| Q1 firing logic | Phase-only. Active cell derived from `relativeTick` via cumulative duration table (§3); intra-stage phase falls out of the same math. |
| Q2 cursor-leaves-mid-stage | N/A — stage duration tracks cursor by construction (cursor is derived, not advanced). |
| Q3 curve interpolation | CurveTrack LUT per shape, borrowed verbatim. |
| Q4 stage duration | Per-stage `stageDivisor` (Performer divisor primitive). Engine sums to cumulative table (§3.1). |
| Q5 reset behavior | `_resetTickOffset = tick` ⇒ cursor and phase derive to zero. Counters preserved. Pulse-fire memory + gate timer cleared. |
| Q6 counter direction | Sign of `accumulatorStep`. No separate direction bit. |
| Q7 wrap vs ping-pong | mod-N wrap. Ping-pong deferred. |

---

## 14.1 Open questions — unresolved

| Spec Q | Status |
|---|---|
| Q8 curve "squash" transform — applies to BOTH temporal and pitch axes | **PARTIALLY RESOLVED.** Option 3 (Fold) is now spec'd as `temporalRepeat` / `pitchRepeat` in §6.1.2 / §6.2.4 / §14.2 — handles repeated curve traversal cleanly with R=1..8 per cell. Options 1 (Output quantize), 2 (Density compress/expand), 4 (Staircase) remain open if those characters are needed later. Original wording preserved for reference: "Observed limitation (2026-05-25): with `pulseCount ≤ 8` (3-bit field cap), pulses distribute monotonically across the curve output but skip intermediate values as the output span grows. Current transforms (warp, response, flipV/H) do not address this. Candidate semantics: (1) Output quantize/step — Marbles `steps`; (2) Density compression/expansion — pull pulses into a narrower window of the available span; (3) **Fold** — `curve_out × K mod 1` so the curve traverses the span K times within one stage; (4) Staircase — quantize curve to N plateaus." |
| Q9 global 16-stage compression/expansion | **OPEN.** Phaseque-borrow (2026-05-26): a `cycleSpread` field at sequence level that scales the active stages into a narrower window of the total cycle (low spread = compression toward midpoint) or pushes them to the endpoints (high spread = bernoulli-like expansion), independent of `clockMultiplier` and `divisor` which already scale the cycle period. This is **Marbles `spread`** applied to the time axis (distribution width), not `bias` (which would shift the cluster's center position). Low value = busy middle with silent edges; high value = burst-at-bar-edges feel. Bipolar 0..127 unsigned (low=compression, mid=neutral, high=expansion) or signed ±100 (centered at neutral); 7-bit, Routable target. Decide: live alongside `divisor` and `clockMultiplier` or merged into one of them with broader range? Also: pair with a `cycleBias` later (Marbles bias = WHERE the cluster sits, separate axis from compression). |
| Q10 global phase warp | **RESOLVED** as `cyclePhaseWarp` in §14.2. Selected the master-phase warp variant (warp the phase pre-slot-derivation, keep cycle length, move per-stage hits). The alternative (warp cumulative-table durations) was rejected because it would mutate the table on every nudge tick — expensive and incompatible with `detectLayoutChange()` caching. Routable per-sequence `int8_t`, ±64, default 0. Sequence-level Response on CV considered and rejected (breaks scale-degree contract). |
| Q11 snap-to-grid utility | **OPEN.** Once continuous `stageLen` and `sequenceShift` land, edits drift off the natural grid. Need a sequence-level "snap" action (not a stored field) that quantizes per-stage `stageLen` multipliers and `phaseShift` values to nearest grid increment, and `sequenceShift` to the nearest 1/16 (or 1/32) of cycle. Bound to an F-key or context-menu entry on EditPage. Decide: pre-Phase-C UI lock. |
| Q12 scale-degree hysteresis for continuous CV (§6.2.2) | **OPEN.** When `cvUpdateMode = Always`, the pitch curve evaluates every tick and re-quantizes to the nearest scale degree. Phase positions near a degree boundary oscillate between two adjacent degrees as the curve crosses, producing audible jitter. Marbles uses a hysteresis quantizer (`marbles/random/quantizer.cc:91`) that keeps the previous output until the curve has moved past half a degree-width. Candidate semantics: (1) **No hysteresis** — re-quantize each tick (jitter on near-boundary motion, but truthful to the curve); (2) **Fixed half-degree hysteresis** — Marbles-style; smooth glides at the cost of slight pitch lag; (3) **Configurable hysteresis amount** — Marbles `steps` knob equivalent, per-stage 0..1 hysteresis width. Decide before audible Always-mode ships. |

---

## 14.2 Proposed new fields — design sketches

These are accepted-in-principle but not yet bit-pack-budgeted or test-covered. Each retains an "OPEN" item for any unresolved sub-question.

### Per-stage: `Repeat` and `Window` — curve-shape transforms (paired addition)

Two paired sets of per-cell fields. Repeat multiplies curve traversals within the cell; Window crops which portion of the curve the engine evaluates. Per-axis (temporal and pitch independently). Both apply BEFORE Warp/Response in the pipeline.

**Repeat** — `temporalRepeat` and `pitchRepeat`, each a 4-value enum stored as `UnsignedValue<2>`:

| Value | Multiplier |
|---|---|
| 0 (default) | ×1 (no repeat) |
| 1 | ×2 |
| 2 | ×3 |
| 3 | ×5 |

Coverage: multi-bump / multi-decay / N-cycle pulse patterns become reachable from base shapes. No new curve enum slots needed for those.

**Window** — `temporalWindow` and `pitchWindow`, each a 5-value enum stored as `UnsignedValue<3>`:

| Value | Mode | What engine evaluates |
|---|---|---|
| 0 (default) | Off | full curve `[0, 1]` |
| 1 | Focus 70 | center 70% — input `[0.15, 0.85]` (outer 30% hidden) |
| 2 | Focus 50 | center 50% — input `[0.25, 0.75]` (outer 50% hidden) |
| 3 | Polarize 70 | outer 70% — input `[0, 0.35] ∪ [0.65, 1.0]` (middle 30% hidden) |
| 4 | Polarize 50 | outer 50% — input `[0, 0.25] ∪ [0.75, 1.0]` (middle 50% hidden) |

3 spare enum slots in the 3-bit field (values 5/6/7) reserved for future window shapes.

- **Focus** vs **Polarize** give opposite musical gestures:
  - Focus narrows pulses/pitches into the center of the curve (Bell at Focus 50 = sustained near-peak).
  - Polarize concentrates pulses/pitches at the cell's edges with a silent middle (Bell at Polarize 50 = small rise on left, gap, small fall on right).
- Hidden portion: engine does not evaluate; for temporal axis pulses falling in the hidden band are dropped; for pitch axis the curve doesn't contribute (held at boundary value or skipped depending on `cvUpdateMode`).

**Pipeline order** (per axis): `Warp → Repeat → Window → FlipH → Curve eval → Response → FlipV`. Window applies on raw input, then Repeat multiplies the windowed result, then Warp bends, then the curve evaluates, then FlipV mirrors output, then Response bends output, then FlipH mirrors trigger timing. Window-then-Repeat (rather than Repeat-then-Window) was chosen so each repeated sub-section carries the windowed shape — gives `Bell + x3 + Polarize 50` three "rise-gap-fall" cycles, much richer than three half-bells with a single crop applied after.

- **Bit-pack cost**: 4 bits Repeat (2 per axis × 2 axes) + 6 bits Window (3 per axis × 2 axes) = 10 bits per stage × 16 stages = 160 bits total. `_data3` has 25 spare bits after `stageLen` (7 used); 10 used, **15 remain**.
- Layout (locked):
  ```
  _data3:
    bits  0..6   stageLen (7)                 [existing]
    bits  7..8   pitchRepeat (2)              [NEW Repeat]
    bits  9..10  temporalRepeat (2)
    bits 11..13  pitchWindow (3)              [NEW Window]
    bits 14..16  temporalWindow (3)
    bits 17..31  spare (15)
  ```
- Routable target? No for either — per-stage discrete enums, not natural CV targets.
- Pitch Repeat and Pitch Window are **Cell-mode only**; both ignored in Global pitch mode where `pitchRate` owns multi-cycle. See §6.2.4 / §6.2.5 for rationale.

**UI surface** (subject to the paged-footer layout in §17.2.1 — these slots are not yet wired):

Footer order for transform slots follows pipeline order: `Window → Repeat → Warp → FlipV → Response → FlipH`. Window and Repeat are NEW slots; they slot in BEFORE Warp on the encounter path. Existing slot positions (`Curve`, `Warp`, `Resp`, `FlipV`, `FlipH`, topic-specific F4) stay put.

- TEMP: add a Page-2 (P2) extension carrying `Window / Repeat / — / — / Next` (Window F1, Repeat F2 — pipeline order).
- PTCH Cell: same P2 pattern, `Window / Repeat / — / — / Next`.
- PTCH Global: Repeat and Window are no-ops (ignored in Global mode); no P2.
- ACCUM sets: not applicable.

**Render reference**: `ui-preview/window-preview/window-bell-bounce.png` shows Bell and Bounce at Off / F70 / F50 / P70 / P50.

**OPEN**: footer placement (P2 of TEMP/PTCH-Cell vs Shift+F variants) decide at UI wire-up time.

### Per-sequence: accumulator `sleep` — divisor on trigger advance

**Status: draft, not yet wired.**

Per-accumulator field on `AccumulatorConfig` (lives alongside `scope`, `order`, `polarity`, `reset`, `posLim`, `negLim`). Range 0..15 (4 bits), default 0.

**Semantic**: the counter advances every `(sleep + 1)` trigger events. `sleep = 0` (default) = advance every trigger (current behavior). `sleep = 3` = advance every 4th trigger; counter holds for 3 trigger events, then jumps by the full `step`. Higher values produce slower, chunkier drift.

**Compared to step magnitude**: lowering `step` gives smoother drift in smaller increments. Increasing `sleep` keeps the step size but introduces discrete plateaus — counter pauses, then jumps. Different musical character, especially with step values ≥ 2 (audible jumps with rest gaps between).

| step | sleep | Counter trajectory |
|---|---|---|
| 2 | 0 | 0, 2, 4, 6, 8, … (fast, smooth) |
| 1 | 0 | 0, 1, 2, 3, 4, … (half-speed, smooth) |
| 2 | 1 | 0, 0, 2, 2, 4, 4, … (same effective slope, stutter) |
| 2 | 3 | 0, 0, 0, 0, 2, 2, 2, 2, 4, … (slow + chunky plateaus) |

**Engine state**: per-counter sleep counter — `uint8_t _noteSleepTick[16]` + `uint8_t _pulseSleepTick[16]` = 32 bytes added. Increments on each trigger event; advances the main counter when it hits `sleep + 1`, then resets to 0.

**Storage**: 4 bits per accumulator on `AccumulatorConfig`. Current flags byte uses all 8 bits exactly (1 scope + 2 order + 1 polarity + 4 reset). Sleep needs a second flags byte — total `AccumulatorConfig` grows from 3 to 4 bytes (1 byte flags1 + 1 byte flags2 + 1 byte posLim + 1 byte negLim). Per-sequence cost across both N + P accumulators: 8 bytes total.

**Interactions**:

- **Sleep × Reset**: `sleep = 3, reset = 4` → counter holds for 4 trigger events between advances, then a 4-cycle auto-reset wipes the counter back to zero. Patient drift inside a phrase. When auto-reset fires, **also resets the sleep counter** so the next phrase starts fresh.
- **Sleep × Pendulum**: counter bounces normally, but each bounce step takes `sleep + 1` triggers to complete. Slower swing.
- **Sleep × Random**: re-rolls every `(sleep + 1)` trigger events. Sparse chaotic motion.
- **Sleep × Pulse-trigger**: with `*AccumTrigger = Pulse` and `sleep = 3`, the counter advances every 4th pulse fire (not every pulse). Lets dense pulse patterns drive sparse drift.
- **Sleep × hard reset (transport / pattern switch)**: clears sleep counters along with main counters. Both reset to zero.

**UI**: Shift+F2 or Shift+F3 on ACCUM.N / ACCUM.P set (both currently free in the post-MVP slot map). Encoder edits 0..15. Display as `Sleep N` or as a ratio `1:1..1:16`. Slot label kept short: `Sleep`.

**OPEN**: none — semantic fully specified. Bit-pack repack of `AccumulatorConfig` (3→4 bytes) is the only storage decision; second-flags-byte is the simpler choice over shrinking `reset` to 3 bits.

### Per-sequence: `warpNudge` / `responseNudge` / `pulseNudge` — performative global offsets

**Status: draft, not yet wired.**

Three per-sequence `Routable<int8_t>` fields that ADD to every stage's per-cell value live. Performative gestures — twist on stage, every cell shifts together. Modelled on NoteTrack's `transpose` (which shifts all step pitches uniformly).

| Field | Range | Default | Effect |
|---|---|---|---|
| `warpNudge` | ±64 | 0 | added to `temporalWarp` AND `pitchWarp` of every stage |
| `responseNudge` | ±64 | 0 | added to `temporalResponse` AND `pitchResponse` of every stage |
| `pulseNudge` | ±7 | 0 | added to `pulseCount` of every stage |

**One knob per concept, applies to both axes**: Warp Nudge moves both temporal and pitch warp simultaneously. Single performative knob per character, not split per-axis. (Per-axis variants — `tempWarpNudge`, `pitchWarpNudge` — can be added later as Tier-2 if audition shows they're needed; this keeps v1 simple.)

**Engine math** at the start of each stage's pipeline (per §6.1 / §6.2):

```
effectiveTempWarp     = clamp(stage.temporalWarp()     + seq.warpNudge(),     −64, +64)
effectiveTempResponse = clamp(stage.temporalResponse() + seq.responseNudge(), −64, +64)
effectivePitchWarp    = clamp(stage.pitchWarp()        + seq.warpNudge(),     −64, +64)
effectivePitchResponse= clamp(stage.pitchResponse()    + seq.responseNudge(), −64, +64)
effectivePulseCount   = clamp(stage.pulseCount()       + seq.pulseNudge(),    0, 16)
// engine then proceeds with effective* values everywhere the per-stage fields were previously read
```

Apply BEFORE the existing `effectivePulseCount` computation that incorporates `pulseAccOffset` (§13.4 / §13.10 Task 7). So the full pulse-count formula becomes:

```
effectivePulseCount = clamp(stage.pulseCount() + seq.pulseNudge() + pulseAccumCounter, 0, 16)
```

Order doesn't matter for the warp/response Nudge — it's a uniform offset before any non-linear bending.

**Storage**: 3 × `Routable<int8_t>` on `PhaseFluxSequence` = ~6 bytes per sequence. Across 17 sequences = ~102 bytes per track. PhaseFluxTrack 4772 → ~4874. Negligible against 9544 ceiling.

**Routing targets**: three new `Routing::Target::WarpNudge` / `ResponseNudge` / `PulseNudge`. CV-modulatable for live shaping (LFO, envelope, expression pedal via CV in).

**UI**: 3 new rows on `PhaseFluxSequenceListModel` (between existing rows). Labels short: `WarpN`, `RespN`, `PulsN`. Encoder edits the values.

**Combined with accumulator drift**: drift via accumulator (per-cell step values walk a counter) + bend via nudge (uniform live offset) = two independent live gestures with different feel. Accumulator gives evolving over time; nudge gives instantaneous global shift.

**OPEN**:

- Whether to split into per-axis (6 knobs) at all? Recommend no for v1; revisit if audition feedback says one axis needs separate control.

### Per-sequence: `cyclePhaseWarp` — macro swing across the 16-stage cycle (resolves §14.1 Q10)

**Status: draft, not yet wired.**

Single per-sequence `Routable<int8_t>` field, ±64, default 0. PowerBend transform applied to the master cycle phase `relativeTick / cycleTicks` BEFORE slot derivation (§3.2 per-tick math).

Engine math:

```
cyclePhase    = relativeTick % cycleTicks / float(cycleTicks)        // ∈ [0,1)
warped        = applyPowerBend(cyclePhase, seq.cyclePhaseWarp())     // PowerBend, same math as per-stage warp
effectiveTick = uint32_t(warped * cycleTicks)                        // re-map back to ticks
// slot derivation then proceeds from effectiveTick instead of relativeTick % cycleTicks
```

**Effect**: positive nudge stretches early stages and compresses late stages (or vice versa via negative). The 16-stage cycle gets a macro swing/lag character that's independent of per-stage `temporalWarp` (which warps within each stage).

**Composability**:
- Per-stage warp bends within each cell.
- `cyclePhaseWarp` bends WHERE in the cycle each cell happens.
- Both apply; effects compound.

**Routing target**: `Routing::Target::CyclePhaseWarp`. CV-modulatable for live macro-swing.

**Storage**: 1 × `Routable<int8_t>` per sequence = 2 bytes × 17 sequences = 34 bytes per track. Negligible.

**UI**: one new row on `PhaseFluxSequenceListModel` ("Cycle Warp"). Encoder edits ±64.

**Why this one and not "sequence-level Response on CV"**:
- Response on CV would bend the final voltage non-linearly. PhaseFlux's CV is integer scale-degree quantized via `Scale::noteToVolts()` — bending pre-quantization mangles scale-degree intuition; bending post-quantization breaks the integer-quantization contract that defines pitch in PhaseFlux (§6.2). Not viable.
- Response on time globally (bend trigger positions across the cycle) is mostly covered by `cyclePhaseWarp` — same family of non-linear time mapping. No need for a second variant.

**OPEN**: none — semantic fully specified and resolves §14.1 Q10.

### Per-sequence: `firstStage` / `lastStage`

NoteSequence-style stage-range gating. Stages outside `[firstStage..lastStage]` contribute zero to the cumulative table (treated as if `skip=true`).

- Fields: `firstStage`, `lastStage`, both 4-bit unsigned 0..15. Stored on `PhaseFluxSequence`. Validate `firstStage ≤ lastStage` at edit time.
- Semantics: snake-walk visits cells `kSnakeOrder[firstStage..lastStage]` and only those contribute their `stageDivisorTicks × sequenceDivisor / kReferenceSequenceDivisor` to the cycle. Stages outside the range act as `skip=true` for purposes of `computeCumulativeTicks` but their stored attributes are preserved.
- Routable? Probably no — discrete edit, not a continuous knob.
- **Bit-pack cost:** 8 bits total on `PhaseFluxSequence`. Cheap.
- Redundancy note: equivalent musically to setting `skip` on the excluded stages, but cheaper to edit and matches NoteSequence ergonomics. User flagged this as a convenience addition, not a strict need.
- **OPEN:** Default values (probably 0 / 15). Behaviour when `firstStage > lastStage` (clamp on set, or treat as "all stages skipped"?).

---

## 15. Deferred to v2

Explicit list. Any of these reappearing in MVP scope requires a written impact assessment and re-locking of this doc.

- Independent Y-axis clock (dual-clock per the original spec)
- Per-axis Ping-Pong / Random direction
- Walk pitch curve (RNG dependency)
- Sample/Hold temporal curve (RNG dependency)
- Flat pitch curve (covered by `pitchRange = 0.5oct` + `pulseCount = 1`; no enum slot needed)
- Exp / Log as separate enum slots — absorbed into `temporalWarp` knob via PowerBend (Linear+warp produces Exp/Log shapes)
- Mirror as a separate pitch enum slot — reachable as Bell+vflip (smooth) or Triangle+vflip (sharp)
- 4th base shape slot (1 spare in both `temporalCurve` and `pitchCurve` 2-bit enums) — reserved for v2
- Per-stage transforms still deferred: Reverse, Octave Shift, Humanize (RNG), Ratchet, per-trigger Probability (RNG). PHAS (phase shift) and MASK (temporal mute pattern) **moved in** to MVP §5–§6 as Flux borrows. MaskMelody+TiltMelody (pitch-degree centrality filter) also moved in from Stochastic borrow.
- Compression/Expansion (Flux `COMP`) — subsumed by `temporalResponse` PowerBend axis; not a separate field
- Per-degree bitmask (alternative to centrality-based MaskM+TiltM)
- Sequence-level global accumulator step + per-stage override (NoteTrack's Accumulator pattern) — PhaseFlux uses per-stage independent for MVP
- Accumulator advance mode C (per-full-16-cell-cycle). Modes A (per-pulse) and B (per-stage) are shipped via the per-stage `accumulatorTrigger` / `pulseAccumTrigger` Stage/Pulse enum (`PhaseFluxTrackEngine.cpp:421,428,703-711`).
- Accumulator ping-pong counter motion
- Per-track modulation input subsystem
- Fill alt sequence
- Harmony follower fields
- Pattern-switch quantised-to-bar option
- Mutation system

---

## 16. Engineering punch list — not product decisions

Items the implementer resolves; locked design above doesn't depend on them.

- Stage record packs to **3 × `uint32_t` (12 B)** with 3 spare bits (refined curve model: 93 bits/stage) — use the NoteSequenceStep BitField pattern (`model/Bitfield.h`, anonymous unions with `raw` + named slots). 16 stages × 12 B = 192 B for stage data per sequence.
- **Serialization version: do NOT bump.** Dev branch stays on the existing `Latest` per `src/apps/sequencer/model/ProjectVersion.h` policy. Add new model fields by writing/reading them unconditionally — no `dataVersion() >= VersionN` guards, no placeholder constant. Old dev project files on SD card are accepted to break across branches. Bumps happen only at user-driven release prep.
- Same-branch read/write symmetry: round-trip test gate per retro #4 (any field added to `write()` must be read back identically by `read()` in the same build — orthogonal to version policy).
- Default `_resetMeasure` UI step granularity (powers-of-two from NoteSequence)
- Whether PhaseFlux participates in Project-level scale/rootNote inheritance
- UI page implementation (layout locked in §17; only pixel-level details remain)
- RAM measurement on STM32 target before Phase 1 approval
- Adversarial review schedule (after model layer, after engine, before merge)

---

## 17. UI surfaces — modelled on NoteTrack

PhaseFlux exposes three UI surfaces — same shape as NoteTrack. Visual reference for the edit page is `ui-preview/phaseflux-preview.png` (rendered by `ui-preview/pages_phaseflux.py`).

### 17.1 Three new UI files

| File | Class | Base | Role |
|---|---|---|---|
| `ui/pages/PhaseFluxEditPage.h/.cpp` | `PhaseFluxEditPage` | `BasePage` | Main playing/editing surface. 4×4 grid on left + dual scopes (temporal + pitch) on right in default view. When a top-row hardware key is held, right pane swaps to 8 named param boxes mapped to the bottom-row keys. Encoder rotates the focused value. Left/right arrows toggle `<1-8>` ↔ `<9-16>` bank. Equivalent of `NoteSequenceEditPage`. |
| `ui/pages/PhaseFluxSequencePage.h/.cpp` | `PhaseFluxSequencePage` | `ListPage` | Sequence-level list. Backed by `PhaseFluxSequenceListModel`. Equivalent of `NoteSequencePage`. |
| `ui/model/PhaseFluxSequenceListModel.h` | `PhaseFluxSequenceListModel` | `RoutableListModel` | Backs the sequence list page (§17.3). |
| `ui/model/PhaseFluxTrackListModel.h` | `PhaseFluxTrackListModel` | `RoutableListModel` | Backs the shared `TrackPage` when track is in PhaseFlux mode (§17.4). Equivalent of `NoteTrackListModel`. |

### 17.2 PhaseFluxEditPage — visual layout

Inside the 256×42 px safe content area (header/footer guards per `ui-preview/UI-VARIANTS.md`):

- **Left half (x 0..45)**: 4×4 grid of 9×9 px cells with 1 px gap.
  - Active (playing) cell — inverted bright fill.
  - Selected cell — bright outline.
  - `skip=true` — hollow X.
  - pulseCount visualised as a tiny horizontal bar across the bottom of the cell (0..16 px wide, clamped to cell width). pulseCount=0 draws no bar (silent stage).
  - `phaseShift ≠ 0` — top-left corner dot.
  - `mask ≠ Off` — top-right corner dot.
- **Selection vs active cell**: NoteTrack/Stochastic convention. The **selected** cell (bright outline, scopes/params display its values) is set by the player via top-row hardware key and **stays put** as the cursor advances. The **active** cell (currently playing, inverted fill) moves independently as the engine ticks. Encoder rotation always edits the user-selected cell, regardless of which cell is playing. No "follow" mode in MVP.
- **Right half (x 50..253) — default view**: two side-by-side scopes ~100×38 each (temporal left, pitch right) with a 1 px divider at x=152.
  - Temporal scope: curve trace + per-pulse marks **vertically centred** at `y = ScopeY + ScopeH/2`. Spacing matches the engine's §6.1 `i/N` formula (last pulse at `(N-1)/N`, not the cell boundary). Inset 2 px from the scope outline so 3-wide marks stay 1 px clear of the edge at both extremes. Three glyphs:
    - **3×3 hollow ring** — one per base pulse (`stage.pulseCount()`). `Color::Medium` by default; flips to `Color::Bright` once the playhead crosses its trigger time in the active cell.
    - **3 wide × 2 tall U** (open top, closed bottom) — one per accumulator-added extra pulse. Same dim/bright behaviour. Hidden if `pulseAccOffset` is negative (decreased pulses don't render — only the `effective = clamp(base + offset, 0, 16)` pulses appear).
    - **Single `Color::Low` centre dot** — muted pulse (`(maskByte >> ((i + maskShift) & 7)) & 1`).
  - Pitch scope: curve trace + bright dot at each fired pulse's sampled CV.
  - **No parameter labels** — pure shape feedback.
- **Right half — edit-hold view** (top-row stage key held): 8 boxes ~24×38 each, one per bottom-row hardware key, spanning x=50..253.
  - Each box: NAME (top), value (bottom), separator line between.
  - Cursor focus drawn with bright outline. Encoder rotation edits the focused value.
  - **Names visible only in this view** — borrowed pattern from `StochasticSequenceEditPage`.
- Header gutter (y=6, right side): bank indicator `<1-8>` or `<9-16>`.
- Footer F-keys (default view): STAGE / TIME / PITCH / DIV / TRK.
- Footer F-keys (edit-hold view): TIME / CURVE / XFRM / ACCM / TRK — cycle param banks for the 16 params not shown on the bottom row (basePitch, pitchRange, pitchDirection, pitchResponse, temporalResponse, temporalFlipV, temporalFlipH, pitchFlipV, pitchFlipH, maskMelody, tiltMelody, maskShift, accumulatorStep, accumulatorLength, gateLength, skip).

### 17.2.1 Footer pages — all topics use F5 = Next

All four topics (TEMP / PTCH / ACCUM.N / ACCUM.P) page their footer with **F5 = Next** cycling `_topicPage` between 0 and 1. L/R nav (topic change) resets `_topicPage = 0` and `_selectedSlot = 0`. Shift-modifier bindings are dropped entirely — binary toggles (`FlipV`, `FlipH`) are first-class on Page 1 slots and fire on plain F-press without selecting (no encoder edit needed).

**TEMP**
| Page | F1 | F2 | F3 | F4 | F5 |
|---|---|---|---|---|---|
| 0 / shape | `Curve` | `Warp` | `Resp` | `Puls` | `Next` |
| 1 / mods  | `Len` | **`FlipV`** | **`FlipH`** | `Mask` | `Next` |

**PTCH (Cell mode)** — header `PTCH`, stage badge = stage number:
| Page | F1 | F2 | F3 | F4 | F5 |
|---|---|---|---|---|---|
| 0 / shape | `Curve` | `Warp` | `Resp` | `Note` | `Next` |
| 1 / mods  | `Span` | `Dir` | **`FlipV`** | **`FlipH`** | `Next` |

**PTCH (Global mode)** — header `PTCH.G`, stage badge = `G`, scope draws `stage[0]`'s curve:
| Page | F1 | F2 | F3 | F4 | F5 |
|---|---|---|---|---|---|
| 0 / shape | `Curve` | `Warp` | `Resp` | `Rate` | `Next` |
| 1 / mods  | `Note` | `Span` | **`FlipV`** | **`FlipH`** | `Next` |

**ACCUM.N / ACCUM.P** — see `docs/accumulator-v2-spec.md §13.5`.

Bold slots = press-to-flip (no slot selection, no encoder edit). All other slots: F-press selects, encoder edits.

`Note` was previously labelled `Base` for Cell mode — renamed to `Note` for label clarity (the value is rendered as a note name via `scale.noteName(... Format::Long)`). `Dir` cycles `PitchDirectionType` (Up / Down / Bipolar).

In PTCH Global, P1 `Note` / `Span` edit the **active** stage (per-cell anchor stays per-cell), while P0 `Curve` / `Warp` / `Resp` edit `stage[0]` (the master pitch curve). P1 flips also operate on `stage[0]`.

Pitch scope visual (`drawPitchScope`):
- Same outer container as TEMP scope (`100 × 38` at `(scopeX, ScopeY)`).
- Internal split: 22 px **label column** on the left (`x = scopeX..scopeX+21`), 1-px divider at `x = scopeX+22`, then **trace zone** (`x = scopeX+23..scopeX+98`).
- Curve trace at `Color::Medium` lives only in the trace zone.
- Pulse-fire 2×2 dots on the curve at each unmuted pulse's (t, pitch). Spacing uses `i/N` (matches engine §6.1). Dim by default; brighten as the playhead crosses each in the active cell.
- Label column shows **top-4 reachable scale degrees by visit count** (sampling the pitch pipeline at 64 phi-points + quantizing). Always includes the currently-playing degree (pushes lowest-count entry out). Labels right-aligned at `x = scopeX + 20`, evenly distributed in 4 slots. Currently-playing label drawn with an inverted highlight box.

Render variants live in `ui-preview/phaseflux-pitchmode/` and `ui-preview/pitch-scope/`.

### 17.3 PhaseFluxSequenceListModel items

Subset of `NoteSequenceListModel` per spec §12.2:

| Item | Routing target | Source field |
|---|---|---|
| `Divisor` | `Routing::Target::Divisor` | `_divisor` (Routable, §12.2) |
| `ClockMult` | `Routing::Target::ClockMult` | `_clockMultiplier` (Routable, §12.2) |
| `GlobalPhase` | — | `_globalPhase` (CurveTrack precedent) |
| `PitchMode` | — | `_pitchMode` (Cell/Global, §6.2.3) |
| `PitchRate` | — | `_pitchRate` (P:T ratio index, §6.2.3) |
| `ResetMeasure` | — | `_resetMeasure` (§3.3) |
| `Scale` | `Routing::Target::Scale` | `_scale` (optional, `−1 = inherit`) |
| `RootNote` | `Routing::Target::RootNote` | `_rootNote` (optional, `−1 = inherit`) |

Dropped vs Note: `Mode` (only one PhaseFlux variant), `RunMode` (snake is self-managed), `FirstStep`/`LastStep` (per-cell `skip` replaces), all `DivisorY*` (dual-clock deferred to v2), `NoteFirstStep`/`NoteLastStep` (Ikra-specific).

### 17.4 PhaseFluxTrackListModel items

Subset of `NoteTrackListModel` per spec §12.1 — same shape as Stochastic's borrow:

| Item | Routing target | Source field |
|---|---|---|
| `PlayMode` | — | `_playMode` |
| `FillMode` | — | `_fillMode` (stored, Fill deferred to v2) |
| `CvUpdateMode` | — | `_cvUpdateMode` (default `Gate`) |
| `SlideTime` | `Routing::Target::SlideTime` | `_slideTime` (Routable, §12.1) |
| `Octave` | `Routing::Target::Octave` | `_octave` (Routable, §12.1) |
| `Transpose` | `Routing::Target::Transpose` | `_transpose` (Routable, §12.1) |

Dropped vs Note: `FillMuted` (Fill deferred), `Rotate` (not in spec), all four probability-bias knobs (no probability fields to bias).

### 17.5 Access wiring — three touch-points

Three keys reach the three surfaces, dispatched on `track.trackMode()`:

#### Page+SequenceEdit (or double-tap Track) → PhaseFluxEditPage

`ui/pages/TopPage.cpp::setSequenceEditPage()` (existing switch ~lines 354-380) add:

```cpp
case Track::TrackMode::PhaseFlux:
    setMainPage(pages.phaseFluxEdit);
    break;
```

Also extend the `onSequenceEditView` page-identity check in `TopPage::keyDown()` (~line 97) to include `currentPage == &pages.phaseFluxEdit` so track-switching while on the edit page navigates the new track's PhaseFlux edit surface.

#### Page+Sequence → PhaseFluxSequencePage

`ui/pages/TopPage.cpp::setSequenceView()` (existing switch ~lines 296-348) add:

```cpp
case Track::TrackMode::PhaseFlux:
    setMainPage(pages.phaseFluxSequence);
    break;
```

#### Page+Track → shared TrackPage uses PhaseFluxTrackListModel

`ui/pages/TrackPage.cpp::setTrack()` (~line 152) add:

```cpp
case Track::TrackMode::PhaseFlux:
    _phaseFluxTrackListModel.setTrack(track.phaseFluxTrack());
    newListModel = &_phaseFluxTrackListModel;
    break;
```

Declare the backing model in `ui/pages/TrackPage.h`:

```cpp
PhaseFluxTrackListModel _phaseFluxTrackListModel;
```

### 17.6 Pages.h registration

`ui/pages/Pages.h` (the page aggregator, ~line 63) — two new includes + two new members:

```cpp
#include "PhaseFluxEditPage.h"
#include "PhaseFluxSequencePage.h"
...
PhaseFluxEditPage phaseFluxEdit;
PhaseFluxSequencePage phaseFluxSequence;
```

### 17.7 Model-layer prerequisites

Must exist before any UI wiring will compile (locked by §12, not yet built):

- `Track::TrackMode::PhaseFlux` enum value in `model/Track.h`
- `Track::phaseFluxTrack()` accessor returning `PhaseFluxTrack&`
- `Project::selectedPhaseFluxSequence()` accessor returning `PhaseFluxSequence&`
- `PhaseFluxTrack`, `PhaseFluxSequence`, and the stage record type from §5 and §12

---

## 18. Testing & MVP acceptance

### 18.1 Test tiers — tiered delivery

Tests land in three phases. Each phase gates the next.

**Phase A — math & storage foundations** (must pass before any engine code beyond skeletons):

| Test family | Coverage |
|---|---|
| Serialization round-trip | Every field of `PhaseFluxTrack`, `PhaseFluxSequence`, stage record: write → clear → read → assert equality. Retro lesson #4 gate. |
| Cumulative duration table | 16 cells × various stageDivisors + skip permutations → expected sums. Verify `cycleTicks = cumulativeTicks[16]`. Verify skip cells contribute 0. |
| Per-tick derivation | `relativeTick → cycleTick → slotIdx → activeCell + stagePhase` for synthetic tick streams. Verify boundary cases (slot transitions, cycle wraparound). |
| PowerBend encoding | `SignedValue<7>` ±63 → user-float ±0.984 via `/64`. Verify formula at p=0 (identity), p=±0.5 (Exp/Log-like). |

**Phase B — first hardware guard** (in spec history: "hear stages with sub-stage curved playback and transforms"):

| Test | Coverage |
|---|---|
| HW boot smoke test | Boot STM32, switch a track to PhaseFlux, hear default 1/16 click train at scale tonic. |
| Sub-stage curves audible | Set a cell to pulseCount=8 + temporalCurve=Bell (or Bounce). Verify audible curve shape on hw. |
| Transforms audible | Set phaseShift=2 (90°) and mask=1in2 on a stage. Verify rotation + thinning on hw. |

**Phase C — remaining test families** (post-hw-guard, can trail Phase B by 1-2 weeks):

| Test family | Coverage |
|---|---|
| Skip mask correctness | Cursor walks past skipped cells; cumulative table absorbs zero-width slots. |
| kMinCycleTicks floor | Edge case: most cells skipped, cycleTicks would underflow. Verify scaling to floor. |
| Idle state | All-skipped → engine emits gate low, holds CV, freezes counters. |
| Phase shift mod | Pulse times correct after phaseShift × 45° rotation. |
| Temporal mask × shift | All 8 mask patterns × all 8 shifts × pulseCount range. Gate suppressed correctly per `(i + maskShift) mod 8`. |
| Melody mask (centrality+tilt) | Byte-for-byte match against Stochastic `engine/StochasticTrackEngine.cpp:454-466`. |
| Pulse schedule (§6.4) | Overlap-allowed schedule (no collision drop). gateLength rules: 0 → silent; 1 → audible-minimum floor at 6 ticks; ≥ 2 → percent × pulse_period, drop if < 6. |
| Accumulator lifecycle | Counter advance on slotIdx change; mod-N wrap; preservation across Reset Measure; clear on pattern switch (§7.1 asymmetry). |
| Pitch quantization | `degree → noteToVolts` across all defined scales. Octave + transpose application. Scale-extends-infinitely (out-of-nominal-range degrees). |
| Curve LUTs at φ=0, 0.25, 0.5, 0.75, 1 | Linear/Bell/Bounce + Ramp/Bell/Triangle, including vflip + hflip composition. |
| Cumulative-recompute side effects | Edit triggers gateTimer + pulseFired cleared per §3.4. Counters NOT cleared. |
| Transport behavior | Stop / Start / Continue per §11.1 — counter preserve/clear semantics, gate-off-on-stop. |

### 18.2 MVP acceptance — comprehensive lock

PhaseFlux is "MVP done" only when **all** of the following pass. None is optional.

| Gate | Verification |
|---|---|
| **Engine functional** | Every §5 field wired through every §6 math path. All 3 base shapes × 4 flip variants + 3 directions + 2 masks reach audible output. Idle state correctly engaged when all-skipped. |
| **§17 UI surfaces complete** | `PhaseFluxEditPage` (4×4 grid + dual scopes + 8-box edit-hold strip) + `PhaseFluxSequencePage` + `PhaseFluxTrackListModel` all live, dispatched via TopPage/TrackPage as in §17.5. All 24 per-stage fields editable. |
| **Phase A + B + C tests pass** | Per §18.1 — round-trip + math + hw smoke + full coverage all green. |
| **STM32 RAM ceilings respected** | `make -C build/stm32/release sequencer`. Verify `Track::_container` and `Engine::TrackEngineContainer` did not grow past PROJECT.md §244-272 ceilings (NoteTrack 9544 B / TeletypeTrackEngine 912 B). Document any growth with worked-example justification. |
| **Boot-to-sound** | STM32 module boots, project loads, PhaseFlux track makes audible sound at default settings within 30s of power-on. |
| **Pattern A/B/C switching** | Switch between patterns mid-play. Verify full reset per §11 (counters cleared, cursor → (0,0), gate cleared). |
| **Snapshot save/load** | Per-pattern snapshot saves and reloads with full state intact (cursor position, accumulator counters, transient engine state). Round-trip preserves output. |
| **Routable<>-via-global-Routing verified** | A CV input routed to `Divisor`, `ClockMult`, `Octave`, `Transpose`, `SlideTime`, `Scale`, `RootNote` audibly affects PhaseFlux output. No PhaseFlux-specific Routing UI needed (per §10) — global Routing chassis handles it. |
| **All retro lessons (§13) gate-passed** | Each row of §13 verified true on the shipped firmware, not just spec text. |
| **Adversarial review clean on running firmware** | Re-run the same audit that produced the spec-level reviewer findings, but on the running engine. Surface and fix any divergence from spec. |
