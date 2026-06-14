---
id: feat-tt2-midi
schema: plan
title: "feat: TT2 native MIDI (MI.* family + MIDI-triggered firing)"
type: feat
status: completed
date: 2026-06-14
depth: standard
---

# feat: TT2 native MIDI (MI.* family + MIDI-triggered firing)

## Summary

Make `TrackMode::TeletypeV2` respond to incoming MIDI natively — the last
bridge-parity gap (P6) and an essential feature. Mirror what the legacy
`TeletypeTrackEngine` already does through `scene_state.midi`, but with a native
TT2 buffer and no `scene_state_t`: a per-track MIDI source filter, a runtime MIDI
event buffer, a `TT2TrackEngine::receiveMidi` path that populates the buffer and
fires a script on the event, and the native `MI.*` query op family reading that
buffer. The legacy `TeletypeTrackEngine` (`receiveMidi`/`processMidiMessage` +
`scene_midi_t`) is the source-of-truth reference.

---

## Scope Boundaries

**In scope:** a native MIDI event buffer in `TT2Runtime`; a per-track MIDI
source filter + per-event fire-script config in `TeletypeProgram` (serialized,
defaulted, UI deferred); `TT2TrackEngine::receiveMidi` + `processMidiMessage`
(filter → populate → fire); the native `MI.*` query family reimplemented to read
the runtime buffer; MIDI-triggered script firing.

### Deferred to Follow-Up Work
- **MIDI clock ops** `MI.CLKR` / `MI.CLKD` — need MIDI-clock pulse counting/division; defer unless cheap during U4.
- **The editor MIDI-source UI** — choosing port/channel/fire-script per track rides with the deferred editor I/O grid (`docs/plans/2026-06-14-001-feat-tt2-editor-repoint-plan.md`); this plan ships the config *data* (defaulted).
- **MIDI output** (sending MIDI from scripts) — not part of the `MI.*` query surface.

---

## Key Technical Decisions

- **Native MIDI buffer in `TT2Runtime`, not `scene_state_t`.** Add a `TT2Midi`
  struct mirroring the fields of upstream `scene_midi_t` (`state.h`,
  `MAX_MIDI_EVENTS = 10`): `last_channel/last_note/last_velocity/last_controller/
  last_cc`; `on_count` + `on_channel[10]`/`note_on[10]`/`note_vel[10]`; `off_count`
  + `note_off[10]`/`off_channel[10]`; `cc_count` + `cc_channel[10]`/`cn[10]`/`cc[10]`.
  Transient (not serialized; reset on load). The `MI.*` ops read it from `runtime`.
- **`MI.*` ops are reimplemented, not reused.** The C-lib `MI.*` bodies
  (`teletype/src/ops/midi.c`) read `scene_state_t`, so — unlike euclid/drum/chaos
  (P2) — they can't be called. Reimplement each native op reading `runtime.<midi
  buffer>`, faithful to the upstream read semantics (last-event accessors,
  indexed-list accessors, counts, channels).
- **Per-track MIDI config in `TeletypeProgram`** (serialized raw like
  `triggerSource`/`cvInputSource`): a `MidiSourceConfig` (port + channel filter,
  reuse the existing `MidiSourceConfig` type + `MidiUtils::matchSource`) plus three
  fire-script selectors (`midiOnScript`, `midiOffScript`, `midiCcScript`) mirroring
  the legacy `midi.on_script`/`off_script`/`cc_script`. Default: source = match-any
  (or a sensible default port/channel), fire-scripts = none (0 = disabled), so MIDI
  populates the buffer for `MI.*` reads even before any fire-script is configured.
- **Engine receive path mirrors the legacy.** `TT2TrackEngine::receiveMidi(port,
  msg)` → `MidiUtils::matchSource(port, msg, program.midiSource)` → if matched,
  `processMidiMessage(msg)` populates the buffer and, if the matching per-event
  fire-script is set, runs it (under `ScopedHost`, like the other script entries).
  Returns true to consume on match (legacy semantics). Wired by `Engine::receiveMidi`
  which already dispatches to `trackEngine->receiveMidi`.
- **Behavioral-faithful, not bit-parity.** The `MI.*` ops are stateful reads of a
  buffer the two engines populate independently, so they are unit-tested against
  the buffer (not the `TestTeletypeV2Parity` C-VM harness, which is for stateless
  deterministic ops). `SCENE` stays the parity unsupported-op fixture.

---

## High-Level Technical Design

Directional guidance for review, not implementation spec.

```
Engine::receiveMidi(port, cable, msg)
  -> for each trackEngine: trackEngine.receiveMidi(port, msg)
       TT2TrackEngine::receiveMidi(port, msg):
         if !MidiUtils::matchSource(port, msg, program.midiSource): return false
         processMidiMessage(msg):
           clear/append into runtime.midi (note_on/off ring, cc ring, last_*)
           pick fireScript = on/off/cc selector for this event type
           if fireScript valid: ScopedHost h(this); runScript(fireScript)
         return true   // consume on match

