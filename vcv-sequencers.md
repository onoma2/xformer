# VCV-Rack Sequencer Modules → Performer Port Analysis

Source: `temp-ref/vcv-seq/`
Target: performer (8 tracks, 1 binary gate + 1 DAC per track, 1 kHz engine loop)

**Hardware constraint that kills many ports:**
- 1 gate (binary)
- 1 DAC (CV)
- No stereo, no polyphonic output per track (MidiCvTrack is the exception but is passthrough)

---

## 1. VCV-Biset Tracker

**Files:** `VCV-Biset/src/Tracker/*.hpp`, `TrackerSynth/*`, `TrackerDrum/*`, `TrackerControl/*`, `TrackerClock/*`, `TrackerPhase/*`, `TrackerQuant/*`, `TrackerState/*`

Full DAW — 1000 patterns, 100 synths, 32 tracks. Too large.

**Survives 1 gate + 1 DAC:**
- Per-step effects: VIBRATO, TREMOLO, CHANCE — modulate the single CV/gate stream. **Port to NoteTrack step effects layer.**
- Tuning/scale tables — quantize the single CV output. **Port as per-track quantizer.**
- TrackerQuant 4-ch quantizer — same concept. Already covered by above.

**Dies:**
- PatternCV (value+delay+curve per CV step) — needs extra CV outputs for separate CV streams
- Beat+phase LPB clock — already covered by CurveTrack

---

## 2. VCV-Biset Regex

**Files:** `VCV-Biset/src/Regex/*.{hpp,cpp}`

12-channel text-expression pattern generator. String-based sequence language.

**Survives 1 gate + 1 DAC:**
- Core compiles string to single CV + gate stream. **Full port as TrackMode.**
- Navigation modes (FWD, BWD, PINGPONG, SHUFFLE, RAND, XRAND, WALK) all produce one CV/gate.
- Bias input → probabilistic variation on single CV.

**Dies:**
- 12 channels → can't output independently. Cap to 1 per track. (Or make it produce multiple tracks in a row.)
- RegexExp expander (12 gate outputs) — needs extra jacks.

**Verdict:** ✅ Full port viable. ~1000 B engine. Maps to one CV + one gate.

---

## 3. VCV-Biset Tree

**Files:** `VCV-Biset/src/Tree/*.{hpp,cpp}`

L-system fractal tree, 1024 branches, 5 CV outputs, wind simulation.

**Survives 1 gate + 1 DAC:**
- Sample one branch dimension → one CV. You lose 4 of 5 outputs. **Port to TuesdayTrack algorithm.**
- Wind simulation → modulates single CV. **Port as CurveTrack modulation option.**

**Dies:**
- 1024 branches × struct per branch — RAM. Need to cap branches severely.
- 5 CV outputs reduced to 1 kills the core value (multi-dimensional branch observation).

**Verdict:** ❌ Full port not worth it. Single-output L-system is a TuesdayTrack algorithm at best.

---

## 4. ZZC Phaseque

**Files:** `ZZC/src/Phaseque/*.{hpp,cpp}`

Phase-based step sequencer. Per-step VALUE, LEN, SHIFT, EXPR_CURVE. Mutation engine.

**Survives 1 gate + 1 DAC:**
- Core: VALUE (CV) on DAC, GATE on gate. **Full port as TrackMode.**
- EXPR_CURVE sweeps the single CV within the step — alive.
- LEN per step (variable duration) — alive, affects gate timing.
- SHIFT per step (phase offset) — alive, affects when CV lands.
- Mutation engine (base + mutator offset) — modifies the single CV.
- Per-step gate toggle — gate binary, works fine.
- Clock advance, reset, pattern navigation — all controller logic, no outputs.

**Dies:**
- EXPR_POWER, EXPR_OUT — extra CV outputs. No target on performer.
- EXPR_IN — external CV modulates curve. Could use ADC input, but no spare CV input per track.
- Polyphony modes — pointless with one DAC.
- VBPS (V/oct → BPM) clock input — CV input contention.
- Pattern CV outputs (SHIFT_LEN, EXPRESSION, PHASE, WENT, PTRN) — extra jacks.

**Verdict:** ✅ Full port viable. Phaseque's core musical value (variable-length steps + per-step shaping on a single CV/gate) maps perfectly. Mutation is included but not the selling point. ~1200-1800 B engine.

---

## 5. OrangeLine Dejavu

