# Node Spec — PhaseFlux Polyphonic LFO Node Engine

> A new track-type: 8 LFO nodes tracing triangle/sine paths across XY space, triggering MIDI on boundary crossings. Extends PhaseFlux's tick infrastructure with Lissajous motion instead of grid traversal.

## 0. What is Node?

Node is an **8-voice LFO node engine** — each node is a pair of oscillators (X + Y) whose **triangle or sine** wave outputs trace Lissajous-like paths across a 2D field. When a node's trajectory crosses a configurable boundary line, a MIDI trigger fires with pitch and velocity derived from the crossing position.

**Waveforms are triangle and sine only** — no saw, no square, no noise. This matches the original hardware design: triangle gives linear ramps that make boundary crossings predictable and evenly spaced; sine gives smooth curves that make crossings cluster at peaks and troughs. These two shapes cover the useful Lissajous territory:
- Triangle X + Triangle Y → diamond patterns with even boundary crossings
- Triangle X + Sine Y → parabolic arcs, crossings cluster at Y extremes
- Sine X + Sine Y → elliptical patterns, crossings cluster at both extremes
- Sine X + Triangle Y → inverted parabolic arcs

The conceptual ancestry is **NOD-E by Antonio Blanca** (Reaktor User Library #6679) — a generative MIDI sequencer where 8 nodes travel in XY space driven by LFOs, triggering notes at boundary crossings. The Buchla 245, HetrickCV Phase Driven Sequencer, and Mutable Instruments Marbles are secondary influences. This spec is a ground-up hardware port of NOD-E's core algorithm: triangle/sine LFO pair per node, XY boundary-crossing triggers, Y-axis velocity mapping, scale quantization, and cross-modulation — translated into the Performer's Track/Sequence/Engine architecture with stateless-ramp timing and recipe-replay storage.

---

## 1. Class shape

New TrackMode trio, peer to Note / Curve / Stochastic / Tuesday / PhaseFlux / DiscreteMap:

- `NodeTrack` (model)
- `NodeSequence` (model, one per pattern slot)
- `NodeTrackEngine` (engine)

Shares PhaseFlux's curve LUTs and accumulator ops infrastructure. Does NOT inherit from PhaseFlux — distinct model/engine with borrowed utilities.

### 1.1 NOD-E panel mapping

The original **NOD-E by Antonio Blanca** (Reaktor ensemble #6679) is the reference design. This spec translates its panel layout into the Performer's Track/Sequence/Engine architecture:

| NOD-E Panel Section | Performer Mapping | Track/Sequence Scope |
|---|---|---|
| **NODE TUNING** — octave, pitch bar offsets, scale/key | `NodeTrack` (baseNote, scale) + `NodeSequence` (pitchOffset[8]) | Track owns base scale/key; Sequence owns per-node offsets |
| **XY DISPLAY** — node positions, trails, boundaries | EditPage XY visualization (render-only, not a control surface) | Engine reads positions, UI draws dots |
| **VECTOR SETTINGS** — Wave X/Y, Phase X/Y, Freq X/Y, Var X/Y, AM X/Y, Freq XY% | `NodeSequence` (waveX, waveY, phaseXOffset[8], phaseYOffset[8], freqX, freqY, amX, amY, xyMod) | All sequence-scoped |
| **NOTE TRIG** — trigger mode, quantize grid, left/right boundaries | `NodeSequence` (boundaryX[4], boundaryY[4]) + trigger logic in engine | Boundaries are per-sequence; trigger mode is fixed (X-axis crossing) for MVP |
| **NOTE PITCH** — transpose, base note, random threshold, step size | `NodeTrack` (baseNote) + `NodeSequence` (pitchOffset[8], accumulator drift) | Base note is track-global; offsets and drift are per-sequence |
| **NOTE VELOCITY** — invert, min/max velocity | `NodeSequence` (velScale) + `NodeTrack` (velocity range) | Velocity scaling per-sequence; invert is a sign bit |
| **GLOBALS** — node size (display), line/trail, MIDI channel, voice allocation | Engine config (monophonic for MVP) + display params | Not stored in model; runtime-only |

**NOD-E's "Var X / Var Y"**: This is randomization amount added to the respective LFO's frequency. In the Performer, this maps to a future v2 feature (per-node frequency jitter). MVP defers this — all frequency is deterministic.

**NOD-E's "Freq XY%"**: This is cross-modulation — X output modulating Y frequency. Direct 1:1 mapping to `xyMod` in `NodeSequence`.

---

## 2. Grid / traversal

**No grid.** Node replaces the 4×4 boustrophedon snake with 8 independently moving XY trajectories.

Each node is a pair of phasors (X and Y) whose phase advances per tick. The node's position at any tick is a pure function of `relativeTick`:

```
node[i].phaseX = (phaseXOffset[i] + relativeTick × freqX) mod 1.0
node[i].phaseY = (phaseYOffset[i] + relativeTick × freqY) mod 1.0
node[i].x = waveX(phaseX) × amX           // shaped, amplitude-scaled
node[i].y = waveY(phaseY) × amY
```

There is no cursor. There is no visitation order. There is no skip. The "traversal" is the continuous trajectory of each node through XY space, and events are generated when these trajectories cross boundary lines (see §3).

---

## 3. Timing model — boundary crossing as trigger

### 3.1 Core principle

PhaseFlux triggers on **slot transitions** (discrete grid position changes). Node triggers on **boundary crossings** (continuous trajectory passing a threshold line in XY space).

A boundary crossing is detected by comparing current and previous position:

```
crossed = (x_prev < threshold && x_current >= threshold) ||
          (x_prev >= threshold && x_current < threshold)
```

This exactly mirrors `go.ramp2trig` from *Generating Sound & Organizing Time* Ch 2 — detect the wrap/jump of a signal between two samples. The "previous" value is stored per-node, per-axis.

### 3.2 Per-tick computation

For each of 8 nodes, per tick:

1. **Advance phase**: `phaseX += freqX_per_tick; phaseY += freqY_per_tick` (accumulated `float`, mod 1.0)
2. **Apply phase offsets**: `effectivePhaseX = phaseX + phaseXOffset[nodeIdx]; effectivePhaseY = phaseY + phaseYOffset[nodeIdx]`
3. **Apply cross-modulation** (XY%): `freqY_current = freqY × (1.0 + xyMod × x_output)` — X output modulates Y frequency (FM routing)
4. **Waveform shaping**: `x = waveFn(effectivePhaseX, waveX); y = waveFn(effectivePhaseY, waveY)` where waveFn is triangle or sine only
5. **Amplitude scaling**: `x_output = x × amX; y_output = y × amY`
6. **Boundary detection**: For each configured boundary line on each axis, check crossing from `x_prev` → `x_output` (and Y axis similarly)
7. **On crossing**: Sample the crossing axis value for velocity, the other axis for secondary parameter. Scale to MIDI range. Quantize pitch to scale.

### 3.3 Reset measure

Identical to PhaseFlux: `_resetTickOffset = tick` on reset boundary. All node phases are preserved across reset (§7: accumulators persist). After reset, `relativeTick = 0`, so all phase offsets recalculate from the same origin.

### 3.4 Knob-turn behavior

Editing frequency, amplitude, or waveshape during playback:
- Frequency/amplitude: takes effect **next tick** — phases continue from wherever they are. No discontinuity in position (only in rate/depth).
- Phase offset: takes effect next tick. Potential phase discontinuity at offset change — acceptable for MVP, same behavior as DiscreteMap.
- Boundary position: recomputed next tick. If a boundary moves past a node's current position, no retroactive trigger — crossing is only detected in forward time.

Accumulator counters (**not** cleared on knob turn — see §7).
Boundary-crossing edge memory (**is** cleared on boundary layout change — prevents ghost triggers from stale edges).

### 3.5 Idle state

Node enters idle when `amX = 0 && amY = 0` (all nodes still). Gate held low. CV freezes at last value. Phase accumulation **continues** — the internal phasors keep running even when output amplitude is zero, so resuming from idle snaps to the current trajectory rather than resetting.

---

## 4. Identity / storage model

**Recipe-replay** — same as PhaseFlux. Every parameter is stored (not derived). No accumulated cursor state to persist; the only per-pattern state is the phase offsets themselves (if user wants restart-from-zero pattern behavior, that's `resetMeasure`).

State ownership:

| Field | Track | Sequence | Engine | UI |
|---|---|---|---|---|
| freqX, freqY | — | ✓ | — | ✓ |
| waveX, waveY | — | ✓ | — | ✓ |
| amX, amY | — | ✓ | — | ✓ |
| xyMod | — | ✓ | — | ✓ |
| phaseXOffset[8] | — | ✓ | — | ✓ |
| phaseYOffset[8] | — | ✓ | — | ✓ |
| boundaryX[], boundaryY[] | — | ✓ | — | ✓ |
| baseNote | ✓ | — | — | ✓ |
| scale | ✓ | — | — | ✓ |
| resetMeasure | — | ✓ | — | ✓ |
| accumulators[8] | — | — | ✓ (transient) | — |
| xPrev[8], yPrev[8] | — | — | ✓ (transient) | — |
| phaseX[8], phaseY[8] | — | — | ✓ (transient) | — |

Engine transient state resets on transport stop/start per DiscreteMap precedent.

---

## 5. Per-cell / per-step record

Node doesn't use the 16-cell grid. Instead, per-sequence storage is:

### NodeSequence (per pattern)

```
BitField layout — NodeSequence (draft, not final bit assignment):

freqX: UnsignedValue<8>       // 8 bits, maps via /127 → 0..1.0 (phasor frequency multiplier)
freqY: UnsignedValue<8>       // 8 bits, maps via /127 → 0..1.0
waveX: UnsignedValue<1>       // 1 bit, enum: triangle=0, sine=1
waveY: UnsignedValue<1>       // 1 bit, same enum
amX: UnsignedValue<7>         // 7 bits, /64 → 0..0.984 (PhaseFlux clamp convention §17)
amY: UnsignedValue<7>         // 7 bits, /64 → 0..0.984
xyMod: SignedValue<7>         // 7 bits, /64 → ±0.984, X→Y frequency modulation depth
phaseXOffset[8]: UnsignedValue<7> each  // 56 bits total, per-node phase offset /64 → 0..0.984
phaseYOffset[8]: UnsignedValue<7> each  // 56 bits total
boundaryX[4]: UnsignedValue<7> each      // 28 bits, 4 X boundary positions /64 → 0..0.984
boundaryY[4]: UnsignedValue<7> each      // 28 bits, 4 Y boundary positions
pitchOffset[8]: SignedValue<4> each     // 32 bits, per-node semitone offset ±7
velScale: UnsignedValue<7>             // 7 bits, overall velocity scaling

Total: 8+8+1+1+7+7+7+56+56+28+28+32+7 = ~246 bits ≈ 31 bytes per sequence
```

### NodeTrack (global)

```
baseNote: UnsignedValue<7>     // 7 bits, MIDI note 0..127 (/64 clamp)
scale: UnsignedValue<5>        // 5 bits, enum index into scale table (32 scales)
resetMeasure: UnsignedValue<8> // 8 bits, 0=off, 1..128 measures
```

Bit budgets are approximate; final layout in §16 after design iteration.

---

## 6. Curve math — triangle and sine only

Node uses **two waveform shapes**: triangle and sine. No saw, no square, no noise.

- **Triangle** (wavetype 0): Linear ramp from -1 to +1 and back. Produces diamond-shaped Lissajous patterns when both axes are triangle. Boundary crossings are evenly spaced in time — every crossing happens at the same interval.
- **Sine** (wavetype 1): `sin(2π × phase)`. Produces elliptical Lissajous patterns when paired with another sine, or parabolic arcs when paired with triangle. Boundary crossings cluster near the peaks and troughs of the sine wave.

**Implementation**: Direct math, not LUT. Triangle is `4 × abs(phase - 0.5) - 1.0` (bipolar triangle from unipolar phase). Sine is `sin(2π × phase)`. No LUT needed for two shapes.

**PowerBend**: Not applicable. Triangle and sine are their own shapes — no curve warping. If a user-wants-nonlinear-response knob is added in v2, it would be applied as a unit shaper on the output amplitude (mapping velocity/pitch curvature), not on the waveform itself.

**Cross-modulation (XY%)**: `freqY_current = freqY × (1.0 + xyMod × x_output)`. This is standard FM routing: the X oscillator's output modulates the Y oscillator's frequency. When xyMod = 0, both axes run independently. When xyMod = 1.0, Y frequency is pulled by ±1× the X output amplitude. This produces the classic Lissajous frequency-ratio patterns (1:1 circles, 1:2 figure-eights, 1:3 trefoils, etc.) when both axes are sine, and complex folded patterns when one or both are triangle.

---

## 7. Accumulator

Node has 8 independent accumulators, one per node. Each is a **persistent drift counter** preserved across boundaries and resets, exactly like PhaseFlux.

```
counter[nodeIdx] += (boundaryCrossedThisTick ? increment[nodeIdx] : 0)
```

The `increment` per accumulator is user-configurable per-sequence (semitone offset per crossing). Accumulator values are the running pitch offset added to `baseNote` on each trigger.

**Accumulators are NOT cleared on reset.** They are NOT cleared on knob-turn. They persist until:
- Explicit INIT (context menu or quick-edit)
- Pattern change (new pattern loads with zeroed accumulator state)

This gives Node its "evolving melody" character: each boundary crossing shifts the pitch by a stepped amount, and the melody drifts through pitch space rather than repeating a fixed pattern.

---

## 8. Transforms

### 8.1 Global transforms (per-Sequence scope)

| Transform | Scope | Description |
|---|---|---|
| Scale snap | Sequence | Quantize every trigger's pitch to the nearest note in the selected scale |
| Vel scale | Sequence | Scale all velocities by `velScale` (0..0.984) |
| Reset measure | Sequence | Synchronize all node phases on bar boundaries |
| Accumulator reset | Sequence | INIT context-menu target |
| Cycle length | Sequence | Fixed vs Adaptive (same semantics as PhaseFlux) |

No per-stage transforms — there are no stages. Nodes move continuously.

### 8.2 Order of application (on each trigger):

1. Accumulator addition (drift the pitch by `pitchOffset` per crossing)
2. Scale quantization (snap to scale)
3. Velocity scaling (apply `velScale`)
4. Gate timer start (Note On with computed duration)

---

## 9. Outputs

### Jack assignments

| Output | Signal |
|---|---|
| CV A | Pitch voltage (quantized to scale) |
| CV B | Y-axis position at trigger time (continuous, 0..10V mapped from y_output) |
| Gate A | Note trigger (pulse on boundary crossing) |

Multi-jack mirroring: Gate A fires for all 8 nodes monophonically (last-node-wins if simultaneous). CV A tracks the most recent node's pitch. CV B tracks the most recent node's Y position.

**Polyphonic output is deferred to v2.** MVP is monophonic: any node crossing any boundary fires a single note. If two nodes cross in the same tick, the last-evaluated node wins (node 7 > node 6 > ... > node 0).

### Quantization model

Same as PhaseFlux: scale-degree integer storage + `Scale::noteToVolts()` at output. `pitchOffset` per-node is stored as a semitone offset integer (±7 range), quantized against the `Scale` table after accumulator addition.

---

## 10. Modulation inputs

Node uses the global Routing system (same as PhaseFlux). All per-sequence parameters are Routable:

- `freqX`, `freqY` (frequency modulation from CV)
- `amX`, `amY` (amplitude modulation / depth)
- `xyMod` (cross-modulation depth)
- `velScale` (velocity scaling from external CV)

Per-track `baseNote` and `scale` are NOT routable (set from UI only).

**Defer policy**: Modulation of boundary positions and phase offsets is v2. MVP exposes them as fixed per-sequence values only.

---

## 11. Performer integration

| Feature | Participation |
|---|---|
| Mute | Gate held low, CV freezes, phase accumulation continues |
| Solo | Standard solo (other tracks muted) |
| Pattern | Full pattern switch with accumulator-preservation option |
| Snapshot | All NodeTrack + NodeSequence fields captured |
| Fill | v2 — no MVP fill mode (continuous-motion tracks don't have an obvious "fill" gesture) |

Transport behavior mirrors PhaseFlux/DiscreteMap:
- **Stop** → phase accumulation halts, gate held low
- **Start** → phase accumulation resumes from last position (not reset)
- **Continue** → seamless (phase was never halted, gate re-opens)

---

## 12. Performer alignment — Track / Sequence borrowed fields

Borrow pattern from PhaseFlux / NoteTrack:

| Field | Source | NodeTrack equivalent |
|---|---|---|
| `_playMode` | NoteTrack | Reuse `PlayMode::Aligned` only (MVP) |
| `_fillMode` | NoteTrack | Not used (no fill in MVP) |
| `_cvUpdateMode` | Stochastic | Reuse `CvUpdateMode` enum |
| `_slideTime` | Stochastic | Not used (Node doesn't slide between pitches) |
| `_octave` | NoteTrack | → `baseNote` (mapped octaves) |
| `_transpose` | NoteTrack | Part of `pitchOffset` per node |
| `_resetMeasure` | DiscreteMap | Verbatim |
| `_cycleLength` | PhaseFlux | Verbatim (Fixed / Adaptive) |

Routable wrappers follow Stochastic borrow pattern (§5 of spec template).

---

## 13. Retro lesson gates

| Retro Lesson | Node MVP Satisfies |
|---|---|
| R1: Accumulated state drifts | ✓ Accumulators drift intentionally — that's the design |
| R2: Skip breaks cycle math | N/A — no skip, no grid ("Fixed" cycle concept doesn't apply the same way; see §3.5) |
| R3: Curve discontinuity on edit | ✓ Waveform change takes effect next tick, no discontinuity in output position |
| R4: Gate held past boundary | ✓ Gate timer model from NoteTrack, explicit Note Off scheduling |
| R5: Divisor zero → div-by-zero | ✓ freqX/freqY never zero (minimum clamp at 1/128 of base rate) |
| R6: Phase unwinding on pause | ✓ Phase halts on Stop, resumes on Start — no unwind |
| R7: Snapshot capture of derived state | ✓ Only stored fields captured; engine state recomputed from recipe |
| R8: Random seed reproducibility | ✓ KeyedRng per-node, deterministic seed from pattern+nodeIdx |
| R9: Pattern switch leaves ghost notes | ✓ Gate cutoff on pattern switch (same as NoteTrack) |
| R10: Container size ceiling | ✓ NodeSequence ≤ 32 bytes, within existing Track::_container ceiling |

---

## 14. Spec open questions — resolutions

| # | Question | Resolution |
|---|---|---|
| Q1 | Can two nodes fire in the same tick? | Yes. Last-evaluated wins monophonically (MVP). v2: polyphonic voice allocation. |
| Q2 | Should accumulator reset on boundary crossing or per-cycle? | Per-crossing. Each crossing adds the offset. This is the "evolving melody" mechanism. |
| Q3 | What happens when freqX or freqY = 0? | Node is paused at current phase position. Phase continues to be computed (just doesn't advance). Same behavior as phasor with enable=0 in gen~. |
| Q4 | Should Y-axis value at trigger time or trigger+1 be used for velocity? | Trigger time — use `y_current` at the exact tick of crossing detection, not `y_prev` or `y_next`. |
| Q5 | How many boundary lines per axis? | MVP: 4 per axis (configurable position). Total 8 boundary lines. Any crossing of any boundary fires a trigger. |

---

## 15. Deferred to v2

- **Polyphonic output**: Multiple simultaneous notes from overlapping nodes (voice-stealing or round-robin allocation). NOD-E has voice allocation in its GLOBALS section.
- **Per-node mute/enabled**: Individual node enable/disable
- **Node count configuration**: 4 or 8 nodes (MVP is fixed 8)
- **CV B as modulation source**: Route Y position to other track parameters via Routing
- **Boundary position modulation**: CV-modulated boundary positions (currently fixed per-sequence)
- **Phase offset modulation**: CV-modulated phase offsets
- **Reverse / pause per node**: Individual node direction control
- **Var X / Var Y (frequency jitter)**: NOD-E's randomization amount per axis — adds noise to the LFO frequency. Requires RNG state per node per sequence per pattern (seeded deterministically). Maps to KeyedRng per-node.
- **Fill mode**: What gesture makes sense for continuous-motion tracks?
- **PlayMode::Free**: Sub-tick ramp mode (PhaseFlux precedent)
- **MIDI channel selection**: NOD-E's GLOBALS section has MIDI out channel configuration. MVP hardcodes to channel 1 (or inherits from Performer global).
- **Note trigger mode**: NOD-E's NOTE TRIG section has options for which axis triggers (X only, Y only, both). MVP is X-axis only.
- **Velocity invert**: NOD-E's NOTE VELOCITY section has an invert toggle. MVP uses velScale range only (no sign flip).
- **Export/import of node trajectories**: Trajectory recording and replay

---

## 16. Engineering punch list

- Bit-layout finalization for NodeSequence (§5 draft is approximate; need exact BitField mapping + version gate)
- RAM measurement: estimate total per-pattern + per-track overhead vs. container ceiling (current largest variant is DiscreteMap at ~48 bytes; NodeSequence at ~32 bytes is well within budget)
- `NodeTrackEngine::tick()` performance: 8 nodes × 2 axes × (phase accumulate + waveform LUT + boundary check + crossing detect) per tick. Estimate: ~400 float ops per tick at 1 kHz = negligible on Cortex-M7
- No RNG needed in MVP — triangle and sine are deterministic functions of phase
- Scale table: reuse existing `Scale` infrastructure from NoteTrack (no new scale logic)
- Gate timer: reuse NoteTrack's `gateTimer` pattern (countdown ticks, fire Note Off at zero)

---

## 17. UI surfaces — modelled on NoteTrack

### 17.1 Three primary files

- `NodeEditPage` (EditPage subclass) — per-cell editing of frequency, waveform, amplitude, phase offset, boundary positions
- `NodeSequenceListModel` — sequence list for SequencePage
- `NodeTrackListModel` — track list for TrackPage (base note, scale, reset)

### 17.2 Clipboard hooks

Extend `ClipBoard` with `copyNodeSequence` / `pasteNodeSequence` / `canPasteNodeSequence`. Same 3-method pattern as PhaseFlux (see `ClipBoard.cpp:62-71, 230-247, 322-340`).

### 17.3 Context menu — INIT / COPY / PASTE / DUP

Same four-item structure as PhaseFlux:
- **INIT → Stage | Sequence | Track** (no "Topic" sub-level since there are no per-cell curve topics — all parameters are sequence-global except per-node offsets)
- **COPY / PASTE / DUP** — standard sequence-level operations

### 17.4 QuickEdit shortcuts — PAGE+S8..S15

| Slot | S9 | S10 | S11 | S12 | S13 | S14 | S15 | S16 |
|---|---|---|---|---|---|---|---|---|
| Action | freqX– | freqX+ | freqY– | freqY+ | INIT | EVEN | RAND | SHAKE |

- EVEN: distribute phase offsets evenly across 8 nodes (0/8, 1/8, 2/8, ... 7/8)
- SHAKE (S15): randomize all phase offsets (whole-topic)
- SHAKE (S16): randomize phase offsets ± small random (local shake per current axis)

### 17.5 USB keyboard baseline

Standard two-line override matching PhaseFlux:
```cpp
void NodeEditPage::keyboard(KeyboardEvent &event) override {
    if (handleFunctionKeys(event)) return;
    BasePage::keyboard(event);
}
```

---

## 18. Testing & MVP acceptance

### Phase A — Math + Storage foundations

- NodeSequence serialization round-trip (save/load)
- Phase accumulation for all 8 nodes advances correctly per tick
- Boundary crossing detection: synthetic test with known prev/cur x values
- Scale quantization matches NoteTrack output for same scale+baseNote inputs
- Accumulator drift produces expected semitone offsets
- All waveform shapes produce correct output for known phase inputs

### Phase B — First hardware guard

- Boot without crash
- One node, one X boundary, triangle wave, audible trigger on each crossing
- Gate length produces Note Off after expected duration
- Mute kills gate output, unmute resumes from current phase position (not reset)

### Phase C — Remaining test families

- All 8 nodes crossing multiple boundaries simultaneously (monophonic last-node-wins)
- Pattern switch preserves accumulator state
- Reset measure re-synchronizes all phases
- Boundary position edit during playback (no ghost triggers)
- Scale change during playback (next trigger uses new scale)
- CV routing to freqX/freqY (pitch modulation from external LFO)
- Triangle waveform: deterministic per-node, sine waveform: deterministic per-node
- Context menu INIT clears accumulators only (phases untouched)
- ClipBoard copy/paste between pattern slots
- Edge: all nodes at phase 0.0 simultaneously (simultaneous boundary crossings)
