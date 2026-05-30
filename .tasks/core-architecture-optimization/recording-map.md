# Recording map

_Surveyed 2026-05-30. Research artifact — no direction chosen. Live MIDI/CV record-into-sequence. The second Note/Curve-only subsystem (after linking) that doesn't generalize to the 8 track types — and it has a logged consequence (the NoteTrack-parity gap in `stochastic-track-port/finalize.md`)._

## Components

- `RecordHistory` — ring of recent note events with tick stamps; tracks the currently-active note for **live monitoring** (`isNoteActive()` / `activeNote()`).
- `StepRecorder` — writes incoming notes into a `NoteSequence` at a step cursor; `start(sequence)` / `stop()` / `process(message, sequence, noteMap)` / `setStepIndex`.
- `CurveRecorder` — the Curve-track analogue (records into curve steps).

## Flow (Note track)

The base `TrackEngine` declares the contract: `receiveMidi`, `monitorMidi`, `clearMidiMonitoring`. Only **NoteTrackEngine** (notes) and **CurveTrackEngine** (curve) implement it.

`NoteTrackEngine::monitorMidi(tick, message)` (`NoteTrackEngine.cpp:520`):
- `_recordHistory.write(tick, message)` — feed history.
- if recording, `_stepRecorder.process(message, *_sequence, noteMap)` — write the note into the sequence at the cursor.

Live monitoring path (`:494`): when not recording but a note is held, output the live note (`_recordHistory.activeNote()` → `noteFromMidiNote` → transpose).

Record start/stop (`:445-450`) is driven by record mode (PlayState); the fall-through at step 8 of `Engine::receiveMidi` (see midi map) routes un-consumed MIDI into `monitorMidi`.

## The Note/Curve assumption

`StepRecorder` writes into a `NoteSequence` step model; `RecordHistory` reasons in MIDI notes. Both assume a **stepped note/curve sequence** as the record target. The procedural types have no equivalent "write this note at step N" target:
- Stochastic generates from seeds/laws, not stored per-step notes.
- Tuesday algo-generates; PhaseFlux is a stateless ramp; DiscreteMap/Indexed map inputs.

So recording (like linking) is structurally a Note/Curve feature. New track types return the base no-ops → can't accept external MIDI to record or monitor. This is the parity gap flagged in `finalize.md` ("No MIDI input path" for Stochastic).

## Candidates (deferred — not chosen)

- **Accept and scope:** recording stays Note/Curve-only; make the UI/record-arm not imply it works on other types. Cheap, honest.
- **Generalise:** a per-track-type "capture" contract (what does this engine do with an incoming note?) — large, overlaps the Fractal-track "capture is a model rule" design and the Stochastic capture-from-Live work. Only worth it if record-into-procedural-tracks is a real goal.

## Relation to other maps

- Fed by the MIDI input dispatch (step 8 fall-through) — see `midi-subsystem-map.md`.
- Same era-smell as `io-layout-linking-map.md` (linking) — both are Note/Curve-only subsystems; a generalisation effort should consider them together.
- `stochastic-track-port/finalize.md` logs the parity consequence.
