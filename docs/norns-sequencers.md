# Norns Sequencers: Port Analysis for X|Former

**Inventory and feasibility assessment for porting scripts from `temp-ref/norns/` into full tracks, features, or xformer extensions.**

---

## RAM Constraint

`.data + .bss = 119,960` (~91.4% of 128 KB SRAM). New track types that stay **≤ 9,544 B** (current `NoteTrack` size, the container gate) add zero per-track container cost. Anything above multiplies by 8.

| Anchor | Size | Role |
|---|---|---|
| `NoteTrack` | 9,544 B | Container gate — max of the variant union |
| `CurveTrack` | 9,480 B | 2nd largest, xformer wavefolder carrier |
| `TeletypeTrack` | 7,104 B | Scripting track |
| `TeletypeTrackEngine` | 912 B | Engine container gate |
| `NoteTrackEngine` | 588 B | Next-largest engine |

Design target for a new model: **≤ 9,000 B**. Design target for a new engine: **≤ 588 B**.

---

## Full Inventory

`temp-ref/norns/` contains 24 scripts. Roughly 1/4 have direct CV/gate output (Crow native). The rest are MIDI/audio-only but contain portable algorithms.

| Script | Files | Core Concept | Output | CV Fit |
|---|---|---|---|---|
| `awake-rings/` | `awake-rings.lua`, `lib/karplus_rings.lua`, `lib/Engine_KarplusRings.sc` | Karplus-Rings ambient pad synth | Audio | Poor |
| `bakeneko/` | `bakeneko.lua`, `lib/Engine_Goldeneye_*.sc` | 6-track generative sample beat | Audio | Poor |
| `bistro/` | `bistro.lua`, `lib/midi_lib.lua` | Generative cafe soundscape | Audio | Poor |
| `bline/` | `bline.lua`, `lib/Engine_Bline_Synth.sc` | TB-303 acid bassline | MIDI | Good |
| `circles/` | `circles.lua`, `lib/libCircles.lua`, `lib/libCirclesTest.lua`, `lib/math_helpers.lua` | Physics: circles collide → notes | Crow CV/gate | **Excellent** |
| `constellations/` | `constellations.lua`, `lib/sequencer.lua`, `lib/stars.lua`, `lib/starfactory.lua`, `lib/lfos.lua`, `lib/actions.lua`, `lib/params.lua`, `lib/gui.lua`, `lib/crosshair.lua`, `lib/cosmicdust.lua`, `lib/midi_util.lua` | Starfield targeting sequencer | Crow CV/gate + JF | **Excellent** |
| `delinquencer/` | `delinquencer.lua`, `lib/active_patterns.lua`, `lib/mod_patterns.lua`, `lib/seq_patterns.lua`, `lib/note_patterns.lua`, `lib/prob_patterns.lua`, `lib/length_patterns.lua`, `lib/vel_patterns.lua`, `lib/probability.lua`, `lib/patches.lua`, `lib/synth_sounds.lua` | 64-cell grid + X/Y modulation + probability + 22 read-order patterns | MIDI | Fair |
| `dunes/` | `dunes.lua`, `lib/dunes_delay.lua` | Function-command step sequencer | Crow + audio | Good |
| `ekombi/` | `ekombi.lua`, `lib/Track.lua`, `lib/Pattern.lua`, `lib/Beat.lua`, `lib/SubBeat.lua`, `lib/Cycle.lua`, `lib/ParamsPage.lua`, `lib/Saving.lua` | Polyrhythmic grid: independent track lengths, subdivisions, swing | Audio + MIDI | Fair |
| `Hiswing/` | `Hiswing.lua` | MIDI groove generator w/ templates | MIDI | Poor |
| `initenere/` | `initenere.lua`, `lib/lattice.lua` | 2D lattice: 4 time lanes + 4 note lanes, independent direction/speed | MIDI + audio | Fair |
| `kreislauf/` | `kreislauf.lua`, `lib/core/kreislauf_app.lua`, `lib/core/stueck.lua`, `lib/core/ui.lua` | Circular drum sequencer (PO-style) | MIDI + crow | Fair |
| `less-concepts/` | `less-concepts.lua`, `lib/refrain.lua` | Cellular automata → 16-bit grid + refrain looper | MIDI + audio | Fair |
| `loudnumbers_norns/` | `loudnumbers_norns.lua`, `lib/csv.lua`, `lib/lightergraph.lua` | CSV data sonification | Crow CV/gate | **Excellent** |
| `luck/` | `luck.lua` | 7-node Markov chain, weighted probability matrix | MIDI + audio | Fair |
| `m18s/` | `m18s.lua`, `lib/polysub.lua`, `lib/crow_standalone/m18s.lua` | 2-voice × 8-step × 1-8 stages, gate modes per stage | Crow CV/gate | **Excellent** |
| `pitter-patter/` | `pitter-patter.lua`, `lib/sequence.lua`, `lib/ggrid.lua` | 4-track grid sequencer, tied notes, evolve | MIDI + audio | Fair |
| `qfwfq/` | `qfwfq.lua` | ASCII password-cracking gimmick | MIDI + audio | Poor |
| `skylines/` | `skylines.lua`, `lib/helpers.lua`, `presets/*.txt` | Enhanced M-185: 2-voice × 8-step × 8-stage, gate modes, grid presets | Crow CV/gate | **Excellent** |
| `spirals/` | `spirals.lua`, `lib/spiral.lua`, `lib/spirals_midi_listener.lua` | Circular/spiral rotating arm → notes | MIDI + audio | Fair |
| `thirtythree/` | `thirtythree.lua`, `lib/*.lua`, `lib/Engine_Thirtythree.sc` | PO-33 micro-sampler clone | Audio | Poor |
| `traffic/` | `traffic.lua` | Circle-of-5ths harmonic sequencer | MIDI + audio | Fair |
| `tulpamancer/` | `tulpamancer.lua`, `lib/hid.lua`, `lib/graphics.lua`, `lib/clocks.lua` | Daily MIDI pattern from internet | MIDI | Poor |
| `zxcvbn/` | `zxcvbn.lua`, `lib/*.lua`, `lib/Engine_Zxcvbn.sc` | Full-featured pattern sampler/sequencer w/ 40+ commands, tracker UI | MIDI + crow + JF | **Excellent** |

