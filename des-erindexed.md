# ER-Style Indexed Timing Plan

Goal: make Indexed behave like ER-101 timing: duration + gate share the same unit scale, divisor scales both, and clock multiplier behaves like per-track PPQN with sub-tick scheduling. Free bits in the step mask for future params.

## Summary of the problems
1) Indexed divisor is effectively meaningless in playback today (it only affects UI edit step size). Playback uses raw duration ticks and divides them by clock multiplier.
2) Gate length uses a lookup table (0–127 encoded to ticks), so it does not scale consistently with duration.
3) In free/aligned playback, Indexed does not generate sub-ticks per track; timing resolution is bounded by global ticks.

## Design goals
- Make divisor meaningful in playback (ER-style): duration and gate scale by divisor.
- Unify duration + gate scales (same unit range).
- Allow high timing resolution via per-track PPQN (clock multiplier), without collapsing into tiny durations.
- Preserve sync with global clock and other tracks.
- Free bits in the step format for future parameters.

## Step bitmask change (intentional format change, dev-only)
### Current step mask
- bits 0–6: note_index (7 bits, signed -63..63)
- bits 7–21: duration (15 bits, 0–32767 ticks)
- bits 22–28: gate_length (7 bits, 0–127 encoded)
- bits 29–30: unused
- bit 31: slide

### New step mask (duration + gate in ticks, shared range)
- bits 0–6: note_index (7 bits, signed -63..63)
- bits 7–16: duration_ticks (10 bits, 0–1023)
- bits 17–26: gate_ticks (10 bits, 0–1023)
- bit 27: slide
- bits 28–31: unused (4 bits reserved)

Decision rationale:
- Duration + gate share the same tick scale (0–1023), so gate is truly comparable to duration.
- Divisor scales both equally, preserving step‑independent timing while enabling coarse time stretch.
- We still free 4 bits for future params.
- We accept breaking serialized projects (dev-only).

## Timing math (ER-style divide on tick durations)
### Definitions
- durationTicks: 0..1023 (stored per step)
- gateTicks: 0..1023 (stored per step)
- divisor: sequence divisor (time scale factor)
- ppqnMult: clockMultiplier (acts like per-track PPQN)

### Effective ticks
- effectiveDuration = durationTicks * divisor
- effectiveGate = gateTicks * divisor
- clamp: effectiveGate <= effectiveDuration

### Step advance
- Advance to next step when per-track tick counter >= durationTicks.
- Gate output stays high while per-track ticks < gateTicks.

Decision rationale:
- Matches ER: both duration and gate are scaled by divide.
- Removes inconsistent gate math/lookup table.
- Keeps per-step arbitrary durations, with divisor as a global time scale.

## Per-track PPQN (clock multiplier) for Indexed
### Behavior
- clockMultiplier controls sub-tick rate (per-track PPQN), not duration scaling.
- When ppqnMult > 1, Indexed generates synthetic sub-ticks between global ticks.

### Implementation sketch
- Maintain a per-track sub-tick accumulator.
- On each global tick (or in update dt), add base tick delta * ppqnMult.
- Process multiple sub-ticks if accumulator >= 1.
- Each sub-tick increments the per-track tick counter used for duration/gate.
- Sync resets and external sync keep the accumulator phase-locked.

Decision rationale:
- Emulates ER synthetic clock timers.
- Improves high-speed resolution without shrinking duration units.
- Keeps Indexed locked to global clock phase.

## Migration / dev-only stance
- We explicitly accept breaking existing Indexed patterns.
- Old step durations will be reinterpreted as units (0..127) after repack.

## UI changes
- Display duration and gate in ticks (0..1023), no lookup table.
- Editing duration/gate uses tick steps; shift still uses divisor for coarse changes.
- Gate value formatting should be linear ticks.

Decision rationale:
- Duration/gate become consistent and predictable.
- UI matches ER mental model (units scaled by divisor).

## Optional extension (future use of freed bits)
- Use the 4 reserved bits for a small durationScale (e.g., 0..3 mapped to x1..x8).
- New timing: effectiveDuration = durationTicks * divisor * scale.
- Gate scales the same way.

Why optional:
- Gives long durations without forcing huge divisors.
- Keeps fine resolution by allowing small divisors + scale.

## Implementation checklist
1) Repack step bitmask in IndexedSequence::Step (duration/gate fields + slide bit).
2) Remove GateTickTable and replace gateTicks/gateEncodeTicks with linear tick math.
3) Update IndexedTrackEngine timing math to use durationTicks * divisor and gateTicks * divisor.
4) Add per-track sub-tick accumulator for clockMultiplier (PPQN-style).
5) Update UI formatting/logic where gate ticks are displayed or edited.
6) Sweep references to GateTickTable and duration tick assumptions.
