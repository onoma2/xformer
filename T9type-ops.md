# T9type Ops (Performer Teletype Track)

**Voltage note:** 

Teletype raw values are 0–16383. Performer maps that to **-5V..+5V**.  

Example: `V 5` outputs ~0V (raw mid-point ~8192).

---
## Inside T9type New Ops

### BUS (global CV bus)

**Syntax**

- `BUS n` → read slot `n` (1–4)
- `BUS n x` → write raw value `x` to slot `n`

**Notes**

- Shared across the whole project.
- Values are clamped to ±5V in the engine.

**Quick recipes**

```
# Store current CV input into BUS 1
BUS 1 IN

# Send BUS 1 to CV 1
CV 1 N BUS 1
```

### BAR (bar phase)

**Syntax**

- `BAR` → 0..16383 phase across current bar
- `BAR x` → phase across `x` bars (1–128)

**Quick recipes**

```
# 1-bar ramp
CV 1 V BAR

# Gate on second half of bar
IF BAR > 8192: TR.P 1
```

### RT (route source readback)

**Syntax**

- `RT n` → read routing source for route slot `n` (1–16), 0..16383

**Quick recipes**

```
# Map route 1 source to CV 2
CV 2 N RT 1
```

### E.* (per-CV envelopes)

**Syntax**

- `E n x` → set target (raw)
- `E.O n x` → offset/baseline (raw)
- `E.A n ms` → attack ms
- `E.D n ms` → decay ms
- `E.T n` → trigger
- `E.L n k` → loop `k` cycles (0 = infinite)
- `E.R n tr` → pulse TR at end of rise (tr=1–4, 0=off)
- `E.C n tr` → pulse TR at end of cycle (tr=1–4, 0=off)

Defaults: `E`=16383, `E.A`=50 ms, `E.D`=300 ms, `E.O`=0, `E.L`=1.

**Quick recipes**

```
# Simple AD envelope on CV 1
E.O 1 0
E 1 12000
E.A 1 10
E.D 1 80
E.T 1

# End-of-rise trigger on TR 1
E.R 1 1
```

### LFO.* (per-CV LFO)

**Syntax**

- `LFO.R n ms` → set cycle length (20..32767 ms, <=0 stops)
- `LFO.W n x` → wave morph (0..100)
- `LFO.A n x` → amplitude (0..100 → 0..±5V)
- `LFO.F n x` → fold threshold (0..100 → -5V..+5V)
- `LFO.O n x` → offset/bias (raw 0–16383)
- `LFO.S n x` → start/stop (<=0 stops, holds last output)

Defaults: `LFO.R`=1000 ms, `LFO.W`=0, `LFO.A`=100, `LFO.F`=0, `LFO.O`=8192, `LFO.S`=0.

**Quick recipes**

```
# 2s triangle to saw LFO on CV 1
LFO.R 1 2000
LFO.W 1 15
LFO.A 1 80
LFO.F 1 0
LFO.S 1 1
```

### TR.D / TR.W (trigger shaping)

**Syntax**

- `TR.D n div` → divide triggers (n=1–4)
- `TR.W n pct` → width % of post-div interval (1–100, 0 disables)

**Quick recipes**

```
# Every 4th pulse on TR 1
TR.D 1 4
```

---
## Geode Ops (G.*)

**Syntax**

- `G.TIME x` → base time (0..16383)
- `G.TONE x` → intone spread (0..16383, 8192 = noon)
- `G.RAMP x` → attack/decay balance (0..16383)
- `G.CURV x` → curve shape (0..16383, 8192 = linear)
- `G.RUN x` → physics macro (0..16383)
- `G.MODE x` → 0=Transient, 1=Sustain, 2=Cycle
- `G.O x` → output offset (raw 0–16383)
- `G.TUNE v num den` → per-voice ratio (v=0 all, 1–6), 0/0 resets defaults
- `G.V v divs reps` → trigger voice (v=0 all, divs 1–64, reps -1..255)
- `G.R cv v` → route mix/voice to CV out (cv=1–4, v=0 mix, 1–6 voice, <0 none)
- `G.VAL` → read mixed output (raw 0–16383)

Defaults: `G.TIME`=8192, `G.TONE`=8192, `G.RAMP`=8192, `G.CURV`=8192, `G.RUN`=0, `G.MODE`=0, `G.O`=0, tune ratios 1/1..6/1.

**Quick recipes**

```
# Route mix to CV1 and trigger a 7‑div burst on voice 1
G.R 1 0
G.V 1 7 8

# Voice 3 to CV2, tuned to 5/4
G.R 2 3
G.TUNE 3 5 4
```

---
## Westlicht / Performer Ops

### W.ACT / WR (transport)

**Syntax**
- `WR` → running flag (0/1)
- `W.ACT x` → start/stop/pause (alias: `WR.ACT`)

**Notes**

- `W.ACT 1` starts
- `W.ACT 0` stops (reset)
- other values pause

**Quick recipes**

```
# Toggle play on button press
IF TR 1: W.ACT 1
```

### WNG / WNN (Note track step access)

**Syntax**

- `WNG t s` → get gate at step `s` (0/1)
- `WNG t s v` → set gate (v!=0 on)
- `WNN t s` → get note (0–127)
- `WNN t s v` → set note (0–127)
- `WNG.H t` → gate at current gate step
- `WNN.H t` → note at current note step

**Quick recipes**

```
# Flip gate at current step
WNG 1 P.I 0
```

### WP / WP.SET (pattern index)

**Syntax**

- `WP t` → current playing pattern (1–16) for track `t`
- `WP.SET t p` → set track `t` to pattern `p` (1–16)

**Quick recipes**

```
# Force track 1 to pattern 2
WP.SET 1 2
```

### WBPM / WMS / WTU (tempo utilities)

**Syntax**

- `WBPM` → current BPM
- `WBPM.S x` → set BPM (1–1000)
- `WMS [n]` → ms for 1/16 note × n
- `WTU n [m]` → (beat_ms / n) × m

**Quick recipes**

```
# 1/16 ms
X WMS

# Triplet ms
X WTU 3
```
