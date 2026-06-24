# MIDI Output Phase 1 — Microtuning Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans (or superpowers:subagent-driven-development for same-session) to implement this plan task-by-task.

**Goal:** Global per-output **microtuning over MIDI** — emit `note + signed pitch-bend` carrying a track's true (non-12-ET) pitch instead of snapping to the nearest semitone. Fixes the standing 12-ET snap for *every* track (n-tet scales, user `VoltScale`s, future JI harmony).

**Architecture:** Fix the dead `makePitchBend`; add a per-output `microtune`+`bendRange` flag to the `Event::Note` output; `OutputState` carries the computed `signedBend`; `sendCv` computes `(nearest, signedBend)` when microtune is on; the note path emits the bend **before** the note-on (unthrottled) and the tie check includes the bend. **Microtune off is the exact current `floor` path → byte-identical.** Per-output opt-in.

**Tech stack:** C++11 (`src/CMakeLists.txt:22` — no `std::clamp`), STM32F405 + desktop sim. Build sim: `make -C build/sim/debug sequencer`. Unit tests: `make -C build <TestName>` then `./build/src/tests/unit/sequencer/<TestName>`.

**Scope:** Spec `docs/midi-output-spec.md` **Phase 1 only**. Phase 2 (polyphony) is OUT — it's blocked on the harmony track existing. Dev branch: **no `ProjectVersion` bump, no migration**.

**Checkpoints, not commits:** each task ends at a Checkpoint — build/tests green, then stop and ask the user to commit (this repo commits at task/phase boundaries, not via in-plan `git commit`).

**Line numbers are approximate** — the file + symbol names are the stable references; grep to confirm each before editing.

---

### Task 1: Fix `makePitchBend` (signed contract)

**Files:**
- Modify: `src/core/midi/MidiMessage.h:373-376`
- Create: `src/tests/unit/sequencer/TestMidiMessage.cpp`
- Modify: `src/tests/unit/sequencer/CMakeLists.txt` (register)

**Step 1: Write the failing test** (`TestMidiMessage.cpp`)
```cpp
#include "UnitTest.h"
#include "core/midi/MidiMessage.h"

UNIT_TEST("MidiMessage") {
    CASE("makePitchBend signed round-trips through pitchBend()") {
        expectEqual(MidiMessage::makePitchBend(0, 0).pitchBend(), 0, "center");
        expectEqual(MidiMessage::makePitchBend(0, 4096).pitchBend(), 4096, "up");
        expectEqual(MidiMessage::makePitchBend(0, -4096).pitchBend(), -4096, "down");
        expectEqual(MidiMessage::makePitchBend(0, 99999).pitchBend(), 8191, "clamp hi");
        expectEqual(MidiMessage::makePitchBend(0, -99999).pitchBend(), -8192, "clamp lo");
    }
}
```
Register in `CMakeLists.txt` next to the others: `register_sequencer_test(TestMidiMessage TestMidiMessage.cpp)`.

**Step 2: Run to verify it fails**
`make -C build TestMidiMessage && ./build/src/tests/unit/sequencer/TestMidiMessage`
Expected: FAIL — current `makePitchBend` clamp `std::min(0, std::max(0x3fff, …))` collapses to 0, so every `pitchBend()` reads `-0x2000` (−8192).

**Step 3: Fix** (`MidiMessage.h:373-376`)
```cpp
static MidiMessage makePitchBend(uint8_t channel, int pitchBend) {  // signed −8192..+8191
    int v = std::max(0, std::min(0x3fff, pitchBend + 0x2000));      // C++11: no std::clamp; <algorithm> already included
    return MidiMessage(PitchBend | channel, v & 0x7f, (v >> 7) & 0x7f);
}
```
Contract: argument is the **signed** bend, matching reader `pitchBend()` (`:198`, returns `value − 0x2000`).

**Step 4: Run to verify it passes** — same command → PASS.

**Step 5: Checkpoint** — `make -C build/sim/debug sequencer` clean; stop, ask user to commit.

---

### Task 2: Reconcile the CC-modulator bend + audit the Python binding

The modulator pitch-bend path open-codes an *unsigned* value then calls the (now-fixed) `makePitchBend`, which adds `+0x2000` itself → double-offset. (It produced nothing before — the message was dead — so this also un-breaks modulator pitch-bend.)

**Files:**
- Modify: `src/apps/sequencer/engine/MidiOutputEngine.cpp` (the `OutputState::PitchBend` block, ~`:102-108`)
- Audit: `src/apps/sequencer/python/core.cpp:21`

