# Routing Hold Target — Notes and Plan

## Purpose / Desired Behavior
- Add a routing target `Hold` (per-track capable) that acts as an AND gate on track ticking.
- Default behavior unchanged: tracks advance normally until a route is set to `Hold`.
- When `Hold` target is routed and its source is <= 0.5 (normalized), the corresponding track does not advance; sources > 0.5 allow ticking.
- Routes are saved with the project; reopening a project with an active Hold route keeps the track halted until a high signal arrives. Only the transient gate state is non-persistent.
- Transport, global timers, DAC/ADC refresh, UI, etc. remain unaffected; this only gates per-track tick execution.
- Min/max mapping: Hold decision uses the routed normalized value after min/max scaling. With default `min=0`, `max=1`, >0.5 = allow, <=0.5 = hold. Swapping min/max (e.g., `min=1`, `max=0`) inverts the behavior, giving an intentional “NOT gate” option via routing polarity.

## Feasibility / Impact
- Safe for all track types (Note/Curve/DiscreteMap/Tuesday/etc.): skipping `track.tick()` simply freezes their phase/masks until released.
- Tuesday’s internal masking coexists; when Hold is low the Tuesday engine won’t be called, so its mask counters and phases pause.
- Song follow and clock continue globally; only the gated track stops advancing.
- Output policy to define: likely force gate low and hold last CV, or hold both gate/CV. For predictability, forcing gate low while holding last CV is acceptable; document the choice.

## Implementation Plan
1) **Routing target**: Add `Routing::Target::Hold` (label “Hold”), include in per-track capable set and serialization.
2) **State**: Add per-track `tickHold` flag/bitmask in `Engine` (default false/open; not serialized).
3) **Routing hookup**: In `RoutingEngine::writeEngineTarget`, when target is Hold, set/clear `tickHold` per track based on normalized > 0.5.
4) **Tick guard**: In the main per-track dispatch loop, if `tickHold` is true, skip `track.tick()` and emit no-update; force gate low and keep/hold CV per chosen policy.
5) **Resets**: On play/stop/clock reset, leave `tickHold` unchanged to honor the saved route; optionally clear on pattern change only if needed (default leave as-is).
6) **UI copy**: Expose the target name automatically via routing target lists; no special UI beyond existing routing selection needed.
7) **Test**: Verify normal playback (no Hold routes) unaffected; with Hold routed, track freezes until source goes high; project save/load retains route definition and resumes halted state when reopened without high source.
