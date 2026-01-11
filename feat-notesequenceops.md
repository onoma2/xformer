# NoteSequence Teletype Ops (Plan)

Goal: add minimal getter/setter ops for per-step Gate and Note, plus current
Gate/Note reads from the active playhead.

## Naming (proposed)

Step access (explicit index):
- NS.G t s           -> get Gate (0/1) at step s
- NS.G t s v         -> set Gate at step s (v != 0 => on)
- NS.N t s           -> get Note at step s (signed, -63..63)
- NS.N t s v         -> set Note at step s (clamped)

Current step access (playhead index):
- NS.GCUR t          -> get Gate at current gate step (0/1)
- NS.NCUR t          -> get Note at current note step (signed, -63..63)

## Indexing / Ranges

- Track index t: 1..8 in Teletype scripts (convert to 0..7 internally).
- Step index s: 1..64 in scripts (convert to 0..63 internally).
- Gate values: 0/1 (non-zero treated as 1).
- Note values: -63..63 (uses NoteSequence::Step::setNote clamp).

## Behavior notes

- Current step uses the engine playhead (gate stream for NS.GCUR).
- In Ikra mode, NS.NCUR should read the note stream playhead; otherwise use
  the main playhead.
- If track is not Note mode, ops should no-op or return 0 (decision TBD).
- All setters should mark the project/track dirty and trigger UI refresh.

## Implementation outline

1) Add tele bridge functions:
   - tele_ns_gate_get(track, step)
   - tele_ns_gate_set(track, step, value)
   - tele_ns_note_get(track, step)
   - tele_ns_note_set(track, step, value)
   - tele_ns_gate_cur(track)
   - tele_ns_note_cur(track)
2) Add Teletype ops (ops/hardware.* or new ops/performer.*):
   - NS.G, NS.N, NS.GCUR, NS.NCUR
3) Wire into tele_ops[] and regenerate op enums.
4) Implement engine access in TeletypeTrackEngine:
   - resolve track -> NoteSequence -> Step
   - clamp indices
   - get current gate/note step from NoteTrackEngine state
5) UI/dirty: call scene/project dirty hooks after setters.
