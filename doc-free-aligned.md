Free vs Aligned (Performer timing notes)

Global grid
- Global tick runs at CONFIG_PPQN (192) ticks per quarter note.
- Bar length is time signature dependent: measureDivisor = PPQN * beats per bar.

Aligned (Note/Curve)
- Step index is derived from global tick (relativeTick / divisor).
- Step 1 is forced to land on bar boundaries.
- Pattern changes synced to the grid keep phase aligned.
- Linked tracks follow the same relativeTick, so they stay phase-locked.

Free (Note/Curve) and free-running tracks (Indexed/Discrete/Tuesday)
- Step index advances by an internal counter.
- Phase is not tied to bar boundaries.
- Even if a loop is exactly 192*4 ticks long, it can stay offset if it started mid-bar.

Track-specific notes
- Indexed: no aligned/free mode. Can be bar-locked via SyncMode::ResetMeasure. Without it, phase is free-running.
- DiscreteMap: no aligned/free mode. Internal ramp can be reset by SyncMode::ResetMeasure; external clock mode cannot be aligned to bars.
- Tuesday: no aligned/free mode. Uses divisor to trigger steps on the tick grid, but its algorithm state is free-running unless explicitly reset on bar.

Alignment possibilities (if implemented)
- Indexed: aligned could quantize step changes to global tick or reset phase on bar boundaries.
- DiscreteMap: aligned only makes sense for internal ramp; external input has no global phase.
- Tuesday: aligned could force algorithm phase reset on bar boundaries; free would keep current behavior.

Why phase can drift despite equal length
- Length equals bar, but phase is independent.
- Starting mid-bar creates a fixed offset that repeats every cycle.
- Pattern changes do not necessarily reset phase unless the track explicitly resets.

When to use
- Aligned: tight bar-locked sequencing, predictable pattern sync, stable links.
- Free: generative drift, independent phase, intentional offsets.