**Step 1: Read** `MidiOutputEngine.cpp:102-108` — the `if (sendCC && hasRequest(PitchBend))` block computing `bend = clamp((control − 64)·128 + 8192, 0, 16383)`.

**Step 2: Change to the signed contract**
```cpp
int bend = clamp((int(outputState.control) - 64) * 128, -8192, 8191);   // was: …*128 + 8192, 0, 16383
sendMidi(port, MidiMessage::makePitchBend(channel, bend));
```
(`clamp` here is the project `core/math/Math.h` clamp, already in scope in this .cpp.)

**Step 3: Audit the Python binding** — `src/apps/sequencer/python/core.cpp:21` exposes `makePitchBend(channel, pitchBend)` (signature unchanged). Run `rg -n 'makePitchBend\(' src` (there is **no** root `python/` dir — it lives under `src/apps/sequencer/python`). The only hits are the definition, the line fixed in Step 2, and the binding passthrough — **no script callers**, so this audit comes up empty (expected); nothing to change.

**Step 4: Verify** — `make -C build/sim/debug sequencer` clean; re-run any existing MIDI tests green (`TestRouteMidiLearn`, `TestParamTableMidiCv`).

**Step 5: Checkpoint** — stop, ask user to commit.

---

### Task 3: Microtune math helper + `TestMidiTuning`

A pure, testable `cv → (nearest, signedBend)`. Put it in a small header so both the engine (Phase 1) and tests use it.

**Files:**
- Create: `src/apps/sequencer/engine/MidiTuning.h`
- Create: `src/tests/unit/sequencer/TestMidiTuning.cpp` + register in `CMakeLists.txt`

**Step 1: Write the failing test** (`TestMidiTuning.cpp`)
```cpp
#include "UnitTest.h"
#include "apps/sequencer/engine/MidiTuning.h"

static float semisToCv(float semisAbove60) { return semisAbove60 / 12.f; }

UNIT_TEST("MidiTuning") {
    CASE("on-grid note → bend 0") {
        int n, b; cvToNoteBend(semisToCv(0.f), 2, n, b);    // cv 0 == note 60
        expectEqual(n, 60, "note"); expectEqual(b, 0, "bend");
    }
    CASE("scaling: +0.3 st (non-boundary) → bend = lround(0.3/range · 8192), nearest unchanged") {
        int n, b;
        cvToNoteBend(semisToCv(0.3f), 1, n, b); expectEqual(n, 60); expectEqual(b, 2458);  // 0.3·8192
        cvToNoteBend(semisToCv(0.3f), 2, n, b); expectEqual(n, 60); expectEqual(b, 1229);  // 0.3/2·8192
    }
    CASE("negative offset → negative bend") {
        int n, b; cvToNoteBend(semisToCv(-0.3f), 1, n, b); expectEqual(n, 60); expectEqual(b, -2458);
    }
    CASE("half-step rounds away from zero (lround) — sounding pitch identical either way") {
        int n, b; cvToNoteBend(semisToCv(0.5f), 2, n, b);   // lround(60.5)=61, bend back down
        expectEqual(n, 61, "61 at .5"); expectEqual(b, -2048, "-0.5/2·8192");
    }
    CASE("offset to nearest note is ≤ half-step → |bend| ≤ 4096 even at range 1") {
        int n, b; cvToNoteBend(semisToCv(7.49f), 1, n, b);
        expectTrue(b >= -4096 && b <= 4096, "within half-step (clamp at ±8192 is unreachable in normal use)");
    }
}
```

**Step 2: Run to verify it fails** — `make -C build TestMidiTuning && ./build/...` → FAIL (header missing).

**Step 3: Implement** (`MidiTuning.h`)
```cpp
#pragma once
#include <algorithm>
#include <cmath>

// cv in 1V/oct (note 60 == 0V). Returns nearest MIDI note + signed 14-bit pitch-bend
// (−8192..+8191) carrying the fractional offset, scaled by bendRange semitones.
inline void cvToNoteBend(float cv, int bendRange, int &note, int &signedBend) {
    if (bendRange < 1) bendRange = 1;                       // divisor guard (model also defaults ≥1)
    float notePos = 60.f + cv * 12.f;
    int nearest = std::max(0, std::min(127, int(std::lround(notePos))));
    float bendSemis = notePos - nearest;                   // signed, ~−0.5..+0.5 for ET input
    int b = int(std::lround(bendSemis / bendRange * 8192.f));
    signedBend = std::max(-8192, std::min(8191, b));
    note = nearest;
}
```

