# New Teletype Ops from Live-Sequencer Reference

Operations from `temp-ref/live-sequencers/` that are genuinely absent from the
current teletype VM (425 ops) and cannot be expressed through existing
combinations. Source references point to the reference implementation in
Tidal or Mercury.

---

## Signal Generation

### PERLIN — Smooth Perlin noise

One-shot Perlin noise value (0–16383). Like `CHAOS` but produces smoothly
interpolated pseudo-random output. Useful for natural-sounding modulation
(filter cutoff, pitch vibrato).

```
CV 1 V PERLIN
```

- **`temp-ref/live-sequencers/tidal/tidal-core/src/Sound/Tidal/UI.hs`** — `perlin` (194), `perlinWith` (178), `perlin2` (234), `perlin2With` (212)
- **`temp-ref/live-sequencers/tidal/tidal-core/src/Sound/Tidal/Core.hs`** — `timeToRand` (used internally for seeded rng)

### NORMAL — Gaussian distribution

Box-Muller transform returning normally distributed values in 0–16383
(clamped to ±3σ). Unlike `RAND`/`RRAND` which are uniform. Useful for
natural pitch distributions, velocity curves.

```
CV 1 V NORMAL
```

- **`temp-ref/live-sequencers/tidal/tidal-core/src/Sound/Tidal/UI.hs`** — `normal` (248-255), uses Box-Muller on two uniform randoms

### LFO→Script readback — Read LFO output into variables

LFO currently drives CV only and can't be read into script variables.
An op like `LFO.V n` → var would allow LFO to modulate pattern params,
gate probability, slew rates, etc.

```
X LFO.V 1    # read LFO1 into X
CV 2 N ADD 60 X   # use it as transposition
```

- No equivalent in Tidal or Mercury (both treat LFOs as opaque signal paths)

---

## Pattern Transforms (P.* namespace)

### P.PAL — Palindrome (alternate forward/reverse)

Alternates pattern forward/reverse each cycle. Like `P.REV` but
auto-toggles.

```
P.PAL
```

- **`temp-ref/live-sequencers/tidal/tidal-core/src/Sound/Tidal/UI.hs`** — `palindrome` (572): `palindrome p = slowAppend p (rev p)`
- **`temp-ref/live-sequencers/mercury/grammar/totalSerialismIR.js`** — `palindrome` (246), `palin` (249), `mirror` (252) → `Mod.palindrome`

### P.WTH — Sub-range transform

Apply a transform (rev, shuffle, add, etc.) to only a portion of the
pattern defined by start/end indices, leaving the rest untouched.

```
P.WTH 2 6 REV      # reverse only steps 2-6
P.WTH 0 3 SHUF    # reshuffle first 4 steps
```

- **`temp-ref/live-sequencers/tidal/tidal-core/src/Sound/Tidal/UI.hs`** — `within` (807-812), `within'` (838-843), `withinArc` (814)
- Tidal impl: splits pattern at arc boundaries, applies f to inner, stacks with outer

### P.STR — Stretch/resample to length

Resample the pattern to a target length with interpolation.

```
P.STR 16      # stretch active pattern to 16 steps
```

- **`temp-ref/live-sequencers/mercury/grammar/totalSerialismIR.js`** — `stretch` (284), `stretchFloat` (290) → `Mod.stretch`
- **`temp-ref/live-sequencers/mercury/node_modules/total-serialism/src/transform.js`** — `stretch` (583), linear interpolation, aliases: stretchF, stretchFloat

### P.URN — Sample without replacement

Pick N distinct values from the pattern. Like `P.RND` but guarantees no
repeats until the pool is exhausted.

```
P.URN 4       # fill 4 steps with random non-repeating values from pattern
```

- **`temp-ref/live-sequencers/mercury/grammar/totalSerialismIR.js`** — `urn` (137) → `Rand.urn`
- **`temp-ref/live-sequencers/mercury/node_modules/total-serialism/src/gen-stochastic.js`** — `urn` (256)

