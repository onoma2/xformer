# Route Shapers (per-track)

Shapers run in normalized source space (after per-track bias/depth) and before the route’s min/max mapping. Crease is still supported as a legacy flag; use the Shaper to pick it explicitly. Abbreviations match the UI overlay (`Sxx`).

- **NO** (None): Pass-through of bias/depth output.
  - Sine LFO: unchanged.
  - Envelope: unchanged.

- **CR** (Crease): Waveslice around 0.5 with a fixed ±0.5 jump; clamp to 0..1.
  - Sine LFO: becomes a two-level, folded shape; spends time at 0 or 1 with hard flips.
  - Envelope: mostly zeros unless biased into midrange; mid values jump toward edges.

- **LO** (Location): Integrator of (src-0.5); `loc = clamp(loc + (src-0.5)*kRate)`, kRate=0.000125 (~4s rail-to-rail at 1 kHz). Holds last value when src is 0.5.
  - Sine LFO: drifts toward rails; positive half pushes up, negative half pushes down; slow glide.
  - Envelope: rises if above 0.5, falls if below; holds when mid; slow one-pole feel.

- **EN** (Envelope): Rectified slew; `rect = |src-0.5|*2; env += (rect-env)*coeff` with attack=1.0, release=0.0005 (τ≈2 s at 1 kHz).
  - Sine LFO: follows absolute magnitude, peaks at extremes, dips near midpoint; smoothed by release.
  - Envelope: behaves like a classic envelope follower; fast attack, ~2 s decay.

- **TF** (Triangle Fold): Bipolar triangle fold; x=2*(src-0.5); folded triangle to ±1, back to 0..1.
  - Sine/triangle LFO: full-wave triangle-ish, doubles bumps per cycle.
  - Envelope: if near 0 or 1 it maps toward 0; mid values map high; produces a “double-bump” response.

- **FF** (Frequency Follower): Crossing counter with leak; on sign flip acc+=0.10 (clamped), each tick acc*=0.9999 (τ≈10s at 1 kHz). Tuned for 1s LFO: builds to full in 14 crossings (7s). If pegged at 1.0 for 3s, slews back to 0 over 7s. Creates symmetrical 7s rise / 7s fall breathing envelope.
  - Sine LFO (1s): 7s ramp up to full amplitude, holds briefly, then 7s fade to silence; creates ~17s breathing cycle.
  - Envelope: no crossings, so mostly decays unless biased to cross 0.5; works best with bipolar sources.

- **AC** (Activity): Blends slope energy and crossing pulses; `level = level*0.9995 + |s-prev|*0.05;` sign flip -> level=1; clamp (τ≈2s decay at 1 kHz). Tuned for 1-3s LFOs with higher sensitivity. If pegged near 1.0 for 6s, slews back to 0 over 3s.
  - Sine LFO: level stays high while oscillating; decays when slowing/stopping; fades to silence when pegged.
  - Envelope: movement drives level up; flat sections decay; no need for zero crossings.

- **PD** (Progressive Divider): Counts crossings; toggles output when count >= threshold, then raises threshold (growth=1.25, cap=128); threshold decays toward 1 at ~τ=1s (decay≈0.999 per tick at 1 kHz) when idle. If threshold sits near the cap for 2s of routing updates, it resets to 1. Gate output is slewed over 1s for smooth transitions.
  - Sine LFO: starts frequent toggles, then sparser as threshold rises; output is smooth gate that slows over time; recovers density if input stops.
  - Envelope: few/no crossings unless biased around 0.5; mostly holds current gate and slowly densifies if idle.

---

## Shaper Behavior by Input Type

This section provides comprehensive behavior analysis for each shaper with common modulation sources.

### 1s LFO (1 Hz, 2 crossings/sec)

| Shaper | Behavior |
|--------|----------|
| **None** | Clean sine/triangle, unchanged |
| **Crease** | Folded waveform with hard transitions at 0/1; spends time at rails |
| **Location** | Drifts ~0.25 range per cycle; slow glide up on positive half, down on negative |
| **Envelope** | Follows peaks with fast attack, ~2s decay; wobbles between peak tracking and decay |
| **TriangleFold** | Double-frequency triangle output; 2 bumps per LFO cycle |
| **FreqFollower** | Builds to 1.0 over 7s, holds 3s, fades to 0 over 7s; ~17s breathing cycle |
| **Activity** | High level (~0.7-0.9) while oscillating; decay visible between peaks |
| **ProgDivider** | Initially toggles every cycle (1Hz square), then slows to 0.5Hz, 0.25Hz...; smooth 1s ramps |

