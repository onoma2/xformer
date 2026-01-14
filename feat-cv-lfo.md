# Feature Spec: T9type CV LFO

## Summary
Add a Teletype LFO generator per CV output (4 channels) with ms-based rate,
wave morphing, amplitude control, and fold-below threshold. LFO writes to the
CV output directly at control rate and holds the last value when stopped.

## Ops
- `LFO.R n ms`  
  Set cycle length for LFO `n` (1..4). Clamp 20..32767 ms. `ms <= 0` idles.
- `LFO.W n x`  
  Wave morph 0..100.
- `LFO.A n x`  
  Amplitude 0..100 (maps to 0..±5V peak).
- `LFO.F n x`  
  Fold threshold 0..100 (maps to -5V..+5V). Fold values below threshold.
- `LFO.S n x`  
  Start/stop (`x` <= 0 stops). Stop holds last output.

## Wave Morph
`LFO.W` uses three ranges:
- 0..33: triangle → saw
- 34..67: saw → pulse (50% duty)
- 68..100: pulse duty 50% → 5%

## Output Mapping
- `LFO.A` is mapped to volts: `ampVolts = (A/100) * 5`.
- LFO output is bipolar and clamped to -5..+5V (Performer bridge range).
- Fold threshold: `threshold = -5 + (F/100) * 10`.
- Fold below threshold: `out = 2*threshold - out` when `out < threshold`.

## Behavior Notes
- LFO output overrides CV output while active.
- Uses existing CV output mapping (range, offset, quantize) via `handleCv`.
- `LFO.S 0` holds the last output; it does not force 0V.

## Implementation Notes
- Engine: `TeletypeTrackEngine` owns LFO state and updates per tick.
- Bridge: add `tele_lfo_*` functions mirroring envelope ops.
- Ops: add `LFO.R`, `LFO.W`, `LFO.A`, `LFO.F`, `LFO.S` in hardware ops.

## Files
- `src/apps/sequencer/engine/TeletypeTrackEngine.h`
- `src/apps/sequencer/engine/TeletypeTrackEngine.cpp`
- `src/apps/sequencer/engine/TeletypeBridge.cpp`
- `teletype/src/teletype_io.h`
- `teletype/src/ops/hardware.h`
- `teletype/src/ops/hardware.c`
- `teletype/src/ops/op.c`
- `teletype/src/ops/op_enum.h` (auto-generated)