---

## Full New Track Candidates

Each requires: new `TrackMode` enum entry, model class, engine class, UI pages, and registration in `Track::Container`, `Engine::TrackEngineContainer`, and `Engine::updateTrackSetups()`.

### 1. PolyTrack — Polyrhythmic Multi-Lane Sequencer

**Source:** `temp-ref/norns/ekombi/` + `initenere/lib/lattice.lua`

**Concept:** Multiple independent lanes (up to 4 CV/gate pairs), each with its own:
- **Track length** (number of beats before wrapping)
- **Beat subdivision** (how many sub-beats per beat)
- **Per-step**: CV value, gate on/off, probability, swing multiplier
- **Lattice timing** from `initenere`'s ppqn superclock for precise sub-beat resolution

**Why new track:** No existing track does polyrhythm. The closest is NoteTrack, which is a fixed single-lane step sequencer. Polyrhythmic lanes with independent lengths/subdivisions produce phase-shifting patterns no existing paradigm can generate.

**Key files to study:**
- `temp-ref/norns/ekombi/lib/Track.lua` — track model (length, beat cycle)
- `temp-ref/norns/ekombi/lib/Beat.lua` — beat container with subdivisions
- `temp-ref/norns/ekombi/lib/SubBeat.lua` — trigger + params per sub-step
- `temp-ref/norns/ekombi/lib/Cycle.lua` — circular state iterator
- `temp-ref/norns/initenere/lib/lattice.lua` — superclock timing engine

