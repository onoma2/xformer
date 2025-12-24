Performer MIDI Output - Standard Implementation Notes

Principle
- MIDI output is driven by per-track engines. The MIDI output layer only sends messages when track engines call sendGate/sendCv/sendSlide.
- CV clamping is output-stage only (Calibration voltsToValue). MIDI clamping is output-stage only (MidiOutputEngine sendCv clamps to 0..127).

Why not centralize in Engine
- Engine sees only final CV channels, not per-track gate edges or slide state.
- Track-to-CV routing can rotate/override, so channel->track mapping is not stable.
- Internal timing (micro-gates, retriggers, per-step gates) is only known inside each engine.

Per-track wiring pattern (reference)
- NoteTrackEngine: sendGate/sendCv/sendSlide on gate/CV changes.
- CurveTrackEngine: sendGate on gate queue edges, sendCv on CV updates.

Recommended wiring for other tracks
- Indexed: sendCv when triggerStep sets new _cvOutputTarget; sendGate on gate edges (0->>0 and >0->0); sendSlide when slide toggles.
- DiscreteMap: sendGate when currentGate changes; sendCv when _cvOutput changes (or when stage change updates _targetCv).
- Tuesday: sendGate on micro-gate queue events and retrigger edges; sendCv when gate events carry a cvTarget or when slide updates _cvOutput.

MIDI mapping behavior
- MidiOutputEngine maps CV to MIDI note: note = 60 + floor(cv*12).
- Velocity/control use CV scaled to 0..127 and clamped.
