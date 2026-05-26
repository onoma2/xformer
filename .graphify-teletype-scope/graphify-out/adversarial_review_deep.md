# Adversarial Architectural Review: Teletype Integration

The integration of the `teletype` submodule into Performer is not just a feature addition; it is the collision of two completely different operating paradigms. The current implementation attempts to shove a monophonic, hardware-owning, imperative virtual machine into a polyphonic, model-driven, deterministic sequencer. This creates massive architectural friction across four major domains.

## 1. I/O Ownership (The "Double Indirection" Nightmare)

**The Paradigm Clash:**
Teletype was designed to *own* the hardware. When a script says `CV 1 5000`, the Teletype VM assumes it is directly addressing physical DAC channel 1. Performer, however, uses a late-binding Routing Matrix (`Engine::updateTrackOutputs()`) where Tracks have agnostic outputs that the user dynamically maps to physical pins via Project settings.

**The Reality in Code:**
When a script runs `CV 1 500`, it hits the C callback `tele_cv()`, which invokes `activeEngine()->handleCv(1, 500)`. This updates `_performerCvOutput[0]` inside the TrackEngine. Finally, Performer's global `Engine` might route that to physical CV 3 (if the user configured it), or drop it entirely.
*   **The UX Failure:** A user types `CV 1` in the script, but the voltage comes out of physical `CV 3`. To fix this, the user has to mentally map "Teletype Internal CV 1" -> "Track Output 1" -> "Physical CV 3". 
*   **The Hardware Failure:** Teletype scripts frequently read back I/O state (`CV 1` as a getter) expecting real-world calibration and physical pin states. Performer's bridge has to spoof this state, leading to endless calibration and slew sync bugs.

## 2. Running the VM Machine (The Global Scope Trap)

**The Paradigm Clash:**
Performer tracks are lightweight, instantiate-able C++ objects. You can have 8 Curve tracks without breaking a sweat. Teletype is a heavy C-based Virtual Machine built around a monolithic state struct (`scene_state_t`) and global I/O bindings.

**The Reality in Code:**
To make the Teletype C code communicate with the C++ TrackEngine, the codebase uses a dangerous global pointer hack: `TeletypeBridge::ScopedEngine`. Every time `update()` is called, it sets `g_activeEngine = this` and fires `tele_tick()`.
*   **Performance:** Parsing and executing string-based scripts (`run_script`) inside a real-time track engine update loop is a recipe for CPU starvation and audio dropouts on an STM32. 
*   **Scalability:** What happens if the user creates two Teletype tracks? The system will thrash the `g_activeEngine` global pointer back and forth. While memory is isolated per track (each has its own `scene_state_t _state`), the VM's heavy footprint (scripts, tracker arrays, history) means instantiating multiple Teletype VMs will obliterate the STM32's RAM.

## 3. Conflicting State Models (`scene_state_t` vs Performer `Model`)

**The Paradigm Clash:**
Performer uses a strict MVC architecture. Data lives in `Model` (Projects, Tracks, Patterns, Steps) which provides undo/redo, deterministic serialization, and UI separation. Teletype uses `scene_state_t`—a massive, opaque C struct containing executable scripts, runtime variables, and sequencer grids.

**The Reality in Code:**
`TeletypeTrack` holds `scene_state_t _state` as a blob. 
*   **The Pattern Problem:** Performer expects tracks to have "Patterns" (e.g., Pattern 1 vs Pattern 2). Teletype has no concept of track-level patterns; it has "Scenes". To bridge this, the integration forces a hack (`switchClipForPerformerPattern`) that literally swaps out chunks of the `scene_state_t` blob whenever the Performer pattern changes. 
*   **State Contamination:** Runtime variables (`X`, `Y`) and script text are mangled into the same struct as the sequences. Saving a Performer project now requires dumping the entire VM memory state to the SD card.

## 4. The Bridge (API Surface Area Bloat)

**The Paradigm Clash:**
Instead of abstracting the VM, the integration attempts to implement every single hardware feature Teletype ever supported.

**The Reality in Code:**
`TeletypeBridge.cpp` is a 600+ line graveyard of ~100 intercepted C functions (`tele_env_attack`, `tele_g_bar`, `tele_ii_rx`, `tele_wng`). 
*   Because Teletype's firmware contains DSP (envelopes, LFOs, and the entire "Geode" synthesizer), `TeletypeTrackEngine` is forced to implement 50+ stub methods to satisfy the C linker. 
*   A sequencer track engine now contains synthesizer voice allocation (`updateGeode()`) just because the VM requires it. This obliterates separation of concerns.

---

## Architectural Refactoring Paths

To fix this, Teletype must be treated as a **First-Class Controller**, not a generic Sequencer Track.

### 1. Elevate Teletype to a Global "Super-Track" (Singleton)
Instead of allowing Teletype to be instantiated as Track 1-8, make Teletype a **Project-Level Global Controller**.
*   **Why:** This matches the reality of the VM footprint and I/O mapping. There is only one Teletype VM. Its `CV 1-4` directly maps to physical `CV 1-4` outputs (or acts as 4 dedicated global Routing Sources).
*   **Benefit:** Resolves the `g_activeEngine` hack, prevents OOM from multi-track instantiation, and fixes the I/O "Double Indirection" nightmare.

### 2. Isolate the VM to a Background Thread
Move `tele_tick()` and script execution out of the real-time track `update()` or `tick()` loop.
*   **Why:** String parsing and imperative loops cannot block the audio/sequencer ticking.
*   **How:** Teletype execution should run in a lower-priority task. When a script fires a trigger (`TR 1`), it pushes a command to a lock-free ring buffer. The Performer high-priority audio engine drains this buffer on the next `tick()` and fires the physical gates.

### 3. Decapitate the DSP (Kill Geode & Envelopes)
Teletype is being used here as a sequencer and logic brain, not a synthesizer. 
*   **Why:** Performer natively does envelopes, LFOs, and CV generation better.
*   **How:** Strip the `teletype` submodule down to just the script parser, variables, and basic I/O logic. Delete `Geode`, `LFO`, and `ENV` from the C codebase before compilation. Remove the 50+ bridge callbacks for them. If a script calls `LFO.RATE`, the bridge intercepts it and maps it to a native Performer `CurveTrack` parameter via the Routing matrix.

### 4. Separate Script State from Sequence Data
Deconstruct `scene_state_t`. 
*   **Why:** `TeletypeTrack` cannot keep swapping a monolithic blob on pattern changes.
*   **How:** Extract the Teletype `Tracker` data into Performer's native memory layout (so it behaves like a normal Performer Pattern). Keep the script text and global variables at the Project level. Patterns switch the sequences; the scripts remain persistent.
