---
id: feat-tt2-text-scene-save-load
schema: plan
title: "feat: TT2 text scene save/load (upstream-compatible .txt)"
type: feat
status: completed
date: 2026-06-18
depth: standard
---

# TT2 Text Scene Save/Load

> **For Claude:** REQUIRED SUB-SKILL: use superpowers:executing-plans to implement this plan unit-by-unit.

**Goal:** Save and load a TT2 program as a human-readable `.txt` scene on the SD card, in the **upstream teletype scene format** for the common sections (scripts + patterns) plus appended TT2-only sections for fields a stock Teletype has no slot for. A native, **streaming** serializer over `TeletypeProgram` — no big file-task stack buffers, one static staging program with atomic swap-on-success. This closes the last TT1-only capability gap for TT2.

---

## Problem Frame

TT1 (legacy C-VM track) can save/load scenes as text, but its serializer is the cautionary tale: it hand-rolled a **buffered** writer with 256-byte section buffers, then had to push `ttActiveClip`, `ttSharedScriptsBackup`, `ttLineBuffer` into static SRAM to stop the file task's small call stack overflowing (`src/apps/sequencer/model/FileManager.cpp:552,629`). The format itself is gnarled because TT1 storage is **S1–S3 shared / S4 per-slot** with per-slot IO, forcing bespoke `#S4`/`#IO` sections and read-transaction rollback backups.

TT2 has neither problem. A TT2 track is **one `TeletypeProgram`** (10 scripts + 4 patterns + IO routing + per-output shaping) — exactly the shape of a single teletype scene. Upstream already ships a clean **streaming** serializer (`teletype/src/scene_serialization.c`: `serialize_scene`/`deserialize_scene` over a `tt_serializer_t` char-callback, one ~36-byte line buffer, no rollback staging). The format and the streaming technique are the precedent; TT2 just needs a native serializer over its own owned model rather than re-coupling to the C-VM `scene_state_t`.

---

## Context & References

- **Upstream format + streaming technique** — `teletype/src/scene_serialization.c` `serialize_scene` (`:26`): emits scene description lines, then `#1`–`#8`/`#M`/`#I` (one `print_command` line each), then `#P` (4 patterns: len/wrap/start/end rows + 64×4 tab-separated values), then `#G` (grid). Streams via `stream->write_char`/`write_buffer`. This is the section layout and the no-buffer discipline to mirror.
- **Line printer (write) — already exists:** `src/apps/sequencer/engine/TT2Printer.h` — `tt2PrintCommand(const TT2Command&, char *out, size_t cap)` and `tt2PrintScript(...)`. (Built in `docs/plans/2026-06-14-007-feat-tt2-native-printer-plan.md`.)
- **Line parser (read) — already exists:** `src/apps/sequencer/engine/TT2ScriptLoader.h` — `parseLine(const char *text, TT2Command &dst)`, `loadScriptText(program, scriptIndex, text)`.
- **Model:** `src/apps/sequencer/model/TeletypeProgram.h` — `TT2Script scripts[10]`, `TT2Pattern patterns[4]`, `triggerSource[8]`, `cvInputSource[6]`, `cvOutputRange/Offset/QuantizeScale/RootNote[8]`. Runtime-only `metroSyncNum/Den` (M.C) live in `src/apps/sequencer/model/TeletypeRuntime.h`.
- **FileManager pattern to mirror:** `src/apps/sequencer/model/FileManager.{h,cpp}` — `writeTeletypeTrack`/`readTeletypeTrack` slot + path overloads, `writeFile`/`readFile` slot helpers, `FileHeader`, `readFirstLine` (name probe). `FileType` enum in `src/apps/sequencer/model/FileDefs.h` (Project=0, UserScale=1, TeletypeScript=2, TeletypeTrack=3).
- **TT1 anti-pattern (what NOT to do):** `FileManager.cpp` `writeScriptSection` (256-byte `buffer`), `ttActiveClip`/`ttSharedScriptsBackup` static rollback staging, the per-field parser sprawl (`parseTriggerInputSource`/`parseCvInputSource`/…).

---

## Scope Boundaries

