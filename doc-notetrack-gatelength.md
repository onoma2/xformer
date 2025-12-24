# NoteTrack Gate Length Notes

## How gate length is computed

NoteTrack does not store a per-step gate percentage. Gate duration is derived from step length.

1) Compute length units (1..8):
- `length = clamp(step.length + lengthBias, 0..7) + 1`
- length variation may adjust this value on a per-step basis.

2) Convert to ticks:
- `stepLengthTicks = divisor * length / NoteSequence::Length::Range`
- `NoteSequence::Length::Range` is 8.

3) Apply gate offset:
- `gateOffset = divisor * gateOffset / (GateOffset::Max + 1)`

4) Gate mode affects scheduling:
- ALL / FIRST / FIRSTLAST: gate off at `tick + gateOffset + stepLength`.
- HOLD: gate length is forced to `divisor * (pulseCount + 1)` (ignores stepLength).
- RETRIGGER: gate pulses repeat while `retriggerOffset <= stepLength`.

5) CV update timing:
- CV updates at `tick + gateOffset` if gate is firing or CV update mode is Always.

## Where this logic lives

- `src/apps/sequencer/engine/NoteTrackEngine.cpp`:
  - `evalStepLength(...)`
  - `NoteTrackEngine::triggerStep(...)`

## Implications

- Changing the step-length mapping changes gate length and retrigger count behavior.
- It does not change step timing or pulse count.
- Recording uses a linear conversion from ticks to step length; if the runtime mapping becomes nonlinear, record/playback will not round-trip unless updated.