**Model estimate:** `~5,000 – 6,000 B` (well under 9,544 B gate)
**Engine estimate:** `~400 – 500 B` (under 588 B gate)
**Registration points:**
- `src/apps/sequencer/model/Track.h` — add `TrackMode::Poly` to enum
- `src/apps/sequencer/model/Track.h` — add `PolyTrack` to `Container<>` template args
- `src/apps/sequencer/engine/Engine.cpp` — add `PolyTrackEngine` case in `updateTrackSetups()`
- `src/apps/sequencer/ui/pages/Pages.h` — register `PolySequencePage`, `PolySequenceEditPage`

---

### 2. StageTrack — Staged Gate Sequencer

**Source:** `temp-ref/norns/m18s/` + `temp-ref/norns/skylines/`

**Concept:** 2 voices × 8 steps × up to 8 stages per step. Each stage has a **gate mode**:
- `OFF` — no gate
- `SINGLE` — single trigger
- `ALL` — gate held across all stages
- `EVERY2` / `EVERY3` / `EVERY4` — trigger every N stages
- `RANDOM` — random trigger per stage
- `LONG` — extended gate length

Each step also has: pitch (scale-quantized), glide, direction (fwd/rev/ping-pong/random).

**Why new track:** The gate mode system is entirely absent from the firmware. `DiscreteMapTrack` does CV-driven stage scanning but has no gate mode concept. `NoteTrack` has a single gate per step. This track is a dedicated rhythm gate generator with CV overlay.

**Key files to study:**
- `temp-ref/norns/m18s/m18s.lua` — main script, parameter mapping
- `temp-ref/norns/m18s/lib/polysub.lua` — polyphonic sub-engine (voice mgmt)
- `temp-ref/norns/skylines/skylines.lua` — enhanced version
- `temp-ref/norns/skylines/lib/helpers.lua` — utility functions

**Model estimate:** `~3,000 – 4,000 B`
**Engine estimate:** `~300 – 400 B`
**Registration points:** same pattern as PolyTrack above

---

### 3. CellularTrack — Cellular Automata Sequencer

**Source:** `temp-ref/norns/less-concepts/`

**Concept:** A cellular automaton operates on a 16-bit wide grid. 8 rows each drive one CV/gate output pair. CA rules evolve the grid each generation, producing emergent pattern structures.

- Multiple rule sets (Conway, rule 30, custom)
- Snapshot/recall of grid states
- Density and rate controls per row
- Can layer multiple CA layers

**Why new track:** Cellular automata produce evolving, non-repeating patterns that no existing track type can generate. This is the most sonically unique of the candidates — organic, chaotic, emergent.

**Key files to study:**
- `temp-ref/norns/less-concepts/less-concepts.lua` — main script, CA parameter definitions
- `temp-ref/norns/less-concepts/lib/refrain.lua` — companion looper (less relevant for CV, but interesting pattern-capture concept)

**Model estimate:** `~4,000 – 5,000 B`
**Engine estimate:** `~400 – 500 B`
**Registration points:** same pattern

---

### 4. ProbGrid — Probabilistic Grid Sequencer

**Source:** `temp-ref/norns/delinquencer/`

**Concept:** 64 cells (8×8 grid), each cell can be ON/REST/SKIP/CONTROL. Two CV inputs act as X/Y modulation matrix axes, modifying all cell parameters. 22 read-order patterns (forward, spiral, snake, knight, frog, blocks, etc.). Per-cell probability rolls. Pattern mutation.

**Why new track:** The X/Y modulation matrix + probability + 22 read-orders make this the most feature-rich generative sequencer. It's also the largest and tightest RAM fit.

**Key files to study:**
- `temp-ref/norns/delinquencer/delinquencer.lua` — main logic
- `temp-ref/norns/delinquencer/lib/active_patterns.lua` — 64-bit on/off masks
- `temp-ref/norns/delinquencer/lib/mod_patterns.lua` — X/Y modulator definitions
- `temp-ref/norns/delinquencer/lib/seq_patterns.lua` — 22 read-order patterns
- `temp-ref/norns/delinquencer/lib/note_patterns.lua` — note value presets
- `temp-ref/norns/delinquencer/lib/prob_patterns.lua` — probability distributions
- `temp-ref/norns/delinquencer/lib/probability.lua` — per-cell probability roll