**In scope:**
- A native streaming serializer/deserializer over `TeletypeProgram` (new `src/apps/sequencer/engine/TT2SceneSerializer.{h,cpp}`).
- Upstream-compatible common sections: `#1`–`#8`, `#M`, `#I`, `#P`.
- TT2 extension sections for routing + shaping + clock-sync (see Format).
- `FileManager` read/write (slot + path) + a new `FileType` value.
- One static staging `TeletypeProgram` + atomic swap-on-success (no per-field rollback scatter).
- A UI save/load action on the TT2 track (file-select slot flow, mirroring TT1).
- Round-trip + edge-case tests (host unit tests over the serializer, no SD).

**Out of scope / deferred:**
- **Grid `#G`** — TT2 has no grid: write skips it, read ignores it.
- **Scene bank** (multiple programs per w-pattern) — `docs/plans/2026-06-14-004-feat-tt2-supertrack-design.md`.
- **Guaranteeing TT2→stock-Teletype reload** — TT2 *reads* standard scenes (common sections); a TT2 scene with extension sections loading cleanly on real hardware is best-effort (depends on upstream parser tolerance), not a requirement.
- **Per-line comment/disable round-trip beyond what `tt2PrintCommand` already emits.**

---

## Scene Text Format (directional)

> Directional guidance for review, not implementation specification. Section tags below are proposals; finalize exact tags at implementation.

```
# optional name/comment lines (TT2 emits the program name as a leading comment; ignored on read)

#1
<line 0 of script 1, via tt2PrintCommand>
...
#8
...
#M
<metro script lines>
#I
<init script lines>

#P
<len0>\t<len1>\t<len2>\t<len3>
<wrap0..3>
<start0..3>
<end0..3>
<val[0][0..3]>          ; 64 rows × 4 patterns, tab-separated
...

; --- TT2 extension sections (a stock Teletype scene omits these → init() defaults on load) ---
#TI                      ; 8 trigger-input sources, one per line (enum name or index)
#CI                      ; 6 CV-in sources (IN/PARAM/X/Y/Z/T)
#CO                      ; 8 CV-output shaping rows: range  quantizeScale  offset  rootNote
#MC                      ; metro clock-sync: num  den   (absent/den=0 ⇒ free ms)
```

**Read rules:** dispatch on `#`-tag; known tags fill their fields, unknown tags skip to the next `#` or EOF; missing extension sections leave `init()` defaults. Streaming both directions — one ~48-byte line buffer, never a whole-section buffer.

---

## Implementation Units

### U1. Native streaming scene serializer — write path

**Goal:** Emit a `TeletypeProgram` as upstream-format `.txt` + TT2 extension sections, streaming through a minimal char/line sink (no section buffers).

**Requirements:** In-scope serializer (write); upstream-compatible common sections; extension sections.

**Dependencies:** none.

**Files:**
- Create: `src/apps/sequencer/engine/TT2SceneSerializer.h`, `src/apps/sequencer/engine/TT2SceneSerializer.cpp`
- Test: `src/tests/unit/sequencer/TestTT2SceneSerializer.cpp`

**Approach:**
- Define a tiny write sink so tests stream to a memory buffer and `FileManager` streams to `fs::FileWriter`. Mirror upstream `tt_serializer_t` (a `void *data` + `writeBuffer(data, ptr, len)` / `writeChar(data, c)` pair), or an equivalent callback — no `std::function`, no SD coupling in the engine layer.
- Walk: name comment → `#1`–`#8`/`#M`/`#I` (per line: `tt2PrintCommand` into one ~48-byte stack buffer, then stream it) → `#P` (len/wrap/start/end + 64×4 values, tab/newline separated, integer-to-text inline) → `#TI`/`#CI`/`#CO`/`#MC`.
- Sources/range/scale: emit a stable text token per enum (short codes already used by the I/O page, e.g. `I1`/`G1`/`5B`) — keep the encode helper next to the decode helper (U2) so they stay paired, unlike TT1's scattered name funcs.

**Patterns to follow:** upstream `serialize_scene` streaming structure; `tt2PrintCommand` for lines; the I/O-page 2-char source/range codes (`TT2IoConfigPage.cpp`).

**Test scenarios:**
- Happy: a known program (2 non-empty scripts, metro+init set, one pattern with values, non-default shaping on CV1, `M.C` set) serializes to a buffer; assert the section headers appear in order and a sampled script line round-trips textually via `tt2PrintCommand`.
- Edge: empty program (all scripts length 0, default IO/shaping) emits headers with no body lines and `#MC` reflecting free mode (den 0 / section omitted).
- Edge: a script at full 6-line length emits exactly 6 lines under its header.
- Verification: output contains no whole-section buffer (review) and uses only a fixed small line buffer.