MI.* op (native)  -> read runtime.midi.<field>  (last-event / indexed / counts / channel)
```

---

## Implementation Units

### U1. TT2 MIDI buffer in the runtime + populate/clear helpers

**Goal:** A native MIDI event buffer + the pure helpers that clear it and append note-on / note-off / cc events (the testable core).
**Dependencies:** none.
**Files:** `src/apps/sequencer/model/TeletypeRuntime.h`, `src/tests/unit/sequencer/TestTeletypeV2Midi.cpp` (new), `src/tests/unit/sequencer/CMakeLists.txt`.
**Approach:** Add `TT2_MIDI_EVENTS = 10` and a `TT2Midi` struct mirroring `scene_midi_t`'s event fields (last_*, on ring + count, off ring + count, cc ring + count). Add `TT2Midi midi;` to `TT2Runtime` (zeroed by `init`). Free helpers: `tt2MidiClear(TT2Midi&)`, `tt2MidiNoteOn(TT2Midi&, ch, note, vel)`, `tt2MidiNoteOff(TT2Midi&, ch, note)`, `tt2MidiCc(TT2Midi&, ch, cc, val)` — each updating the ring (bounded by `TT2_MIDI_EVENTS`), counts, and `last_*` exactly as upstream `processMidiMessage` does. Bump the `TT2Runtime`/`TT2Track` size asserts.
**Execution note:** Test-first on the append/clear helpers.
**Patterns to follow:** upstream `scene_midi_t` (`teletype/src/state.h`) + the population logic in `TeletypeTrackEngine::processMidiMessage` (`src/apps/sequencer/engine/TeletypeTrackEngine.cpp` ~1719); the `inputLevel`/`clockMs` runtime-field precedent + size-assert bumps.
**Test scenarios:**
- Note-on append: count increments, `note_on`/`note_vel`/`on_channel` slots set, `last_note`/`last_velocity`/`last_channel` updated.
- Overflow: appending > `TT2_MIDI_EVENTS` note-ons does not write out of bounds (bounded like upstream).
- Note-off and CC append into their rings + counts + `last_*`.
- `tt2MidiClear` zeroes counts and last-event fields.

### U2. Per-track MIDI source + fire-script config in TeletypeProgram

**Goal:** Serialized, defaulted per-track MIDI source filter + which-script-fires selectors the engine reads (editor edits later).
**Dependencies:** none.
**Files:** `src/apps/sequencer/model/TeletypeProgram.h`, `src/tests/unit/sequencer/TestTeletypeV2Midi.cpp`.
**Approach:** Add a `MidiSourceConfig midiSource;` (reuse `src/apps/sequencer/model/MidiSourceConfig.h`) plus `uint8_t midiOnScript; uint8_t midiOffScript; uint8_t midiCcScript;` (0 = disabled, else 1-based script index) to `TeletypeProgram`. `init()` defaults: source = the `MidiSourceConfig` default (match-any / first port), fire-scripts = 0. Serializes with the existing raw-struct write; bump the size assert. No migration.
**Patterns to follow:** `TeletypeTrack::midiSource()` + `midi.on_script`/`off_script`/`cc_script` (legacy); the `triggerSource`/`cvInputSource` defaulted-config precedent in `TeletypeProgram.h`.
**Test scenarios:**
- After `init`, `midiOnScript`/`midiOffScript`/`midiCcScript` default to 0 (disabled).
- `midiSource` defaults to the `MidiSourceConfig` default (round-trips through the existing program serialization).

### U3. TT2TrackEngine::receiveMidi + processMidiMessage (populate + fire)

**Goal:** Route matched MIDI into the buffer and fire the configured script.
**Dependencies:** U1, U2.
**Files:** `src/apps/sequencer/engine/TT2TrackEngine.h`, `src/apps/sequencer/engine/TT2TrackEngine.cpp`.
**Approach:** Override `virtual bool receiveMidi(MidiPort, const MidiMessage&)`. Filter via `MidiUtils::matchSource(port, msg, program.midiSource)`; on no match return false (let other tracks see it). On match, `processMidiMessage(msg)`: dispatch note-on (vel>0) / note-off (or note-on vel 0) / cc into the U1 helpers, then if the matching fire-script selector is non-zero run it under `ScopedHost host(this)` (so `MI.*` + host ops resolve during the fired script). Return true (consume). Mirror the legacy `receiveMidi`/`processMidiMessage` control flow.
**Patterns to follow:** `TeletypeTrackEngine::receiveMidi`/`processMidiMessage` (`src/apps/sequencer/engine/TeletypeTrackEngine.cpp` ~1703); the existing `ScopedHost` + `runScript` use in `TT2TrackEngine` (trigger-input firing).
**Test scenarios:** `Test expectation: none (engine integration)` — covered by U1 helper tests + U4 op tests + the engine smoke test (`TestTeletypeV2TrackEngineSmoke`) compiling/linking the receive path; on-device a MIDI note fires the configured script and `MI.*` reads it.

### U4. Native MI.* op family

**Goal:** Reimplement the `MI.*` query ops reading the runtime MIDI buffer.
**Dependencies:** U1.
**Files:** `src/apps/sequencer/engine/TeletypeNativeOps.cpp`, `src/tests/unit/sequencer/TestTeletypeV2Midi.cpp`.
**Approach:** Add native handlers for the query family reading `runtime.midi`, faithful to upstream `teletype/src/ops/midi.c` semantics, and register them in the op table. Cover: last-event (`MI.LE`/`MI.LN`/`MI.LNV`/`MI.LV`/`MI.LVV`/`MI.LO`/`MI.LC`/`MI.LCC`/`MI.LCCV`/`MI.LCH`), indexed-list (`MI.NL`/`MI.N`/`MI.NV`/`MI.V`/`MI.VV`/`MI.OL`/`MI.O`/`MI.CL`/`MI.C`/`MI.CC`/`MI.CCV`), channels (`MI.NCH`/`MI.OCH`/`MI.CCH`), and `MI.$` (get/set the MIDI-trigger script binding — wire to the U2 selectors). Defer `MI.CLKR`/`MI.CLKD` (clock) unless cheap. Confirm `E_OP_MI_*` enum ids + get/set arities against `teletype/src/ops/midi.c`.
**Patterns to follow:** upstream `op_MI_*` bodies; the existing native get-only / indexed op patterns in `TeletypeNativeOps.cpp` (e.g. `STATE`, `Q.*`).
**Test scenarios:**
- Seed the buffer via U1 helpers (or `MI.*`-adjacent setup), then: `MI.LN`/`MI.LV`/`MI.LCH` return the last note/velocity/channel; `MI.N 1`/`MI.NV 1` return the indexed note/velocity; `MI.NL` returns the note-on count; CC: `MI.LCC`/`MI.LCCV`, `MI.CC 1`/`MI.CCV 1`, `MI.CL` count.
- Empty buffer: counts are 0, last-event reads return 0; out-of-range index reads return 0 (no OOB).
- `MI.$` set then get round-trips the trigger-script binding.

### U5. Verify + roadmap flip

**Goal:** Confirm end-to-end and close the bridge-parity roadmap.
**Dependencies:** U1, U2, U3, U4.
**Files:** `docs/teletype_v2.md` (P6 → done; bridge-parity complete), full `TestTeletypeV2*` suite + STM32 release.
**Approach:** Suite + STM32 green; engine smoke links the receive path. Flip the roadmap: P6 done → all bridge-parity gaps closed (only the editor + deferred buckets remain). Update `.tasks/STATUS.md`.
**Test scenarios:** `Test expectation: none (verification + docs)`.

---

## System-Wide Impact

- `TT2Runtime` grows by the `TT2Midi` buffer (~80 bytes) — bump `sizeof(TT2Runtime)` + `sizeof(TT2Track)` asserts; transient (not serialized), so no save-format impact.
- `TeletypeProgram` grows by `MidiSourceConfig` + 3 script selectors — bump its size assert; serialized (dev files break freely, no migration).
- `TT2TrackEngine` gains a `receiveMidi` override; `Engine::receiveMidi` already dispatches to it — no new engine plumbing.
- No change to the parity harness's stateless surface; `MI.*` are unit-tested against the buffer.

---

## Risks & Mitigations

- **`MidiSourceConfig` shape / `matchSource` signature** may differ from assumption. Mitigation: U2 reads `MidiSourceConfig.h` + `MidiUtils.h` directly before wiring.
- **Fire-script reentrancy** — a MIDI-fired script running mid-`update` while `ScopedHost` is set. Mitigation: mirror the legacy guard ordering; `receiveMidi` runs in the engine MIDI-dispatch pass, set `ScopedHost` around the fired script as the trigger path does.
- **`MI.*` read-semantics drift** from upstream (indexing, 1-based vs 0-based). Mitigation: port each op against `teletype/src/ops/midi.c`; unit-test indexed + last-event + count reads.
- **Size asserts** (`TT2Runtime`/`TT2Track`/`TeletypeProgram`) trip. Mitigation: bump; confirm STM32 release links.

---

## Verification

- U1/U4 unit tests green (buffer append/overflow/clear; `MI.*` last-event/indexed/count/channel reads; `MI.$` round-trip).
- Full `TestTeletypeV2*` suite + STM32 release green; engine smoke links the receive path.
- On-device: a MIDI note/CC matching the track's source fires the configured script and is readable via `MI.*`.
- `docs/teletype_v2.md` + `.tasks/STATUS.md` updated — P6 done, bridge-parity complete.
- Execution posture: U1 buffer helpers test-first.