**Model estimate:** `~7,000 – 10,000 B` (may hit or exceed 9,544 B gate — needs careful sizing)
**Engine estimate:** `~500 – 600 B`
**Registration points:** same pattern

---

### Comparison

| Track | RAM Risk | Unique Value | Effort | Priority |
|---|---|---|---|---|
| PolyTrack | Low (~5-6 KB) | **Highest** — polyrhythm is a paradigm gap | Medium | **1** |
| StageTrack | Low (~3-4 KB) | High — gate modes don't exist | Low | **2** |
| CellularTrack | Low (~4-5 KB) | High — most unique output character | Medium | **3** |
| ProbGrid | **Medium** (~7-10 KB) | High — most feature-rich | High | **4** |

---

## Feature Additions to TuesdayTrack

All of these slot into the existing `TuesdayTrack` algorithm enum (`src/apps/sequencer/model/TuesdaySequence.h`) and engine dispatch (`src/apps/sequencer/engine/TuesdayTrackEngine.cpp`). No new model RAM, just a new algorithm function per entry.

| Algorithm | Source | Effort | File Reference |
|---|---|---|---|
| **Markov Chain** | `temp-ref/norns/luck/luck.lua` | Low | 7-node weighted probability matrix, note-to-node transitions. TuesdayTrack already has `Markov` in enum — verify if stub or full impl. |
| **Cellular Automata algorithm** | `temp-ref/norns/less-concepts/less-concepts.lua` | Low | CA rule → pitch sequence. Single-row extraction from CellularTrack concept above. |
| **Spiral/rotation** | `temp-ref/norns/spirals/lib/spiral.lua` | Low | Rotating arm angle → pitch mapping. Distance-from-center → velocity. Configurable rotation speed and LFO modulation. |
| **Circle physics** | `temp-ref/norns/circles/lib/libCircles.lua` | Medium | Circle growth/collision/burst → note events. More complex state than other algorithms. |

**Registration points for each:**
- `src/apps/sequencer/model/TuesdaySequence.h` — add enum value to `TuesdaySequence::Algorithm`
- `src/apps/sequencer/engine/TuesdayTrackEngine.cpp` — add `case` in algorithm dispatch + implement `processXxx()` function

---

## CurveTrack / Xformer Extensions

The "xformer" in X|Former is the `CurveTrack` wavefolder processing chain: `CurveSequenceEditPage` has a WAVEFOLDER edit mode with FOLD, GAIN, FILTER, XFADE per step.

### Command-Based Step Mutation

**Source:** `temp-ref/norns/dunes/dunes.lua`

Add a per-step "command" slot to `CurveSequence` that transforms the CV value:
- `OCTAVE_UP` / `OCTAVE_DN` — shift CV by octave
- `RANDOMIZE` — random CV jump
- `REVERSE` — reverse direction
- `GLIDE_ON` / `GLIDE_OFF` — toggle glide
- `WAVE_SIN` / `WAVE_SAW` / `WAVE_SQR` — force wave shape for that step

**Key files to study:**
- `temp-ref/norns/dunes/dunes.lua` — command dispatch table
- `src/apps/sequencer/model/CurveTrack.h` — where new command enum would go
- `src/apps/sequencer/engine/CurveTrackEngine.cpp` — where command processing would be inserted

### Additional Wavefolder Topologies

The current wavefolder is a single topology. Additional topologies (sine-fold, asymmetric-fold, dual-stage fold) can be added as a new `WavefolderMode` enum.

### Additional Chaos Generators

`CurveSequence` already has `Latoocarfian` and `Lorenz` chaos generators. More can be added from standard chaos maps (Henon, Logistic, Tinkerbell, Gingerbreadman) with minimal code.

### Data Sonification CV Lanes