---

### U2. Native streaming deserializer — read path + staging

**Goal:** Parse a `.txt` scene into a **staging `TeletypeProgram`**, tolerant of missing/unknown sections, then the caller swaps it into the track only on success (atomic).

**Requirements:** In-scope deserializer (read); missing-field defaulting; atomic load.

**Dependencies:** U1 (shared section tags + source/range codec).

**Files:**
- Modify: `src/apps/sequencer/engine/TT2SceneSerializer.h`, `src/apps/sequencer/engine/TT2SceneSerializer.cpp`
- Test: `src/tests/unit/sequencer/TestTT2SceneSerializer.cpp`

**Approach:**
- Read sink mirrors upstream `tt_deserializer_t` (`void *data` + `readChar(data)`), accumulating into one ~48-byte line buffer split on `\n`.
- `init()` the staging program first (so any omitted section keeps defaults), then dispatch each line: `#`-tag switches the current section; body lines fill the active section. `#1`–`#8`/`#M`/`#I` reuse `parseLine`/`setScriptCommand`; `#P` parses tab-separated ints; `#TI`/`#CI`/`#CO`/`#MC` decode via the U1 codec. Unknown `#`-tag → skip body until next `#`.
- Return success/failure (parse error, truncation). The function fills a caller-provided `TeletypeProgram&` (the staging buffer); it does **not** touch the live track — the caller swaps on success. This is the single-staging-program discipline replacing TT1's clip/script rollback scatter.

**Patterns to follow:** upstream `deserialize_scene` section dispatch; `parseLine`/`loadScriptText`; `init(program)`.

**Test scenarios:**
- Happy: serialize (U1) → deserialize → `memcmp`/field-compare equals the original program (full round-trip).
- Edge: a stock-Teletype-style input (only `#1`/`#M`/`#P`, no extension sections) loads with `init()` defaults for triggerSource/cvInputSource/shaping and free-ms metro.
- Edge: unknown `#G` section present → skipped, surrounding sections still parse.
- Error: malformed pattern row / over-long script (>6 lines) → returns failure and the staging program is not partially trusted by the caller (caller-swap contract).
- Edge: blank lines and trailing whitespace tolerated.
- Covers round-trip: serialize→deserialize identity is the core acceptance.

---

### U3. FileManager read/write + new FileType

**Goal:** Wire the serializer to SD slots, mirroring `writeTeletypeTrack`/`readTeletypeTrack`, with the streaming sinks bound to `fs::FileWriter`/`fs::FileReader` and a single static staging program for load.

**Requirements:** In-scope FileManager integration; atomic swap.

**Dependencies:** U1, U2.

**Files:**
- Modify: `src/apps/sequencer/model/FileDefs.h` (add `TeletypeV2Program = 4`)
- Modify: `src/apps/sequencer/model/FileManager.h`, `src/apps/sequencer/model/FileManager.cpp`

**Approach:**
- Add `FileType::TeletypeV2Program = 4`; extend the file-type plumbing (extension/dir/name) the way TT1 types are registered.
- `writeTt2Program(const TT2Track&, name, slot/path)`: write `FileHeader` then stream the program via U1's sink bound to `fileWriter.write(...)`.
- `readTt2Program(TT2Track&, slot/path)`: read header, deserialize (U2) into the **file-scope static staging `TeletypeProgram`** (SRAM, not the file-task stack — the explicit fix for the TT1 stack mess), and on success copy it into `track.program()`. On failure leave the track untouched.
- Reuse `readFirstLine` for the name probe in file lists.

**Patterns to follow:** `FileManager::writeTeletypeTrack`/`readTeletypeTrack`, `writeProject`/`readProject` header flow, the `ttActiveClip` static-buffer idea but reduced to **one** staging program.

**Test scenarios:**
- Test expectation: none in the unit harness for the SD path (FileManager I/O is not host-unit-tested in this repo; round-trip correctness is covered by U2 over the serializer). Verify by sim audition in U4.
- Verification: builds; `FileType::TeletypeV2Program` registered; staging program is a single static instance (review confirms no per-field rollback scatter).

