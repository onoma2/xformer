# Performer MIDI Output — microtuning + polyphony

**Status:** spec / not yet built. Driver: the harmony track needs polyphonic + microtonal MIDI output,
which exposed limitations that are **global**, not harmony-specific. Fixed at the top-level MIDI-output
layer so every track benefits.

**Phased** (both halves reviewed against the code):
- **Phase 1 — Microtuning.** Independently shippable now; fixes the global 12-ET snap for *every* track.
- **Phase 2 — Polyphony.** Needs an engine data-structure restructure **and** a real poly source (the
  harmony track, not yet built). Decisions are spec'd here so it's ready when picked up.

---

## 1. Problem

`MidiOutputEngine` (`engine/MidiOutputEngine.cpp`) has two limits:
1. **Snaps every track to 12-ET** — `sendCv` does `note = 60 + floor(cv·12 + 0.01)` (`:148`). The n-tet
   scales, user `VoltScale`s, and JI harmony all lose tuning over MIDI. Standing bug.
2. **One note per source** — each of 16 `Output` slots → one note. No chords.

---

# Phase 1 — Microtuning (ready)

## 1.0 Prerequisite — fix `makePitchBend` (lands first)

`MidiMessage::makePitchBend` (`src/core/midi/MidiMessage.h:373-376`) is **dead**:
`std::min(0, std::max(0x3fff, pitchBend + 0x2000))` always returns `0`. Fix:

```cpp
static MidiMessage makePitchBend(uint8_t channel, int pitchBend) {  // signed −8192..+8191
    int v = std::max(0, std::min(0x3fff, pitchBend + 0x2000));      // repo is C++11 (CMakeLists:22) — no std::clamp; <algorithm> already included
    return MidiMessage(PitchBend | channel, v & 0x7f, (v >> 7) & 0x7f);
}
```

**Contract:** argument is the **signed** bend `−8192..+8191`, matching reader `pitchBend()` (`:198`).
This message has never been emitted correctly, so two callers are affected:
- **CC-modulator path** `MidiOutputEngine.cpp:104` open-codes `clamp((control−64)·128 + 8192, 0, 16383)`
  (unsigned) then calls the dead `makePitchBend` — i.e. **modulator pitch-bend output is currently
  broken too**. Change it to pass the signed value: `clamp((control−64)·128, −8192, 8191)`.
- **Python binding** `python/core.cpp:21` exposes `makePitchBend(channel, pitchBend)` — any sim/test
  script passing the old `0..16383` form silently changes meaning; audit + update.

Readers already on the signed contract (`RoutingEngine.cpp`, `MidiCvTrackEngine.cpp`) need no change.

## 1.1 Microtuning — note + signed pitch-bend

Per-output **`microtune`** flag on the **`Event::Note`** output (default **off**; *not* `Event::PitchBend`,
which is the CC-modulator source). When on:

```
notePos    = 60 + cv·12                                   // continuous, no floor
nearest    = clamp(round(notePos), 0, 127)
bendSemis  = notePos − nearest                            // signed
signedBend = clamp(round(bendSemis / bendRange · 8192), −8192, 8191)
emit:  makePitchBend(channel, signedBend)   THEN   noteOn(channel, nearest, vel)
```

- **Bend before note-on, unthrottled** — emitted together in the note path (`MidiOutputEngine.cpp:60-94`,
  already unthrottled; the `sendCC` rate gate at `:103` guards only modulator-class events).
- **`bendRange`** per-output (semitones, default **2**, range **1..48**). Must be **≥1** — it's a divisor;
  see §1.3 on the memset trap. Must match the synth's bend range.
- **Tuning-agnostic** — one mechanism for n-tet scales, voltage scales, and JI harmony; whatever non-ET
  CV a track produces is transmitted faithfully. Technique: `temp-ref/Xen-MIDI-Retuner` (minus MTS-ESP).

## 1.2 Engine — microtune branch + `OutputState` bend fields

**`OutputState` gains two fields** — it currently has `note`/`velocity`/`control`/`activeNote` but **no
bend** (`MidiOutputEngine.h:49-55`): `int16_t signedBend` (computed in `sendCv`, carried to the deferred
note emission in `update()`) and `int16_t activeBend` (bend of the currently-sounding note, for tie
comparison). Both default 0.

`sendCv`:
- **microtune off → unchanged:** `note = clamp(60 + floor(cv·12 + 0.01), 0, 127)`; `signedBend` left 0,
  never emitted — the *exact* current line (`:148`), byte-identical.
- **microtune on →** compute `(nearest, signedBend)` (§1.1); store both on the `OutputState`.

Note emission (`update()`, `:60-94`): when `microtune` and a note-on fires, emit
`makePitchBend(ch, signedBend)` **then** the note-on, and set `activeBend = signedBend`. No bend when off.

**Tied notes + bend.** The current tie check is note-only — `note == outputState.activeNote` → suppress
the on/off (`:70`). For a microtune output the tie condition becomes
**`note == activeNote && signedBend == activeBend`**: same nearest note with a *different* bend is a
different sounding pitch, so it is **not tied → retrigger** (note-off, new bend, note-on). (A bend-only
update without re-articulation is a deferred refinement.)

Pitch source stays the pushed **event-target CV** (`_cvOutputTarget`, `NoteTrackEngine.cpp:407`), *not*
the slid `cvOutput()` (`NoteTrackEngine.h:57`). (Push kept / pull rejected: pull reads slid CV and
misses sub-tick edges — multi-tick-per-update, `performer-architecture.md:581,596`.)

## 1.3 Model + UI — `model/MidiOutput.{h,cpp}`, `ui/model/OutputListModel.h`