**Source:** `temp-ref/norns/loudnumbers_norns/`

CSV data → CV mapping. Less a CurveTrack extension and more a standalone mode, but the CV-mapping paradigm (column → pitch CV, column → trigger threshold, column → modulation CV) could be a new `CurveSequence` processing block that reads a lookup table.

**Key files to study:**
- `temp-ref/norns/loudnumbers_norns/loudnumbers_norns.lua` — main data → CV mapping
- `temp-ref/norns/loudnumbers_norns/lib/csv.lua` — CSV parser (could be adapted for flash-stored tables)

---

## Reusable Utility Algorithms

These are standalone algorithms extracted from norns scripts that could live in `src/core/` and benefit multiple track types.

| Algorithm | Source | File | Description |
|---|---|---|---|
| **Lattice superclock** | `initenere/lib/lattice.lua` | `temp-ref/norns/initenere/lib/lattice.lua` | ppqn-based timing engine with phase tracking. All tracks benefit. |
| **Circle physics** | `circles/lib/libCircles.lua` | `temp-ref/norns/circles/lib/libCircles.lua` | Collision detection, growth, burst. Useful for generative algorithms. |
| **Spiral point generation** | `spirals/lib/spiral.lua` | `temp-ref/norns/spirals/lib/spiral.lua` | Rotating arm + radial drift. Modular standalone. |
| **Weighted probability matrix** | `luck/luck.lua` | `temp-ref/norns/luck/luck.lua` | N×N weight matrix for Markov chains. Generalizable to any N. |
| **Object pooling** | `constellations/lib/starfactory.lua` | `temp-ref/norns/constellations/lib/starfactory.lua` | Fixed-capacity reusable object pool. If engine needs dynamic entity tracking. |
| **Read-order patterns** | `delinquencer/lib/seq_patterns.lua` | `temp-ref/norns/delinquencer/lib/seq_patterns.lua` | 22 sequence traversal algorithms. Useful for any step-sequencing context. |
| **Modulation matrix** | `delinquencer/lib/mod_patterns.lua` | `temp-ref/norns/delinquencer/lib/mod_patterns.lua` | X/Y CV → parameter mapping. Useful for CV-input-driven modulation. |

---

## What Not to Port

These are audio/MIDI-only scripts with no CV-relevant algorithms:

| Script | Reason |
|---|---|
| `awake-rings/` | Audio-only Karplus-Rings synth. No CV/gate output or reusable algorithm. |
| `bakeneko/` | Audio sample playback. Pattern-language (`x`/`-`) is trivial and already covered. |
| `bistro/` | Ambient cafe simulation. Gimmick, no reusable structure. |
| `Hiswing/` | MIDI groove templates. Swing/shuffle concept is too simple to justify port. |
| `qfwfq/` | Password-cracking gimmick. No musical utility. |
| `thirtythree/` | Full PO-33 sampler clone. Requires audio engine, no CV pathway. Huge surface area. |
| `tulpamancer/` | Web-fetched daily pattern. Requires network stack. |

---

## Integration Checklist (for any new track)

Before writing code, verify each:

- [ ] RAM: Model size ≤ current container gate (9,544 B). Engine size ≤ engine gate (588 B) or identify the tradeoff.
- [ ] TrackMode enum: add to `Track.h` line ~40
- [ ] Container registration: add type to `Track::Container<>` and `Engine::TrackEngineContainer<>`
- [ ] Engine dispatch: add `case` in `Engine::updateTrackSetups()` (`Engine.cpp:504-543`)
- [ ] UI pages: register in `Pages.h` and implement editing
- [ ] List model: implement `*ListModel.h` for UI data binding
- [ ] Init/randomize: implement per-page init and randomize actions
- [ ] CV output: implement `cvOutput(int index)` and `gateOutput(int index)`
- [ ] Gate/Run/Link: wire `_runGate`, `_linkTrack`, gate output rotation
- [ ] Build: `make sequencer` in `build/stm32/release` passes, `.data + .bss` flat