### 10s LFO (0.1 Hz, 0.2 crossings/sec)

| Shaper | Behavior |
|--------|----------|
| **None** | Very slow sine/triangle wave |
| **Crease** | Very slow fold transitions; long dwells at 0 and 1 |
| **Location** | Drifts nearly full rail-to-rail per cycle; almost a sawtooth response |
| **Envelope** | Tracks peak, fully decays during negative half; clean envelope follower behavior |
| **TriangleFold** | Slow double-bump triangle at 0.2 Hz |
| **FreqFollower** | Stabilizes at ~0.38; moderate level from slow crossings |
| **Activity** | Spiky pattern (1.0 → 0.08 every 5s); rhythmic pulses at crossing points |
| **ProgDivider** | Very slow initial toggles (10s period), then 20s, 40s...; threshold grows very high |

### Trigger (brief pulse, ~50ms)

| Shaper | Behavior |
|--------|----------|
| **None** | Brief pulse output |
| **Crease** | Pulse jumps to 1.0 if amplitude > 0.5, else stays near 0 |
| **Location** | Tiny bump (~0.006 per trigger); integrates very slowly |
| **Envelope** | Fast rise to pulse height, then 2s exponential decay to zero |
| **TriangleFold** | Brief triangle spike |
| **FreqFollower** | Single crossing → +0.10 bump, then leaks back down over 10s |
| **Activity** | Spike to 1.0 on pulse (if crosses 0.5), then 2s decay to zero |
| **ProgDivider** | One crossing → output toggles, stays at new level with smooth 1s ramp |

### Sustained Gate (constant high or low)

| Shaper | Behavior |
|--------|----------|
| **None** | Flat 1.0 (high gate) or 0.0 (low gate) |
| **Crease** | Both high and low gates map to 0.5 after fold |
| **Location** | High gate → drifts toward 1.0 over ~4s; Low gate → drifts toward 0 over ~4s |
| **Envelope** | High gate → 1.0 (instant); Low gate → 0.0 (instant attack, then decay) |
| **TriangleFold** | Both high and low extremes fold to 0.0 |
| **FreqFollower** | No crossings → leaks to 0 over 10s |
| **Activity** | No movement → decays to 0 over 2s |
| **ProgDivider** | No crossings → threshold decays to 1; output holds last state |

### Decay Envelope (fast attack, slow decay)

| Shaper | Behavior |
|--------|----------|
| **None** | Original envelope shape |
| **Crease** | If peak > 0.5: flips to 1.0 then back; if peak < 0.5: stays low with fold artifact |
| **Location** | Rises during attack/decay (if above 0.5), falls slowly after; S&H-ish behavior |
| **Envelope** | Envelope-of-envelope: follows magnitude, smooths the decay with 2s release tail |
| **TriangleFold** | Attack maps high, decay creates descending triangle bumps |
| **FreqFollower** | Usually 1 crossing (at 0.5 threshold) → +0.10 bump, leaks back over 10s |
| **Activity** | High activity during attack → level=1.0, then tracks decay slope, fades to 0 |
| **ProgDivider** | Usually 1 crossing → output toggle with smooth 1s ramp; holds until next envelope |

### AD Envelope (attack-decay, returns to zero)

| Shaper | Behavior |
|--------|----------|
| **None** | Original AD shape |
| **Crease** | Peak folds to 1.0, decay portion clamps low; creates step function |
| **Location** | Rises during attack (if peak > 0.5), drifts back down after; momentum effect |
| **Envelope** | Envelope follower: fast attack tracking, 2s decay tail extends original decay |
| **TriangleFold** | Attack becomes triangle peak, decay creates inverted triangle shape |
| **FreqFollower** | Typically 2 crossings (up & down) → +0.20 total, then 10s leak back |
| **Activity** | Spike to 1.0 at attack crossing, high during movement, decays to 0 after envelope ends |
| **ProgDivider** | 2 crossings → output toggle twice (or once if threshold grew); smooth transitions |

### Key Insights

**Unexpected behaviors to note:**
- **Crease + Gate**: Gates map to midpoint (0.5), not extremes
- **TriangleFold + Gate**: Gates fold to zero, opposite of expected
- **Location + Trigger**: Almost no effect; needs sustained input
- **FreqFollower + Short Envelope**: Only 1-2 crossings → small bump, not dramatic

**Best pairings:**
- **1s LFO + FreqFollower**: Creates beautiful 17s breathing amplitude envelope
- **Trigger + Activity**: Perfect instant spike with clean 2s decay
- **Trigger + Envelope**: Classic envelope follower response
- **AD Envelope + FreqFollower**: 2 crossings give noticeable bump
- **Gate + Location**: Creates slow 4s glide to rails
- **10s LFO + Activity**: Rhythmic spiky pulses every 5s

