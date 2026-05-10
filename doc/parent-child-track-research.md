# Parent-Child Track Relationships in Modular Sequencers

## Overview

This document researches existing modules, norns scripts, and VCV Rack plugins that implement Bloom-like parent-child track relationships with morphing/variation applied.

---

## Hardware Eurorack Modules

### 1. Qu-Bit Electronix Bloom v2

**Relationship Type**: Single parent, multiple child channels

| Aspect | Details |
|--------|---------|
| Architecture | 1 parent sequencer, 3 independent child channels |
| Channel Count | 3 channels × 64 steps |
| Morphing | Per-step modifiers (ratchets, trills, modulation CV) |
| Parent Data | Shared seed/state for fractal growth |
| Child Independence | Each channel can have different algorithm/mutation |
| Output | Gate, CV, Modulation CV, MIDI per channel |

**Key Features**:
- Fractal sequencer with infinite melody possibilities
- Two independent channels with shared fractal algorithms
- Per-step clock-synced ratchets
- New random variation functions
- Extra CV output for each channel

**Reference**: [Bloom v2 - Qu-Bit Electronix](https://www.qubitelectronix.com/shop/p/bloom-v2)

---

### 2. Intellijel Metropolix

**Relationship Type**: Linked tracks with variation propagation

| Aspect | Details |
|--------|---------|
| Architecture | 2 independent tracks with 8 stages each |
| Linked Behavior | Tracks can share scale/root, propagate changes |
| Morphing | Per-stage modifiers (pulse count, gate type, ratchet, probability, pitch, octave) |
| Parent Role | Global scale/root affects all tracks |
| Child Role | Individual track settings can override parent |
| Output | MIDI, Crow, CV per track |

**Key Features**:
- Two independent tracks with 8 stages each
- Control via grid (pulse count, gate type, ratchets, probability, pitch, octave)
- Accumulation (transposition that builds)
- Slide/glide between stages
- Quantize to scale and root via global params
- 64 preset slots

**Norns Port**: [Metrix](https://norns.community/metrix) - "It's like Metropolix for norns"

**Reference**: [Metropolix - Intellijel](https://intellijel.com/shop/eurorack/metropolix/)

---

### 3. Winter Modular Eloquencer

**Relationship Type**: 8 independent tracks with chance/probability linking

| Aspect | Details |
|--------|---------|
| Architecture | 8 tracks, each with CV/Gate |
| Linked Behavior | Probability-based firing across tracks |
| Morphing | Per-track probability, direction, length |
| Parent Role | Master clock/scale affects all |
| Child Role | Individual track probability/rhythm |

**Key Features**:
- 8-track controlled chance sequencer
- 16 step sequencer with bar chaining up to 256 steps
- Parameters linked to probability

**Reference**: [Eloquencer Manual](https://winter-modular.com/wp-content/uploads/2019/06/Eloquencer_Manual_V1.3.pdf)

---

### 4. DROID Universal CV Processor

**Relationship Type**: Master-slave with up to 16 controllers

| Aspect | Details |
|--------|---------|
| Architecture | 1 master + up to 16 controller modules + G8 output |
| Linked Behavior | CV routing and transformation between parent/child |
| Morphing | Rules-based CV processing |
| Parent Role | Master defines processing rules |
| Child Role | Controller inputs feed into master processing |

**Key Features**:
- 8 CV inputs and 8 CV outputs
- Universal CV processor
- Programmable via simple text commands
- Performance sequencer option (Moto Kit)

**Reference**: [DROID - Der Mann mit der Maschine](https://shop.dermannmitdermaschine.de/pages/droid-universal-cv-processor)

---

## Norns Scripts

### 1. Metrix

**Implementation**: "It's like Metropolix for norns"

| Aspect | Details |
|--------|---------|
| Tracks | 2 independent tracks with 8 stages each |
| Parent Behavior | Shared scale/root for both tracks |
| Child Behavior | Individual track settings, playback order, clock division |
| Morphing | Per-stage: pulse count, gate type, ratchet, probability, pitch, octave, slide |
| Accumulation | Transposition that builds over sequence |
| Output | Crow (gate/trigger/envelope), MIDI |

**Grid Layout**:
- Page 1: Pulses/gates + ratchets/probability
- Page 2: Pitch + octave + accumulation + slide
- Page 3: Presets and track settings
- Page 4: Scales and root note

**Reference**: [Metrix - norns community](https://norns.community/metrix) | [GitHub](https://github.com/kasperbauer/metrix)

---

### 2. QUENCE

**Implementation**: Probabilistic 4-track sequencer

| Aspect | Details |
|--------|---------|
| Tracks | 4 tracks with probabilistic firing |
| Parent Behavior | Shared scale/root for chord generation |
| Child Behavior | Individual track probability, output (MIDI, Crow, Molly the Poly) |
| Morphing | Euclidean patterns with logic and probability |
| Linked Behavior | Chord-based generation across tracks |

**Reference**: [QUENCE - norns community](https://norns.community/quence/) | [GitHub](https://github.com/millxing/QUENCE)

---

### 3. Dreamsequence

**Implementation**: Chord-based sequencer, arpeggiator, harmonizer

| Aspect | Details |
|--------|---------|
| Tracks | Multi-track with chord focus |
| Parent Behavior | Chord shapes define harmony |
| Child Behavior | Arpeggiated/harmonized variations |
| Morphing | Rhythmic variation within chord families |

**Reference**: [Dreamsequence - norns community](https://norns.community/dreamsequence/)

---

### 4. Foulplay

**Implementation**: Euclidean sampler with logic and probability

| Aspect | Details |
|--------|---------|
| Tracks | Euclidean pattern-based |
| Parent Behavior | Logic rules for pattern generation |
| Child Behavior | Probability-based variations |
| Morphing | Euclidean rotation, probability gates |

**Reference**: [Foulplay - GitHub](https://github.com/justmat/foulplay)

---

## VCV Rack Modules

### 1. Hallucigenia (XTRTN)

**Implementation**: Evolving, mutating random sequencer with memory

| Aspect | Details |
|--------|---------|
| Relationship Type | Parent-child with memory-based mutation |
| Morphing | Rules evolve based on history |
| Parent Behavior | Seeds patterns with constraints |
| Child Behavior | Mutations propagate with selection pressure |

**Reference**: [VCV Library - Hallucigenia](https://library.vcvrack.com/Extratone/Darwinism)

---

### 2. Partly Deterministic Fractal Sequencer

**Implementation**: Custom VCV Rack module (open source)

| Aspect | Details |
|--------|---------|
| Relationship Type | Fractal/self-similar parent-child |
| Morphing | Deterministic rules with probabilistic mutations |
| Parent Behavior | Base pattern definition |
| Child Behavior | Scaled/transformed variations |

**Reference**: [VCV Community Discussion](https://community.vcvrack.com/t/partly-deterministic-fractal-sequencer/19578)

---

### 3. Metrix VCV

**Implementation**: Metropolix-inspired VCV module

| Aspect | Details |
|--------|---------|
| Tracks | Multi-track with linked behavior |
| Parent Behavior | Shared scale/root |
| Child Behavior | Individual track variation |
| Morphing | Per-stage modifiers across tracks |

---

## XFormer's Existing Track Modes

### NoteTrack → TuesdayTrack

| Aspect | XFormer Implementation |
|--------|----------------------|
| Parent | NoteTrack (manual step entry) |
| Child | TuesdayTrack (algorithmic generation) |
| Linking | Harmony engine, scale/root sharing |
| Morphing | Octave, transpose, glide per step |

### DiscreteMapTrack

| Aspect | Implementation |
|--------|-----------------|
| Relationship | Input-driven stage selection |
| Parent | Threshold/ramp configuration |
| Child | Stage-specific output values |
| Morphing | Slew, pluck per stage |

---

## Proposed FractalTrack Design (from previous research)

### NoteTrack-FractalTrack Hybrid

```
┌─────────────────────────────────────────────────────┐
│                    NoteTrack                         │
│  (Parent - Manual step entry, core sequence data)   │
└─────────────────────────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────┐
│                  FractalTrack                         │
│  (Child - Reads core info from parent, applies      │
│   fractal mutation algorithm)                        │
│                                                      │
│  Inherits: scale, root, loop length, pattern        │
│  Adds:    fractal seed, mutation depth, variation    │
└─────────────────────────────────────────────────────┘
```

### FractalTrack Parameters

| Parameter | Source | Description |
|-----------|--------|-------------|
| seed | New | Fractal generation seed |
| mutationDepth | New | Recursion depth (1-4) |
| variationMode | New | EUCLID/CHAOS/BRANCH |
| parentScale | Inherited | Scale from NoteTrack |
| parentRoot | Inherited | Root from NoteTrack |
| parentLoopLength | Inherited | Pattern length |

### Mutation Types

| Mode | Description | Use Case |
|------|-------------|----------|
| EUCLID | Euclidean variation of parent notes | Rhythmic variation |
| CHAOS | Chaotic attractor (Lorenz/Chen) | Evolving melodies |
| BRANCH | Probabilistic branching | Generative exploration |

---

## Implementation Patterns Found

### 1. Inheritance Pattern (Metropolix/Metrix)
- Parent defines base scale/root
- Children inherit but can override
- Variation applied per-stage

### 2. Fractal Pattern (Bloom)
- Single seed generates self-similar variations
- Each channel applies same algorithm differently
- Recursive transformation

### 3. Probability Pattern (Eloquencer)
- Shared probability space across tracks
- Each track samples from parent distribution
- Random variation with constraints

### 4. Rules Pattern (Hallucigenia)
- Parent defines evolution rules
- Children apply rules with memory
- Selection pressure from history

---

## Key Takeaways for XFormer Implementation

1. **Scale/Root inheritance** is common pattern (Metrix, Bloom)
2. **Per-stage modifiers** provide fine-grained morphing control
3. **Seed-based variation** enables reproducible mutations
4. **Linked probability** allows cross-track influence
5. **Accumulation/transposition** builds over sequence (Metropolix feature)

---

## References

- Bloom v2: https://www.qubitelectronix.com/shop/p/bloom-v2
- Metropolix: https://intellijel.com/shop/eurorack/metropolix/
- Metrix: https://norns.community/metrix
- Eloquencer: https://winter-modular.com/
- DROID: https://shop.dermannmitdermaschine.de/pages/droid-universal-cv-processor
- Hallucigenia: https://library.vcvrack.com/Extratone/Darwinism
- QUENCE: https://norns.community/quence/
- Fractal Sequencer Discussion: https://community.vcvrack.com/t/partly-deterministic-fractal-sequencer/19578