**Step 4: Run to verify it passes** — PASS.

**Step 5: Checkpoint** — stop, ask user to commit.

---

### Task 4: Model — `Note` payload `microtune` + `bendRange`

**Files:**
- Modify: `src/apps/sequencer/model/MidiOutput.h` (`Note` struct ~`:271-282`, `setEvent` `:89-100`, accessors, `operator==` `:276`)
- Modify: `src/apps/sequencer/model/MidiOutput.cpp` (`Event::Note` write/read, ~`:13-40`)
- Create: `src/tests/unit/sequencer/TestMidiOutput.cpp` + register

**Step 1: Write the failing test** (`TestMidiOutput.cpp`)
```cpp
#include "UnitTest.h"
#include "model/MidiOutput.h"
#include "core/io/VersionedSerializedWriter.h"   // confirm the actual writer/reader headers used by other model tests

UNIT_TEST("MidiOutput") {
    CASE("setEvent(Note) defaults: microtune off, bendRange 2 (no /0)") {
        MidiOutput::Output o;
        o.setEvent(MidiOutput::Output::Event::Note, true);
        expectTrue(!o.microtune(), "microtune default off");
        expectEqual(int(o.bendRange()), 2, "bendRange default 2");
    }
    CASE("microtune + bendRange round-trip via serialize") {
        MidiOutput::Output a;
        a.setEvent(MidiOutput::Output::Event::Note, true);
        a.setMicrotune(true); a.setBendRange(12);
        // write a to a buffer, read into b (mirror an existing model serialize test's harness)
        // expectTrue(b.microtune()); expectEqual(int(b.bendRange()), 12);
    }
}
```
Adapt (don't copy) the harness from `TestStochasticSequenceSerialization.cpp:10-23` (MemoryWriter + `VersionedSerializedWriter` at `ProjectVersion::Latest`, mirror for the reader) — but call **`MidiOutput::Output::write/read`** (`MidiOutput.cpp` write ~`:20-24`, read ~`:43-47`), *not* a Sequence's. Add the `VersionedSerializedReader.h`/`MemoryReaderWriter.h`/`ProjectVersion.h` includes the stub omits.

**Step 2: Run to verify it fails** — accessors/fields don't exist.

**Step 3: Implement**
- `Note` struct: add `bool microtune; uint8_t bendRange;`; extend its `operator==` with both.
- Accessors on `Output`: `bool microtune() const` / `void setMicrotune(bool)`; `uint8_t bendRange() const` / `void setBendRange(int v) { _data.note.bendRange = clamp(v, 1, 48); }` / `void editBendRange(int value, bool shift) { setBendRange(bendRange() + value); }` (mirrors `editControlNumber`, ~`MidiOutput.h:173-175`). **No `editMicrotune`** — `microtune` is a plain `bool` with no existing toggle idiom (every existing `edit*` is an enum/int); the UI toggles it directly via `setMicrotune(value > 0)` in Task 6.
- `setEvent` `Event::Note` case (after the `memset`, alongside the `velocitySource` init): `_data.note.microtune = false; _data.note.bendRange = 2;` — **required**, or `memset` leaves `bendRange = 0` → divide-by-zero in Task 3's helper.
- `MidiOutput.cpp` `Event::Note` write: `writer.write(_data.note.microtune); writer.write(_data.note.bendRange);` and the mirrored `read`. **No `ProjectVersion` bump** (dev branch — appending fields is forward-only).

**Step 4: Run to verify it passes** — PASS; re-run `TestParamTableMidiCv` green.

**Step 5: Checkpoint** — `make -C build/sim/debug sequencer` clean; stop, ask user to commit.

---

### Task 5: Engine — `OutputState` bend fields + `sendCv` branch + emit + tie

**Files:**
- Modify: `src/apps/sequencer/engine/MidiOutputEngine.h` (`OutputState` fields ~`:46-55` + `reset()` ~`:59-67`)
- Modify: `src/apps/sequencer/engine/MidiOutputEngine.cpp` — **add `#include "MidiTuning.h"`** at the top (it also pulls `<cmath>`, which the existing `std::floor` at `:148` currently only gets transitively); then `sendCv` note assign `:148`; note path `:60-94`.

**Step 1: `OutputState` fields** — add `int16_t signedBend; int16_t activeBend;`; in `reset()` set `signedBend = 0; activeBend = 0;`. Note: `activeBend` is **don't-care when `activeNote == -1`** — the note-off / steal paths (~`:79-87`) need no `activeBend` reset (the existing `-1` guard already forces the next note-on). Don't add one.

**Step 2: `sendCv` microtune branch** (`:147-148`, inside the per-output loop that already has `output`):
```cpp
if (output.takesNoteFromTrack(trackIndex)) {
    if (output.event() == MidiOutput::Output::Event::Note && output.microtune()) {
        int note, bend;
        cvToNoteBend(cv, output.bendRange(), note, bend);   // #include "MidiTuning.h"
        outputState.note = note;
        outputState.signedBend = bend;
    } else {
        outputState.note = clamp(60 + int(std::floor(cv * 12.f + 0.01f)), 0, 127);  // UNCHANGED — exact current line
    }
}
```
The `else` is the verbatim current line → microtune-off is byte-identical by construction.

**Step 3: Note emission + tie** (`:60-94`). Define `bool mt = output.microtune();` and a bend-aware tie:
- Tie-suppress only when the *sounding* pitch is unchanged: replace `note == outputState.activeNote` with
  `note == outputState.activeNote && (!mt || outputState.signedBend == outputState.activeBend)`.
- The note-off-on-change and note-on conditions (`outputState.activeNote != note`) must also fire on a bend change at the same note: use `(outputState.activeNote != note || (mt && outputState.signedBend != outputState.activeBend))`.
- Immediately **before** `makeNoteOn`, when `mt`: `sendMidi(port, MidiMessage::makePitchBend(channel, outputState.signedBend));` (unthrottled — this is the note path, not the `sendCC` block).
- On note-on, set `outputState.activeBend = outputState.signedBend;`.

Write the full modified `:60-94` block in the implementation (it's ~6 conditionals). Net behavior: same nearest note + different bend ⇒ note-off, pitch-bend, note-on (retrigger). Bend-only update is a deferred refinement.

**Step 4: Verify**
- `make -C build/sim/debug sequencer` clean.
- **Back-compat** — microtune-off: the CV path is the untouched `floor` line, so identity holds by construction; confirm existing MIDI tests stay green (`TestRouteMidiLearn`, `TestParamTableMidiCv`).
- Manual sim check: a Note output on a track using an n-tet scale, microtune on → DAW/monitor shows note + bend tracking the true pitch; microtune off → plain ET notes unchanged.

**Step 5: Checkpoint** — stop, ask user to commit.

---

### Task 6: UI — `OutputListModel` Microtune + Bend Range rows

**Files:** Modify `src/apps/sequencer/ui/model/OutputListModel.h` (`NoteItem` enum ~`:17-22`, `itemName` ~`:66-89`, `formatValue` ~`:108-120`, `editValue` ~`:135-173`).

**Step 1: Enum** — add before `LastNoteItem`:
```cpp
enum NoteItem {
    GateSource = Last,
    NoteSource,
    VelocitySource,
    Microtune,
    BendRange,
    LastNoteItem,
};
```
**Step 2: `itemName`** (the name switch is `itemName()` — `formatName()` only forwards to it) — `case Microtune: str("Microtune"); break;` `case BendRange: str("Bend Range"); break;`
**Step 3: `formatValue`** — `case Microtune: str(_output.microtune() ? "On" : "Off"); break;` `case BendRange: str("%d st", _output.bendRange()); break;`
**Step 4: `editValue`** (the edit switch is `editValue()` — `edit()` only dispatches to it) — `case Microtune: _output.setMicrotune(value > 0); break;` (right = on, left = off) `case BendRange: _output.editBendRange(value, shift); break;`

**Gate** (list-model UI isn't unit-tested): `make -C build/sim/debug sequencer` clean; walkthrough — on a Note output the **Microtune** row toggles and **Bend Range** edits, both persist across save/load (Task 4 serialize), and rows appear only for `Event::Note` (the `rows()` switch already returns `LastNoteItem` for Note).

**Step 5: Checkpoint** — stop, ask user to commit.

---

## Done = Phase 1 shippable

- `makePitchBend` signed + modulator bend reconciled (Tasks 1–2).
- Any track with a non-ET scale outputs true pitch over MIDI when its Note output has Microtune on (Tasks 3–6).
- Microtune off = byte-identical to today (Task 5, by construction + green legacy tests).
- Phase 2 (polyphony) remains a separate plan, gated on the harmony track.