---

## Recommended Shapers by Routing Target Type

This section helps you choose the best shaper for your routing destination.

### Binary/Trigger Targets (Play, Record, TapTempo, Mute, Fill)
**Needs:** Threshold crossings and clean gate behavior

| Source | Recommended Shapers | Why |
|--------|-------------------|-----|
| **1s LFO** | None, ProgDivider | None: toggles every half cycle; ProgDivider: creates evolving rhythmic patterns |
| **10s LFO** | None, Activity | Slow rhythmic triggers; Activity adds spiky timing |
| **Trigger** | None, ProgDivider | Direct triggering; ProgDivider creates probabilistic triggering |
| **Gate** | None | Direct gate-to-trigger conversion |
| **Envelope** | Activity | Triggers on envelope movement/attack |

**Avoid:** Crease (creates midpoint artifacts), TriangleFold (double triggers), Location (drifts unpredictably)

---

### Tempo & Smooth Continuous Targets (Tempo, Swing, SlideTime)
**Needs:** Gradual, musical changes without abrupt jumps

| Source | Recommended Shapers | Why |
|--------|-------------------|-----|
| **1s LFO** | None, FreqFollower, Location | None: direct smooth LFO; FreqFollower: 17s breathing tempo; Location: slow drift |
| **10s LFO** | None, Envelope | Ultra-slow tempo changes; Envelope smooths the curve |
| **Trigger** | Location, Envelope | Location: tiny incremental tempo nudges; Envelope: brief tempo dips |
| **Gate** | Location | Slow glide to tempo extremes over 4s |
| **Envelope** | None, Envelope | Smooth tempo following envelope shape; Envelope-of-envelope extends decay |

**Avoid:** Crease (tempo jumps), TriangleFold (chaotic tempo), ProgDivider (steppy tempo changes)

**Best:** None (direct LFO), FreqFollower (breathing tempo), Location (slow drift)

---

### Discrete/Stepped Targets (Pattern, Scale, RootNote, Algorithm, RunMode)
**Needs:** Clear discrete steps, no in-between values

| Source | Recommended Shapers | Why |
|--------|-------------------|-----|
| **1s LFO** | ProgDivider, Crease, Activity | ProgDivider: stepped gates → discrete jumps; Crease: binary levels; Activity: threshold-based switching |
| **10s LFO** | None, ProgDivider | Slow discrete changes; ProgDivider adds rhythmic pattern selection |
| **Trigger** | ProgDivider | Each trigger increments through patterns/scales |
| **Gate** | Crease | Gate high → one value, gate low → another (binary selection) |
| **Envelope** | Activity, Envelope | Activity: switch on attack; Envelope: envelope peak determines selection |

