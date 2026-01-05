# Meadow-Lite (Draft)

## Goal
Provide a lightweight Meadowphysics-inspired **global gating engine** that writes per-track RUN routing targets (1:1 lane → track). This allows each track to advance only when its lane gate is high.

## Behavior
- 8 lanes (A–H), 1:1 mapping to tracks 1–8.
- Lane gate **high** → track runs normally.
- Lane gate **low** → track pauses (step position is preserved; resumes from same step).
- No automatic track reset on gate-off (can be added later as an optional rule).

## Minimal Lane Parameters (Meadow-Lite)
Per lane:
- `count` (uint8): reset target value.
- `speed` (uint8): ticks per decrement.
- `min` / `max` (uint8): bounds for count.
- `rule` (enum): None / Inc / Dec / Max / Min / Rnd / Pole / Stop.
- `target` (uint8): destination lane index (0–7).

Dropped from full Meadow:
- triggerMask / toggleMask / syncMask
- speedMin / speedMax
- soundMode
- multi-target rule destinations

## Engine Loop (High-Level)
Advance per **step** (not per engine tick). Each lane owns a step clock.

1. For each lane, accumulate time until its next **step** based on `speed`.
2. On step:
   - Decrement `position`
   - If `position` hits 0:
     - **Gate event** for lane (step-level gate)
     - Apply `rule` to `target` lane’s `count`
     - Reset `position` to `count` (clamped to min/max)
3. Output lane gates.
4. Write RUN routing targets:
   - Track 1 RUN = Lane 1 gate
   - Track 2 RUN = Lane 2 gate
   - ... etc.

## Routing Integration
- Use existing RUN routing target per track.
- Meadow writes to RUN via Project routing write calls (same path used by Routing engine).
- No changes needed to Engine tick loop.

## Open Questions
- Should lane gate be a one-step pulse or a sustained high during the lane’s “on” phase?
- Do we need a “gate length” or default 1-tick pulse is enough?
- Should rules apply before or after emitting a gate?
- Should target lane updates clamp into `min..max` or wrap?

## RAM Estimate (Lite)
Per lane (~6 bytes) × 8 lanes ≈ ~48 bytes + minimal engine state.
