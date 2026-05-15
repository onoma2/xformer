# orca-mvp

## Goal

Port Orca (the hundredrabbits esoteric grid sequencer language) to Performer as a new track type: **OrcaTrack**. The MVP must execute Orca programs on a fixed-size grid, map Orca musical output operators to Performer's gate/CV/MIDI routing system, and provide a minimal OLED-based grid editor. This follows the same TrackModel + TrackEngine + UI Pages pattern established by Teletype integration.

## Key files

### Reference implementations
- `temp-ref/orca-c/sim.c` / `sim.h` — official C VM core (`orca_run()`)
- `temp-ref/orca-c/gbuffer.c` / `gbuffer.h` — glyph/mark buffer helpers
- `temp-ref/orca-c/vmio.c` / `vmio.h` — event list (MIDI note/CC/PB, OSC, UDP)
- `temp-ref/orca-c/field.c` / `field.h` — grid field with malloc-based resize
- `temp-ref/orcaduino/src/instr.cpp` — C++ embedded operator implementations (no malloc)
- `temp-ref/orcaduino/include/config.h` — fixed 9×8 grid, static allocation approach
- `temp-ref/orca-lua/lib/orca.lua` — Lua Norns port (readable operator semantics)

### Performer integration points
- `src/apps/sequencer/model/Track.h` — `TrackMode` enum, `Container<...>` pattern
- `src/apps/sequencer/model/TeletypeTrack.h` — reference esolang track model (7104 B)
- `src/apps/sequencer/engine/TeletypeTrackEngine.h` — reference engine (912 B, CCMRAM)
- `src/apps/sequencer/engine/TeletypeBridge.cpp` — VM-to-engine callback bridge
- `src/apps/sequencer/ui/pages/TeletypeEditPage.cpp` — reference UI page for esolang editing
- `src/apps/sequencer/sequencer.ld` — SRAM/CCMRAM layout

### New files (to create)
- `src/apps/sequencer/model/OrcaTrack.h` — grid + pattern slots + I/O mapping
- `src/apps/sequencer/engine/OrcaTrackEngine.h/.cpp` — tick driver + event-to-CV bridge
- `src/apps/sequencer/ui/pages/OrcaGridEditPage.h/.cpp` — 2D grid cursor editor
- `src/apps/sequencer/ui/pages/OrcaIoMapPage.h/.cpp` — output routing config

## Decisions log

- 2026-05-15: Official C implementation (`orca-c`) uses `malloc`/`realloc` for grid + events. Must be replaced with fixed-size static arrays for embedded.
- 2026-05-15: `ORCAduino` (Arduino C++) proves embedded Orca is viable with flat arrays and no dynamic memory. Grid is 9×8; operators are simple switch/case functions.
- 2026-05-15: RAM gate is `NoteTrack` at 9544 B. Any new track model must stay at or below this size or it multiplies by 8 tracks. Grid size is the critical design variable.
- 2026-05-15: `TeletypeTrackEngine` = 912 B is the engine container gate in CCMRAM. Orca engine must stay under this.

## Open questions

- [ ] What fixed grid size fits inside `NoteTrack` container budget? (e.g., 16×16 = 256 cells → ~512 B glyphs+marks + variables + event buffer + metadata = ?)
- [ ] Which Orca I/O operators to implement for MVP? (`:` midi note, `!` cc, `%` mono, `=` osc, `;` udp — or a smaller subset mapped to gate/CV?)
- [ ] How to map Orca event model to Performer's 8 gate + 8 CV outputs + MIDI routing? (channel→track output? operator→routing target?)
- [ ] Grid editing UI: cursor navigation with encoders + step buttons? Or character-cycle per cell? Need minimal viable interaction.
- [ ] Should Orca support pattern slots like Teletype (4 scripts + pattern data per slot), or one grid per pattern?
- [ ] File format: reuse `.orca` text format, or custom serialization? How to load/save from Performer's FileManager?

## Completed steps

- [x] Researched all official Orca implementations (JS/Electron, C, Lua, Uxn assembly)
- [x] Cloned reference codebases: `orca-c`, `orca-lua`, `orcaduino` → `temp-ref/`
- [x] Analyzed Performer Teletype integration pattern (model + engine + bridge + UI)
- [x] Assessed RAM constraints: `.data+.bss=119,960`, `NoteTrack=9544`, `TeletypeTrackEngine=912`
- [x] Read `orca-c` core: `orca_run()` is stateless aside from grid buffers; event list is the output boundary
- [x] Read `orcaduino` C++ source: all operators implemented as simple grid_cell mutation functions

## Notes

- Orca creator quote: "I'm pretty sure you can make Orca client in less than 500 lines of code now."
- `orcaduino` is missing MIDI output operators — it focuses on visual/LED output. We need to add the musical I/O operators for Performer.
- The C `Oevent` struct is a tagged union. For Performer we only need a subset: `Oevent_midi_note` → gate+CV, `Oevent_midi_cc` → CV, possibly `Oevent_midi_pb` → CV.
- Consider using the Lua `orca.lua` as a readable spec for operator semantics when the optimized C is too dense.