**Files:** `OrangeLine/src/Dejavu.hpp`

4-row probabilistic sequencer. Per-row LEN/DUR/GATE/SH/REP/HEAT. Wobble. Seed-based.

**Survives 1 gate + 1 DAC:**
- Core: one row → one CV + one gate. **But the musical value is 4 interacting rows.**
- Per-row probability/heat → port as NoteTrack gate probability features.
- Wobble → micro-variation on single CV. **Port to NoteTrack/StochasticTrack wobble.**
- Seed input → deterministic randomization. Already in StochasticTrack.

**Dies:**
- 4 independent rows → need 4 CV+gate pairs to hear the row interaction.
- REP (repetition CV output) → extra CV.
- TRG (trigger output) → extra gate.

**Verdict:** ❌ Full port worthless — multi-row interaction is the point. Row-split heat/wobble/seed are single-CV features already covered by StochasticTrack.

---

## 6. OrangeLine Morpheus/Morph

**Files:** `OrangeLine/src/Morpheus.hpp` `Morph.hpp`

CV/Gate looper. 16 memory slots, 128 steps, lock/hold/balance/smart hold.

**Survives 1 gate + 1 DAC:**
- Loop one CV + one gate. The core works — capture CV in, play CV out.
- Smart hold / gate-is-trigger → port as NoteTrack gate mode feature.
- Balance (source vs processed) → mix two CV values into one. Works.

**Dies:**
- 16 poly channels — pointless.
- The value is being a looper (capture + replay). Performer has no CV input capture infrastructure per track (only ADC inputs are CV jacks shared across tracks). **Full looper requires recording.**
- Lock modes (CV, GATE, BOTH) — need both CV streams to lock independently.

**Verdict:** ❌ Full port not viable without CV recording infrastructure. Smart hold / gate-is-trigger mode are small feature ports to NoteTrack.

---

## 7. OrangeLine Phrase

**Files:** `OrangeLine/src/Phrase.hpp`

Master/slave phrase sequencer. Two interdependent CV+gate streams with delay.

**Survives 1 gate + 1 DAC:**
- Master produces one CV+gate. **So does slave.** The phrase interaction requires hearing both simultaneously.

**Dies:**
- Core value is master vs slave — two voices interacting. Performer has 8 tracks but no cross-track phasing concept built in. You'd need track linking + two dedicated tracks.
- SPA/SPH trigger outputs — extra gates.

**Verdict:** ❌ Port as **track linking feature** (master/slave relationship between two tracks). But that's a cross-track architecture change, not a single-track port.

---

## 8. OrangeLine Swing

**Files:** `OrangeLine/src/Swing.hpp`

16-step swing/shuffle generator. Per-step timing offset.

**Survives 1 gate + 1 DAC:**
- Timing offsets affect the single gate/CV stream. **Port as global swing engine.**
- Only needs to shift gate edge timing — no extra outputs needed.
- ~200 B RAM for swing table + per-track amount.

**Dies:**
- PHS, ECLK, CMP, TCLK extra outputs — not needed if integrated into tick distribution.

**Verdict:** ✅ Feature port. Add swing phase before tick distribution in Engine::update().

---

## 9. OrangeLine Gator

**Files:** `OrangeLine/src/Gator.hpp`

Gate processor: LEN, JITTER, RATCHET, RATCHET DELAY, STRUM.

**Survives 1 gate + 1 DAC:**
- Processes a single gate stream. Works directly on performer's binary gate.
- Ratchet → multiple gate pulses within one step. Already similar to NoteTrack retrigger.
- Strum → sequential gate offsets. Port as gate timing offset.
- Gate jitter → probabilistic gate timing. Port to NoteTrack gate probability.

**Dies:**
- Nothing — all features mutate a single binary gate stream. Polyphonic gate manipulation → already single-gate compatible in basic form.

**Verdict:** ✅ Feature port to NoteTrack gate processing. Strum timing, ratchet delay, jitter all mutate one gate.

---

## 10. OrangeLine Mother

**Files:** `OrangeLine/src/Mother.hpp` `MotherScales.hpp`

Scale/chord generator / quantizer. Per-note weights, root/scale/chord selectors.

**Survives 1 gate + 1 DAC:**
- Quantize single CV. **Port as per-track quantizer to NoteTrack/StochasticTrack.**
- Fate (probability-based note selection) → port to StochasticTrack.