- `_data.note` gains `bool microtune` (false), `uint8 bendRange` (2).
- **`setEvent` trap:** it `memset(&_data, 0, …)` (`MidiOutput.h:92`) then re-inits only `velocitySource`.
  Add the Note-case inits for `microtune=false`, `bendRange=2` — otherwise `bendRange=0` → divide-by-zero
  in §1.1.
- **Serialize** (`MidiOutput.cpp`, `Event::Note` case): append the two fields. Dev branch → no version
  bump, no migration (`ProjectVersion.h`).
- **Equality:** extend `Note::operator==`.
- **UI** (`OutputListModel.h`): `NoteItem` gains `Microtune` (toggle) + `BendRange`; `rows()` extends
  `LastNoteItem`.

## 1.4 Testing
- `TestMidiMessage` — fixed `makePitchBend` signed round-trips through `pitchBend()`.
- `TestMidiTuning` — `notePos → (nearest, signedBend)`: on-grid → 0; n-tet + JI offsets → expected
  signed values; clamp at ±`bendRange`; round-to-nearest at the half-step.
- **Back-compat characterization** — a microtune-off output emits byte-identical MIDI (the off path is
  the untouched `floor` math, so identity is by construction).

---

# Phase 2 — Polyphony (blocked on the restructure + a real source)

## 2.1 Per-voice `OutputState` — the load-bearing restructure

Today `_outputStates` is `std::array<OutputState, 16>` and `OutputState` holds **scalar** `note`,
`activeNote`, `velocity`, `slide`, `requests` (`MidiOutputEngine.h:48-56`); the note-off/steal logic
(`:71-91`) is written against the single `activeNote`. Polyphony requires **per-voice** held-note state:

```
OutputState {
    // shared (output-level):  target, control
    VoiceState voices[MAX_VOICES];   // MAX_VOICES = 8  (MidiCvTrack max, MidiCvTrack.h:73)
}
VoiceState { int8 note, activeNote, velocity, slide; uint8 requests; }   // per channel base+v
```

`16 × 8` VoiceState ≈ bytes-scale on the F405. **Voice 0 must be bit-for-bit today's scalar state** —
that is what makes Phase 1 / mono back-compat hold (§1.4 test guards it).

## 2.2 Push API + fan-out

- Add a voice index: `sendVoiceGate(trackIndex, voice, gate)` / `sendVoiceCv(trackIndex, voice, cv)`;
  existing `sendGate`/`sendCv` = voice 0. Routing still matches outputs by `source == trackIndex`
  (`MidiOutput.h:224-234`); an output bound to that track consumes voices `0..polyVoices−1`, voice `v` →
  channel `base + v`, into `voices[v]`.

## 2.3 Source capability — model-side, not the engine

`polyVoices` (1..8) is **clamped against the source track's voice count, looked up model-side** via the
editor's project handle — `project.track(srcIndex)`: Harmony → 4, `MidiCvTrack` → `voices()`
(`MidiCvTrack.h:71`), every other mode → 1. **Not** a `TrackEngine` runtime virtual: `OutputListModel`
holds only the `Output` (`OutputListModel.h`) and can't see engines; the clamp lives where the project
is reachable. Non-poly sources clamp to 1, so N>1 is unsettable on them (no silent unison).

> **Poly source reality:** the only multi-voice engine today, `MidiCvTrackEngine`, **has no push path**
> (zero `sendGate`/`sendCv` calls) — it can't be a poly MIDI source without adding that wiring (out of
> scope). So the **harmony track is the first real poly source**, and it isn't built yet. Phase 2 can't
> be end-to-end tested until harmony exists (or a stub poly source is added for tests).

## 2.4 Channel-span overflow + reset

- **Overflow clamp is NOT in `MidiConfig::setChannel`** (`MidiConfig.h:51-53`) — that's a shared generic
  template with no view of `polyVoices`. Clamp at the `Output`/`polyVoices`-edit layer: when `polyVoices`
  or base changes, enforce `0 ≤ base ≤ 16 − polyVoices`.
- **All-Notes-Off across the span:** add `MidiOutputEngine::allNotesOff()` (CC 123 on every channel in
  each output's span) and call it from `reset()` **and** the transport-stop path (`MidiOutputEngine` has
  only `reset()`/`resetOutput()` today, `:21,74` — name the new entry + its call site in `Engine`).
- Cross-output channel collisions = user's responsibility (documented), same as overlapping mono outputs.

## 2.5 Harmony as consumer

Harmony exposes `4` voices (model-side count) and **pushes** its post-voicing, post-JI voices via
`sendVoiceGate/sendVoiceCv` (event-target CV). One Note output, `microtune` on, `polyVoices=4` → the
just-tuned chord on `base…base+3`. No bespoke harmony MIDI. `harmony-spec.md`'s MIDI section → one-line
pointer here.

---

## 3. Receiver requirements / deferred / out of scope

- Receiver: per-channel pitch-bend (range = `bendRange`); for poly, multitimbral (one note/channel).
  Synths ignoring bend get plain ET; single-channel synths tune only the lowest voice.
- **Deferred:** MPE mode. **Out of scope:** MidiCv as a poly MIDI source (needs push wiring); MTS SysEx;
  `libMTS` master (sim-only dev aid at most — desktop shared-lib/IPC, won't run on F405).

## 4. Constraints

STM32F405 + desktop sim. Per-event cost is a few integer ops; `OutputState` growth (16×8 VoiceState) is
bytes-scale. Reuses `MidiOutputEngine::sendMidi`. Serialization adds fields on a dev branch (no version
bump, no migration).
