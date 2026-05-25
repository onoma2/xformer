# LIVE Page Constellation

The LIVE page is a quiet event monitor, not an animation engine. The picture
only changes when a knob moves, a gate fires, or NewR/NewM rerolls the seed.

## Metaphor

A field of up to 12 rings sits at deterministic XY positions in the viewport.
The bell haze behind them shows the Bias/Spread probability field the engine
draws from. Each ring is one recently-fired event. The newest event is bright
and gets a filled center + cross pips; older events fade in place across five
brightness tiers before vanishing. NewR / NewM re-rolls the seed mix so future
events land in fresh positions.

This replaces the prior comet-trail walker (pitch guide rails, slide ribbons,
legato rails, age-based stride). That design encoded time on the X axis; the
constellation does not.

## Stable XY contract

Each recorded event carries a `uint16_t serial` — a monotonic counter the
engine increments in `recordDirectHistory` and stamps onto the slot.
`PackedDirectHistoryEvent::serial` lives in
`src/apps/sequencer/engine/StochasticTrackEngine.cpp` (file-scope packed
struct); `DirectHistoryEvent::serial` surfaces it to the page.

The page's `ringXY(serial, ...)` lambda hashes
`(serial * 2654435761) XOR (constSeed * 0x9E3779B1)` where
`constSeed = seq.rhythmSeed() ^ seq.melodySeed()`. The high and low halves of
the hash map to (x, y) inside the viewport, scaled by Complexity:

```
scale = 35 + (complexity * 65) / 100   // 35..100% of half-viewport
```

Because the hash is keyed on `serial` (per-event) and not on the age slot,
each event keeps the same XY for its entire lifetime in the history buffer.
It only changes when `constSeed` changes — i.e., on NewR / NewM.

## Age fade tiers

```
age 0      -> Color::Bright       (filled center + 4 cross pips)
age 1      -> Color::MediumBright
age 2..3   -> Color::Medium
age 4..5   -> Color::MediumLow
age 6..8   -> Color::Low
age 9+     -> invisible (skipped)
```

Brightness comes from the slot index in `_directTrail`, which mirrors the
engine's age buffer. The XY does *not* read this index.

## Knob → visual map

| Knob | Effect |
|---|---|
| Bias | Bell haze vertical center |
| Spread | Bell haze width (Gaussian envelope) |
| Duration | Ring base radius (`2 + duration/2`, range 2..5) |
| Variation | Per-ring radius wobble keyed on serial |
| Rest | Sparsity cut — culls a slot to a faint dot when `(serial*37 + seed) % 100 >= 100 - rest` |
| Complexity | XY scale (cloud tightness vs full-viewport spread) |
| Contour | Per-event tilt via `(serial & 0xF) - 8`, scaled by Contour |
| Burst | Toggles petals around the newest ring |
| Burst Count | Caps petal count at `min(children, BurstCount-1)` |
| Burst Rate | Petal radius offset |
| Repeat | Above 50, ring is double-stroked |

The 5 knobs not in the always-on chip strip (Feel, Gate Length, Legato, Range,
Slide) still edit fine — they appear in the held-step Small label.

## Why Contour reads serial bits, not age

A tilt formula like `(age - 5) * contour` made the cloud lean as a function of
age slot. With per-event XY, the same age slot maps to different events over
time, so an age-based tilt would jitter every ring on every beat. Reading the
low bits of `serial` keeps tilt stable per event without losing the visual.

## Implementation notes

- Circle stroke uses Bresenham midpoint, no float trig in the inner loop.
- Burst petal positions are an 8-direction integer lookup, not `cosf`/`sinf`.
- `kDirectTrailMax = 12`, `kDirectHistoryMax = 12` — sized to the visible cap.
- Chip strip dropped from 16 single-letter slots to 11 important knobs
  (D V R BU BC BR top, E S O X I bottom), `%-2s%3d` grid-locked.

## Where to look

- `src/apps/sequencer/ui/pages/StochasticSequenceEditPage.cpp` — `drawLivePage()`.
- `src/apps/sequencer/ui/pages/StochasticSequenceEditPage.h` — `DirectParticle` (now `{serial, flags, children}`).
- `src/apps/sequencer/engine/StochasticTrackEngine.cpp` — `PackedDirectHistoryEvent`, `recordDirectHistory`, `gDirectHistorySerial`.
- `src/apps/sequencer/engine/StochasticTrackEngine.h` — `DirectHistoryEvent` (engine-facing struct, gains `serial`).
- `ui-preview/pages_stochastic_live.py` — Python mirror; `--page stochastic-live-constellation[-burst|-complex|-newseed]` for renders.