**Dies:**
- Polyphonic CV/GATE output — needs voice allocation and multiple outputs.
- Chord output — requires multiple CVs.
- POWER modes (POW, GATES, SCALE, PES) — some need polyphony.

**Verdict:** ⚠️ Partial port. Scale weights + fate are single-CV features. Chord/polyphony don't map.

---

## 11. OrangeLine Resc

**Files:** `OrangeLine/src/Resc.hpp`

CV → source scale → target scale → target chord re-mapper.

**Survives 1 gate + 1 DAC:**
- Scale-to-scale remapping on one CV. Works.

**But:** Performer already has MIDI/CV pipeline with scale quantization. No gap.

**Verdict:** ❌ Redundant.

---

## 12. OrangeLine Fence

**Files:** `OrangeLine/src/Fence.hpp`

CV range limiter/quantizer: RAW, QTZ, SHPR modes. Trigger-on-change.

**Survives 1 gate + 1 DAC:**
- Limits/quantizes one CV. Works.
- Trigger-on-change → single gate trigger.

**But:** Too simple. RAW = passthrough, QTZ = quantizer (redundant), SHPR = shaper (CurveTrack already does per-step shapes).

**Verdict:** ❌ Redundant.

---

## 13. TeknoLogical Seq4 / Drum5 / Odd5

**Files:** `TeknoLogical/src/TL_Seq4.cpp` `TL_Drum5.cpp` `TL_Odd5.cpp` `TL_Bass.cpp`

Simple 4/5-step sequencers. Drum5 has 5 drum channels (needs 5 gates).

**Survives 1 gate + 1 DAC:**
- Seq4 → 4 steps. Covered by NoteTrack.
- Odd5 → 5 steps, odd-meter. Covered by IndexedTrack variable lengths.
- Bass → bass-focused. No musical feature that NoteTrack can't do.

**Dies:**
- Drum5 → 5 independent drum trigger channels. Needs 5 gates. Performer has one.

**Verdict:** ❌ Covered by existing tracks. Odd-meter is already in IndexedTrack.

---

## 14. Rebel Tech Stoicheia (Euclidean)

**Files:** `rebel-tech-vcv/src/` (module listed in plugin.hpp)

Euclidean rhythm generator. Distributes M pulses across N steps.

**Survives 1 gate + 1 DAC:**
- Produces one gate pattern. Works on binary gate.
- Euclidean algorithm is small — no LUT, ~50 lines of math.

**Verdict:** ✅ Feature port to NoteTrack or TuesdayTrack. Euclidean gate pattern as play mode or step generator.

---

## 15. dbRackSequencer Uno

**Files:** `dbRackSequencer/src/Uno.hpp`

8-step CV/gate. 3 direction modes, per-step probability/glide/reset.

**Survives 1 gate + 1 DAC:**
- One CV, one gate. Works.
- PENDULUM (ping-pong) direction → port to NoteTrack play mode.
- Per-step skip probability → already covered by NoteTrack gate probability.
- Per-step glide → already covered by NoteTrack slide.
- Step address output → one CV. **Port as NoteTrack CV output mode.**

**Verdict:** ✅ Feature ports to NoteTrack. PENDULUM mode + step address output are the new value.

---

## 16. Sha-Bang Photron

**Files:** `Sha-Bang-Modules/src/Photron.hpp`

Flocking/boids sequencer. Blocks in 3D color space → CV outputs.

**Survives 1 gate + 1 DAC:**
- Sample one dimension of one block → one CV.

**Dies:**
- Boids N² CPU cost is absurd for one CV.
- Core value is multi-dimensional flocking interaction (separation/alignment/cohesion produces emergent patterns across multiple blocks). Single CV is a pale shadow.
- Color-space CV derivation → need multiple CVs to hear the color mapping.

**Verdict:** ❌ Not worth it. A simple 3-particle flocking model as TuesdayTrack algorithm is the ceiling, and the value is dubious.

---

## 17. Sha-Bang StochSeq / Cosmosis / Collider / Talea

**Files:** `Sha-Bang-Modules/src/` (modules listed in plugin.hpp)

Various stochastic/cut-up/collision generative sequencers.

**Survives 1 gate + 1 DAC:**
- StochSeqGrid (grid-based stochastic) → one CV + one gate. **Port as StochasticTrack grid mode.**
- Talea (cut-up) → rearranges existing material into one stream. **Port to TuesdayTrack algorithm.**
- Cosmosis → overlaps with Tuesday.
- Collider → collision detection between two sequences. Need two tracks to collide.

