# MIDI subsystem map

_Surveyed 2026-05-30. Research artifact — no direction chosen. The routing maps treat "Midi" as one source enum; this maps the actual MIDI engine: output composition, the layered input dispatch, learn, and CV/Gate→MIDI._

## Output — `MidiOutputEngine`

- Per-output state: `std::array<OutputState, CONFIG_MIDI_OUTPUT_COUNT>` (`MidiOutputEngine.h:78`). Each output maps a track/modulator to a MIDI port+channel and tracks last-sent note/CC.
- Send API: `sendGate`, `sendSlide`, `sendCv`, `sendModulator`, `sendProgramChange`, plus Malekko select/save handshakes (`:24-33`).
- `update(forceSendCC)` flushes; **CC send is rate-limited** to ~50/s via `_lastSendCCTicks` against `os::ticks()` (`:79`) — a wall-time consumer (see wallclock map; candidate `WallTimer`).
- Called throughout `Engine::update()`: per-track `sendGate`/`sendCv` from track engines, `sendModulator` in the modulator loop, `update()`/`update(true)` at tick 0 and tail.

## Input — `Engine::receiveMidi()` layered dispatch

Sources pulled each update (`Engine.cpp` `receiveMidi()`): hardware `_midi`, `_usbMidi` (per cable), and **CV/Gate-derived** MIDI via `CvGateToMidiConverter` (CV1/2 or CV3/4 → notes, gated by `cvGateInput()`).

Then `receiveMidi(port, cable, message)` dispatches in **priority order** — first consumer wins:

1. drop real-time / system messages.
2. `_midiReceiveHandler` (UI-task controllers, e.g. Launchpad) — consumes if it returns true.
3. discard non-cable-0.
4. **Program change** → `playState.selectTrackPattern` for all tracks (pattern select, wraps `% CONFIG_PATTERN_COUNT`), forwarded to output. Gated by `midiIntegrationProgramChangesEnabled`.
5. `_midiLearn.receiveMidi` — learn inspects (except CV/Gate-derived).
6. `_routingEngine.receiveMidi` — routing source consumes (returns).
7. all `trackEngine->receiveMidi` — MIDI/CV tracks consume; *all* tracks see it even if one consumes ("allow all tracks to receive").
8. fall-through → MIDI monitoring + recording (see recording map).

## Components

- `MidiLearn` — maps incoming CC/note to a routing/parameter target.
- `CvGateToMidiConverter` — turns a CV+gate input pair into note on/off (so a CV/gate source can drive MIDI/CV tracks or routing).
- `MidiPort` — Midi / UsbMidi / CvGate.

## Notes / candidates (deferred)

- The input dispatch is a clean priority chain, but the order encodes policy (controllers > program-change > learn > routing > tracks > recording) in one long function — worth documenting as a contract so changes don't reorder silently.
- CC rate-limit is a second ad-hoc `os::ticks()` site → folds into the `wallclock-time-architecture` WallTimer.
- Step 7's "all tracks receive even if consumed" + step 6 routing-consume interplay is subtle; confirm no double-handling.

## Relation to other maps

- Routing maps cover MIDI-as-source (`Routing::Source::Midi`) and `_routingEngine.receiveMidi` (step 6 here).
- Recording is the step-8 fall-through → `midi-subsystem` feeds `recording-map`.
- CC rate-limit wall-time use → `wallclock-time-architecture`.
