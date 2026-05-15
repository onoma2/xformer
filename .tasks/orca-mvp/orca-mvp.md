# Orca MVP Plan — Performer Track Type

## Overview

Bring [Orca](https://wiki.xxiivv.com/site/orca.html) (the hundredrabbits esoteric grid sequencer language) to Performer as a first-class track type. The MVP targets a single Orca track that can drive Performer's gate/CV/MIDI outputs via Orca's grid-based operators.

This document is the implementation plan. Deep context (research, decisions, open questions) lives in `TASK.md`.

---

## Architecture

Follow the exact same layering as Teletype:

```
Model          Engine           UI Pages           Bridge
─────────────────────────────────────────────────────────────────
OrcaTrack  →  OrcaTrackEngine  →  OrcaGridEditPage  →  (events)
                 ↓                    OrcaIoMapPage
            orca_run()
                 ↓
            Oevent_list
                 ↓
            Gate/CV/MIDI out
```

### Design constraints (hard)

| Constraint | Value | Implication |
|------------|-------|-------------|
| `Track` container gate | `NoteTrack` = **9544 B** | `OrcaTrack` must be ≤ this or it inflates all 8 tracks |
| `TrackEngine` container gate | `TeletypeTrackEngine` = **912 B** | `OrcaTrackEngine` must be ≤ this or it inflates all 8 engines in CCMRAM |
| No `malloc` | — | All grid buffers, event lists, and state must be static arrays |
| Grid editing | OLED + 4 encoders + step buttons | No keyboard, no mouse, no touchscreen |

---

## Phase 0 — Model Prototype (RAM gate check)

**Goal:** Define `OrcaTrack` data layout and verify it compiles under the `NoteTrack` size gate.

### Data layout (proposed)

```cpp
class OrcaTrack {
    // Grid: fixed size, one per pattern slot
    static constexpr int GridWidth  = 16;  // or 32, TBD by sizeof probe
    static constexpr int GridHeight = 16;
    static constexpr int GridCells  = GridWidth * GridHeight; // 256 or 512

    // Two grids per slot: current + previous (for mark buffer)
    struct GridSlot {
        std::array<char, GridCells> glyphs{};
        std::array<uint8_t, GridCells> marks{};
    };

    static constexpr int SlotCount = 2;  // like Teletype's PatternSlotCount
    std::array<GridSlot, SlotCount> _slots{};
    uint8_t _activeSlot = 0;

    // I/O mapping: which Orca outputs → which Performer outputs
    std::array<uint8_t, 4> _midiChannelToGateOut{};   // operator ':' channel → gate
    std::array<uint8_t, 4> _midiChannelToCvOut{};     // operator ':' channel → CV
    std::array<uint8_t, 4> _ccChannelToCvOut{};       // operator '!' channel → CV

    // Playback state (saved per slot or global?)
    uint16_t _bpm = 120;
    uint32_t _tickNumber = 0;
    uint8_t _randomSeed = 1;

    // ... plus Track integration fields (linkTrack, cvOutputRotate, etc.)
};
```

### Acceptance criteria

- [ ] `cd build/stm32/release && make sequencer` succeeds
- [ ] `size` or `nm` shows `sizeof(OrcaTrack) <= sizeof(NoteTrack)` (9544 B)
- [ ] If oversize: reduce `GridWidth`/`GridHeight` or move grids outside container

---

## Phase 1 — Core VM (headless)

**Goal:** Port Orca's simulation core to Performer with static allocation, no UI.

### 1.1 Grid buffers

- Replace `field.c` dynamic allocation with `std::array<char, GridCells>`
- Replace `mbuf_reusable` with `std::array<uint8_t, GridCells>`
- Grid is stored in the model; engine gets a pointer/reference each tick

### 1.2 Event list (static ring buffer)

- Replace `Oevent_list` dynamic `realloc` with fixed-size ring buffer
- Size: e.g., 32 or 64 events per tick (plenty for Eurorack-scale grids)
- Structure: `std::array<Oevent, MaxEvents>` + head/tail indices

### 1.3 Operator implementations

Port from `temp-ref/orca-c/sim.c` + `temp-ref/orcaduino/src/instr.cpp`.

**MVP operator set** (full alphabet minus exotic I/O):

| Op | Name | Status | Notes |
|----|------|--------|-------|
| `A` | add | ✅ C ref | |
| `B` | subtract | ✅ C ref | |
| `C` | clock | ✅ C ref | |
| `D` | delay | ✅ C ref | |
| `E` | east | ✅ C ref | |
| `F` | if | ✅ C ref | |
| `G` | generator | ✅ C ref | |
| `H` | halt | ✅ C ref | |
| `I` | increment | ✅ C ref | |
| `J` | jumper (Y) | ✅ C ref | |
| `K` | konkat | ✅ C ref | |
| `L` | less | ✅ C ref | |
| `M` | multiply | ✅ C ref | |
| `N` | north | ✅ C ref | |
| `O` | read | ✅ C ref | |
| `P` | push | ✅ C ref | |
| `Q` | query | ✅ C ref | |
| `R` | random | ✅ C ref | |
| `S` | south | ✅ C ref | |
| `T` | track | ✅ C ref | |
| `U` | uclid | ✅ C ref | |
| `V` | variable | ✅ C ref | |
| `W` | west | ✅ C ref | |
| `X` | write | ✅ C ref | |
| `Y` | jymper | ✅ C ref | |
| `Z` | lerp | ✅ C ref | |
| `*` | bang | ✅ C ref | |
| `#` | comment | ✅ C ref | |
| `:` | midi | ⚠️ MVP | Map to gate+CV output instead of PortMidi |
| `!` | cc | ⚠️ MVP | Map to CV output |
| `%` | mono | ⚠️ MVP | Map to gate+CV (monophonic) |
| `=` | osc | ❌ defer | No OSC stack in Performer |
| `;` | udp | ❌ defer | No UDP stack in Performer |

### 1.4 `OrcaTrackEngine::tick()`

```cpp
TickResult OrcaTrackEngine::tick(uint32_t tick) {
    // 1. Prepare buffers from model
    Glyph* gbuf = _orcaTrack.activeGrid().glyphs.data();
    Mark*  mbuf = _orcaTrack.activeGrid().marks.data();

    // 2. Run one Orca frame
    orca_run(gbuf, mbuf, GridHeight, GridWidth,
             _tickNumber++, &_eventBuffer, _randomSeed);

    // 3. Drain events → Performer outputs
    for (const Oevent& ev : _eventBuffer) {
        switch (ev.any.oevent_type) {
            case Oevent_type_midi_note:
                handleMidiNote(ev.midi_note);
                break;
            case Oevent_type_midi_cc:
                handleMidiCc(ev.midi_cc);
                break;
            // ... etc
        }
    }
    _eventBuffer.clear();

    // 4. Return tick result
    return ...;
}
```

### Acceptance criteria

- [ ] Simulator build runs headless Orca tick without crash
- [ ] Can load a simple `.orca` file (e.g., `D8...\n.:03C`) and step through frames
- [ ] Event buffer correctly captures `:`, `!`, `%` operators

---

## Phase 2 — I/O Bridge (gate/CV/MIDI mapping)

**Goal:** Turn Orca `Oevent`s into actual Performer outputs.

### 2.1 Event-to-output mapping

Orca operators output abstract MIDI events. We map them to Performer's concrete outputs:

**`:` midi note (channel, octave, note, velocity, length)**
- Gate: trigger on bang, hold for `length`
- CV: pitch from `octave` + `note` (semitone voltage)
- Routing: per-track config selects which gate/CV pair receives the note

**`!` cc (channel, control, value)**
- CV: `value` mapped to voltage range (0–127 → unipolar or bipolar)
- Routing: per-track config selects which CV output

**`%` mono (channel, octave, note, velocity, length)**
- Same as `:` but monophonic: retrigger gate only if pitch changes, legato otherwise

### 2.2 Routing config (model)

Add to `OrcaTrack`:

```cpp
enum class OrcaOutputMode : uint8_t { GateCv, CvOnly, Midi, Last };

struct OutputRoute {
    uint8_t channel = 0;      // Orca operator channel (0-15)
    OrcaOutputMode mode = OrcaOutputMode::GateCv;
    uint8_t gateOut = 0;      // 0-7 → Performer gate output
    uint8_t cvOut = 0;        // 0-7 → Performer CV output
    Types::VoltageRange range = Types::VoltageRange::Bipolar5V;
};

std::array<OutputRoute, 4> _outputRoutes{};
```

### Acceptance criteria

- [ ] Orca `:03C` produces gate + 1V (C3) on routed outputs
- [ ] Orca `!14z` produces ~5V on routed CV output
- [ ] Multiple channels can route to different gate/CV pairs

---

## Phase 3 — Grid Editor UI

**Goal:** Minimal viable 2D grid editing on Performer's OLED.

### 3.1 Interaction model

Performer has 4 encoders + 16 step buttons + 5 function buttons. No keyboard.

**Proposed interaction:**

| Control | Action |
|---------|--------|
| **Encoder 1** | Move cursor X (column) |
| **Encoder 2** | Move cursor Y (row) |
| **Encoder 3** | Cycle character at cursor (`. 0-9 a-z A-Z * # : ! %`) |
| **Encoder 4** | Adjust parameter (e.g., BPM, grid size) |
| **Step button (hold)** | Multi-select or quick-place common chars |
| **F1** | Play / Stop |
| **F2** | Frame step (advance one tick) |
| **F3** | Context menu (clear grid, load, save, resize) |
| **F4** | Toggle uppercase/lowercase for alpha operators |
| **F5** | Switch to I/O map page |

### 3.2 Display layout

OLED is 256×64. Grid cells must be tiny.

Example for 16×16 grid:
- Cell size: ~14×3 px (very tight, maybe unreadable)
- Better: 16×8 grid with 14×6 px cells = 224×48 px viewport + 32 px status bar
- Cursor: invert cell or draw box

Alternative: **viewport into larger grid**
- Grid is 32×16 but viewport shows 16×8
- Encoder 1/2 scrolls viewport, shift+encoder moves cursor within viewport

### 3.3 Pages to create

1. **`OrcaGridEditPage`** — main grid editor (cursor, character entry, play/stop)
2. **`OrcaIoMapPage`** — route Orca channels to gate/CV outputs
3. **`OrcaFilePage`** — load/save `.orca` grids from SD card

### Acceptance criteria

- [ ] Can navigate cursor, place operators, and see grid update
- [ ] Can place `D8` + `:03C` and hear/see output when playing
- [ ] Grid persists across pattern switches

---

## Phase 4 — Track Integration

**Goal:** Wire `OrcaTrack` into Performer's track system.

### 4.1 `Track.h` changes

- Add `TrackMode::Orca` to enum
- Add `OrcaTrack` to `Container<...>` template list
- Add `orocaTrack()` accessor
- Update serialization (`trackModeSerialize`, `trackModeName`)

### 4.2 Serialization

- Grid stored as raw text (same as `.orca` format) inside project file
- Pattern slots: each slot stores its own grid text
- I/O routes stored per slot

### 4.3 Routing engine

- Orca track appears in `Routing` as a source for gate/CV (like any other track)
- Orca track can also receive clock/reset from routing (optional)

### Acceptance criteria

- [ ] Can create new track and set mode to "Orca"
- [ ] Project save/load preserves grid and I/O routes
- [ ] Grid plays in sync with transport (clock tick aligned)

---

## Phase 5 — Polish & File I/O

**Goal:** Load external Orca files, save grids, and match user expectations.

### 5.1 File format compatibility

- Read/write standard `.orca` text files (same as `orca-c` CLI)
- Import from Patchstorage / online Orca repos
- Grid size capped to model maximum; larger files rejected or truncated

### 5.2 Simulator development support

- Simulator build can load `.orca` files from host filesystem for rapid testing
- Headless CLI mode for the simulator to run N ticks and print grid state

### 5.3 RAM regression check

- Final build: verify `.data+.bss` and `.ccmram_bss` flat vs baseline
- Verify `sizeof(OrcaTrack) <= sizeof(NoteTrack)`
- Verify `sizeof(OrcaTrackEngine) <= sizeof(TeletypeTrackEngine)`

---

## Risk register

| Risk | Likelihood | Impact | Mitigation |
|------|-----------|--------|------------|
| Grid too large for container gate | High | Blocks feature | Start with tiny grid (8×8), measure, expand iteratively |
| OLED too small for usable grid editor | Medium | Poor UX | Viewport model; accept that editing is cramped |
| Orca operators too CPU-heavy at tick rate | Low | Audio glitch | Profile `orca_run()`; grid is tiny vs desktop Orca |
| MIDI/CV mapping confuses users | Medium | Support burden | Simple default routing; good docs |
| No pre-existing C++ port to reuse | — | Extra work | `orcaduino` + `orca-c` provide complete operator logic |

---

## Success criteria

1. **RAM flat**: No increase in `.data+.bss` or `.ccmram_bss` vs baseline (or identified and accepted)
2. **Grid runs**: A 16×8 (or larger) Orca grid executes correctly for at least 10 standard Orca example programs
3. **Musical output**: `:`, `!`, `%` operators produce audible/visible gate and CV output
4. **Editable**: User can create and edit a simple sequence (e.g., clock + midi note) entirely on-device
5. **Persistent**: Grid saves and loads with project