### P.INV — Invert/mirror around center

Mirror all values around a center point. E.g. center=8192: 0→16383,
16383→0, 4096→12288.

```
P.INV 8192    # invert around midpoint
```

- **`temp-ref/live-sequencers/mercury/grammar/totalSerialismIR.js`** — `invert` (208), `inverse` (211), `flip` (214), `inv` (221) → `Mod.invert`
- **`temp-ref/live-sequencers/mercury/node_modules/total-serialism/src/transform.js`** — `invert` (197), signature: `invert(array, [center], [lowRange])`

### P.WALK — Random walk fill

Fill pattern values using a brownian/random walk starting from a seed
value. Step size adjustable.

```
P.WALK 4096 0   # start at 0, max step change 4096
```

- **`temp-ref/live-sequencers/mercury/grammar/totalSerialismIR.js`** — `drunk` (127), `drunkF` (130), `drunkFloat` (133) → `Rand.drunk`
- **`temp-ref/live-sequencers/mercury/node_modules/total-serialism/src/gen-stochastic.js`** — `drunk` (144), `drunkFloat` (106)
- Note: teletype has `DRUNK` as a single global var; this is a pattern-fill variant

### Wave-fill pattern gen — P.SINE / P.COS / P.SQR

Fill a pattern with N periods of a wave function. Takes length, periods,
amplitude range.

```
P.SINE 16 1 0 16383   # 16 steps, 1 period, full range
```

- **`temp-ref/live-sequencers/mercury/grammar/totalSerialismIR.js`** — `sine` (43), `cosine` (53)
- **`temp-ref/live-sequencers/mercury/node_modules/total-serialism/src/gen-basic.js`** — `sine` (294), `cosine` (315), `square` (395)

---

## Control Flow

### CHOOSE / WCHOOSE — Weighted selection

Pick one value from a list with equal or weighted probability. `CHOOSE`
returns one of N args. `WCHOOSE` takes (value, weight) pairs.

```
CHOOSE 60 64 67 72          # equal weight, pick one
WCHOOSE 60 50 64 30 67 20   # C4 50%, E4 30%, G4 20%
```

- **`temp-ref/live-sequencers/tidal/tidal-core/src/Sound/Tidal/UI.hs`** — `choose` (268), `chooseBy` (283), `wchoose` (297), `wchooseBy` (303)
- Tidal `chooseBy`: `chooseBy f xs = (xs !!!) . floor <$> range 0 (fromIntegral $ length xs) f`

### SUPERIMPOSE — Layer original + transformed

Apply a transform and sum with the original. In teletype, this means
blending two CV values (add/sub) on a single output rather than write-last.

```
SUPERIMPOSE 2 REV   # output = CV + REV(CV) on CV 2
```

- **`temp-ref/live-sequencers/tidal/tidal-core/src/Sound/Tidal/UI.hs`** — `superimpose` (740): `superimpose f p = stack [p, f p]`

### TRIGGER JOIN — Pattern re-evaluation on trigger

Not an op but an execution model concept: a script that produces a
pattern-of-patterns where each trigger re-evaluates the inner pattern.
Teletype's immediate-execution model makes this less relevant, but
`reset` semantics (restart pattern counter on trigger) are partially
covered by `P.I 0`.

- **`temp-ref/live-sequencers/tidal/tidal-core/src/Sound/Tidal/Pattern.hs`** — `_trigJoin` (294), `trigJoin` (313), `trigZeroJoin` (317), `reset` (319), `restart` (325)

---

## Logic

### Gate/trigger pattern combination

Combine trigger inputs with AND/OR/XOR before routing to a script or
output. Currently each TI maps 1:1 to a script. A `TR.COMBINE` op or
routing-level gate combinator would enable complex trigger logic.

```
TR.COMBINE AND 1 2    # fire when TI1 AND TI2 high
```

- No direct Tidal equivalent (tidal operates on event streams, not gate logic)
- Concept drawn from trigger routing in modular synthesis convention
