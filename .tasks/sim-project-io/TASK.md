# sim-project-io

## Goal
Make the simulator (`testsim` Python module) load and save projects in the same format as the XFORMER hardware, using the hardware-identical FatFs → virtual SD card code path. Enable slot-based project I/O via `FileManager` from Python, and ensure two-way binary compatibility.

## Key files
- `src/apps/sequencer/python/testsim.cpp` — Python module entry point, Environment
- `src/apps/sequencer/python/project.cpp` — existing `loadProject`/`saveProject` bindings (bypasses FileManager)
- `src/apps/sequencer/python/sequencer.cpp` — Sequencer + Model bindings
- `src/apps/sequencer/SequencerApp.h` — creates Volume + calls FileManager::init
- `src/apps/sequencer/model/FileManager.h` — FileManager API (needs Python bindings)
- `src/apps/sequencer/model/FileManager.cpp` — writeProject, readProject, init, slotInfo
- `src/platform/sim/drivers/SdCard.h` — virtual SD card backed by `sdcard.iso`
- `src/core/fs/FileSystem.cpp` — FatFs disk_io glue to SdCard
- `build/sim/debug/src/apps/sequencer/python/testsim.cpython-313-darwin.so` — built module

## Decisions log
- (empty — not started)

## Open questions
- [ ] Does `SequencerApp` constructor complete before Environment exposes it in Python? (Simulator lazily calls `_target.create()` on first `step()`)

## Completed steps
- (none)

## Notes
Two I/O paths exist:
- **Path A (direct):** `project.load(filename)` / `project.save(filename)` — std::ifstream/ofstream, format-compatible but bypasses FileManager
- **Path B (hardware-identical):** FileManager::writeProject/readProject → FatFs → SdCard → sdcard.iso. Infrastructure in SequencerApp but no Python bindings and no formatted disk image