**Dies:**
- Collider needs two audible voices for collision.

**Verdict:** ⚠️ Partial. StochSeqGrid and Talea map to one CV+gate. Collider doesn't.

---

## 18. Geodesics TwinParadox / Branes / Ions / Torus

**Files:** `Geodesics/src/TwinParadox.cpp` `Branes.cpp` `Ions.cpp` `Torus.cpp`

Dual/paradox relationship sequencers. Trigger primitives (TriggerRiseFall, HoldDetect).

**Survives 1 gate + 1 DAC:**
- TriggerRiseFall/HoldDetect → gate edge detection. **Port to DiscreteMapTrack** (already threshold-based, add edge detection options).
- One side of twin/paradox → one CV+gate.

**Dies:**
- Twin/paradox (dual relationship) needs two voices to hear the paradox. Performer needs two tracks + cross-track linking.

**Verdict:** ⚠️ Partial. Trigger primitives port easily. Dual relationship requires cross-track feature.

---

## 19. CVfunk-Modules (Cartesia, Collatz, JunkDNA, PressedDuck, Rat, etc.)

**Files:** `CVfunk-Modules/src/plugin.hpp`

Various: Cartesian/grid sequencer, Collatz-conjecture sequence, DNA-based generative, probabilistic gate, ratcheting.

**Survives 1 gate + 1 DAC:**
1. Cartesia (Cartesian/grid) → maps 2D CV→XY→single CV. **Port to DiscreteMapTrack** as 2D scan mode.
2. Collatz → math sequence produces one CV per step. **Port to TuesdayTrack algorithm.** ~20 lines.
3. JunkDNA (DNA-based) → one evolutionary stream. **Port to TuesdayTrack algorithm.**
4. PressedDuck (probabilistic gate) → one gate stream. **Port to NoteTrack gate mode.**
5. Rat (ratcheting) → one gate stream. Already covered by NoteTrack retrigger.

**Verdict:** ✅ Feature ports. Small algorithms, all produce one CV+gate.

---

## Final Rankings (1 gate + 1 DAC constraint)

### Full TrackMode Ports
| Sequencer | Priority | RAM est. | Why |
|---|---|---|---|
| **Phaseque** | High | ~1500 B | Variable-length steps + per-step shaping on one CV/gate. Nothing else does this. |
| **Regex** | Medium | ~1000 B | Text→CV+gate compiler. Complements Teletype's script model. |

### Feature Ports to Existing Tracks
| Feature | Source | Target | Priority |
|---|---|---|---|
| Euclidean gates | Stoicheia | NoteTrack/Tuesday | High |
| Per-step effects (vibrato, tremolo, chance) | Tracker | NoteTrack | High |
| Swing/timing offsets | OrangeLine Swing | Engine global | Medium |
| PENDULUM direction + step address | dbRack Uno | NoteTrack | Medium |
| Gate jitter/strum | OrangeLine Gator | NoteTrack | Medium |
| Scale weights + per-note probability | OrangeLine Mother | StochasticTrack | Medium |
| TriggerRiseFall/HoldDetect | Geodesics | DiscreteMapTrack | Medium |
| StochSeqGrid | Sha-Bang | StochasticTrack | Low |
| Cartesian/grid 2D scan | CVfunk Cartesia | DiscreteMapTrack | Low |
| Collatz/JunkDNA/Talea algorithms | CVfunk/Sha-Bang | TuesdayTrack | Low |
| Mutation engine | Phaseque | PhasequeTrack (included) | — |

### ❌ Dropped by 1 gate + 1 DAC
| Port | Reason |
|---|---|
| Dejavu (full) | 4-row interaction is the point. Single row = StochasticTrack. |
| Morpheus looper (full) | Needs CV capture infrastructure. Smart hold mode survives as feature. |
| Phrase master/slave (full) | Needs two audible tracks + cross-track linking. |
| Tree (full) | 5 outputs → 1 kills the multi-dimensional value. |
| Photron (full) | Boids N² for one CV is insane. |
| Resc scale remapper | Redundant with existing quantizer. |
| Fence limiter | Too simple, redundant. |
| TeknoLogical Drum5 | 5 gates needed. |
| Track linking / dual relationships | Cross-track architecture change, not single-track port. |