---

### U4. UI save/load action on the TT2 track

**Goal:** Let the user save the current TT2 program to a slot and load one back, via the existing file-select flow.

**Requirements:** In-scope UI save/load.

**Dependencies:** U3.

**Files:**
- Modify: the TT2 track UI entry point (script view context menu or the I/O config page) — `src/apps/sequencer/ui/pages/TeletypeScriptViewPage.cpp` and/or `src/apps/sequencer/ui/pages/TT2IoConfigPage.cpp`; `src/apps/sequencer/ui/pages/FileSelectPage.*` if a new file-type case is needed.

**Approach:**
- Mirror how TT1 surfaces `writeTeletypeTrack`/`readTeletypeTrack` (context action → `FileSelectPage` over `FileType::TeletypeV2Program` slots → `FileManager::write/readTt2Program`). Run under `_engine.lock()` for the load swap (UI thread mutating the program the engine reads).
- Exact entry point (context menu item vs. a footer action) is an **implementation-time UI decision**; keep it consistent with the other TT2 pages and add no bindings beyond save + load.

**Patterns to follow:** TT1 track save/load trigger (`TrackPage`/context menu), `FileSelectPage` usage, the `_engine.lock()` discipline in `TeletypeScriptViewPage`/`TT2TrackEngine` UI mutations.

**Test scenarios:**
- Test expectation: none (UI wiring) — verified by sim audition: save a program to a slot, reset/modify it, load it back, confirm scripts/patterns/IO/shaping/`M.C` restore.

---

### U5. Round-trip + portability tests

**Goal:** Lock the serializer contract with host unit tests, including the defaulting and tolerance behaviors.

**Requirements:** All serializer requirements.

**Dependencies:** U1, U2.

**Files:**
- Modify: `src/tests/unit/sequencer/TestTT2SceneSerializer.cpp`
- Modify: `src/tests/unit/CMakeLists.txt` (register the new test) if not auto-globbed.

**Approach:** Consolidate the round-trip identity test, the stock-scene-defaults test, the unknown-section-skip test, and the malformed-input failure test. Build a representative program in a helper to avoid duplication.

**Patterns to follow:** existing `TestTeletypeV2*` UNIT_TEST/CASE structure; `init(program)` fixtures.

**Test scenarios:**
- Happy: full program → serialize → deserialize → field-equal (the headline acceptance).
- Edge: stock scene (common sections only) → extension fields at defaults.
- Edge: unknown section skipped.
- Error: malformed pattern/over-length script → failure, no partial commit.
- Edge: an empty program round-trips to an empty program.

---

## Verification

- New `TestTT2SceneSerializer` suite green: round-trip identity, stock-scene defaulting, unknown-section skip, malformed-input failure, empty-program round-trip.
- Full `TestTeletypeV2*` suite stays green; both trees build (`build/sim/debug`, `build/stm32/release`), STM32 links with no region overflow (the static staging program adds ~3.6 KB SRAM — confirm against the ~2 KB `.data+.bss` headroom; if it doesn't fit, the staging program shares the existing file-task static region rather than adding a new one).
- Sim: save a TT2 program to a slot, modify it, load it back — scripts/patterns/IO/shaping/`M.C` all restore.
- Review: serializer streams (no whole-section buffers); exactly one staging `TeletypeProgram`; encode/decode source-code helpers are paired in one place.

## Risks

- **SRAM headroom** — the staging `TeletypeProgram` (~3.6 KB) plus any read buffer must fit. Mitigation: reuse/extend the existing file-task static region (TT1 already parks Teletype buffers there) instead of adding a fresh static; measure `.data+.bss` against the ~2 KB headroom and fall back to a shared region if needed.
- **Format drift vs upstream** — section layout must match upstream for the common sections to keep read-portability. Mitigation: mirror `serialize_scene` field order exactly for `#1`–`#8`/`#M`/`#I`/`#P`; keep TT2 additions strictly appended.
- **Enum-token stability** — source/range codes are persisted to disk; renaming them breaks old files. Mitigation: treat the U1 codec tokens as a serialization contract; pair encode/decode and cover with the round-trip test.

## Rollback

Each unit is one commit; revert in reverse. The serializer (U1/U2) is self-contained and testable without the FileManager/UI units, so a partial landing (serializer + tests, no UI) is safe.