**Avoid:** Location (smooth drift between discrete values looks glitchy), FreqFollower (amplitude doesn't map well to selection)

**Best:** ProgDivider (rhythmic stepping), Crease (binary selection), Activity (movement-based switching)

---

### Bipolar Bias Targets (GateProbabilityBias, LengthBias, NoteProbabilityBias, etc.)
**Needs:** Centered modulation (0 = no bias, ±max = full bias)

| Source | Recommended Shapers | Why |
|--------|-------------------|-----|
| **1s LFO** | None, FreqFollower, Activity | None: direct bipolar LFO; FreqFollower: breathing bias; Activity: bias tied to LFO movement |
| **10s LFO** | None, Location | Ultra-slow bias drift; Location creates momentum |
| **Trigger** | Location | Accumulates triggers into slowly building bias |
| **Gate** | Location | Glides bias toward extremes over 4s |
| **Envelope** | None, Envelope | Envelope peak drives bias; Envelope-of-envelope smooths |

**Avoid:** ProgDivider (binary output doesn't suit bipolar), Crease (creates harsh bias jumps)

**Best:** None (direct LFO for bipolar sweep), FreqFollower (breathing bias), Location (drift)

---

### Generative/Expressive Targets (Tuesday: Algorithm, Flow, Power, Glide, Trill, etc.)
**Needs:** Dynamic, expressive modulation for musical variation

| Source | Recommended Shapers | Why |
|--------|-------------------|-----|
| **1s LFO** | **ALL SHAPERS!** | FreqFollower: breathing generative intensity; Activity: movement-responsive; ProgDivider: rhythmic parameter switching; Crease/TriangleFold: chaotic variations |
| **10s LFO** | None, FreqFollower, Activity | Slow evolving textures; FreqFollower creates long-form breathing |
| **Trigger** | Activity, Envelope | Burst-triggered generative changes |
| **Gate** | Location, Activity | Location: slow glide into generative mode; Activity: gate movement drives intensity |
| **Envelope** | Envelope, Activity, FreqFollower | Envelope following for expressive control; Activity for attack-sensitive generation |

**Best combinations:**
- **Flow + 1s LFO + FreqFollower**: Breathing flow intensity over 17s cycles
- **Algorithm + Trigger + ProgDivider**: Rhythmic algorithm switching
- **Power + Envelope + Activity**: Envelope attack drives power bursts
- **Glide + 10s LFO + Location**: Slow glide parameter drift

**Philosophy:** These targets reward experimentation - try unconventional shapers for happy accidents!

---

### Chaos Targets (ChaosAmount, ChaosRate, ChaosParam1/2)
**Needs:** Dynamic chaos control, often benefits from slow changes

| Source | Recommended Shapers | Why |
|--------|-------------------|-----|
| **1s LFO** | None, FreqFollower, Location | FreqFollower: breathing chaos intensity; Location: wandering chaos; None: direct cyclic chaos |
| **10s LFO** | None, Location | Ultra-slow chaos evolution; Location adds drift |
| **Trigger** | Envelope, Activity | Brief chaos bursts; Activity adds decay tail |
| **Gate** | Location, None | Location: slow chaos ramp; None: binary chaos on/off |
| **Envelope** | None, Envelope, Activity | Envelope-shaped chaos (musical!); Activity: attack-triggered chaos |

**Best:** FreqFollower (breathing chaos), Location (wandering chaos), None (direct LFO)

**Avoid:** ProgDivider on ChaosAmount (steppy chaos less interesting than smooth)

---

### Wavefolder Targets (Fold, Gain, DjFilter, XFade)
**Needs:** Audio-rate or fast modulation for timbral changes

| Source | Recommended Shapers | Why |
|--------|-------------------|-----|
| **1s LFO** | None, FreqFollower, TriangleFold | None: cyclic wavefold sweep; FreqFollower: breathing timbre; TriangleFold: double-rate modulation |
| **10s LFO** | None, Location | Slow timbral evolution; Location creates drift |
| **Trigger** | Envelope, Activity | Percussive wavefold hits |
| **Gate** | Location, None | Location: slow fold sweep; None: binary fold on/off |
| **Envelope** | None, Envelope, TriangleFold | Envelope-shaped timbre (very musical!); TriangleFold: double-bump response |

**Best for Fold:** None (direct LFO sweep), FreqFollower (breathing fold amount), Envelope (expressive)

**Best for XFade:** None (smooth crossfade), Location (slow drift between sources)

---

### DiscreteMap Targets (Input, Scanner, Sync, RangeHigh/Low)
**Needs:** Control over discrete sample scanning and synchronization

| Source | Recommended Shapers | Why |
|--------|-------------------|-----|
| **1s LFO** | None, Location, ProgDivider | None: cyclic scanning; Location: wandering scan; ProgDivider: rhythmic sync |
| **10s LFO** | None, Location | Ultra-slow sample evolution; Location adds momentum |
| **Trigger** | ProgDivider, Activity | Rhythmic sync triggers; Activity: trigger intensity → scan position |
| **Gate** | Location, Crease | Location: slow scan drift; Crease: binary range selection |
| **Envelope** | None, Activity | Envelope shapes scan position; Activity: movement drives scanning |

**Best for Scanner:** None (direct LFO scan), Location (wandering), FreqFollower (breathing scan rate)

**Best for Sync:** ProgDivider (rhythmic re-sync), Activity (movement-triggered sync)

**Best for Input:** None (direct modulation), TriangleFold (chaotic input), Envelope (expressive)

---

## Quick Reference: Shaper Personalities

**Smooth & Musical:**
- None (direct pass-through)
- Location (slow drift/momentum)
- Envelope (peak follower)
- FreqFollower (breathing amplitude)

**Rhythmic & Stepped:**
- ProgDivider (evolving rhythm)
- Activity (movement triggers)
- Crease (binary levels)

**Chaotic & Experimental:**
- TriangleFold (double-rate chaos)
- Crease (harsh transitions)
- FreqFollower + Activity (compound breathing/spikes)

**Best Defaults:**
- **Don't know what to use?** Start with **None** (direct source)
- **Want movement?** Use **FreqFollower** (1s LFO) or **Location** (any source)
- **Want rhythm?** Use **ProgDivider** (LFO) or **Activity** (Envelope/Trigger)
- **Want chaos?** Use **TriangleFold** or **Crease** on continuous sources
